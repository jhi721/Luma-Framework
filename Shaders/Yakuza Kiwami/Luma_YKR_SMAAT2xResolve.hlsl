// SMAA T2x resolve for Yakuza Kiwami (SMAAResolvePS of Includes/SMAA.hlsl), drawn by main.cpp after Core's T2x neighborhood blend: this frame's blend
// and the previous frame's, reprojected by the motion vectors and weighted by the velocity difference kept in the blends' alpha. The blends are
// linear (RGBA16F, alpha = sqrt(5 * velocity length)), the result goes back to the post-ccr canvas' gamma 2.2 and keeps that canvas' alpha.

// For "linear_to_gamma" only: Luma's cbuffers aren't in this pass' root signature, nothing here reads them
#include "Includes/Common.hlsl"

// SMAA.hlsl's default
#define SMAA_REPROJECTION_WEIGHT_SCALE 30.0

// "DrawFullscreenPassD3D12()": the target's width and height
cbuffer ResolveConstants : register(b0)
{
   float4 resolve_params;
}

Texture2D<float4> current_blend : register(t0);
Texture2D<float4> previous_blend : register(t1);
Texture2D<float2> motion_vectors : register(t2); // UV deltas, previous minus current
Texture2D<float4> canvas : register(t3);
SamplerState linear_sampler : register(s0); // "DrawFullscreenPassD3D12()": linear, clamped

float4 main(float4 position : SV_Position) : SV_Target
{
   const int2 pixel = int2(position.xy);
   const float4 current = current_blend.Load(int3(pixel, 0));
   const float2 reprojected_uv = position.xy / resolve_params.xy + motion_vectors.Load(int3(pixel, 0));
   const float4 previous = previous_blend.SampleLevel(linear_sampler, reprojected_uv, 0);

   const float delta = abs(current.a * current.a - previous.a * previous.a) / 5.0;
   const float weight = 0.5 * saturate(1.0 - sqrt(delta) * SMAA_REPROJECTION_WEIGHT_SCALE);
   const float3 resolved = lerp(current.rgb, previous.rgb, weight);
   return float4(linear_to_gamma(resolved, GCT_MIRROR), canvas.Load(int3(pixel, 0)).a);
}
