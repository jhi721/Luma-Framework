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
//   6. RenoDX's PsychoV31 maps the result to the user's peak and paper white, with the user's contrast, saturation and highlights
//      desaturation.
// The RenoDX donor and its MacLeod–Boynton emulation are ported exactly.
//
// Output is stored gamma-encoded (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) so the game's gamma SDR HUD blends on top as in
// vanilla (a linear buffer washes it out). With UI_DRAW_TYPE >= 2 the scene is also pre-scaled by GamePaperWhite / UIPaperWhite so
// the HUD lands at its own paper white. The Display Composition decodes, applies paper white, encodes scRGB and gamut maps at present.

// clang-format off
// Do not sort: the game-local "Includes/Common.hlsl" must come first (see there). Sorted below the shared "../Includes/*" block,
// Settings.hlsl's empty dummy wins -> "invalid subscript 'BloomIntensity'".
#include "Includes/Common.hlsl"             // LumaGameSettings (the grade sliders)
#include "../Includes/Color.hlsl"
#include "../Includes/PsychoV31HueReference.hlsl" // PsychoVDisplayMapWithHueReference
// clang-format on

// HDR / vanilla. 1 = extended UE3 grade + PsychoV31 display map (default). 0 = vanilla clamped SDR reference.
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
   const float3 extendedLinear = gamma_to_linear(GradeUE3_FromPostMidtones(GradeUE3_PostMidtones(untonemapped, false), false));

   // 4-6. The RenoDX BL1 colour stage (common.hlsli, ApplyCustomGrading; its hue reference is the only hue source), then the
   // display map: see "PsychoVDisplayMapWithHueReference". The output is linear BT.709, 1.0 = paper white.
   outColor = PsychoVDisplayMapWithHueReference(extendedLinear,
                                                LumaSettings.GameSettings.Contrast,
                                                LumaSettings.GameSettings.Saturation,
                                                LumaSettings.GameSettings.HighlightDechroma);
#else
   // Vanilla reference: the exact clamped SDR grade (gamma-encoded), linearized.
   outColor = gamma_to_linear(saturate(GradeUE3_FromPostMidtones(postMidtones, true)));
#endif

   // --- Common tail: the UI paper white pre-scale and the post-process space encode ---
   outColor *= GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, false);
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
      ApplyOutputDithering(outColor, v1.xy, false, true, LumaSettings.UIPaperWhiteNits);
   }
#endif
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white. Mirrored: the PQ dither
   // can step a black channel below 0.
   outColor = linear_to_gamma(outColor, GCT_MIRROR);
#if TONEMAP_TYPE >= 1
   if (dither && LumaSettings.DisplayMode == 0)
   {
      ApplyOutputDithering(outColor, v1.xy, true, false, 1.0);
   }
#endif
#endif
}
