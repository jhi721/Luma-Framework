// Shared stage-1 body for ME1LE/ME2LE analytic scene permutations used by the galaxy map, some Mako scenes, and
// cutscenes. These permutations have no LUT, motion blur, or film grain. Bindings: t0 scene, t1 DoF, t2/t3
// near/far DoF, t4 bloom.
//
// ME1LE 0xAAE8755A and ME2LE 0xCC76075F share the decompiled scene preparation and grade. Thin entry points select
// vignette parameters and ME2LE's post-gamma white point. Preserve register-level swizzles for comparison with the
// live CSOs. This body produces linear graded_hdr and native gamma sdr_gamma; the shared tail applies DICE.
// ME3LE analytic shader 0x225A8330 has a different cbuffer layout and no exponential curve.

// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/Color.hlsl"
#include "../Includes/DICE.hlsl"
#include "../Includes/Reinhard.hlsl" // ReinhardRange, used by the experimental grade proxy.
#include "Includes/Tonemap_MELE_ExperimentConfig.hlsli" // Experimental HDR selectors; every one defaults to 0.
#include "Includes/Tonemap_MELE_ExpExtended.hlsli"      // Tangent continuation of the native curve.
#include "Includes/Tonemap_MELE_HDRBridge.hlsli"        // Max-channel grade proxy; needs Reinhard above.
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
// value in every Display Mode: SDR is its output and nothing else, HDR only scales it. Keep its register-level
// swizzles and optional ME2LE white point unchanged.
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

#if MELE_HDR_EXP_ANALYTIC
// Experimental family 02. A minimal copy of MELE_Analytic_GradeChain with
// exactly three upper tonal caps lifted and nothing else touched, so the
// working branch can carry values above the native ceiling. There is no LUT
// here and no max-channel proxy: the grade is available as a formula, so it is
// extended as a formula rather than routed through an invented table. The
// native function above is left untouched and still produces SDR.
//
// Lifted, and only these:
//   the saturate opening the Scene grade becomes max(0, .). The lower bound
//   must stay - log2 follows it. MELE_NativeGammaCurve becomes its cap-free
//   twin. clampFloor was already false for this family and no
//     floor is introduced, because these two permutations feed mul_sat straight
//     into log in the vanilla code.
//   the closing min(1, rgb) is not applied.
// Kept: the highlight-desaturation rule and its 1.1 threshold, the
// ImageAdjustments mix, every Scene* field meaning and the order they are
// applied in, the RGB desaturation weights, GammaOverlayColor, both halves of
// GammaColorScaleAndInverse, and the ME2LE encoded white point in the same
// encoded domain.
//
// The result is native-encoded and uncapped; the caller decodes it once.
//
// WHY THIS ONE IS CHECKED AND THE NATIVE ONE IS NOT. Lifting the caps is what
// makes the arithmetic reachable: a saturate and a min(1) between every stage
// cannot produce an infinity, and once they are gone SceneMidTones acting on a
// value above 1 grows without bound. So the guards belong here and only here -
// the native chain above keeps its caps and is left alone.
//
// The checks are placed BEFORE the operation each one protects, not after the
// whole grade. Judging the output alone cannot distinguish a value that was
// always fine from one that overflowed and came back finite, and it cannot see
// a NaN that a later max() swallowed. A failure of any single channel returns
// false for the WHOLE triple: switching channels independently would move hue,
// which is the failure this branch exists to avoid.
bool MELE_TryAnalyticGradeChainHDR(float3 c, float gamma_exponent, out float3 encoded_hdr)
{
   encoded_hdr = float3(0.0, 0.0, 0.0);
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
   // The artistic head is signed by construction - the desaturation delta and
   // the adjustment mix both subtract - so this is the finite check, not the
   // non-negative one.
   if (!MELE_IsFinite(r0.xyz))
   {
      return false;
   }

   // Native analytic Scene grade, with only the upper half of the opening
   // saturate removed. The shift is kept as its own value so it can be checked
   // BEFORE the max: a max() over a NaN returns the NaN on some hardware and the
   // other operand on others, and either way it stops being visible afterwards.
   const float3 shifted = -SceneShadowsAndDesaturation.xyz + r0.xyz;
   if (!MELE_IsFinite(shifted))
   {
      return false;
   }
   r0.xyz =
       max(float3(0, 0, 0),
           shifted); // A negative shift is ordinary artist data, not an error.
   r0.xyz = SceneInverseHighLights.xyz * r0.xyz;

   // The actual base of the logarithm, checked as data rather than inferred from
   // the sign of the coefficient that produced it: a negative
   // SceneInverseHighLights against a zero lower bound is still a perfectly good
   // zero.
   if (!MELE_IsFiniteNonNegative(r0.xyz))
   {
      return false;
   }
   // log2(0) is -inf in SM4/5, and exp2(m * -inf) is 0 for any m > 0. That is
   // the vanilla result for a black channel and it must survive: a blanket
   // isfinite() on the logarithm would reject it, and an epsilon floor would
   // change the picture instead of protecting it. What cannot survive is a zero
   // base with a non-positive exponent - 0 * -inf is a NaN and -m * -inf is +inf
   // - so that combination is refused here, before the logarithm, rather than
   // invented into 0^0 = 1.
   if ((r0.x == 0.0 && SceneMidTones.x <= 0.0) ||
       (r0.y == 0.0 && SceneMidTones.y <= 0.0) ||
       (r0.z == 0.0 && SceneMidTones.z <= 0.0))
   {
      return false;
   }
   r0.xyz = log2(r0.xyz);
   r0.xyz = SceneMidTones.xyz * r0.xyz;
   r0.xyz = exp2(r0.xyz);
   // exp2 never returns a negative, so this rejects only NaN and +inf. An
   // underflow to exactly zero after a positive base is a legitimate result and
   // is deliberately accepted.
   if (!MELE_IsFiniteNonNegative(r0.xyz))
   {
      return false;
   }

   r0.w = dot(r0.xyz, SceneScaledLuminanceWeights.xyz);
   if (!MELE_IsFinite(r0.w)) // The weights are signed, so the dot is too.
   {
      return false;
   }
   r0.xyz = r0.xyz * SceneShadowsAndDesaturation.www + GammaOverlayColor.xyz;
   r0.xyz = r0.xyz + r0.www;
   if (!MELE_IsFinite(r0.xyz))
   {
      return false;
   }

   // Same scale and exponent as the native tail, without its cap. The scaled value is formed once,
   // checked, then encoded: the caller already turned invGamma into the exponent linear_to_gamma
   // takes, so neither the multiply nor the reciprocal is repeated here.
   const float3 gamma_input = GammaColorScaleAndInverse.xyz * r0.xyz;
   if (!MELE_IsFinite(gamma_input))
   {
      return false;
   }
   r0.xyz = linear_to_gamma(gamma_input, GCT_MIRROR, gamma_exponent);
   // GCT_MIRROR is the odd extension, so a negative encoded value here is signed
   // data and not a bad pow base. Finiteness is the contract at this point;
   // non-negativity is the caller's, after the decode.
   if (!MELE_IsFinite(r0.xyz))
   {
      return false;
   }

#ifdef TM_ANALYTIC_WHITEPOINT
   r0.xyz = TM_ANALYTIC_WHITEPOINT *
            r0.xyz; // Still encoded, still the same tint; it may now exceed 1.
   if (!MELE_IsFinite(r0.xyz))
   {
      return false;
   }
#endif
   encoded_hdr = r0.xyz;
   return true;
}

// This family has no grade proxy - the grade is available as a formula and is
// extended as a formula - so the bridge's early checks never run here and the
// equivalent ones are written out. Sources, then the cbuffer parameters the RGB
// formula actually reads, then the working value, then the checked chain, then
// a single decode of its result. Nothing is judged only after the fact, and the
// grade is never run twice.
//
// SIGN IS FREE for every artist dial. A negative shadow lift, a negative
// luminance weight and a negative overlay offset are all legitimate game data,
// so they get the finite check and not the non-negative one. Only
// GammaColorScaleAndInverse.w is required positive, because the tail divides by
// it; a zero or negative exponent there is refused rather than quietly replaced
// with 1/2.2. A zero output scale is NOT refused - that is a fade, and fades
// are normal.
//
// The unused .w of SceneMidTones, SceneInverseHighLights,
// SceneScaledLuminanceWeights and GammaOverlayColor are deliberately not
// checked: they take no part in the RGB formula and validating them would
// invent a contract the shader does not have.
//
// A negative decoded result selects the legacy triple where it once reached
// MELE_NativeColorAtLuminance as a target luminance. That is a deliberate
// change to the invalid/fallback contract, not to the tonal model: on finite,
// non-negative data with usable parameters this returns exactly what it always
// returned.
//
// Bad parameters are never repaired. Nothing here writes a neutral constant
// over the game's cbuffer and nothing touches the native SDR path; false only
// means this HDR branch declines, and the caller keeps its own legacy value. If
// the legacy value is itself unusable because the cbuffer is corrupt, that is
// the game's state and not something this branch can fix.
bool MELE_TryAnalyticGradeHDR(float3 scene_linear, float3 bloom_linear,
                              out float3 work_hdr)
{
   work_hdr = float3(0.0, 0.0, 0.0);
   if (!MELE_IsFiniteNonNegative(scene_linear) ||
       !MELE_IsFiniteNonNegative(bloom_linear))
   {
      return false;
   }
   const float inv_gamma = GammaColorScaleAndInverse.w;
   if (!MELE_IsFinite(SceneShadowsAndDesaturation.xyz) ||
       !MELE_IsFinite(SceneShadowsAndDesaturation.w) ||
       !MELE_IsFinite(SceneInverseHighLights.xyz) ||
       !MELE_IsFinite(SceneMidTones.xyz) ||
       !MELE_IsFinite(SceneScaledLuminanceWeights.xyz) ||
       !MELE_IsFinite(GammaOverlayColor.xyz) ||
       !MELE_IsFinite(GammaColorScaleAndInverse.xyz) ||
       !MELE_IsFinite(inv_gamma) || inv_gamma <= 0.0)
   {
      return false;
   }
   // Formed only once its denominator is known good, then checked in turn: a tiny invGamma overflows
   // the reciprocal even though the denominator itself is perfectly finite.
   const float gamma_exponent = 1.0 / inv_gamma;
   if (!MELE_IsFinite(gamma_exponent) || gamma_exponent <= 0.0)
   {
      return false;
   }
   const float3 work_native =
       MELE_ExpExtended(scene_linear, MELE_HDR_PIVOT) + bloom_linear;
   if (!MELE_IsFiniteNonNegative(work_native))
   {
      return false;
   }
   float3 encoded;
   if (!MELE_TryAnalyticGradeChainHDR(work_native, gamma_exponent, encoded))
   {
      return false;
   }
   const float3 candidate = gamma_to_linear(encoded, GCT_MIRROR);
   if (!MELE_IsFiniteNonNegative(candidate))
   {
      return false;
   }
   work_hdr = candidate;
   return true;
}
#endif

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

   float3 untonemapped = r0.xyz * r0.www + r1.xyz;
#if MELE_HDR_EXP_ANALYTIC
   // Captured before the curve below overwrites r1. Straight RGB here, unlike the ME1LE/ME2LE LUT body.
   const float3 mele_scene_linear = r1.xyz;
   const float3 mele_bloom_linear = r0.xyz * r0.www;
#endif

   // Native per-channel SDR curve: 1 - exp2(-1.7 * scene).
   r1.xyz = float3(-1.70000005, -1.70000005, -1.70000005) * r1.xyz;
   r1.xyz = exp2(r1.xyz);
   r1.xyz = float3(1, 1, 1) + -r1.xyz;
   r0.xyz = r0.xyz * r0.www + r1.xyz;
   // The native per-channel value reaches the analytic grade untouched, so the vanilla white blowout survives into
   // HDR: HDR only measures the reversible max-channel ratio that expands the grade output below, taken as the exact inverse of the native exponential curve.
   float mele_scale = 1.0;
#if MELE_HDR_EXP_ANALYTIC
   float3 mele_analytic_hdr = 0.0;
   bool mele_analytic_valid = false;
#endif
   if (LumaSettings.DisplayMode == 1)
   {
      float mele_mch = max(max3(untonemapped), 1e-6);
      // Invert the curve the game actually applies, normalized so mid-gray holds still at 1-exp2(-1.7*0.18) = 0.1911
      mele_scale = (MELE_NativeToneCurve(mele_mch) / mele_mch) * (0.18 / MELE_NativeToneCurve(0.18));
#if MELE_HDR_EXP_ANALYTIC
      // Extension on the scene before its curve, bloom added where vanilla adds it, then one decode of the
      // uncapped encoded grade. Below the pivot with an inert grade this reduces to the native result.
      mele_analytic_valid = MELE_TryAnalyticGradeHDR(mele_scene_linear, mele_bloom_linear, mele_analytic_hdr);
#endif
   }

   // Apply the same native grade function to the working value and, below, to the SDR reference.
   float3 sdr_gamma = MELE_Analytic_GradeChain(r0.xyz);

   // Decoded once and reused by both the legacy scale below and, when a family is enabled, the NATIVE
   // composition. fxc already shared this value; the local only stops the source from saying it twice.
   const float3 sdr_linear = gamma_to_linear(sdr_gamma, GCT_MIRROR);

   // Undo compression only where scale < 1, preserving the native diffuse/shadow grade while expanding HDR
   // highlights. SDR leaves mele_scale at 1.
   float3 graded_hdr = sdr_linear / min(1.0, mele_scale);
#if MELE_HDR_EXP_ANALYTIC
   // Lifting the caps lets a game exponent run away, so the whole triple is validated at once. Falling back
   // per channel would move hue, which is the failure this branch exists to avoid.
   if (LumaSettings.DisplayMode == 1 && mele_analytic_valid)
   {
      // Hue and saturation come from the real bounded grade, never from the uncapped twin and never from the
      // scene; only the luminance is the twin's.
      graded_hdr = MELE_NativeColorAtLuminance(sdr_linear, GetLuminance(mele_analytic_hdr, CS_BT709), graded_hdr);
   }
#endif

   // Entry point supplies vignette macros. Analytic ME1LE/ME2LE permutations have no grain and write zero alpha.
#include "Includes/Tonemap_MELE_Output.hlsli"
}
