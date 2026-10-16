// Mass Effect (2007) — Luma HDR tonemap replacement, split across the TWO colour passes that end a frame
// (devkit-measured order): UberPostProcessBlend (PS 0xAC8341E0) reads scene A (fp16, depth in alpha) plus the
// quarter-res DoF/bloom blur into scene B; copy 0x1E37D75B mirrors B into A; FGammaCorrection (PS 0x17CE0932)
// reads A and writes the canvas (dgVoodoo's 8-bit backbuffer, upgraded to an fp16 mirror) the HUD blends onto.
// dgVoodoo 2.81.3 emits ps_4_0 twins (0x786BC3B3, 0x3BEF1CD6) that include these entry points. The passes CHAIN, so
// the work is split: the uber replacement runs the HDR block and leaves LINEAR light (1.0 = paper white) in the
// intermediate; the gamma replacement adds that pass's fade, the UI paper-white pre-scale, the gamma encode, dither
// and sanitize.
// HDR pipeline (analytic in-shader, no LUT; the Borderlands GOTY / MoH Airborne production pipeline): reconstruct the
// scene mix exactly as the game does -> untonemapped; keep the game's own clamped grade as the exact SDR reference;
// run that same grade with its upper saturate()s as max(0), decoded to display-linear -> the HDR signal; in BT.2020 a
// soft per-channel ReinhardPiecewise(5, 1.5) hue reference, whose hue direction MacLeodBoynton::HueOnlyBT2020 rebuilds
// on the signal's own purity; PsychoV31 maps it to the user's peak/paper white (see "PsychoVDisplayMap"); the engine
// fade last, through the vanilla curve.
// The engine SKIPS the uber in elevators and some loading scenes; main.cpp reports that through
// LumaData.GameData.UberRanThisFrame and the gamma replacement then runs the HDR block itself off the ungraded scene
// (DoF and the Luma glow composited by the standalone DOFAndBloom blend 0x218D4AE2 when that chain has one).
// Measured cb4 exponents: uber 1.0, copy 1.0, gamma 0.625 = 1/1.6 (the game's DisplayGamma), so the intermediate
// holds linear graded light. main.cpp hands that exponent over as GameSettings.DisplayGammaInverse, since the
// grade needs it but it lives in another pass. Two dgVoodoo rules hold throughout: every fetch is followed by an
// `and`/`or` pair against b3, and the entry points declare all 13 interpolators - linkage is by REGISTER.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. Game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS before any shared header pulls Settings.hlsl, or GameSettings resolves to the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders) before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/PsychoV31HueReference.hlsl" // PsychoVDisplayMapWithHueReference
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3, DoFBlurAmount

// HDR / vanilla. 1 = extended native grade + MacLeod-Boynton hue + PsychoV31 display map (default). 0 = vanilla clamped SDR
// reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// UE3 UberPostProcess grade constants, at the register indices the 0xAC8341E0 disassembly reads them from. This UE3
// build leaves row 9 unused, so everything from the blur clamp on sits one row above MoH Airborne's map.
#define DoFParams                 PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur                PsConstants[10] // .x max blur near, .y max blur far
#define SceneShadowsAndDesat      PsConstants[11] // .xyz shadows lift (subtracted), .w saturation weight
#define SceneInverseHighLights    PsConstants[12] // .xyz scene scale
#define SceneMidTones             PsConstants[13] // .xyz grade gamma
#define SceneLuminanceWeights     PsConstants[14] // .xyz desaturation luminance weights
#define GammaColorScaleAndInverse PsConstants[15] // .xyz output scale, .w output (inverse) gamma — measured 1.0: this pass does not encode
#define GammaOverlayColor         PsConstants[16] // .xyz tint offset

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState BlurredImageSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene A; .w carries SCENE DEPTH, not alpha
Texture2D<float4> BlurredImage : register(t1);      // quarter-res DoF/bloom blur, stored pre-divided by 4 (scaled back x4 below)

// Luma bloom pyramid mip 0 (core DrawBloom): half-res LINEAR fp16 injected by main.cpp, slot = kLumaBloomSlot.
Texture2D<float4> LumaBloomTexture : register(t6);

// The Luma glow, scene-referred LINEAR, where the vanilla glow was. Gain = engine BloomScale (0.1 measured) x
// Bloom Intensity; the pyramid is energy-preserving, so intensity 1 is vanilla strength.
float3 LumaBloom(float2 sceneUV)
{
   if (LumaSettings.GameSettings.LumaBloomEnable <= 0.5)
      return 0.0;
   const float3 bloom = LumaBloomTexture.SampleLevel(SceneColorTextureSampler_s, sceneUV, 0.0).rgb;
   return max(0.0, bloom) * LumaSettings.GameSettings.BloomScaleLive * LumaSettings.GameSettings.BloomIntensity;
}

// An UberPostProcessBlend grade value (clamped or extended) decoded to the linear light a 2.2 display shows for it.
// The exponent is split across the two passes: this pass encodes with 1.0, FGammaCorrection with DisplayGamma 1.6
// (pow 1.375 linear), so the downstream exponent is part of the decode. Used for the HDR signal, which must be the
// direct continuation of the FULL vanilla chain, not of this pass alone.
float3 VanillaToLinear(float3 graded)
{
   // Read back by main.cpp (0.625 measured, a few frames of latency, seeded 0.625). The fallback, the game's shipped
   // DisplayGamma 1.6, covers an unbound b13, which reads zeros and would make this pow(x, 0) = 1 (TW2).
   const float displayGammaInverse = LumaSettings.GameSettings.DisplayGammaInverse;
   return gamma_to_linear(PowUE3(max(0.0, graded), (displayGammaInverse > 0.0 ? displayGammaInverse : 0.625).xxx));
}

// The game's grade, verbatim from the disassembly. `clampSDR` false = max(0) instead of saturate, keeping the real
// channel ratio. `outputScale` is GammaColorScaleAndInverse.xyz; the HDR path passes 1 and re-applies it later.
// Its gamma encode (GammaColorScaleAndInverse.w) is a measured identity.
float3 GradeUE3(float3 scene, bool clampSDR, float3 outputScale)
{
   return UE3Grade(scene, clampSDR, outputScale, SceneShadowsAndDesat, SceneInverseHighLights.xyz, SceneMidTones.xyz, SceneLuminanceWeights.xyz, GammaOverlayColor.xyz, GammaColorScaleAndInverse.w);
}

// Sanitize anything stored in fp16 or the canvas: the extended grade, the hue stage, encode and dither can each emit
// NaN, which reads back black in a unorm target. Bit test, not "x != x" (optimized away). Covers -nan(ind) on a fade.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// Shared HDR back half: extended grade -> BT.2020 -> soft hue reference -> MacLeod-Boynton hue -> BT.709 -> PsychoV31
// display map, which takes the user's contrast, saturation and highlights desaturation. Takes and returns LINEAR light,
// 1.0 = paper white, NO engine fade (the caller re-applies it: contrast pivots on mid-gray, so a fade before it would
// never reach black). `color` is the pass's own grade with its upper saturate()s as max(0), decoded to display-linear:
// vanilla-exact below the clip and its own analytic continuation above it, so range, luminance and chromaticity all
// come from the game's math.
float3 FinishME1HDR(float3 color)
{
   // 3. Colour stage, in a BT.2020 working space (round-tripped back to BT.709 before the display map: gamut-correct
   // handling of saturated highlights, not a display-gamut expansion). Soft hue reference: ReinhardPiecewise(x, 5, 1.5)
   // per channel, where the RenoDX BL1 port builds it. Linear below 1.5 and rolling toward 5 above, it compresses a
   // saturated highlight's strong channel before its weak ones, so the hue leans the way the vanilla clip leaned it,
   // without the clip's whitening. MacLeod-Boynton then rebuilds that reference's hue direction on the target's own
   // purity and T = L + M anchor (hue strength 1, chrominance 0), before the display map as RenoDX applies it.
   // 4. The display map (see "PsychoVDisplayMapWithHueReference" for both).
   return PsychoVDisplayMapWithHueReference(color,
                                            LumaSettings.GameSettings.Contrast,
                                            LumaSettings.GameSettings.Saturation,
                                            LumaSettings.GameSettings.HighlightDechroma);
}
#endif

// The scene mix, exactly as vanilla, shared by the uber and the standalone DOFAndBloom blend (same rows, same
// interpolators): depth-driven DoF weight, the combined DoF/native-bloom contribution at x4 and the Luma glow in the
// numerator. Returns (numerator, weight sum); each pass divides with its own guard. `sceneUV` is TEXCOORD1 (t0),
// `blurUV` TEXCOORD0 (t1) - the original samples t0 with v6.
float4 ME1SceneMix(float2 blurUV, float2 sceneUV, out float sceneDepth)
{
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   // UE3 packs scene depth in the fp16 alpha. Handed back so the entry point writes it into the target alpha without a
   // second t0 sample: the copy pass carries it to the gamma pass, where the SMAA predication CS reads it.
   sceneDepth = scene.w;
   const float sceneWeight = saturate(1.0 - DoFBlurAmount(sceneDepth, DoFParams, DoFMaxBlur)); // vanilla DoF weight

   float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   // The engine's combined DoF blur + native bloom target, stored pre-divided by 4, hence the x4; vanilla's unorm view
   // also capped it at 4.0, a cap the fp16 upgrade lifted. NEVER scale the blur by BloomIntensity: DoF and bloom are
   // SUMMED into this buffer, so the term carries defocused scene, not only glow.
   // The Luma glow goes into the numerator, where the vanilla glow was, so the DoF weight sum divides it too.
   return float4(scene.xyz * sceneWeight + blurred.xyz * 4.0 + LumaBloom(sceneUV), blurred.w * 4.0 + sceneWeight);
}

// Stage 1: UberPostProcessBlend -> fp16 intermediate. LINEAR light (1.0 = paper white) in HDR, the untouched
// vanilla value in SDR.
float3 RunME1Tonemap(float2 blurUV, float2 sceneUV, out float sceneDepth)
{
   // 1. Scene mix.
   const float4 mix = ME1SceneMix(blurUV, sceneUV, sceneDepth);
   float3 untonemapped = mix.xyz * ((abs(mix.w) > 0.0) ? rcp(mix.w) : FLT_MAX); // rcp guard, as the original does

   // Exposure, scene-referred / pre-grade: the SDR reference derives from the same value, so the grade tracks it.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // The output scale doubles as the engine FADE. Held OUT of the extended grade and re-applied as a gain at the end,
   // after the display map, so it cannot bias the hue stage or the rolloff.
   const float3 outputScale = GammaColorScaleAndInverse.xyz;

   // 2. The game's grade, vanilla-exact, fade included. THE output on TONEMAP_TYPE 0; on the HDR path only the DEV
   // bisect reads it (fxc dead-strips it there, measured).
   const float3 sdr_vanilla = GradeUE3(untonemapped, true, outputScale);

#if TONEMAP_TYPE >= 1
   // The extended grade carries no fade (outputScale = 1) and is decoded through the DOWNSTREAM display gamma
   // (VanillaToLinear, not a plain gamma decode): the direct continuation of the full vanilla chain is the HDR signal.
   float3 hdr = FinishME1HDR(VanillaToLinear(GradeUE3(untonemapped, false, 1.0)));
   // Re-apply the engine fade LAST, after the creative sliders, as the gain vanilla's chain gives it: the scale sits
   // before this pass's exponent and the display gamma, so in linear it is scale^(w * DisplayGamma), not scale. At
   // rest (scale 1) it is a no-op; mid-fade it matches the vanilla curve instead of running brighter.
   float3 outColor = hdr * VanillaToLinear(PowUE3(outputScale, GammaColorScaleAndInverse.www)); // LINEAR, 1.0 = paper white
#else
   float3 outColor = sdr_vanilla; // vanilla encoding, whatever this pass's exponent is
#endif

   outColor = Sanitize(outColor);

#if DEVELOPMENT
   // Bring-up bisect (all off = normal): 01 solid magenta, 02 the reconstructed scene mix, 03 the vanilla grade.
   if (LumaSettings.DevSetting01 > 0.5)
      return float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      return saturate(untonemapped);
   if (LumaSettings.DevSetting03 > 0.5)
      return saturate(sdr_vanilla);
#endif

   return outColor;
}

// Stage 2: FGammaCorrection (PS 0x17CE0932) -> the canvas, the frame's LAST colour pass before the HUD.
// Reads scene A: stage 1's LINEAR HDR when the uber ran, the ungraded fp16 scene when it was skipped. Own register map.
#define GcColorScale   PsConstants[8]  // .xyz ColorScale
#define GcOverlayColor PsConstants[10] // .xyz OverlayColor, .w its blend weight (this pass's fade)
#define GcInverseGamma PsConstants[11] // .x inverse display gamma (measured 0.625 = 1/1.6, the game's DisplayGamma default)

// Vanilla-exact: saturate(lerp(scene * ColorScale, Overlay.rgb, Overlay.a)) then the inverse-gamma pow.
float3 GradeGCVanilla(float3 scene)
{
   float3 c = lerp(scene * GcColorScale.xyz, GcOverlayColor.xyz, GcOverlayColor.w);
   return PowUE3(saturate(c), GcInverseGamma.xxx);
}

// The gamma-only extended native grade: the overlay (this pass's fade) held OUT and re-applied after the HDR block,
// the vanilla saturate() as a lower-only max(0) so highlights keep their real channel ratio. GcInverseGamma belongs
// to this grade, so the caller decodes with a plain gamma_to_linear rather than VanillaToLinear.
float3 GradeGCExtended(float3 scene)
{
   float3 c = scene * GcColorScale.xyz;
   c = max(0.0, c); // mad_sat in the original
   return PowUE3(c, GcInverseGamma.xxx);
}

// `sceneUV` is TEXCOORD0 (v5) — unlike UberPostProcessBlend, which reads the scene from TEXCOORD1.
float3 RunME1GammaCorrection(float2 sceneUV)
{
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

#if TONEMAP_TYPE >= 1
   // The post-load flash (0.35 s re-graded frame: mids x0.10, highlights x2.25, peak 222 -> 953 nits) is NOT a stale
   // graded buffer - gating this on the intermediate's alpha changed nothing in game; do not re-add it.
   float3 outColor; // linear, 1.0 = paper white
   if (LumaData.GameData.UberRanThisFrame > 0.5)
   {
      // Stage 1 left LINEAR light here, its SDR reference already carrying this pass's gamma: only the scale applies,
      // as the gain vanilla's chain gives it (the scale sits before this pass's exponent and the display gamma).
      outColor = scene.xyz * gamma_to_linear(PowUE3(GcColorScale.xyz, GcInverseGamma.xxx));
   }
   else
   {
      // Gamma-only frame: the fp16 scene with no uber grade (DoF and bloom only if a standalone blend ran), so the
      // whole HDR block runs here off this pass's own extended grade. GradeGCExtended applies GcInverseGamma itself, so
      // the decode is a plain gamma_to_linear (NOT VanillaToLinear, which would apply the display gamma twice). No Luma
      // bloom injected here: the uber or the blend composites it.
      float3 untonemapped = scene.xyz * LumaSettings.GameSettings.Exposure;
      outColor = FinishME1HDR(gamma_to_linear(GradeGCExtended(untonemapped)));
   }
   // The fade LAST, after the creative sliders. Branched, not lerped: the decode is uniform but fxc hoists it into
   // the preamble, and .w is 0 whenever no fade runs, so the branch skips the whole re-encode.
   [branch] if (GcOverlayColor.w > 0.0)
   {
      // Vanilla lerps toward the overlay BEFORE its inverse-gamma pow and the display decode, so the fade curve is
      // taken in that encoded domain: the linear HDR is re-encoded through the inverse of the two exponents, blended,
      // and decoded again. Below 1.0 this is the vanilla fade exactly; above it, its continuation.
      const float invGamma = max(GcInverseGamma.x, 1e-4);
      const float3 encoded = PowUE3(linear_to_gamma(outColor, GCT_POSITIVE), (1.0 / invGamma).xxx);
      outColor = gamma_to_linear(PowUE3(lerp(encoded, GcOverlayColor.xyz, GcOverlayColor.w), invGamma.xxx));
   }

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
   outColor = max(0.0, outColor * GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true)); // negatives would turn into NaN in linear_to_gamma below
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(outColor, sceneUV, true, LumaSettings.DisplayMode != 0, LumaSettings.UIPaperWhiteNits);
   }
#endif
#else
   // Vanilla: this pass's saturate and pow are the original's, so both frame shapes come out vanilla-exact. Exposure
   // only on gamma-only frames: when the uber ran, its output already carries it.
   const float exposure = LumaData.GameData.UberRanThisFrame > 0.5 ? 1.0 : LumaSettings.GameSettings.Exposure;
   float3 outColor = GradeGCVanilla(scene.xyz * exposure);
#endif

   return Sanitize(outColor);
}
