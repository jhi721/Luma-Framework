// Medal of Honor: Airborne — Luma HDR tonemap replacement, shared by BOTH final colour passes: UE3's
// UberPostProcessBlend with DoF on (PS 0xB9548800, VS 0x3C98E35B) and FGammaCorrectionPixelShader with it off. That
// one draw does DoF composite + bloom + parametric grade + output gamma into the 8-bit canvas the UI then draws on,
// and its TWO saturate()s clip 19.4% of a bright frame — highlight detail the engine computes and drops. Strategy
// (analytic in-shader TM, no LUT; the Borderlands GOTY production pipeline): reconstruct the scene mix exactly as the
// game does -> untonemapped; keep the game's own clamped grade as the exact SDR reference; run that same grade with
// its two upper saturate()s as max(0) -> extendedLinear, the HDR signal (range, luminance, chroma); in BT.2020 build
// a soft per-channel ReinhardPiecewise(5, 1.5) hue reference and let MacLeod-Boynton rebuild its hue direction on the
// target's own purity (Hue 1, Blowout 0); PsychoV31 maps it to the user's peak/paper white with the user's grade
// (see "PsychoVDisplayMap") -> hdr; the engine fade last. Output is GAMMA space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper
// white) because the HUD blends src-alpha onto this same canvas right after and a linear buffer washes it out;
// UI_DRAW_TYPE 2 pre-scales by GamePaperWhite/UIPaperWhite. Two transcription rules hold throughout: every texture fetch
// in a dgVoodoo-translated shader is followed by an `and`/`or` pair against b3 (its D3D9 format emulation), and the
// replacements declare all 13 interpolators � VS->PS linkage is by REGISTER, so dropping either shifts colour or the
// whole signature.

// clang-format off
// ORDER IS LOAD-BEARING — do not sort. The game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE any shared header pulls Settings.hlsl, so
// LumaSettings.GameSettings resolves to the real grade struct rather than the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders) before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/PsychoV31HueReference.hlsl" // PsychoVDisplayMapWithHueReference
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3

// HDR / vanilla. 1 = extended native grade + MacLeod-Boynton hue + PsychoV31 display map (default). 0 = vanilla clamped SDR
// reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// UE3 UberPostProcess grade constants, at the register indices the disassembly reads them from.
#define DoFParams                 PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur                PsConstants[9]  // .x max blur near, .y max blur far
#define SceneShadowsAndDesat      PsConstants[10] // .xyz shadows lift (subtracted), .w saturation weight
#define SceneInverseHighLights    PsConstants[11] // .xyz scene scale
#define SceneMidTones             PsConstants[12] // .xyz grade gamma
#define SceneLuminanceWeights     PsConstants[13] // .xyz desaturation luminance weights
#define GammaColorScaleAndInverse PsConstants[14] // .xyz output scale, .w output (inverse) gamma
#define GammaOverlayColor         PsConstants[15] // .xyz tint offset

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState BlurredImageSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene color; .w carries SCENE DEPTH, not alpha
Texture2D<float4> BlurredImage : register(t1);      // quarter-res DoF/bloom blur, stored pre-divided by 4 (scaled back x4 below)

// Luma HDR bloom pyramid mip 0 (Luma_Bloom_impl.hlsl, core DrawBloom): half-res LINEAR fp16, injected by main.cpp
// before this draw, slot must match kLumaBloomSlot. It REPLACES the game's bloom, so there is no double glow.
Texture2D<float4> LumaBloomTexture : register(t6);

// Added in scene-referred LINEAR light, before exposure, grade and the HDR display mapping, so the glow takes
// part in the tonemapping instead of being mapped separately. Half-res source, so one bilinear tap.
float3 ApplyLumaBloom(float3 untonemapped, float2 sceneUV)
{
   if (LumaSettings.GameSettings.LumaBloomEnable <= 0.5)
      return untonemapped;
   const float3 bloom = LumaBloomTexture.SampleLevel(SceneColorTextureSampler_s, sceneUV, 0.0).rgb;
   return untonemapped + max(0.0, bloom) * LumaSettings.GameSettings.BloomIntensity;
}

// The game's grade, verbatim from the disassembly. `clampSDR`: true = vanilla saturate() path, false = unclamped
// (max 0), keeping the highlights' real channel ratio instead of a per-channel hue shift. `outputScale` is normally
// GammaColorScaleAndInverse.xyz; the HDR path passes 1 and re-applies the real scale afterwards.
// A bool rather than a clamp ceiling on purpose: both call sites pass a literal, so fxc folds the dead branch, while
// clamp(c, 0, FLT_MAX) keeps two extra per-pixel min() instructions in the HDR path.
float3 GradeUE3(float3 scene, bool clampSDR, float3 outputScale)
{
   // Head: shadows -> scale -> midtones.
   float3 c = scene - SceneShadowsAndDesat.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   c = c * SceneInverseHighLights.xyz;
   c = PowUE3(c, SceneMidTones.xyz);

   // Tail: desat + tint + output scale + gamma encode.
   float desat = dot(c, SceneLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesat.www + GammaOverlayColor.xyz;
   c = c + desat;
   c = c * outputScale;
   c = clampSDR ? saturate(c) : max(0.0, c); // mul_sat in the original
   return PowUE3(c, GammaColorScaleAndInverse.www);
}

// Shared HDR back half, used by both replaced passes: extended grade -> contrast -> BT.2020 -> soft hue reference ->
// MacLeod-Boynton hue -> display rolloff (dechroma inside it) -> saturation -> engine fade -> paper white -> encode ->
// sanitize.
//
// Both grades are GAMMA-space outputs of the pass's own grade over the same `untonemapped`:
//   `sdrVanillaGamma`    exactly as the original computes it, clamped, fade included (the TONEMAP_TYPE 0 output)
//   `extendedGradeGamma` the same grade with its two upper saturate()s as max(0) and the fade held out: vanilla-exact
//                        below the clip and its own analytic continuation above it, the HDR signal
//
// `fadeScale` / `overlay` carry the engine fade the extended grade was stripped of, applied HERE, after the sliders
// (UberPostProcessBlend scales, GammaCorrection lerps toward a colour). fadeScale 1 / overlay.w 0 disables either.
float3 FinishMOHA(float3 untonemapped, float3 sdrVanillaGamma, float3 extendedGradeGamma, float3 fadeScale, float4 overlay, float2 sceneUV)
{
#if TONEMAP_TYPE >= 1
   // 3. The extended grade is the HDR signal: range, luminance and chroma all come from the game's own math.
   float3 extendedLinear = gamma_to_linear(extendedGradeGamma);

   // 4. Colour stage, in a BT.2020 working space (converted back to BT.709 before the display map: gamut-correct
   // handling of saturated highlights, not a display-gamut expansion). Soft hue reference: ReinhardPiecewise(x, 5, 1.5)
   // per channel, where the RenoDX BL1 port builds it. Linear below 1.5 and rolling toward 5 above, it compresses a
   // saturated highlight's strong channel before its weak ones, so the hue leans the way the vanilla clip leaned it,
   // without the clip's whitening. MacLeod-Boynton then rebuilds that reference's hue direction on the target's own
   // purity and T = L + M anchor (hue strength 1, chrominance 0), before the display map as RenoDX applies it.
   // 5. The display map (see "PsychoVDisplayMapWithHueReference" for both).
   float3 hdr = PsychoVDisplayMapWithHueReference(extendedLinear,
                                                  LumaSettings.GameSettings.Contrast,
                                                  LumaSettings.GameSettings.Saturation,
                                                  LumaSettings.GameSettings.HighlightsDesaturation);

   // Re-apply the engine fade linearly, LAST, after the display map: its contrast pivots on mid-gray, so a fade
   // before it would never reach black. At rest both are no-ops.
   hdr *= fadeScale;
   hdr = lerp(hdr, overlay.xyz, overlay.w);

   float3 outColor = hdr; // linear, 1.0 = paper white
#else
   // Vanilla reference: linearize the clamped SDR grade.
   float3 outColor = gamma_to_linear(sdrVanillaGamma);
#endif

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
   outColor *= GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true);
   outColor = max(0.0, outColor); // negatives would turn into NaN in linear_to_gamma below
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR.
#if TONEMAP_TYPE >= 1
   if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(outColor, sceneUV, true, LumaSettings.DisplayMode != 0, max(LumaSettings.UIPaperWhiteNits, 1.0));
   }
#endif
#endif
   // Sanitize LAST: the extended grade, the hue stage, encode and dither can each emit NaN, and a NaN in a unorm target reads
   // back black. Also covers cb4[15] (-nan(ind) on a load fade), which vanilla's saturate flushes to 0.
   outColor = IsNaN_Strict(outColor) ? 0.0 : outColor; // explicit exponent/mantissa bit test
   outColor = max(0.0, outColor);

#if DEVELOPMENT
   // Bring-up bisect (DEV only, all off = normal): DevSetting01 solid magenta (replacement is bound), DevSetting02
   // the reconstructed scene mix (t0/t1/UVs/masks read right), DevSetting03 the vanilla clamped grade.
   if (LumaSettings.DevSetting01 > 0.5)
      return float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      return linear_to_gamma(saturate(untonemapped));
   if (LumaSettings.DevSetting03 > 0.5)
      return sdrVanillaGamma;
#endif

   return outColor;
}

// Core tonemap. `sceneUV` is TEXCOORD1 (t0), `blurUV` is TEXCOORD0 (t1) — note the original samples t0 with v6
// and t1 with v5. Returns the value to store in the canvas (gamma space when POST_PROCESS_SPACE_TYPE==0).
float3 RunMOHATonemap(float2 blurUV, float2 sceneUV)
{
   // 1. Scene mix, exactly as vanilla: depth-driven DoF weight, bloom at x4, normalized by the weight sum.
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   const float sceneWeight = saturate(1.0 - DoFBlurAmount(scene.w, DoFParams, DoFMaxBlur)); // UE3 packs scene depth in the fp16 alpha

   float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   // The engine's combined DoF blur + native bloom target, stored pre-divided by 4, hence the x4; vanilla's unorm view
   // also capped it at 4.0, a cap the fp16 upgrade lifted. NEVER scale this by BloomIntensity: DoF and bloom are SUMMED
   // in it, and in the sniper scope this term IS the frame.
   const float weightSum = blurred.w * 4.0 + sceneWeight;
   float3 untonemapped = scene.xyz * sceneWeight + blurred.xyz * 4.0;
   untonemapped *= (abs(weightSum) > 0.0) ? rcp(weightSum) : FLT_MAX; // rcp guard, as the original does

   // AFTER the normalisation: the weight sum belongs to the DoF composite, and dividing the glow by it would
   // make the bloom fade in and out with the defocus amount.
   untonemapped = ApplyLumaBloom(untonemapped, sceneUV);

   // Scene exposure (multiplier), scene-referred / pre-grade; the SDR reference below derives from the same
   // `untonemapped`, so the grade tracks the exposure change.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // The grade's output scale doubles as the engine FADE (level start, cutscenes). It stays OUT of the extended grade
   // and is re-applied as a gain at the end, after the display map, so it cannot bias the hue stage or the rolloff.
   const float3 outputScale = GammaColorScaleAndInverse.xyz;

   // 2. The game's own grade (artistic intent), gamma-encoded SDR: vanilla-exact, fade included. The extended grade
   // carries no fade (outputScale = 1); it is re-applied inside FinishMOHA as a plain gain.
   return FinishMOHA(untonemapped, GradeUE3(untonemapped, true, outputScale), GradeUE3(untonemapped, false, 1.0), outputScale, float4(0.0, 0.0, 0.0, 0.0), sceneUV);
}

// The DoF-off final pass: UE3 FGammaCorrectionPixelShader (PS 0x52B868E0, VS 0xA2F269CA). With "bAllowDepthOfField =
// False" this makes the final LDR write instead, reading only the fp16 scene (t0) with its own register map.
#define GcColorScale   PsConstants[12] // .xyz ColorScale
#define GcOverlayColor PsConstants[13] // .xyz OverlayColor, .w its blend weight (this pass's fade)
#define GcInverseGamma PsConstants[14] // .x inverse display gamma (1/DisplayGamma, 0.4545 at the ini default 2.2)

// `sceneUV` is TEXCOORD0 (v5) — unlike UberPostProcessBlend, which reads the scene from TEXCOORD1.
float3 RunMOHAGammaCorrection(float2 sceneUV)
{
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   // The engine skips UberPostProcessBlend entirely in this configuration, so it has NO bloom of its own — the
   // Luma pyramid is the only glow this path ever gets, which makes it an addition here rather than a swap.
   float3 untonemapped = ApplyLumaBloom(scene.xyz, sceneUV) * LumaSettings.GameSettings.Exposure;

   // The original ends in a branch on PsConstants[8].x selecting a colour-grading LUT blend. That LUT is never bound,
   // so dgVoodoo folded its six sample stages to the constant (0,0,0,1). Only the direct path is reproduced.
   // Vanilla-exact: saturate(lerp(scene * ColorScale, Overlay.rgb, Overlay.a)) then the inverse-gamma pow.
   // Extended: overlay (the fade) held OUT, FinishMOHA re-applies it after the display map; the vanilla saturate()
   // (mad_sat in the original) as a lower-only max(0), so highlights keep their real channel ratio.
   const float3 scaled = untonemapped * GcColorScale.xyz;
   return FinishMOHA(untonemapped, PowUE3(saturate(lerp(scaled, GcOverlayColor.xyz, GcOverlayColor.w)), GcInverseGamma.xxx),
                     PowUE3(max(0.0, scaled), GcInverseGamma.xxx), 1.0, GcOverlayColor, sceneUV);
}
