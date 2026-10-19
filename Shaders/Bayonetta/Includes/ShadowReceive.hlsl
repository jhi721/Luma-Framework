#ifndef BAYONETTA_SHADOW_RECEIVE_HLSL
#define BAYONETTA_SHADOW_RECEIVE_HLSL

#include "../../Includes/DgVoodoo.hlsl"

// Bayonetta's shadow receive passes ("modelshadershadowrec00_*", "modelshadershadowfixed"), transcribed from the SM3 sources and their
// dgVoodoo translations. They project the pixel into the shadow map (R32F, the caster's 1 - z/w), test it at the centre and at a
// cross of taps "margin.x" target pixels away, and draw the shadow where s = centre * (1 + 2 x + 2 y) * 0.2 reaches 0.5, x and y
// being the two pairs of the cross, with alpha (s - 0.5) * vignette * fog. The original tested single texels and lerped each pair
// with the weights of a 512 texel grid ("frac(uv * 512)") on a 1024 to 4096 map: the outline stepped along that grid. Here a test is
// a bilinear PCF at the map's real size (its 2x2 texels weighted by the sample's position) and a pair is averaged: the same outline
// and alpha ramp, smooth. The taps step in pixels of "kernel_target_size" (b10): the target's size, or with "Fix High Resolution
// Effects" its size at 720p, as far apart as the game spaced them at 1280x720.
// D3D9 c(n) = cb4[n + 8].

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo's texture format emulation masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants
}

cbuffer TargetSize : register(b10)
{
   float2 kernel_target_size; // g_TargetSize as the taps step it (see above)
}

// How much of the bilinear footprint at "uv" occludes "reference": a texel occludes when it's nearer the light (the map stores 1 - z/w)
float ShadowPCF(Texture2D<float4> map, SamplerState map_sampler, float4 mask, float4 fill, float2 map_size, float2 uv, float reference)
{
   const float2 position = uv * map_size - 0.5;
   const float2 weight = frac(position);
   // The texels at (0,1), (1,1), (1,0), (0,0) of the footprint. The ps_4_0 twins (dgVoodoo 2.81.3) have no gather: they define
   // SHADOW_RECEIVE_NO_GATHER (fxc has no "__SHADER_TARGET_MAJOR").
#ifdef SHADOW_RECEIVE_NO_GATHER
   const int2 texel = int2(floor(position));
   const float4 gathered = float4(map.Load(int3(texel + int2(0, 1), 0)).x, map.Load(int3(texel + int2(1, 1), 0)).x, map.Load(int3(texel + int2(1, 0), 0)).x, map.Load(int3(texel, 0)).x);
#else
   const float4 gathered = map.GatherRed(map_sampler, (floor(position) + 1.0) / map_size);
#endif
   const float4 depths = ApplyDgvMask(gathered, mask.xxxx, fill.xxxx);
   const float4 occluded = float4(depths > reference);
   return lerp(lerp(occluded.w, occluded.z, weight.x), lerp(occluded.x, occluded.y, weight.x), weight.y);
}

// s (see above) for the shadow map position "shadow_position" (clip space), or a discard where the original discarded; "ndc" for the
// vignette. "map" is the R32F depth, "soft_map" the RGBA8 one the "margin.z < 1" branch averages.
float ShadowReceive(float4 shadow_position, float4 margin, Texture2D<float4> map, SamplerState map_sampler, float4 map_mask, float4 map_fill, Texture2D<float4> soft_map, SamplerState soft_sampler, float4 soft_mask, float4 soft_fill, out float2 ndc)
{
   const float rcp_w = DgVoodooRcp(shadow_position.w);
   ndc = shadow_position.xy * rcp_w;
   if (shadow_position.w < 0.0 || any(abs(ndc) > 1.0))
      discard;
   const float2 uv = ndc * float2(0.5, -0.5) + 0.5;
   const float2 kernel_texel = float2(DgVoodooRcp(kernel_target_size.x), DgVoodooRcp(kernel_target_size.y));
   const float step = margin.x * kernel_texel.x; // Both axes in the target's x pixels, as the original
   const float2 half_texel = 0.5 * kernel_texel; // The original's D3D9 half pixel on the cross taps
   const float reference = (1.0 - shadow_position.z * rcp_w) + margin.y;

   uint width, height;
   map.GetDimensions(width, height);
   const float2 map_size = float2(width, height);
   const float centre = ShadowPCF(map, map_sampler, map_mask, map_fill, map_size, uv, reference);
   const float2 cross_uv = uv + half_texel;
   const float2 cross_offsets[4] = {float2(0.0, -step), float2(-step, 0.0), float2(0.0, step), float2(step, 0.0)};
   // A branch on the uniform margin: a "?:" in the loop ran both kinds of taps. One return: fxc flags the out "ndc" as uninitialized
   // after an early one.
   float cross_sum = 0.0;
   float cross_weight;
   [branch] if (margin.z >= 1.0)
   {
      [unroll] for (int i = 0; i < 4; i++)
      {
         cross_sum += ShadowPCF(map, map_sampler, map_mask, map_fill, map_size, cross_uv + cross_offsets[i], reference);
      }
      // (1 + 2 x + 2 y) * 0.2 with x and y the pairs' averages
      cross_sum += 1.0;
      cross_weight = 0.2;
   }
   else
   {
      [unroll] for (int i = 0; i < 4; i++)
      {
         cross_sum += ApplyDgvMask(soft_map.Sample(soft_sampler, cross_uv + cross_offsets[i]), soft_mask, soft_fill).w;
      }
      // The soft map's average
      cross_weight = 0.25;
   }
   return centre * cross_sum * cross_weight;
}

// The original's vignette over the shadow map's projection
float ShadowVignette(float2 ndc)
{
   const float distance_squared = dot(ndc, ndc);
   return saturate(1.0 - distance_squared * distance_squared);
}

#endif // BAYONETTA_SHADOW_RECEIVE_HLSL
