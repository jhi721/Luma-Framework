// Shared stage-1 body for ME1LE/ME2LE analytic scene permutations used by the galaxy map, some Mako scenes, and
// cutscenes. These permutations have no LUT, motion blur, or film grain. Bindings: t0 scene, t1 DoF, t2/t3
// near/far DoF, t4 bloom.
//
// ME1LE 0xAAE8755A and ME2LE 0xCC76075F share the decompiled scene preparation and grade. Thin entry points select
// vignette parameters and ME2LE's post-gamma white point. Preserve register-level swizzles for comparison with the
// live CSOs. This body produces linear gradedHDR and native gamma sdrGamma; the shared tail applies DICE.
// ME3LE analytic shader 0x225A8330 has a different cbuffer layout and no exponential curve.

// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/Color.hlsl"
#include "../Includes/DICE.hlsl"
#include "../Includes/Reinhard.hlsl" // ReinhardRange, used by the grade proxy.
#include "Includes/Tonemap_MELE_HDRConfig.hlsli"   // HDR reconstruction constants.
#include "Includes/Tonemap_MELE_ExpExtended.hlsli" // Tangent continuation of the native curve.
#include "Includes/Tonemap_MELE_HDRBridge.hlsli"   // Max-channel grade proxy; needs Reinhard above.
// clang-format on

#define cmp -

cbuffer _Globals : register(b0)
{
   float4 PackedParameters : packoffset(c0);
   float4 InputTextureSize : packoffset(c1);
   float4 MinMaxBlurClamp : packoffset(c2);
   float4 DOFKernelParams : packoffset(c3);
   float4 BloomTintAndScreenBlendThreshold : packoffset(c4);
   float4 SceneShadowsAndDesaturation : packoffset(c5);
   float4 SceneInverseHighLights : packoffset(c6);
   float4 SceneMidTones : packoffset(c7);
   float4 SceneScaledLuminanceWeights : packoffset(c8);
   float4 GammaColorScaleAndInverse : packoffset(c9);
   float4 GammaOverlayColor : packoffset(c10);
}

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState DOFTextureSampler_s : register(s1);
SamplerState DOFBlurredNearSampler_s : register(s2);
SamplerState DOFBlurredFarSampler_s : register(s3);
SamplerState BlurredImageSeperateBloomSampler_s : register(s4);
Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float4> DOFTexture : register(t1);
Texture2D<float4> DOFBlurredNear : register(t2);
Texture2D<float4> DOFBlurredFar : register(t3);
Texture2D<float4> BlurredImageSeperateBloom : register(t4);

// Native analytic SDR grade transcribed from the live CSOs, evaluated exactly once on the untouched per-channel
// value in every Display Mode. SDR uses this result directly. HDR keeps its RGB ratios and replaces only its
// luminance with the reconstruction's. Keep its register-level swizzles and optional ME2LE white point unchanged.
float3 MELE_Analytic_GradeChain(float3 c)
{
   float4 r0, r1, r2;
   r0.xyz = c;
   // Native highlight desaturation.
   r1.xyz = float3(0.98082906, 0.980000436, 0.993047416) * r0.xyz;
   r0.w = dot(r0.xyz, float3(0.333000004, 0.333000004, 0.333000004));
   r0.w = cmp(1.10000002 < r0.w);
   r1.w = r0.w ? 1.000000 : 0;
   r2.x = dot(r1.xyz, float3(0.300000012, 0.589999974, 0.109999999));
   r2.xyz = -r0.xyz * float3(0.98082906, 0.980000436, 0.993047416) + r2.xxx;
   r1.xyz = r2.xyz * float3(0.5, 0.5, 0.5) + r1.xyz;
   r0.w = r0.w ? 0 : 1;
   r0.xyz = r0.www * r0.xyz;
   r0.xyz = r1.www * r1.xyz + r0.xyz;
   // Native ImageAdjustments mix.
   r0.w = dot(r0.xyz, float3(0.300000012, 0.589999974, 0.109999999));
   r1.xyz = float3(0.400000006, 0.400000006, 0.400000006) * r0.xyz;
   r1.xyz = r0.www * float3(0.600000024, 0.600000024, 0.600000024) + r1.xyz;
   r1.xyz = r1.xyz * float3(0.00658500008, 0.0199180003, 1) + -r0.xyz;
   r0.xyz = r1.xyz * float3(0.200000003, 0.200000003, 0.200000003) + r0.xyz;
   // Native analytic Scene grade.
   r0.xyz = saturate(-SceneShadowsAndDesaturation.xyz + r0.xyz);
   r0.xyz = SceneInverseHighLights.xyz * r0.xyz;
   r0.xyz = log2(r0.xyz);
   r0.xyz = SceneMidTones.xyz * r0.xyz;
   r0.xyz = exp2(r0.xyz);
   r0.w = dot(r0.xyz, SceneScaledLuminanceWeights.xyz);
   r0.xyz = r0.xyz * SceneShadowsAndDesaturation.www + GammaOverlayColor.xyz;
   r0.xyz = r0.xyz + r0.www;
   // Native SDR gamma curve.
   r0.xyz = MELE_NativeGammaCurve(r0.xyz, GammaColorScaleAndInverse.xyz, GammaColorScaleAndInverse.w, false);

#ifdef TM_ANALYTIC_WHITEPOINT
   r0.xyz = TM_ANALYTIC_WHITEPOINT * r0.xyz; // ME2LE blue-tinted white point; ME1LE defines none.
#endif
   r0.xyz = min(float3(1, 1, 1), r0.xyz);
   return r0.xyz;
}

// Family 02: a copy of MELE_Analytic_GradeChain with exactly three upper tonal caps lifted, so the working branch can
// carry values above the native ceiling. No LUT and no max-channel proxy: the grade is a formula, so it is extended
// as one. Lifted: the saturate opening the Scene grade becomes max(0, .), since log2 follows it; the gamma curve
// loses its cap and gains no floor, because these permutations feed mul_sat straight into log; the closing min(1)
// is dropped. Everything else - highlight desaturation, ImageAdjustments, every Scene* field and its order,
// GammaOverlayColor, both halves of GammaColorScaleAndInverse, the ME2LE encoded white point - is kept.
//
// Lifting the caps is what makes overflow reachable, so the guards live here and not in the native chain. Each runs
// BEFORE the operation it protects: the output alone cannot show a NaN that a later max() swallowed. One failing
// channel declines the WHOLE triple, since switching channels independently would move hue. Artist dials have a
// free sign, so values derived from them get the finite check, not the non-negative one.
//
// The result is native-encoded and uncapped; the caller decodes it once.
bool MELE_TryAnalyticGradeChainHDR(float3 c, float gammaExponent, out float3 encodedHDR)
{
   float4 r0, r1, r2;
   r0.xyz = c;
   // Native highlight desaturation.
   r1.xyz = float3(0.98082906, 0.980000436, 0.993047416) * r0.xyz;
   r0.w = dot(r0.xyz, float3(0.333000004, 0.333000004, 0.333000004));
   r0.w = cmp(1.10000002 < r0.w);
   r1.w = r0.w ? 1.000000 : 0;
   r2.x = dot(r1.xyz, float3(0.300000012, 0.589999974, 0.109999999));
   r2.xyz = -r0.xyz * float3(0.98082906, 0.980000436, 0.993047416) + r2.xxx;
   r1.xyz = r2.xyz * float3(0.5, 0.5, 0.5) + r1.xyz;
   r0.w = r0.w ? 0 : 1;
   r0.xyz = r0.www * r0.xyz;
   r0.xyz = r1.www * r1.xyz + r0.xyz;
   // Native ImageAdjustments mix.
   r0.w = dot(r0.xyz, float3(0.300000012, 0.589999974, 0.109999999));
   r1.xyz = float3(0.400000006, 0.400000006, 0.400000006) * r0.xyz;
   r1.xyz = r0.www * float3(0.600000024, 0.600000024, 0.600000024) + r1.xyz;
   r1.xyz = r1.xyz * float3(0.00658500008, 0.0199180003, 1) + -r0.xyz;
   r0.xyz = r1.xyz * float3(0.200000003, 0.200000003, 0.200000003) + r0.xyz;

   // Native analytic Scene grade, with only the upper half of the opening saturate removed. The shift is checked
   // BEFORE the max, which would hide a NaN; that check also covers the signed artistic head above.
   const float3 shifted = -SceneShadowsAndDesaturation.xyz + r0.xyz;
   // A negative shift is ordinary artist data, not an error. A non-finite SceneInverseHighLights turns the
   // product into NaN or inf, which the non-negative check on the log base rejects.
   r0.xyz = SceneInverseHighLights.xyz * max(float3(0, 0, 0), shifted);
   if (!MELE_IsFinite(shifted) || !MELE_IsFiniteNonNegative(r0.xyz))
   {
      encodedHDR = r0.xyz;
      return false;
   }
   // log2(0) is -inf in SM4/5, and exp2(m * -inf) is 0 for any m > 0: the vanilla result for a black channel, which
   // must survive. A zero base with a non-positive exponent would be NaN or +inf instead, so it is refused before
   // the logarithm rather than invented into 0^0 = 1.
   if (any(r0.xyz == 0.0 && SceneMidTones.xyz <= 0.0))
   {
      encodedHDR = r0.xyz;
      return false;
   }
   r0.xyz = exp2(SceneMidTones.xyz * log2(r0.xyz));
   // exp2 never returns a negative, so this rejects only NaN and +inf; an underflow to zero is legitimate.
   // The weights are signed, so their dot only needs to be finite; a non-finite weight cannot cancel to finite.
   r0.w = dot(r0.xyz, SceneScaledLuminanceWeights.xyz);
   if (!MELE_IsFiniteNonNegative(r0.xyz) || !MELE_IsFinite(r0.w))
   {
      encodedHDR = r0.xyz;
      return false;
   }
   // Same scale and exponent as the native tail, without its cap. Any non-finite desaturation, overlay or scale
   // field surfaces in gammaInput.
   const float3 gammaInput = GammaColorScaleAndInverse.xyz * (r0.xyz * SceneShadowsAndDesaturation.www + GammaOverlayColor.xyz + r0.www);
   // GCT_MIRROR is the odd extension, so a negative encoded value is signed data and not a bad pow base.
   // Finiteness is the contract here; non-negativity is the caller's, after the decode.
   encodedHDR = linear_to_gamma(gammaInput, GCT_MIRROR, gammaExponent);
#ifdef TM_ANALYTIC_WHITEPOINT
   encodedHDR = TM_ANALYTIC_WHITEPOINT * encodedHDR; // Still encoded, still the same tint; it may now exceed 1.
#endif
   return MELE_IsFinite(gammaInput) && MELE_IsFinite(encodedHDR);
}

// Sources, the two cbuffer values no later guard observes, then the working value, the checked chain, and one
// decode. SceneMidTones is checked here because an infinite exponent on a positive base collapses to an exact 0
// that passes every later guard; the other Scene* and Gamma* fields reach a guarded value through arithmetic that
// turns a non-finite into NaN or inf, and their unused .w never takes part. GammaColorScaleAndInverse.w must be
// positive because the tail divides by it; the exponent is checked too, since a tiny invGamma overflows the
// reciprocal. A zero output scale is a fade and is NOT refused.
//
// Bad parameters are never repaired: false only declines the reconstruction, and the caller keeps the exact native
// SDR reference.
bool MELE_TryAnalyticGradeHDR(float3 sceneLinear, float3 bloomLinear, out float3 workHDR)
{
   const float invGamma = GammaColorScaleAndInverse.w;
   const float gammaExponent = 1.0 / invGamma;
   if (!MELE_IsFiniteNonNegative(sceneLinear) || !MELE_IsFiniteNonNegative(bloomLinear) || !MELE_IsFinite(SceneMidTones.xyz) || !(invGamma > 0.0 && invGamma <= FLT_MAX && gammaExponent <= FLT_MAX))
   {
      workHDR = float3(0.0, 0.0, 0.0);
      return false;
   }
   const float3 workNative = MELE_ExpExtended(sceneLinear, MELE_HDR_PIVOT) + bloomLinear;
   float3 encoded;
   // workNative needs no check of its own: a non-finite value propagates to the shift, which is checked.
   const bool chainValid = MELE_TryAnalyticGradeChainHDR(workNative, gammaExponent, encoded);
   workHDR = gamma_to_linear(encoded, GCT_MIRROR);
   return chainValid && MELE_IsFiniteNonNegative(workHDR);
}

// Included here, not with the headers: MELE_CompositeDOF reads the _Globals fields and DOF textures declared above.
#include "Includes/Tonemap_MELE_Scene.hlsli"

void main(
    float4 v0 : TEXCOORD0,
    float2 v1 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float o1 : SV_Target1)
{
   float4 r0, r1, r2, r3, r4;

   r0.xy = DynamicScale.xy * v0.zw;
   r1.xyz = SceneColorTexture.Sample(SceneColorTextureSampler_s, r0.xy).xyz;
   r0.zw = cmp(float2(0, 0) < MinMaxBlurClamp.xy);
   r1.w = (int)r0.w | (int)r0.z;

   // Native near/far depth-of-field composite.
   if (r1.w != 0)
   {
      r1.xyz = MELE_CompositeDOF(r0.xy, r0.zw, r1.xyz);
   }

   // Scene-referred exposure before tonemapping.
   r1.xyz = r1.xyz * LumaSettings.GameSettings.Exposure;

   // Native screen-blend using Luma's rebound fp16 bloom; preserve unclamped linear scene+bloom for HDR.
   r0.xyz = MELE_BloomScreenBlend(r0.xy, r1.xyz, r0.w);

   // Captured before the curve below overwrites r1. Straight RGB here, unlike the ME1LE/ME2LE LUT body.
   const float3 sceneLinear = r1.xyz;
   const float3 bloomLinear = r0.xyz * r0.www;

   // Native per-channel SDR curve: 1 - exp2(-1.7 * scene).
   r1.xyz = float3(-1.70000005, -1.70000005, -1.70000005) * r1.xyz;
   r1.xyz = exp2(r1.xyz);
   r1.xyz = float3(1, 1, 1) + -r1.xyz;
   r0.xyz = r0.xyz * r0.www + r1.xyz;
   // The native per-channel value still reaches the analytic grade untouched - that is this body's SDR output.
   // Family 02 does not touch it; it builds a second, uncapped working value from the scene instead.
   float3 workHDR = 0.0;
   bool workValid = false;
   if (LumaSettings.DisplayMode == 1)
   {
      // Extension on the scene before its curve, bloom added where vanilla adds it, then one decode of the
      // uncapped encoded grade. Below the pivot with an inert grade this reduces to the native result.
      workValid = MELE_TryAnalyticGradeHDR(sceneLinear, bloomLinear, workHDR);
   }

   // Apply the same native grade function to the working value and, below, to the SDR reference.
   float3 sdrGamma = MELE_Analytic_GradeChain(r0.xyz);

   // The output tail decodes sdrGamma again on purpose; see Tonemap_MELE_Output.hlsli.
   const float3 sdrLinear = gamma_to_linear(sdrGamma, GCT_MIRROR);

   // The exact native SDR result is the starting value and the only fallback, for the whole triple.
   float3 gradedHDR = sdrLinear;
   if (workValid)
   {
      // Hue and saturation come from the real bounded grade, never from the uncapped twin and never from the
      // scene; only the luminance is the twin's.
      gradedHDR = MELE_NativeColorAtLuminance(sdrLinear, GetLuminance(workHDR, CS_BT709));
   }

   // Entry point supplies vignette macros. Analytic ME1LE/ME2LE permutations have no grain and write zero alpha.
#include "Includes/Tonemap_MELE_Output.hlsli"
}
