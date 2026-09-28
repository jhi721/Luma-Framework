// Saints Row: The Third (2011) — Luma HDR replacements for the rl_hdr final composite.
//
// The game writes the final composite straight into the r8g8b8a8 swapchain; diffusion DoF, distortion and the
// rl_prim_2d UI then run on that display-encoded image. So the output stays gamma-encoded
// (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) and the Core display composition linearizes at present.
//
// rl_hdr_13 (PostProcess 1/2): lerp(weightedRGB, scene, sat) * tint + bloom -> saturate(c / 1.49) -> 1.5u - 0.5u^3 ->
// 32^3 LUT read at sqrt(t) from a 256x128 atlas (8x4 slices, sliced by blue). The LUT is per game state (blended every
// frame, a monochrome duotone in the pause menu), so the vanilla result through the live LUT stays the colour
// reference and HDR only decides how much brighter it gets: the cubic shoulder continued along its own tangent
// past a pivot, as ONE scalar over the clamped vanilla output (the Mass Effect 2 filmic recovery construction).
// rl_hdr_09 (no LUT bound, never captured): the same shoulder ending in pow(1/2.2), same recovery.
// rl_hdr_08 (PostProcess 0): lerp(weightedRGB, scene, sat) * tint -> pow(1/2.2), clipped only by the UNORM swapchain.
// rl_hdr_14 0x8ED48FDF ("convert_to_ldr") is left alone: the exe registers the technique but never selects it.
//
// Scene peak and gamut containment are DICE's; nothing after it re-clamps.

// clang-format off
// ORDER IS LOAD-BEARING - the game-local Common defines the settings struct before the shared includes.
#include "Includes/Common.hlsl"
#include "../Includes/DICE.hlsl"
// clang-format on

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// --- Game bindings (must match the original shaders exactly) ---
cbuffer vc0 : register(b0)
{
   float Bloom_amount : packoffset(c22.x);
}

SamplerState Base_samplerSampler : register(s0);
SamplerState Bloom_stage_0_samplerSampler : register(s5);
SamplerState Lut_sampler_2dSampler : register(s7);
Texture2D<float4> Base_samplerTexture : register(t0);
Texture2D<float4> Bloom_stage_0_samplerTexture : register(t5);
Texture2D<float4> Lut_sampler_2dTexture : register(t7);

#include "Includes/Grade.hlsl"

// Bloom, saturation and tint, divided by the 1.49 clip: the shoulder's input u, unclamped.
float3 SR3_ShoulderInput(float4 scene, float2 uv)
{
   const float3 bloom = Bloom_stage_0_samplerTexture.Sample(Bloom_stage_0_samplerSampler, uv).rgb * Bloom_amount * LumaSettings.GameSettings.BloomIntensity * (1.0 / 3.0);
   return (SR_SaturateAndTint(scene.rgb) + bloom) * LumaSettings.GameSettings.Exposure * (1.0 / 1.49);
}

// rl_hdr_13 0xDD93F990. Returns the gamma-encoded colour; alpha is the scene's, as vanilla.
float4 SR3_TonemapLUT(float2 uv)
{
   const float4 scene = Base_samplerTexture.Sample(Base_samplerSampler, uv);
   const float3 u = SR3_ShoulderInput(scene, uv);
   const float3 index = sqrt(SR_Shoulder(saturate(u)));
   // The LUT's identity returns its own index, so the grading fades out in the encoded space it was authored in.
   const float3 vanilla = lerp(index, SR_SampleLUT(index), LumaSettings.GameSettings.ColorGradingIntensity);
   return SR_Output(gamma_to_linear(vanilla), SR_RecoveryGain(u), uv, scene.a);
}

// rl_hdr_09 0x8829E2EA: rl_hdr_13 without the LUT (no LUT bound), ending in pow(1/2.2) instead.
float4 SR3_TonemapNoLUT(float2 uv)
{
   const float4 scene = Base_samplerTexture.Sample(Base_samplerSampler, uv);
   const float3 u = SR3_ShoulderInput(scene, uv);
   const float3 curved = SR_Shoulder(saturate(u));
   // The vanilla pow(1/2.2) is decoded exactly by gamma 2.2, so the linear reference is the shoulder itself.
   return SR_Output(curved, SR_RecoveryGain(u), uv, scene.a);
}

// rl_hdr_08 0x7B539B2E (PostProcess 0): no bloom, shoulder or LUT; vanilla lost everything above 1 to the swapchain.
float4 SR3_TonemapNoPost(float2 uv)
{
   const float4 scene = Base_samplerTexture.Sample(Base_samplerSampler, uv);
   const float3 color = SR_SaturateAndTint(scene.rgb) * LumaSettings.GameSettings.Exposure;
   return SR_Output(color, 1.0, uv, scene.a);
}
