// DLSS / FSR: the DOF/Bloom gather (0xF0D8D818). Below native it runs at output resolution, from the upscaled scene (the game runs it
// over the render sub-rect, which the uber magnifies: a render-resolution DOF and bloom image).
// Its DOF blur amount comes from the max depth of each 4x4 block, and the depth is jittered: a thin object near a block edge moves in
// and out of it every frame, and the block flips between sharp and blurred after the upscaler (a flickering halo around wires against
// the sky, at native too; any max over the block flips like that). With DOF_HISTORY the amount is the mean over the block of the
// accumulated per pixel amounts (Luma_BL_DOFHistory.hlsl). 0 = the game's.

#ifndef DOF_HISTORY
#define DOF_HISTORY 1
#endif

cbuffer OutputPost : register(b5)
{
   float2 share;     // Render / output: the sub-rect the scene depth still covers
   float2 inv_share; // Output / render: the blur quads' sub-rect UVs to the whole target
};

// The gather, as the game's: 4x4 scene texels per output texel (2x2 per thread, 2x2 threads through group memory), the max depth of
// its 16 for the DOF blur amount (alpha), plus the bloom term; written one texel in from the border.
cbuffer GatherGlobals : register(b0)
{
   float4 gather_constants[4]; // [0] focus distance, 1 / range, exponent; [1] near / far max blur; [2].xy texel offset (ints); [3] bloom threshold, scale, range
};
cbuffer PSOffsetConstants : register(b2)
{
   float4 offset_constants[7]; // [6].zw: MinZ_MaxZRatio (device z to 1 / view z)
};

Texture2D<float4> scene : register(t0);
Texture2D<float4> depth : register(t1);
Texture2D<float> dof_history : register(t2); // Render pixels
RWTexture2D<float4> gathered : register(u0);

groupshared float4 group_texels[64];

#include "Includes/DOFBlurAmount.hlsli"

// Output pixel -> render pixel (the sub-rect below native)
int3 RenderPixel(int2 pixel)
{
   return int3(int2(float2(pixel) * share), 0);
}

[numthreads(8, 8, 1)] void gather_cs(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupThreadID, uint group_index : SV_GroupIndex) {
   const int2 pixel = int2(id.xy) * 2 + asint(gather_constants[2].xy);
   const float3 color = scene.Load(int3(pixel, 0)).rgb + scene.Load(int3(pixel + int2(1, 0), 0)).rgb + scene.Load(int3(pixel + int2(0, 1), 0)).rgb + scene.Load(int3(pixel + int2(1, 1), 0)).rgb;
#if DOF_HISTORY
   const float depth_term = dof_history.Load(RenderPixel(pixel)) + dof_history.Load(RenderPixel(pixel + int2(1, 0))) + dof_history.Load(RenderPixel(pixel + int2(0, 1))) + dof_history.Load(RenderPixel(pixel + int2(1, 1)));
#else
   const float depth_term = max(max(depth.Load(RenderPixel(pixel)).x, depth.Load(RenderPixel(pixel + int2(1, 0))).x), max(depth.Load(RenderPixel(pixel + int2(0, 1))).x, depth.Load(RenderPixel(pixel + int2(1, 1))).x));
#endif
   group_texels[group_index] = float4(color, depth_term);
   GroupMemoryBarrierWithGroupSync();
   if (any(group_id.xy & 1))
      return;

   const float4 right = group_texels[group_index + 1];
   const float4 down = group_texels[group_index + 8];
   const float4 diagonal = group_texels[group_index + 9];
   const float3 average = (right.rgb + down.rgb + diagonal.rgb + color) * 0.0625;
#if DOF_HISTORY
   const float blur = (depth_term + right.w + down.w + diagonal.w) * 0.0625;
#else
   const float blur = BlurAmount(max(max(depth_term, right.w), max(down.w, diagonal.w)));
#endif

   const float3 clamped = min(average, 65536.0);
   float bloom = saturate((dot(clamped, float3(0.3, 0.59, 0.11)) * gather_constants[3].y - gather_constants[3].x) / gather_constants[3].z);
   bloom = log2(bloom * 15.0 + 1.0) * 0.25;
   gathered[(id.xy >> 1) + 1] = float4(blur * average + clamped * bloom, blur);
}
