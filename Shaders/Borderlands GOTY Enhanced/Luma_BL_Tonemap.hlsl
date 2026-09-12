// Borderlands GOTY Enhanced — Luma HDR tonemap replacement (shared impl).
//
// Replaces the game's UE3.5 "UberPostProcess" final color pass (PS 0xB030BAA6 with FXAA luma output,
// 0xFE88487E without). The game runs a parametric grade then saturate()s to SDR, throwing away the real
// highlight detail the pre-tonemap RGBA16F scene color holds (sun/sky/emissive/specular/FX, ~5-6 stops).
//
// Strategy (analytic in-shader TM, no LUT):
//   1. Rebuild the native scene + bloom mix exactly as the game does.                         -> untonemapped
//   2. Run the exact vanilla clamped grade: the SDR reference (TONEMAP_TYPE 0) and the FXAA luma source.
//   3. HDR: run the same analytic UE3 grade as an extended function (SDR clamps and floors removed,
//      nothing else changed). It supplies range, luminance and chroma.                       -> extendedLinear
//   4. DICE maps the extended result to the user's peak / paper white, in BT.2020.            -> hdr
//   5. Restore hue only, toward a soft per-channel ReinhardPiecewise(5, 1.5) reference built from the
//      same extended grade — the RenoDX BL1 hue donor, applied after DICE through Luma's JzAzBz
//      RestoreHueAndChrominance. Target chroma is kept; blowout is zero.
//   6. Optional user grading (HighlightDechroma / Saturation / Contrast).
//
// The RenoDX donor precedent is exact; the post-DICE JzAzBz placement is Luma's.
//
// Output is stored in GAMMA space (POST_PROCESS_SPACE_TYPE==0, 1.0 = paper white) so the game's gamma-SDR
// HUD blends on top like vanilla (a linear buffer washes it out). With UI_DRAW_TYPE==2 the scene is also
// pre-scaled by GamePaperWhite/UIPaperWhite so the HUD lands at its own paper white. The core Display
// Composition decodes gamma + applies paper-white scaling + scRGB encode + gamut map at present.

// clang-format off
// ORDER IS LOAD-BEARING — do not sort. The game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE any shared header pulls Settings.hlsl, so
// LumaSettings.GameSettings resolves to the real grade struct rather than the empty dummy. When clang-format
// sorted it below the shared "../Includes/*" block, Settings.hlsl's dummy won -> "invalid subscript 'BloomIntensity'".
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders) before LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // RestoreHueAndChrominance, SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // Reinhard::ReinhardPiecewise: soft hue reference
#include "Includes/RenoDX_MacLeodBoynton.hlsl" // BL1_RenoDX::ApplyHueEmulationBT2020 (BL_HDR_COLOR_STYLE 1)
// clang-format on

// HDR / vanilla. 1 = extended UE3 grade + DICE display map (default). 0 = vanilla clamped SDR reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Colour stage (A/B). 0 = production: DICE, then Luma's JzAzBz hue-only restoration toward the soft reference.
// 1 = RenoDX BL1 style: MacLeod–Boynton hue/purity emulation toward the same soft reference (Hue Shift 1.0,
// Blowout 0), applied BEFORE DICE as RenoDX applies it before its display map, and no post-DICE restoration.
// Same reconstruction, same donor, same DICE and gamut clip either way, so the pair isolates operator + placement.
// DEVELOPMENT-only checkbox in main.cpp; a shipped build compiles the 0 side.
#ifndef BL_HDR_COLOR_STYLE
#define BL_HDR_COLOR_STYLE 0
#endif

// HighlightDechroma is an optional user slider (see step 6 below); default 0 = off (only the mandatory DICE/gamut
// desaturation applies).

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

// Grade steps 1-3 (shadows -> highlights -> midtones), lifted verbatim from the decompiled shader. Returns the
// post-midtones color: the exact point original 0xB030BAA6 takes its FXAA luma from.
// `clampSDR`: true = the vanilla SDR path, verbatim: upper saturate() clamps and 1e-4 floors. false = the same
// grade as an extended HDR function: the upper saturate()s become max(0, ...) and the 1e-4 floors become 0 (so
// black reaches 0 rather than the SDR floor's 0.0152 in gamma); every parameter, every other operation and the
// gamma exponent are unchanged, so highlights keep their real channel ratio instead of the per-channel
// saturate() hue shift.
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

// Tail of the grade: desat + overlay + scale + gamma encode (verbatim from the back half of the original).
// Same `clampSDR` contract as GradeUE3_PostMidtones.
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

float3 GradeUE3(float3 scene, bool clampSDR)
{
   return GradeUE3_FromPostMidtones(GradeUE3_PostMidtones(scene, clampSDR), clampSDR);
}

// Core tonemap. `v0`/`v1` are the game's interpolators (TEXCOORD0/1). Returns scene-referred linear color
// (1.0 = paper white) in `outColor`, and the FXAA luma the game's edge CS expects in `outLuma`.
void RunBLTonemap(float4 v0, float2 v1, out float3 outColor, out float outLuma)
{
   // 1. Scene mix (scene color attenuated by inverse-blur weight, plus bloom) — the real pre-tonemap HDR.
   float3 sceneColor = SceneColorTexture.Sample(SceneColorTextureSampler_s, DynamicScale.xy * v0.zw).xyz;
   float4 blurred = BlurredImage.Sample(BlurredImageSampler_s, v1.xy);
   float3 untonemapped = sceneColor * saturate(1.0 - blurred.w) + blurred.xyz * LumaSettings.GameSettings.BloomIntensity;

   // Scene exposure (multiplier), scene-referred / pre-grade; the SDR reference below derives from the same
   // `untonemapped`, so the grade tracks the exposure change.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // 2. The vanilla grade's post-midtones intermediate: the FXAA luma source on both paths, and the first half
   // of the SDR reference below.
   float3 postMidtones = GradeUE3_PostMidtones(untonemapped, true);

   // FXAA luma (game's edge CS reads SV_Target1): BT.709 luma of the post-midtones color (see GradeUE3_PostMidtones).
   outLuma = 0.25 * log2(dot(postMidtones, float3(0.212670997, 0.715160012, 0.0721689984)) * 15.0 + 1.0);

#if TONEMAP_TYPE >= 1
   // 3. The extended HDR grade (see GradeUE3), computed once: DICE and the hue reference both start from it.
   float3 extendedLinear = gamma_to_linear(GradeUE3(untonemapped, false));

   // 4. Display rolloff to the user's peak/paper-white nits (DICE, hue-preserving by luminance).
   const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
   // The display map runs in a BT.2020 working space (round-tripped back to BT.709 below): gamut-correct
   // handling of highly saturated highlights, not a display-gamut expansion.
   float3 extendedBT2020 = BT709_To_BT2020(extendedLinear);
#if BL_HDR_COLOR_STYLE == 1
   // RenoDX BL1 colour stage: hue direction from the soft reference, the target's own purity kept (Hue Shift 1.0,
   // Blowout 0 - the RenoDX BL1 defaults), in BT.2020 before the display map. Nothing is restored after DICE.
   float3 diceInBT2020 = BL1_RenoDX::ApplyHueEmulationBT2020(extendedBT2020, Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5), 1.0, 0.0);
#else
   float3 diceInBT2020 = extendedBT2020;
#endif
   // Tonemap luminance in PQ (hue-preserving: rgb scaled by the luminance ratio), then CORRECT_CHANNELS_BEYOND_
   // PEAK_WHITE pulls any channel that still exceeds peak back into range by desaturating it toward white. Modern
   // HDR panels clip each rgb channel at peak individually, so an uncorrected saturated highlight (e.g. a bright
   // blue) would hard-clip with a hue shift; the correction trades a little highlight saturation for a clean,
   // in-range rolloff. DesaturationVsDarkeningRatio 1.0
   // (default) = contain by desaturating, not darkening (darkening flattens detail).
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // DICE converts InOutColorSpace -> ProcessingColorSpace on entry and back on exit. The colour is already
   // BT.2020 here and is converted back below, so the default CS_BT709 would make it convert a SECOND time and
   // run its shoulder trigger (an RGB average), its compression and its channel containment on doubly-narrowed
   // primaries. Neutrals cancel out; saturated highlights do not.
   ds.InOutColorSpace = CS_BT2020;
   float3 hdr = DICETonemap(diceInBT2020 * paperWhite, peakWhite, ds) / paperWhite;
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));

#if BL_HDR_COLOR_STYLE == 0
   // 5. Hue restoration toward the soft reference: the extended grade run through ReinhardPiecewise(x, 5, 1.5)
   // per channel in BT.2020, where the RenoDX BL1 port builds it (common.hlsli, ApplyCustomGrading). Linear
   // below 1.5 and rolling toward 5 above, it compresses a saturated highlight's strong channel before its weak
   // ones, so the hue leans the way the vanilla clip leaned it — without the clip's whitening. Hue strength 1.0,
   // chrominance 0.0: only the hue direction is taken; chroma stays the DICE result's, lightness the target's.
   // Reinhard.hlsl's ReinhardPiecewise is the RenoDX formula (identical at the x_min = 0 it fixes). RenoDX applies
   // its emulation before its display map in MacLeod–Boynton; Luma applies the shared JzAzBz operator after DICE.
   float3 hueRefBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5);
   float3 hueRef = BT2020_To_BT709(hueRefBT2020);
   hdr = RestoreHueAndChrominance(hdr, hueRef, 1.0, 0.0);
#endif

   // 6. Perceptual highlight dechroma: bright sources fade toward white as luminance approaches peak (eye/sensor
   // saturation). Keeps colored mid-highlights, whitens only the brightest (so warm-tinted white lamps read as
   // neutral white at peak).
   const float highlightDechroma = LumaSettings.GameSettings.HighlightDechroma;
   if (highlightDechroma > 0.0)
   {
      // Map the slider to an exponent in [1, 0.05]: at the 1.0 max the exponent stays > 0 so mid-tones keep
      // their color (dcWeight < 1) and only luminance->peak fades to white. An exponent of exactly 0 would make
      // pow(x,0)=1 everywhere -> Saturation(hdr,0) -> full-frame greyscale, which is not what the slider means.
      float dcExp = lerp(1.0, 0.05, highlightDechroma);
      float dcWeight = saturate(pow(saturate(GetLuminance(hdr) / peakWhite), dcExp));
      hdr = Saturation(hdr, 1.0 - dcWeight);
   }

   // User saturation (luminance-relative RGB lerp, shared helper). 1.0 = neutral.
   hdr = Saturation(hdr, LumaSettings.GameSettings.Saturation);

   // User contrast: slope around 18% mid-gray (linear, 1.0 = paper white). 1.0 = vanilla. Excursions are
   // caught by the NaN/clamp tail; > peak highlights are acceptable for a creative slider.
   const float midGray = 0.18;
   hdr = (hdr - midGray) * LumaSettings.GameSettings.Contrast + midGray;

   outColor = hdr; // linear, 1.0 = paper white
#else
   // Vanilla reference: the exact clamped SDR grade (gamma-encoded), linearized.
   float3 vanillaSDRGamma = GradeUE3_FromPostMidtones(postMidtones, true);
   outColor = gamma_to_linear(saturate(vanillaSDRGamma));
#endif

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
#if UI_DRAW_TYPE >= 2
   // Pre-scale so the gamma-SDR HUD (drawn on top) lands at UIPaperWhite after composition rescales by it.
   outColor *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   // Sanitize: the scene carries small negative/WCG values; the extended grade + hue restore + gamma encode can
   // emit NaN or negatives (linear_to_gamma of a negative is NaN). Clamp so no garbage reaches the swapchain.
   outColor = (outColor == outColor) ? outColor : 0.0; // NaN -> 0 (NaN != NaN)
   outColor = max(0.0, outColor);
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
   // Anti-banding dither in the stored gamma space (the core composition does not dither). Animated triangular
   // noise; sub-perceptual at bit depth 9 so the later SMAA/RCAS passes don't visibly amplify it. HDR path only.
#if TONEMAP_TYPE >= 1
   if (LumaSettings.GameSettings.Dithering > 0.5)
      ApplyDithering(outColor, v1.xy, true, 1.0, DITHERING_BIT_DEPTH, LumaSettings.FrameIndex, true);
#endif
#endif
}
