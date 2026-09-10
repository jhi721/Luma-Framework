// ME3LE analytic stage-1 permutation used by the galaxy map and some cutscenes. It has no LUT, motion blur, grain,
// or pre-grade tonemap curve: analytic Scene* operates directly on linear scene+bloom, followed by gamma, the
// ME3LE blue-tinted white point, and a clamp. Bindings: t0 scene, t1 DoF, t2/t3 near/far DoF, t4 bloom.
//
// Transcribed from live CSO 0x225A8330. With only an SDR clamp, the reversible max-channel wrap uses an identity
// 0.18 -> 0.18 anchor before restoring linear HDR highlights.

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
   float4 RenderTargetClampParameter : packoffset(c4);
   float4 MotionBlurMaskScaleAndBias : packoffset(c5);
   float4x4 ScreenToWorld : packoffset(c6);
   float4x4 PrevViewProjMatrix : packoffset(c10);
   float4 StaticVelocityParameters : packoffset(c14);
   float4 DynamicVelocityParameters : packoffset(c15);
   float StepOffsetsOpaque[5] : packoffset(c16);
   float StepWeightsOpaque[5] : packoffset(c21);
   float StepOffsetsTranslucent[5] : packoffset(c26);
   float StepWeightsTranslucent[5] : packoffset(c31);
   float4 BloomTintAndScreenBlendThreshold : packoffset(c36);
   float4 HalfResMaskRect : packoffset(c37);
   float4 SceneShadowsAndDesaturation : packoffset(c38);
   float4 SceneInverseHighLights : packoffset(c39);
   float4 SceneMidTones : packoffset(c40);
   float4 SceneScaledLuminanceWeights : packoffset(c41);
   float4 GammaColorScaleAndInverse : packoffset(c42);
   float4 GammaOverlayColor : packoffset(c43);
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

// Native analytic SDR grade transcribed from the live CSO, evaluated exactly once on the untouched per-channel
// value in every Display Mode: SDR is its output and nothing else, HDR only scales it. Preserve its
// register-level operations.
float3 MELE_ME3LEAnalytic_GradeChain(float3 c)
{
   float4 r0;
   r0.xyz = c;
   // Native analytic Scene grade directly on linear input.
   r0.xyz = saturate(-SceneShadowsAndDesaturation.xyz + r0.xyz);
   r0.xyz = SceneInverseHighLights.xyz * r0.xyz;
   r0.xyz = log2(r0.xyz);
   r0.xyz = SceneMidTones.xyz * r0.xyz;
   r0.xyz = exp2(r0.xyz);
   r0.w = dot(r0.xyz, SceneScaledLuminanceWeights.xyz);
   r0.xyz = r0.xyz * SceneShadowsAndDesaturation.www + r0.www;
   r0.xyz = GammaOverlayColor.xyz + r0.xyz;
   // Native SDR gamma curve and ME3LE white point.
   r0.xyz = MELE_NativeGammaCurve(r0.xyz, GammaColorScaleAndInverse.xyz, GammaColorScaleAndInverse.w, true);

   r0.xyz = float3(1.01036298, 1.00000572, 1.16309249) * r0.xyz; // Blue-tinted white point; no radial vignette.
   r0.xyz = min(float3(1, 1, 1), r0.xyz);
   return r0.xyz;
}

// Included here, not with the headers: MELE_CompositeDOF reads the _Globals fields and DOF textures declared above.
#include "Includes/Tonemap_MELE_Scene.hlsli"

#if MELE_HDR_ME3_HARDCLIP
#include "Includes/Tonemap_MELE_ReferenceColor.hlsli"
// Experimental family 05. Both wrappers call MELE_ME3LEAnalytic_GradeChain unchanged, caps and all: the
// working branch earns its range by preparing the INPUT, not by stripping the grade. The blue white point and
// the black floor stay inside that function and are not hoisted into the output tail.
//
// G(K(X)) is not K(G(X)). The reference is soft-clipped first and then graded, which is this experiment's
// choice; building K from the already-graded working value would be a different model and needs its own A/B
// before it could replace this one.
// False means the caller keeps its legacy value for the whole triple; work_hdr must not be read then. This is
// the FIRST of the two fallbacks: a failure to build the working HDR at all. The second - a failure of the
// colour transfer alone, with a valid working HDR - belongs to MELE_ReferenceCombine and returns work_hdr.
bool MELE_ME3LEAnalytic_GradeHDR(float3 work_linear, out float3 work_hdr)
{
   work_hdr = float3(0.0, 0.0, 0.0);
   float q;
   float3 proxy;
   if (!MELE_TryBuildGradeProxy(work_linear, GammaColorScaleAndInverse.w * DefaultGamma, q, proxy))
   {
      return false;
   }
   return MELE_TryRestoreGradeRange(gamma_to_linear(MELE_ME3LEAnalytic_GradeChain(proxy), GCT_MIRROR), q, work_hdr);
}

// Only ever evaluated after the wrapper above returned true, so work_linear is already known finite and
// non-negative and this needs no second copy of that check.
float3 MELE_ME3LEAnalytic_GradeSoftReference(float3 work_linear)
{
   const float r = GammaColorScaleAndInverse.w * DefaultGamma;
   const float3 reference_native = MELE_BridgeUnadapt(MELE_SoftClip(MELE_BridgeAdapt(work_linear, r), MELE_HARDCLIP_REFERENCE_START), r);
   return gamma_to_linear(MELE_ME3LEAnalytic_GradeChain(reference_native), GCT_MIRROR);
}
#endif

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

   // Trilogy-wide native near/far depth-of-field composite.
   if (r1.w != 0)
   {
      r1.xyz = MELE_CompositeDOF(r0.xy, r0.zw, r1.xyz);
   }

   // Scene-referred exposure before tonemapping.
   r1.xyz = r1.xyz * LumaSettings.GameSettings.Exposure;

   // Native bloom screen blend using Luma's rebound fp16 bloom.
   r0.xyz = MELE_BloomScreenBlend(r0.xy, r1.xyz, r0.w);

   float3 untonemapped = r0.xyz * r0.www + r1.xyz;
   r0.xyz = untonemapped;
   // No tonemap curve here, so the raw scene reaches the grade and the saturate its grade opens with is this permutation's vanilla blowout; that is a hard clip, so its inverse is the plain max-channel ratio, identity below the clip and mch above it.
   float mele_scale = 1.0;
#if MELE_HDR_ME3_HARDCLIP
   float3 mele_hardclip_hdr = 0.0;
   bool mele_hardclip_valid = false;
#endif
   if (LumaSettings.DisplayMode == 1)
   {
      float mele_mch = max(max3(untonemapped), 1e-6);
      // A Reinhard anchored 0.18 -> 0.18 reduces to mch + 0.82, lifting mids the clip never touched by +32% at mch 0.5 and +82% at mch 1; the clip inverse also cancels the clip's own kink, keeping the product C1.
      mele_scale = 1.0 / mele_mch;
#if MELE_HDR_ME3_HARDCLIP
      mele_hardclip_valid = MELE_ME3LEAnalytic_GradeHDR(untonemapped, mele_hardclip_hdr);
      if (mele_hardclip_valid)
      {
         mele_hardclip_hdr = MELE_ReferenceCombine(mele_hardclip_hdr, MELE_ME3LEAnalytic_GradeSoftReference(untonemapped), LumaSettings.GameSettings.ClipHueShift, LumaSettings.GameSettings.ClipBlowout);
      }
#endif
   }

   float3 sdr_gamma = MELE_ME3LEAnalytic_GradeChain(r0.xyz);

   // Decoded once and reused by both the legacy scale below and, when a family is enabled, the NATIVE
   // composition. fxc already shared this value; the local only stops the source from saying it twice.
   const float3 sdr_linear = gamma_to_linear(sdr_gamma, GCT_MIRROR);

   // Undo compression only where scale < 1, preserving native diffuse/shadow grading and restoring HDR
   // highlights. SDR leaves mele_scale at 1.
   float3 graded_hdr = sdr_linear / min(1.0, mele_scale);
#if MELE_HDR_ME3_HARDCLIP
   if (LumaSettings.DisplayMode == 1 && mele_hardclip_valid)
   {
      // Validity was decided before the grade, not read off the finiteness of its output. Both sliders at zero
      // leave the working HDR untouched, which is the diagnostic view; raising either one moves it toward the
      // soft reference without ever letting relative chroma grow, and a transfer that leaves the nonnegative
      // BT.709 domain returns that same working HDR rather than a clamped colour.
      graded_hdr = mele_hardclip_hdr;
   }
#endif

   // ME3LE analytic tail: no vignette or grain; preserve native output luma in alpha.
#define TM_VIGNETTE_TYPE 0
#define TM_ALPHA_LUMA    1
#include "Includes/Tonemap_MELE_Output.hlsli"
}
