// Mass Effect 3 (2012) - Luma HDR replacement of FSFXUberPostProcessBlendPixelShader, the frame's last scene colour
// pass (dgVoodoo 2.87.3, ps_5_0). Output goes to an 8-bit canvas (upgraded to fp16 by main.cpp), then a blit, FXAA 3
// (which reads luma from this pass's alpha) and the gamma GFx HUD; Core's composition decodes and applies UI Paper
// White at present (UI_DRAW_TYPE 2, POST_PROCESS_SPACE_TYPE 0), which is the role MELE's stage 2 plays.
//
// Two grade families, chosen per entry point (ME3_UBER_* macros):
//  - LUT (gameplay, galaxy map): scene+bloom -> 4096x1 R16 filmic LUT -> 16-slice colour LUT -> overlay, scale,
//    gamma. The same chain as ME3LE, so its HDR is MELE family 04, ported from "Mass Effect Legendary Edition"
//    (Tonemap_ME3LE_LUT_Body.hlsl + Includes/Tonemap_MELE_*.hlsli): continue the filmic LUT past mid-gray, grade a
//    bounded proxy, divide the scale back out, keep the native result's RGB ratios at the new luminance.
//  - ALU (menus, squad, 3D previews): the classic UE3 formula grade with a hard clip and no tone curve. HDR is the UE3
//    hard-clip canon shipped on ME1 2007 / BL GOTY: the same grade with its upper saturates lifted, a
//    ReinhardPiecewise(5, ME3_HUE_REFERENCE_SHOULDER) hue reference and MacLeod-Boynton hue in BT.2020, then DICE.
// The HDR paths run only in the HDR Display Mode; SDR and TONEMAP_TYPE 0 output the exact vanilla grade.
// Two dgVoodoo rules hold throughout: every fetch is followed by the b3 mask pair, and the entry points declare all 13
// interpolators, because VS->PS linkage is by register.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. Game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS before any shared header pulls Settings.hlsl, or GameSettings resolves to the empty dummy.
#include "Includes/Common.hlsl"
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../Includes/DICE.hlsl"
#include "../Includes/Reinhard.hlsl"        // ReinhardRange (grade proxy), ReinhardPiecewise (ALU hue reference)
#include "Includes/MacLeodBoynton.hlsl"     // Byte-identical copy of the BL GOTY production model: sync it from "Borderlands GOTY Enhanced/Includes", do not edit here
#include "Includes/GameBindings.hlsl"
// clang-format on

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif
#ifndef ME3_UBER_ALU
#define ME3_UBER_ALU 0
#endif
#ifndef ME3_UBER_FILMIC // the 4096x1 filmic LUT ahead of the grade
#define ME3_UBER_FILMIC (1 - ME3_UBER_ALU)
#endif
#ifndef ME3_UBER_HALFRES
#define ME3_UBER_HALFRES 0
#endif
#ifndef ME3_UBER_GRAIN
#define ME3_UBER_GRAIN 0
#endif
#ifndef ME3_UBER_VIGNETTE // 0 = the vignette-less perms (e.g. Adrenaline Rush)
#define ME3_UBER_VIGNETTE 1
#endif
#ifndef ME3_UBER_LINEAR // gamma-less perm: linear out, FGammaCorrection encodes
#define ME3_UBER_LINEAR 0
#endif

// Texture/sampler slots, from the disassembly of each perm: scene, blur (or half-res + bloom), then the LUT (LUT perms),
// noise (grain perms), filmic LUT (filmic perms).
#if ME3_UBER_HALFRES
#define SLOT_SCENE 0 // full-res fp16 scene, read at lod 0
#define SLOT_HALF  1 // half-res DoF + motion blur merge (UberHalfRes 0x21D4387A); alpha = weight toward the full-res scene
#define SLOT_BLOOM 2
#define SLOT_NEXT  3
#else
#define SLOT_SCENE 0 // fp16 scene, linear depth in alpha
#define SLOT_BLUR  1 // quarter-res DoF/bloom gather, stored pre-divided by 4
#define SLOT_NEXT  2
#endif
#define SLOT_LUT    SLOT_NEXT
#define SLOT_NOISE  (SLOT_NEXT + 1 - ME3_UBER_ALU)
#define SLOT_FILMIC (SLOT_NOISE + ME3_UBER_GRAIN)

// cb4 rows (SM3 c<N> = cb4[N+8]).
#define DoFParams             PsConstants[8] // .x focus distance, .y 1/range, .z falloff exponent (full-res, ALU)
#define BloomTintAndThreshold PsConstants[8] // .xyz bloom tint, .w screen-blend threshold (half-res)
#define MinZ_MaxZRatio        PsConstants[10]
#define DoFMaxBlur            PsConstants[11] // .x max blur near, .y max blur far (full-res, ALU)
#define HalfResMaskRect       PsConstants[11] // half-res UV clamp (half-res)
#if ME3_UBER_ALU
#define SceneShadowsAndDesat      PsConstants[12] // .xyz shadows lift (subtracted), .w saturation weight
#define SceneInverseHighLights    PsConstants[13]
#define SceneMidTones             PsConstants[14]
#define SceneLuminanceWeights     PsConstants[15]
#define GammaColorScaleAndInverse PsConstants[16] // .xyz output scale (doubles as the engine fade), .w inverse gamma
#define GammaOverlayColor         PsConstants[17]
#define ME3_TAIL_ROW              18
#else
#define GammaColorScaleAndInverse PsConstants[12]
#define GammaOverlayColor         PsConstants[13]
#define ME3_TAIL_ROW              14
#endif
#define NoiseTextureOffset PsConstants[ME3_TAIL_ROW]                  // grain perms only
#define ScreenUVScaleBias  PsConstants[ME3_TAIL_ROW + ME3_UBER_GRAIN] // vignette and grain UV

Texture2D<float4> Textures[6] : register(t0);
SamplerState Samplers[6] : register(s0);

#define FETCH(slot, uv) ApplyDgvMask(Textures[slot].Sample(Samplers[slot], uv), slot)

// Luma bloom, bound by main.cpp at the uber draw, clear of every perm's t0..t5/s0..s5: core DrawBloom's mip 0 (half-res,
// LINEAR fp16, energy-preserving) and a GPU copy of THIS frame's bloom source b4 (gather 0x699E0C60 or downsample
// 0xC6215545), whose row 8.x is the engine's per-volume bloom scale. A null binding reads 0: no source, no bloom.
Texture2D<float4> LumaBloomTexture : register(t6);
SamplerState LumaBloomSampler : register(s6);
cbuffer BloomSourceCB : register(b5)
{
   float4 BloomSourceConstants[236];
}

// Scene-referred linear, where the vanilla glow goes. Gain = engine bloom scale x Bloom Intensity: vanilla strength at 1.
// Callers gate it on LumaBloomEnable.
float3 ME3_LumaBloom(float2 sceneUV)
{
   return max(0.0, LumaBloomTexture.SampleLevel(LumaBloomSampler, sceneUV, 0.0).rgb) * BloomSourceConstants[8].x * LumaSettings.GameSettings.BloomIntensity;
}

// Finite guards, ported from MELE Includes/Common.hlsl: ordered comparisons reject NaN and INF at 2 instructions per
// channel. Computed values only - fxc deletes them on a raw cbuffer read.
bool MELE_IsFiniteNonNegative(float x)
{
   return x >= 0.0 && x <= FLT_MAX;
}
bool MELE_IsFiniteNonNegative(float3 v)
{
   return all(v >= 0.0) && all(v <= FLT_MAX);
}

// ---------------------------------------------------------------------------------------------------------------------
// Scene mix, exactly as vanilla.
// ---------------------------------------------------------------------------------------------------------------------

#if ME3_UBER_HALFRES
// Half-res merge (camera moving): lerp from the half-res DoF/MB buffer toward the full-res scene by its alpha, then the
// bloom screen blend exp2(-3 * luma601) that the full-res perms bake into their gather instead. The Luma bloom takes
// the vanilla bloom's place inside the same blend and tint, so this path keeps its vanilla behaviour.
float3 ME3_SceneMix(float2 bloomUV, float4 sceneUV)
{
   const float4 halfRes = FETCH(SLOT_HALF, clamp(sceneUV.zw, HalfResMaskRect.xy, HalfResMaskRect.zw));
   const float3 full = ApplyDgvMask(Textures[SLOT_SCENE].SampleLevel(Samplers[SLOT_SCENE], sceneUV.xy, 0), SLOT_SCENE).xyz;
   const float3 c = halfRes.w * (full - halfRes.xyz) + halfRes.xyz;
   const float blend = saturate(exp2(-3.0 * dot(c, float3(0.299, 0.587, 0.114))) * BloomTintAndThreshold.w);
   float3 bloom;
   [branch] if (LumaSettings.GameSettings.LumaBloomEnable > 0.5)
   {
      bloom = ME3_LumaBloom(sceneUV.xy);
   }
   else
   {
      bloom = FETCH(SLOT_BLOOM, bloomUV).xyz * 4.0;
   }
   return bloom * BloomTintAndThreshold.xyz * blend + c;
}
#else
// Full-res DoF composite: depth-driven blur weight, the combined DoF/bloom gather at x4, normalized by the weight sum.
// With the Luma bloom on, the gather carries DoF only and the Luma bloom is added after the normalization (ME1).
float3 ME3_SceneMix(float2 blurUV, float4 sceneUV)
{
   const float4 scene = FETCH(SLOT_SCENE, sceneUV.zw);
   const float z = min(scene.w, 65504.0) * MinZ_MaxZRatio.z - MinZ_MaxZRatio.w;
   const float sceneWeight = saturate(1.0 - ME3_DoFBlur(abs(z) > 0.0 ? rcp(z) : FLT_MAX, DoFParams.xyz, DoFMaxBlur.xy));

   const float4 blurred = FETCH(SLOT_BLUR, blurUV);
   const float weightSum = blurred.w * 4.0 + sceneWeight;
   float3 color = (scene.xyz * sceneWeight + blurred.xyz * 4.0) * (abs(weightSum) > 0.0 ? rcp(weightSum) : FLT_MAX);
   [branch] if (LumaSettings.GameSettings.LumaBloomEnable > 0.5)
   {
      color += ME3_LumaBloom(sceneUV.zw);
   }
   return color;
}
#endif

// ---------------------------------------------------------------------------------------------------------------------
// Shared HDR tail pieces.
// ---------------------------------------------------------------------------------------------------------------------

// User contrast BEFORE the display map so DICE contains whatever it pushes up. Multiplicative around mid-gray
// (Game-Paper-White-relative), the repo's form. [branch] keeps the 1.0 default bit-exact; the floored log2 keeps
// Contrast 0 on black at 0 rather than pow(0, 0) = NaN.
float3 ApplyUserContrast(float3 color)
{
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      color = exp2(LumaSettings.GameSettings.Contrast * log2(max(max(color, 0.0) / MidGray, 1e-30))) * MidGray;
   }
   return color;
}

DICESettings ME3_DICESettings()
{
   DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   settings.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   return settings;
}

float ME3_PaperWhite()
{
   return max(LumaSettings.GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
}

float ME3_PeakWhite()
{
   return max(LumaSettings.PeakWhiteNits, LumaSettings.GamePaperWhiteNits) / sRGB_WhiteLevelNits;
}

// Shoulder of the ALU family's hue reference: the Reinhard donor's own, and the gate that skips the whole colour
// stage below it. Tuned constant, not a user control; path-to-white stays with DICE at the display peak.
#define ME3_HUE_REFERENCE_SHOULDER      1.5
#define MELE_HDR_BRIDGE_SHOULDER        0.75 // k, the max-channel proxy shoulder, in the adapted linear domain.
#define MELE_HDR_PIVOT                  0.18 // p, scene mid-gray, where the tone-curve continuation starts.
#define MELE_HDR_PROBE_LO               0.16 // Sampled-fit probes, in scene-x.
#define MELE_HDR_PROBE_HI               0.20
#define MELE_HDR_BRIDGE_PROXY_EPS       1e-4
#define MELE_FILMIC_MIN_SLOPE_X         1e-5
#define MELE_NATIVE_COLOR_MIN_LUMINANCE 1e-6

// Native filmic input scale: the strip covers scene-linear scene+bloom up to about 16.2.
static const float kFilmicInputScale = 0.0616082214;

float MELE_FilmicLookupZ(float z)
{
   return ApplyDgvMask(Textures[SLOT_FILMIC].SampleLevel(Samplers[SLOT_FILMIC], float2(kFilmicInputScale * z, 0.5), 0), SLOT_FILMIC).x;
}

// Continue the game's own 1D filmic LUT past mid-gray with the secant across the probe window. Every anchor is a read
// of the bound LUT and none depends on the signal. Below the pivot the native sample is kept as is.
bool MELE_TryEvaluateME3LEFilmicExtended(float3 sceneWithBloom, float3 nativeFilmicRGB, out float3 extendedFilmicRGB)
{
   extendedFilmicRGB = nativeFilmicRGB;
   if (!MELE_IsFiniteNonNegative(sceneWithBloom) || !MELE_IsFiniteNonNegative(nativeFilmicRGB))
   {
      return false;
   }
   const float probeLo = MELE_FilmicLookupZ(MELE_HDR_PROBE_LO);
   const float pivotValue = MELE_FilmicLookupZ(MELE_HDR_PIVOT);
   const float probeHi = MELE_FilmicLookupZ(MELE_HDR_PROBE_HI);
   const float slope = (probeHi - probeLo) / (MELE_HDR_PROBE_HI - MELE_HDR_PROBE_LO);
   const bool valid = MELE_IsFiniteNonNegative(float3(probeLo, pivotValue, probeHi)) && probeLo <= pivotValue && pivotValue <= probeHi && MELE_IsFiniteNonNegative(slope) && slope > MELE_FILMIC_MIN_SLOPE_X;
   extendedFilmicRGB = sceneWithBloom <= MELE_HDR_PIVOT ? nativeFilmicRGB : pivotValue + slope * (sceneWithBloom - MELE_HDR_PIVOT);
   return valid;
}

// The game's filmic LUT, per channel, as the perms sample it.
float3 ME3_NativeFilmic(float3 scene)
{
   return float3(FETCH(SLOT_FILMIC, (kFilmicInputScale * scene.x).xx).x, FETCH(SLOT_FILMIC, (kFilmicInputScale * scene.y).xx).x, FETCH(SLOT_FILMIC, (kFilmicInputScale * scene.z).xx).x);
}

// Native smoothstep vignette with its blue-tinted white point, in linear (linear_to_gamma is a pure pow, so this is
// the vanilla gamma multiply exactly). The slider scales only the radial darkening, never the white point.
float3 ME3_VignetteLinear(float2 vignetteUV)
{
   float2 vc = (vignetteUV - 0.5) * float2(0.832050323, 0.554700196);
   const float vd = saturate(4.0 * (dot(vc, vc) - (ME3_UBER_LINEAR ? 0.0 : 0.0500000007))); // the gamma-less perms have no offset
   const float vs = (3.0 - 2.0 * vd) * vd * vd;
   const float3 vig = float3(1.01036298, 1.00000572, 1.16309249) - vs * LumaSettings.GameSettings.VignetteIntensity;
   // The gamma-less perms multiply linear light by it directly.
   return ME3_UBER_LINEAR ? vig : gamma_to_linear(vig, GCT_MIRROR);
}

#if ME3_UBER_ALU
// ---------------------------------------------------------------------------------------------------------------------
// ALU family: classic UE3 grade, verbatim from 0x2C967E0F.
// ---------------------------------------------------------------------------------------------------------------------

// `clampSDR` false = the extended grade: the upper saturates become max(0) and the 1e-6 floor 0, nothing else
// changes. `outputScale` is GammaColorScaleAndInverse.xyz; the HDR path passes 1 and re-applies the fade at the end.
float3 GradeUE3(float3 scene, bool clampSDR, float3 outputScale)
{
   float3 c = scene - SceneShadowsAndDesat.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   c = PowUE3(c * SceneInverseHighLights.xyz, SceneMidTones.xyz);
   const float desat = dot(c, SceneLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesat.www + desat + GammaOverlayColor.xyz;
   c = c * outputScale;
   c = clampSDR ? max(saturate(c), 1e-6) : max(0.0, c); // mul_sat + floor in the original
   return PowUE3(c, GammaColorScaleAndInverse.www);
}

// Returns linear light, Game-Paper-White-relative.
float3 ME3_Grade(float3 untonemapped, bool hdr, float3 vignette)
{
#if ME3_UBER_FILMIC
   const float3 gradeIn = ME3_NativeFilmic(untonemapped);
#else
   const float3 gradeIn = untonemapped;
#endif
   // One exit: an early return inside the branch trips fxc's X4000 on the inlined result. The vanilla grade runs only
   // on the path that returns it.
   float3 color;
#if TONEMAP_TYPE >= 1
   [branch] if (hdr)
   {
      // Extended grade, decoded once: vanilla-exact below the clip and its own analytic continuation above it.
      float3 extendedIn = gradeIn;
#if ME3_UBER_FILMIC
      if (!MELE_TryEvaluateME3LEFilmicExtended(untonemapped, gradeIn, extendedIn))
         extendedIn = gradeIn;
#endif
      const float3 extendedLinear = ApplyUserContrast(gamma_to_linear(GradeUE3(extendedIn, false, 1.0), GCT_POSITIVE) * vignette);

      // Colour stage in BT.2020: the soft per-channel reference bends a saturated highlight the way the vanilla clip
      // did, without its whitening; MacLeod-Boynton rebuilds that hue on the signal's own purity. Then DICE, with
      // InOutColorSpace BT.2020 so it does not convert a second time.
      //
      // The hue stage is gated on the donor's own shoulder, ONE constant for both so they cannot drift: below it
      // ReinhardPiecewise returns its input exactly, so the reference equals the target and the transfer is a
      // no-op. The test is taken in BT.709, before the conversion, because every BT.2020 channel is a convex
      // combination of the BT.709 ones (the matrix rows sum to 1) and so can never exceed their max. Worth a real
      // branch: the model runs purity solves, some 300 instructions that most of the frame does not need.
      const float3 extendedBT2020 = BT709_To_BT2020(extendedLinear);
      float3 diceInBT2020 = extendedBT2020;
      [branch] if (max3(extendedLinear) > ME3_HUE_REFERENCE_SHOULDER)
      {
         const float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, ME3_HUE_REFERENCE_SHOULDER);
         diceInBT2020 = MacLeodBoynton::HueOnlyBT2020(extendedBT2020, hueReferenceBT2020);
      }
      DICESettings settings = ME3_DICESettings();
      settings.InOutColorSpace = CS_BT2020;
      float3 hdrColor = DICETonemap(diceInBT2020 * ME3_PaperWhite(), ME3_PeakWhite(), settings) / ME3_PaperWhite();
      hdrColor = BT2020_To_BT709(SimpleGamutClip(hdrColor, true));
      hdrColor = Saturation(hdrColor, LumaSettings.GameSettings.Saturation);

      // The output scale is also the engine fade: re-applied last, as the gain the vanilla chain gives it (before the
      // gamma exponent and the display decode), so a fade matches vanilla instead of biasing the hue stage or DICE.
      color = hdrColor * gamma_to_linear(PowUE3(GammaColorScaleAndInverse.xyz, GammaColorScaleAndInverse.www), GCT_POSITIVE);
   }
   else
#endif
   {
      color = gamma_to_linear(GradeUE3(gradeIn, true, GammaColorScaleAndInverse.xyz), GCT_POSITIVE) * vignette;
   }
   return color;
}

#else
// ---------------------------------------------------------------------------------------------------------------------
// LUT family: MELE family 04 (ME3LE filmic). Constants and helpers ported from "Mass Effect Legendary Edition"
// Includes/Tonemap_MELE_HDRConfig.hlsli, Tonemap_MELE_FilmicExtended.hlsli and Tonemap_MELE_HDRBridge.hlsli; the
// reasoning behind each lives there and in that folder's AGENTS.md. Deltas: dgVoodoo masks on every fetch, and this
// game's grade floor is 1e-6 where ME3LE's is 1e-4.
// ---------------------------------------------------------------------------------------------------------------------

// Max-channel proxy in the adapted domain (r = the frame's own output gamma * DefaultGamma), so the restore divide is
// exact. On failure the caller keeps the exact native SDR result for the whole triple.
bool MELE_TryBuildGradeProxy(float3 workNative, float r, out float q, out float3 proxyNative)
{
   q = 1.0;
   proxyNative = workNative;
   if (!MELE_IsFiniteNonNegative(workNative) || !(r > 0.0 && r <= FLT_MAX))
   {
      return false;
   }
   const float3 adapted = (r == 1.0) ? workNative : gamma_to_linear(workNative, GCT_MIRROR, r);
   if (!MELE_IsFiniteNonNegative(adapted))
   {
      return false;
   }
   const float m = max3(adapted);
   if (m <= MELE_HDR_BRIDGE_SHOULDER)
   {
      return true;
   }
   const float scale = Reinhard::ReinhardRange(m.xxx, MELE_HDR_BRIDGE_SHOULDER).x / m;
   const float3 compressed = adapted * scale;
   if (!(scale > 0.0 && scale <= 1.0) || !MELE_IsFiniteNonNegative(compressed) || max3(compressed) > 1.0 + MELE_HDR_BRIDGE_PROXY_EPS)
   {
      return false;
   }
   const float3 candidateProxy = (r == 1.0) ? compressed : gamma_to_linear(compressed, GCT_MIRROR, 1.0 / r);
   if (!MELE_IsFiniteNonNegative(candidateProxy))
   {
      return false;
   }
   q = scale;
   proxyNative = candidateProxy;
   return true;
}

bool MELE_TryRestoreGradeRange(float3 gradedLinear, float q, out float3 workHDR)
{
   workHDR = gradedLinear / q;
   return MELE_IsFiniteNonNegative(workHDR);
}

// RGB ratios from the exact native grade result, luminance from the reconstruction. Declines to the reference.
float3 MELE_NativeColorAtLuminance(float3 nativeReferenceLinear, float targetLuminance)
{
   if (!MELE_IsFiniteNonNegative(nativeReferenceLinear) || !MELE_IsFiniteNonNegative(targetLuminance))
   {
      return nativeReferenceLinear;
   }
   if (targetLuminance == 0.0 || all(nativeReferenceLinear == 0.0))
   {
      return float3(0.0, 0.0, 0.0); // The grade produced that black.
   }
   const float referenceLuminance = GetLuminance(nativeReferenceLinear, CS_BT709);
   if (!MELE_IsFiniteNonNegative(referenceLuminance) || referenceLuminance < MELE_NATIVE_COLOR_MIN_LUMINANCE)
   {
      return nativeReferenceLinear;
   }
   const float gain = targetLuminance / referenceLuminance;
   const float3 result = nativeReferenceLinear * gain;
   if (!MELE_IsFiniteNonNegative(gain) || !MELE_IsFiniteNonNegative(result))
   {
      return nativeReferenceLinear;
   }
   return result;
}

// Native grade from filmic output to the gamma-encoded SDR value, verbatim from 0xAE84C59B: 16-slice LUT (slice from
// B, strip x from R, y from G, straight RGB), overlay, saturate(scale), 1e-6 floor, pow(inverse gamma).
float3 ME3_GradeChain(float3 filmic)
{
   const float slice = floor(filmic.z * 15.0);
   const float sliceFrac = filmic.z * 15.0 - slice;
   const float2 uv = float2(slice * 0.0625 + filmic.x * 0.05859375, filmic.y * 0.9375);
   const float3 lutA = FETCH(SLOT_LUT, uv + float2(0.001953125, 0.03125)).xyz;
   const float3 lutB = FETCH(SLOT_LUT, uv + float2(0.064453125, 0.03125)).xyz;
   const float3 c = sliceFrac * (lutB - lutA) + lutA + GammaOverlayColor.xyz;
   return PowUE3(max(saturate(c * GammaColorScaleAndInverse.xyz), 1e-6), GammaColorScaleAndInverse.www);
}

// Returns linear light, Game-Paper-White-relative, BEFORE the vignette (the tail applies it).
float3 ME3_Grade(float3 untonemapped, bool hdr)
{
#if ME3_UBER_FILMIC
   const float3 nativeFilmic = ME3_NativeFilmic(untonemapped);
#else
   const float3 nativeFilmic = untonemapped; // no filmic: the LUT reads the scene itself
#endif

   const float3 sdrLinear = gamma_to_linear(ME3_GradeChain(nativeFilmic), GCT_MIRROR);
   float3 gradedLinear = sdrLinear;
#if TONEMAP_TYPE >= 1
   if (hdr)
   {
      float3 extendedFilmic;
      float q;
      float3 proxy;
      float3 workHDR;
#if ME3_UBER_FILMIC
      // This perm has no pre-curve: the LUT genuinely sees scene+bloom, so the continuation is evaluated on it.
      const bool extended = MELE_TryEvaluateME3LEFilmicExtended(untonemapped, nativeFilmic, extendedFilmic);
      const float r = GammaColorScaleAndInverse.w * DefaultGamma;
#else
      const bool extended = true; // the LUT input is already scene-linear and unbounded
      extendedFilmic = untonemapped;
      const float r = 1.0;
#endif
      if (extended && MELE_TryBuildGradeProxy(extendedFilmic, r, q, proxy) && MELE_TryRestoreGradeRange(gamma_to_linear(ME3_GradeChain(proxy), GCT_MIRROR), q, workHDR))
      {
         gradedLinear = MELE_NativeColorAtLuminance(sdrLinear, GetLuminance(workHDR, CS_BT709));
      }
   }
#endif
   return gradedLinear;
}

#endif

// The frame's last encode: UI Paper White pre-scale, dither, FXAA luma in alpha, SDR cap. Run by the gamma uber perms,
// and by FGammaCorrection after a gamma-less one.
float4 ME3_FinalEncode(float3 encoded, float2 ditherUV)
{
   // UI Paper White pre-scale, in gamma (a pure pow, so equal to the linear multiply).
   const float uiPreScaleEncoded = linear_to_gamma1(GetUIPaperWhitePreScale(), GCT_POSITIVE);
   encoded *= uiPreScaleEncoded;

#if TONEMAP_TYPE >= 1
   // Anti-banding dither, one step of the output quantizer: 8-bit in SDR, 10-bit BT.2020 PQ in HDR.
   if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(encoded, ditherUV, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(encoded, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, ditherUV, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         encoded = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
#endif

   // Native FXAA luma in alpha (FXAA 3 console 0x243EAC75 reads it), of what FXAA will actually see.
   const float luma = dot(encoded, float3(0.299, 0.587, 0.114));

   // Vanilla wrote into an 8-bit UNORM canvas, so outside HDR the HUD always blended against a value capped at white;
   // the canvas is fp16 now, so the cap is explicit. Applied after the alpha, as vanilla's clamp came after its dot.
   if (LumaSettings.DisplayMode != 1)
   {
      encoded = min(encoded, uiPreScaleEncoded);
   }
   return float4(encoded, luma);
}
// ---------------------------------------------------------------------------------------------------------------------
// Entry: `blurUV` = TEXCOORD0.zw (v5), `sceneUV` = TEXCOORD1 (v6: .zw full-res perms, .xy scene / .zw half-res buffer
// in the half-res perms).
// ---------------------------------------------------------------------------------------------------------------------
float4 RunME3Uber(float2 blurUV, float4 sceneUV)
{
   const float3 untonemapped = ME3_SceneMix(blurUV, sceneUV) * LumaSettings.GameSettings.Exposure;
   const bool hdr = TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1;

#if ME3_UBER_HALFRES
   const float2 screenUV = sceneUV.xy;
#else
   const float2 screenUV = sceneUV.zw;
#endif
   const float2 vignetteUV = screenUV * ScreenUVScaleBias.xy + ScreenUVScaleBias.zw;
   // Vignette hoisted ahead of the display map (MELE), so DICE absorbs its >1 blue white point.
#if ME3_UBER_VIGNETTE
   const float3 vignette = ME3_VignetteLinear(vignetteUV);
#else
   const float3 vignette = 1.0;
#endif

#if ME3_UBER_ALU
   float3 color = ME3_Grade(untonemapped, hdr, vignette);
#else
   float3 color = ME3_Grade(untonemapped, hdr) * vignette;
#if TONEMAP_TYPE >= 1
   if (hdr)
   {
      color = DICETonemap(ApplyUserContrast(color) * ME3_PaperWhite(), ME3_PeakWhite(), ME3_DICESettings()) / ME3_PaperWhite();
      color = Saturation(color, LumaSettings.GameSettings.Saturation);
   }
#endif
#endif

   color = IsNaN_Strict(color) ? 0.0 : color;
   color = max(0.0, color);
   // Gamma, as vanilla stored it, so FXAA and the gamma HUD see the domain they expect.
   float3 encoded = linear_to_gamma(color, GCT_POSITIVE);

#if ME3_UBER_GRAIN
   // Native grain in gamma after the vignette, as vanilla: amplitude grows with sqrt of the encoded value.
   {
      const float3 noise = FETCH(SLOT_NOISE, (vignetteUV + NoiseTextureOffset.xy) * 2.0).xyz - 0.6;
      const float3 grain = noise * float3(0.1, 0.15, 0.15) * LumaSettings.GameSettings.FilmGrainIntensity;
#if ME3_UBER_LINEAR
      encoded = linear_to_gamma(max(0.0, color + sqrt(color) * grain), GCT_POSITIVE); // gamma-less perms grain linear light
#else
      encoded = max(0.0, encoded + sqrt(abs(encoded)) * grain);
#endif
   }
#endif

#if ME3_UBER_LINEAR
   // FGammaCorrection encodes after the feedback materials: hand it linear light. Alpha = luma of it, as vanilla.
   const float3 linearOut = gamma_to_linear(encoded, GCT_MIRROR);
   return float4(linearOut, dot(linearOut, float3(0.299, 0.587, 0.114)));
#else
   return ME3_FinalEncode(encoded, sceneUV.zw);
#endif
}

#ifndef ME3_UBER_NO_MAIN // GammaCorrection includes this file and supplies its own main
void main(DGV_MAIN_SIGNATURE)
{
   o0 = RunME3Uber(v5.zw, v6);
}
#endif
