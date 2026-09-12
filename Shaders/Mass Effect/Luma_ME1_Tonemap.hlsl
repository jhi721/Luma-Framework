// Mass Effect (2007) — Luma HDR tonemap replacement, split across the TWO colour passes that end a frame
// (devkit-measured order): UberPostProcessBlend (PS 0xAC8341E0) reads scene A (fp16, depth in alpha) plus the
// quarter-res DoF/bloom blur into scene B; copy 0x1E37D75B mirrors B into A; FGammaCorrection (PS 0x17CE0932)
// reads A and writes the 8-bit canvas the HUD blends onto. They CHAIN here, so the work is split: the uber
// replacement runs the whole HDR block and leaves LINEAR light (1.0 = paper white) in the intermediate; the
// gamma replacement adds that pass's fade, the UI paper-white pre-scale, the gamma encode, dither and sanitize.
// HDR strategy (analytic in-shader, no LUT; the Borderlands GOTY / MoH Airborne production pipeline): reconstruct the
// scene mix exactly as the game does -> untonemapped; keep the game's own clamped grade as the exact SDR reference;
// run that same grade with its upper saturate()s as max(0) and decode it to display-linear -> the HDR signal (range,
// luminance, chroma); in BT.2020 build a soft per-channel ReinhardPiecewise(5, 1.5) hue reference and let
// MacLeod-Boynton rebuild its hue direction on the target's own purity (Hue 1, Blowout 0); DICE rolloff to the user's
// peak/paper white; user saturation and the engine fade last.
// The engine SKIPS the uber in elevators and some loading scenes; main.cpp reports that through
// LumaData.GameData.UberRanThisFrame and the gamma replacement then runs the HDR block itself off the RAW scene.
// Measured cb4 exponents: uber 1.0, copy 1.0, gamma 0.625 = 1/1.6 (the game's DisplayGamma), so the intermediate
// holds linear graded light. main.cpp hands that exponent over as GameSettings.DisplayGammaInverse, since the
// grade needs it but it lives in another pass. Two dgVoodoo rules hold throughout: every fetch is followed by an
// `and`/`or` pair against b3, and the entry points declare all 13 interpolators - linkage is by REGISTER.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. Game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS before any shared header pulls Settings.hlsl, or GameSettings resolves to the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders) before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // Reinhard::ReinhardPiecewise, the soft hue reference
#include "Includes/MacLeodBoynton.hlsl"     // MacLeodBoynton::HueOnlyBT2020. Byte-identical copy of the BL GOTY production model: do not edit here, sync it from "Borderlands GOTY Enhanced/Includes"
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3

// HDR / vanilla. 1 = extended native grade + MacLeod-Boynton hue + DICE display map (default). 0 = vanilla clamped SDR
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
   // Read back by main.cpp (0.625 measured, a few frames of latency, seeded 1/2.2). The fallback also covers an
   // unbound b13, which reads zeros and would make this pow(x, 0) = 1 (TW2).
   const float displayGammaInverse = LumaSettings.GameSettings.DisplayGammaInverse;
   return gamma_to_linear(PowUE3(max(0.0, graded), (displayGammaInverse > 0.0 ? displayGammaInverse : 1.0 / 2.2).xxx));
}

// The game's grade, verbatim from the disassembly. `clampSDR` false = max(0) instead of saturate, keeping the real
// channel ratio. `outputScale` is GammaColorScaleAndInverse.xyz; the HDR path passes 1 and re-applies it later.
float3 GradeUE3(float3 scene, bool clampSDR, float3 outputScale)
{
   // Head: shadows -> scale -> midtones.
   float3 c = scene - SceneShadowsAndDesat.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   c = c * SceneInverseHighLights.xyz;
   c = PowUE3(c, SceneMidTones.xyz);

   // Tail: desat + tint + output scale + (measured identity) gamma encode.
   float desat = dot(c, SceneLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesat.www + GammaOverlayColor.xyz;
   c = c + desat;
   c = c * outputScale;
   c = clampSDR ? saturate(c) : max(0.0, c); // mul_sat in the original
   return PowUE3(c, GammaColorScaleAndInverse.www);
}

// Sanitize anything stored in fp16 or the canvas: the extended grade, the hue stage, encode and dither can each emit
// NaN, which reads back black in a unorm target. Bit test, not "x != x" (optimized away). Covers -nan(ind) on a fade.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// Shared HDR back half: extended grade -> contrast -> BT.2020 -> soft hue reference -> MacLeod-Boynton hue -> display
// rolloff -> gamut clip -> BT.709 -> saturation. Contrast sits ahead of the rolloff on purpose (see 3), saturation
// behind it, as in the repo. Takes and returns LINEAR light, 1.0 = paper white, NO engine fade (the caller re-applies
// it: contrast pivots on mid-gray, so a fade before it would never reach black). `extendedGradeLinear` is the pass's
// own grade with its upper saturate()s as max(0), decoded to display-linear: vanilla-exact below the clip and its own
// analytic continuation above it, so range, luminance and chroma all come from the game's math.
float3 FinishME1HDR(float3 extendedGradeLinear)
{
   float3 color = extendedGradeLinear;

   // 3. Contrast BEFORE the display map so DICE contains whatever it pushes up: after the rolloff the slider would
   // escape the peak it just established, and nothing downstream re-contains it. Multiplicative around mid-gray, the
   // repo's form (RenoDX_Contrast); 0.18 is mid-gray here too, display-referred with 1.0 = paper white (code 0.5).
   // [branch] on a cbuffer uniform: at the 1.0 default this must be a BIT-EXACT no-op. The max() cannot go either -
   // PowUE3 floors with abs(), which would MIRROR a small negative rather than crush it.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      const float midGray = 0.18;
      color = PowUE3(max(0.0, color / midGray), LumaSettings.GameSettings.Contrast.xxx) * midGray;
   }

   // 4. Colour stage, in a BT.2020 working space (round-tripped back to BT.709 after the display map: gamut-correct
   // handling of saturated highlights, not a display-gamut expansion). Soft hue reference: ReinhardPiecewise(x, 5, 1.5)
   // per channel, where the RenoDX BL1 port builds it. Linear below 1.5 and rolling toward 5 above, it compresses a
   // saturated highlight's strong channel before its weak ones, so the hue leans the way the vanilla clip leaned it,
   // without the clip's whitening. MacLeod-Boynton then rebuilds that reference's hue direction on the target's own
   // purity and T = L + M anchor (hue strength 1, chrominance 0), before the display map as RenoDX applies it.
   const float3 extendedBT2020 = BT709_To_BT2020(color);
   const float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5);
   const float3 diceInBT2020 = MacLeodBoynton::HueOnlyBT2020(extendedBT2020, hueReferenceBT2020);

   // 5. Display rolloff to the user's peak/paper white (DICE, hue-preserving). Both floored: DICE divides by them.
   const float paperWhite = max(LumaSettings.GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(LumaSettings.PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
   // CORRECT_CHANNELS_BEYOND_PEAK_WHITE desaturates any channel still over peak: panels clip per channel, so an
   // uncorrected saturated highlight would clip with a hue shift.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Highlight dechroma handed to DICE rather than run as our own pass afterwards. Core's is better placed: it ramps
   // on the MAX CHANNEL (by luminance a bright blue never triggers), exists only between ShoulderStart * PeakWhite
   // and peak (1/3 of peak for this type, so mid-tones cannot be touched), and runs INSIDE the containment in the
   // processing primaries. 0 = off for the OUTPUT but not the cost: DICE's guard carries no [branch], so fxc
   // flattens it to 8 ALU plus a movc for every pixel above the shoulder. Measured +5 slots for the placement.
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   // DICE converts InOutColorSpace -> Processing on entry and back on exit. The colour is already BT.2020 and is
   // converted back below, so the default CS_BT709 would convert a SECOND time and compress on doubly-narrowed
   // primaries. Neutrals cancel out; saturated highlights do not.
   ds.InOutColorSpace = CS_BT2020;
   float3 hdr = DICETonemap(diceInBT2020 * paperWhite, peakWhite, ds) / paperWhite;
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));

   // 6. User saturation LAST, after the display map: the repo's convention. Lerp against BT.709 luminance, not
   // hue-preserving.
   return Saturation(hdr, LumaSettings.GameSettings.Saturation);
}
#endif

// Stage 1: UberPostProcessBlend -> fp16 intermediate. LINEAR light (1.0 = paper white) in HDR, the untouched
// vanilla value in SDR. `sceneUV` is TEXCOORD1 (t0), `blurUV` TEXCOORD0 (t1) - the original samples t0 with v6.
float3 RunME1Tonemap(float2 blurUV, float2 sceneUV, out float sceneDepth)
{
   // 1. Scene mix, exactly as vanilla: depth-driven DoF weight, bloom at x4, normalized by the weight sum.
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   const float depth = scene.w; // UE3 packs scene depth in the fp16 alpha
   // Handed back so the entry point writes it into the target alpha without a second t0 sample: the copy pass carries
   // it to the gamma pass, where the SMAA predication CS reads it.
   sceneDepth = depth;
   // Vanilla DoF weight. Includes/DofBloomGather.hlsl computes the same off the same rows and is deliberately NOT
   // shared: independent transcriptions of two DIFFERENT shaders, each verified against its own disassembly.
   const float signedDistance = depth - DoFParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * DoFParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? DoFMaxBlur.y : DoFMaxBlur.x;
   const float blurAmount = min(PowUE3(normalizedDistance.xxx, DoFParams.zzz).x, maxBlur);
   const float sceneWeight = saturate(1.0 - blurAmount);

   float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   // Stored pre-divided by 4, hence the x4; vanilla's unorm view also capped it at 4.0, a cap the fp16 upgrade lifted.
   // NEVER scaled by BloomIntensity: DoF and bloom are SUMMED into this buffer.
   const float3 bloom = blurred.xyz * 4.0;
   const float weightSum = blurred.w * 4.0 + sceneWeight;

   // The Luma glow goes into the numerator, where the vanilla glow was, so the DoF weight sum divides it too.
   float3 untonemapped = scene.xyz * sceneWeight + bloom + LumaBloom(sceneUV);
   untonemapped *= (abs(weightSum) > 0.0) ? rcp(weightSum) : FLT_MAX; // rcp guard, as the original does

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
   // Re-apply the engine fade linearly, LAST, after the creative sliders. At rest it is a no-op.
   float3 outColor = hdr * outputScale; // LINEAR, 1.0 = paper white, into the fp16 intermediate
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

// Stage 2: FGammaCorrection (PS 0x17CE0932) -> the 8-bit canvas, the frame's LAST colour pass before the HUD.
// Reads scene A: stage 1's LINEAR HDR when the uber ran, the RAW fp16 scene when it was skipped. Own register map.
#define GcColorScale   PsConstants[8]  // .xyz ColorScale
#define GcOverlayColor PsConstants[10] // .xyz OverlayColor, .w its blend weight (this pass's fade)
#define GcInverseGamma PsConstants[11] // .x inverse display gamma (measured 0.625 = 1/1.6, the game's DisplayGamma default)

// Vanilla-exact: saturate(lerp(scene * ColorScale, Overlay.rgb, Overlay.a)) then the inverse-gamma pow.
float3 GradeGCVanilla(float3 scene)
{
   float3 c = lerp(scene * GcColorScale.xyz, GcOverlayColor.xyz, GcOverlayColor.w);
   return PowUE3(saturate(c), GcInverseGamma.xxx);
}

// The grade with the overlay (this pass's fade) held OUT: clamped = the SDR reference, unclamped (lower-only max(0)) =
// the extended HDR signal of the gamma-only frames. Unlike the uber grade it carries the display gamma itself.
float3 GradeGC(float3 scene, bool clampSDR)
{
   float3 c = scene * GcColorScale.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   return PowUE3(c, GcInverseGamma.xxx);
}

// `sceneUV` is TEXCOORD0 (v5) — unlike UberPostProcessBlend, which reads the scene from TEXCOORD1.
float3 RunME1GammaCorrection(float2 sceneUV)
{
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

#if TONEMAP_TYPE >= 1
   // The post-load flash (0.35 s re-graded frame: mids x0.10, highlights x2.25, peak 222 -> 953 nits) is NOT a stale
   // graded buffer - gating this on the intermediate's alpha changed nothing in game. NOTES.md; do not re-add it.
   float3 hdr;
   if (LumaData.GameData.UberRanThisFrame > 0.5)
   {
      // Stage 1 left LINEAR light here, its SDR reference already carrying this pass's gamma: only the scale applies.
      hdr = scene.xyz * GcColorScale.xyz;
   }
   else
   {
      // Gamma-only frame: the RAW fp16 scene, no DoF, no bloom, no uber grade, so the whole HDR block runs here off
      // this pass's own extended grade. GradeGC applies GcInverseGamma itself, so the decode is a plain gamma_to_linear
      // (NOT VanillaToLinear, which would apply the display gamma twice). No Luma bloom: the pyramid is injected at
      // the uber draw.
      float3 untonemapped = scene.xyz * LumaSettings.GameSettings.Exposure;
      hdr = FinishME1HDR(gamma_to_linear(GradeGC(untonemapped, false)));
   }
   // The fade LAST, after the creative sliders. Branched, not lerped: the decode is uniform but fxc hoists it into
   // the preamble, where .w is 0 with no fade - measured 48 -> 42 executed instructions.
   float3 outColor = hdr; // linear, 1.0 = paper white
   [branch] if (GcOverlayColor.w > 0.0)
   {
      // The overlay is a pre-encode SDR value, so a fade toward it happens in linear, decoded as the references are.
      const float3 overlay = gamma_to_linear(PowUE3(saturate(GcOverlayColor.xyz), GcInverseGamma.xxx));
      outColor = lerp(hdr, overlay, GcOverlayColor.w);
   }

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
#if UI_DRAW_TYPE >= 2
   // Pre-scale so the gamma-SDR HUD lands at UIPaperWhite after composition rescales. Guarded: an unset
   // GamePaperWhiteNits would black the scene and leave the HUD, i.e. "the 3D disappeared".
   if (LumaSettings.GamePaperWhiteNits > 0.0)
      outColor *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   outColor = max(0.0, outColor); // negatives would turn into NaN in linear_to_gamma below
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
   // Anti-banding dither in the stored gamma space (composition does not dither); animated triangular noise, sub-
   // perceptual at bit depth 9. [branch] as above: 17 instructions and two sincos, otherwise flattened even when off.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyDithering(outColor, sceneUV, true, 1.0, DITHERING_BIT_DEPTH, LumaSettings.FrameIndex, true);
   }
#endif
#else
   // Vanilla: this pass's saturate and pow are the original's, so both frame shapes come out vanilla-exact.
   float3 outColor = GradeGCVanilla(scene.xyz);
#endif

   return Sanitize(outColor);
}
