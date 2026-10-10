// Replaces the game's UE3.5 UberPostProcess final color pass (0xB030BAA6 with the FXAA luma output, 0xFE88487E without). The game
// grades and then saturate()s to SDR, dropping the highlight detail (sun, sky, emissives, specular, FX; ~5-6 stops) of its RGBA16F
// pre-tonemap scene. Analytic, no LUT:
//   1. Rebuild the game's scene + bloom mix exactly (untonemapped).
//   2. Run the exact vanilla clamped grade: the SDR reference (TONEMAP_TYPE 0) and the FXAA luma source.
//   3. HDR: the same analytic UE3 grade as an extended function (SDR clamps and floors removed, nothing else changed). It supplies
//      range, luminance and purity (extendedLinear).
//   4. Soft hue reference: the extended grade through a per-channel ReinhardPiecewise(5, 1.5) in BT.2020, the RenoDX BL1 hue donor,
//      bending the way the vanilla clip bent without its whitening.
//   5. MacLeod–Boynton hue emulation in BT.2020: the reference's hue direction on the target's own purity (Hue Shift 100%, Blowout 0,
//      the RenoDX BL1 contract), before the display map as RenoDX applies it.
//   6. DICE maps the result to the user's peak and paper white, in BT.2020.
//   7. Optional user grading: Contrast before the colour stage (step 3b) so the rolloff contains it, the highlight dechroma inside
//      DICE (step 6), Saturation last.
// The RenoDX donor and its MacLeod–Boynton emulation are ported exactly; DICE is Luma's.
//
// Output is stored gamma-encoded (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) so the game's gamma SDR HUD blends on top as in
// vanilla (a linear buffer washes it out). With UI_DRAW_TYPE >= 2 the scene is also pre-scaled by GamePaperWhite / UIPaperWhite so
// the HUD lands at its own paper white. The Display Composition decodes, applies paper white, encodes scRGB and gamut maps at present.

// clang-format off
// Do not sort: the game-local "Includes/Common.hlsl" must come first (see there). Sorted below the shared "../Includes/*" block,
// Settings.hlsl's empty dummy wins -> "invalid subscript 'BloomIntensity'".
#include "Includes/Common.hlsl"             // LumaGameSettings (the grade sliders)
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // Reinhard::ReinhardPiecewise: soft hue reference
#include "../Includes/MacLeodBoynton.hlsl"      // MacLeodBoynton::HueOnlyBT2020 (RenoDX-derived hue/purity model)
// clang-format on

// HDR / vanilla. 1 = extended UE3 grade + DICE display map (default). 0 = vanilla clamped SDR reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// --- Game bindings (must match the original shader exactly) ---
cbuffer _Globals : register(b0)
{
   float4 PackedParameters : packoffset(c0);
   float2 MinMaxBlurClamp : packoffset(c1);
   float4 SceneShadowsAndDesaturation : packoffset(c2);
   float4 SceneInverseHighLights : packoffset(c3);
   float4 SceneMidTones : packoffset(c4);
   float4 SceneScaledLuminanceWeights : packoffset(c5);
   float4 GammaColorScaleAndInverse : packoffset(c6);
   float4 GammaOverlayColor : packoffset(c7);
}

cbuffer PSOffsetConstants : register(b2)
{
   float4 ScreenPositionScaleBias : packoffset(c0);
   float4 MinZ_MaxZRatio : packoffset(c1);
   float4 DynamicScale : packoffset(c2);
}

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState BlurredImageSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float4> BlurredImage : register(t1);

// Grade steps 1-3 (shadows, highlights, midtones), verbatim from the decompiled shader. Returns the post-midtones color, where the
// original 0xB030BAA6 takes its FXAA luma. "clampSDR" true is the vanilla SDR path, verbatim (upper saturate()s, 1e-4 floors). False
// is the same grade as an extended HDR function: the upper saturate()s become max(0, ...) and the 1e-4 floors 0 (black reaches 0
// rather than the SDR floor's 0.0152 in gamma); every parameter, every other operation and the gamma exponent are unchanged, so
// highlights keep their channel ratio instead of the per-channel saturate() hue shift.
float3 GradeUE3_PostMidtones(float3 scene, bool clampSDR)
{
   float3 c = clampSDR ? saturate(-SceneShadowsAndDesaturation.xyz + scene)
                       : max(0.0, -SceneShadowsAndDesaturation.xyz + scene);
   c = SceneInverseHighLights.xyz * c;
   c = clampSDR ? max(9.99999975e-05, abs(c)) : max(0.0, abs(c));
   c = log2(c);
   c = SceneMidTones.xyz * c;
   return exp2(c);
}

// The grade's tail, verbatim from the back half of the original: desaturation, overlay, scale and gamma encode. "clampSDR" as in
// GradeUE3_PostMidtones.
float3 GradeUE3_FromPostMidtones(float3 c, bool clampSDR)
{
   float desat = dot(c, SceneScaledLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesaturation.www + GammaOverlayColor.xyz;
   c = c + desat;
   c = clampSDR ? saturate(GammaColorScaleAndInverse.xyz * c) : max(0.0, GammaColorScaleAndInverse.xyz * c);
   c = clampSDR ? max(9.99999975e-05, c) : max(0.0, c);
   c = log2(c);
   c = GammaColorScaleAndInverse.www * c;
   return exp2(c); // gamma-encoded graded color
}

// "v0" / "v1" are the game's interpolators (TEXCOORD0/1). Returns the color in post-process space (1.0 = paper white) in "outColor",
// and the FXAA luma the game's edge CS expects in "outLuma".
void RunBLTonemap(float4 v0, float2 v1, out float3 outColor, out float outLuma)
{
   // 1. The scene mix, the real pre-tonemap HDR: the scene attenuated by the blur's inverse weight, plus bloom.
   float3 sceneColor = SceneColorTexture.Sample(SceneColorTextureSampler_s, DynamicScale.xy * v0.zw).xyz;
   float4 blurred = BlurredImage.Sample(BlurredImageSampler_s, v1.xy);
   float3 untonemapped = sceneColor * saturate(1.0 - blurred.w) + blurred.xyz * LumaSettings.GameSettings.BloomIntensity;

   // Scene exposure (multiplier), scene-referred / pre-grade; the SDR reference below derives from the same
   // `untonemapped`, so the grade tracks the exposure change.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // 2. The vanilla grade's post-midtones intermediate: the FXAA luma source on both paths, and the first half
   // of the SDR reference below.
   float3 postMidtones = GradeUE3_PostMidtones(untonemapped, true);

   // FXAA luma, read by the game's edge CS at SV_Target1: the original's log2 curve of the BT.709-weighted sum of the post-midtones
   // color, a graded intermediate before the gamma exponent (see GradeUE3_PostMidtones).
   outLuma = 0.25 * log2(dot(postMidtones, float3(0.212670997, 0.715160012, 0.0721689984)) * 15.0 + 1.0);

#if TONEMAP_TYPE >= 1
   // 3. The extended HDR grade (clampSDR false), computed once: the hue reference and the display map both start
   // from it.
   float3 extendedLinear = gamma_to_linear(GradeUE3_FromPostMidtones(GradeUE3_PostMidtones(untonemapped, false), false));

   // 3b. Contrast before the display map so DICE contains whatever it pushes up: after the rolloff it would escape the peak, and
   // nothing downstream re-contains it. A power around mid-gray, the repo's form (RenoDX_Contrast in the Call of Duty includes); 0.18
   // is mid-gray here too, display-referred with 1.0 = paper white (gamma code ~0.46). [branch] on a cbuffer uniform: at the 1.0
   // default this must be a bit-exact no-op. The pow is spelled out with a floored log2 so Contrast 0 on a black pixel is
   // 0 * log2(1e-30) = 0 rather than pow(0, 0) = NaN.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      extendedLinear = exp2(LumaSettings.GameSettings.Contrast * log2(max(extendedLinear / MidGray, 1e-30))) * MidGray;
   }

   const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
   // The colour stage and the display map run in a BT.2020 working space (round-tripped back to BT.709 below):
   // gamut-correct handling of highly saturated highlights, not a display-gamut expansion.
   float3 extendedBT2020 = BT709_To_BT2020(extendedLinear);

   // 4. Soft hue reference, per channel in BT.2020 where the RenoDX BL1 port builds it (common.hlsli, ApplyCustomGrading). Linear
   // below 1.5 and rolling toward 5 above, it compresses a saturated highlight's strong channel before its weak ones (see the
   // header). Reinhard.hlsl's ReinhardPiecewise is the RenoDX formula (identical at the x_min = 0 it fixes). BL1 policy: this donor,
   // and nothing else, supplies the hue.
   float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5);

   // 5. MacLeod–Boynton hue emulation (the RenoDX BL1 colour stage): the reference's hue direction rebuilt on the
   // target's own purity and T = L + M anchor. BL1 policy, all of it chosen here and none of it inside the model:
   // hue strength 1.0 and chrominance strength 0 (RenoDX's "Hue Shift 100%, Blowout 0"), and, as RenoDX applies
   // it before its display map, before DICE. Nothing is restored after DICE.
   float3 diceInBT2020 = MacLeodBoynton::HueOnlyBT2020(extendedBT2020, hueReferenceBT2020);

   // 6. Display rolloff to the user's peak and paper white: DICE maps the luminance in PQ and scales rgb by its ratio (hue-preserving),
   // then CORRECT_CHANNELS_BEYOND_PEAK_WHITE desaturates toward white any channel still above peak. Panels clip each channel at peak,
   // so an uncorrected saturated highlight (a bright blue) would clip with a hue shift. DesaturationVsDarkeningRatio 1 (the default)
   // contains by desaturating rather than darkening, which flattens detail.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Highlight dechroma is DICE's rather than a pass of our own afterwards: it ramps on the max channel (by luminance a bright blue
   // never triggers), exists only between ShoulderStart * PeakWhite and peak (1/3 of peak for this type, so mid-tones are untouched),
   // and runs inside the containment in the processing primaries. 0 is off for the output but not the cost: DICE's guard carries no
   // [branch], so fxc flattens it for every pixel above the shoulder.
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   // DICE converts InOutColorSpace to ProcessingColorSpace on entry and back on exit. The colour is already BT.2020 here, so the
   // default CS_BT709 would convert it a second time and run the shoulder trigger (an RGB average), the compression and the channel
   // containment on doubly-narrowed primaries. Neutrals cancel out; saturated highlights do not.
   ds.InOutColorSpace = CS_BT2020;
   outColor = DICETonemap(diceInBT2020 * paperWhite, peakWhite, ds) / paperWhite;
   outColor = BT2020_To_BT709(SimpleGamutClip(outColor, true));

   // 7. User saturation last, after the display map, as the repo does (Color.hlsl's lerp from relative luminance). 1 = neutral.
   // The output is linear, 1.0 = paper white.
   outColor = Saturation(outColor, LumaSettings.GameSettings.Saturation);
#else
   // Vanilla reference: the exact clamped SDR grade (gamma-encoded), linearized.
   outColor = gamma_to_linear(saturate(GradeUE3_FromPostMidtones(postMidtones, true)));
#endif

   // --- Common tail: the UI paper white pre-scale and the post-process space encode ---
   outColor = PreScaleForUIPaperWhite(outColor);
   // The extended grade and the MacLeod-Boynton hue stage can emit NaN or negatives (the scene carries small negative/WCG values):
   // none may reach the swapchain.
   outColor = (outColor == outColor) ? outColor : 0.0; // NaN -> 0 (NaN != NaN)
   outColor = max(0.0, outColor);
#if POST_PROCESS_SPACE_TYPE == 0
   // Anti-banding dither, one step of the output quantizer: in HDR the 10-bit BT.2020 PQ code, from linear before the gamma encode
   // below; in SDR the 8-bit code, after it.
#if TONEMAP_TYPE >= 1
   const bool dither = LumaSettings.GameSettings.Dithering > 0.5;
   if (dither && LumaSettings.DisplayMode != 0)
   {
      const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
      float3 pq = Linear_to_PQ(BT709_To_BT2020(outColor * pqScale), GCT_MIRROR);
      ApplyDithering(pq, v1.xy, true, 1.0, 10u, LumaSettings.FrameIndex, true);
      outColor = BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale;
   }
#endif
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white. Mirrored: the PQ dither
   // can step a black channel below 0.
   outColor = linear_to_gamma(outColor, GCT_MIRROR);
#if TONEMAP_TYPE >= 1
   if (dither && LumaSettings.DisplayMode == 0)
      ApplyDithering(outColor, v1.xy, true, 1.0, 8u, LumaSettings.FrameIndex, true);
#endif
#endif
}
