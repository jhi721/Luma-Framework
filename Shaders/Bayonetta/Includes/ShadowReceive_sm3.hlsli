// Bayonetta shadow receive ("modelshadershadowrec00_*",
// "modelshadershadowfixed"), native D3D9 ps_3_0 port of
// the former dgVoodoo mod's Includes/ShadowReceive.hlsl. The pixel is projected into
// the shadow map (R32F, the caster's 1 - z/w, point sampled by the game),
// tested at the centre and at a cross of 4 taps "margin.x" kernel pixels away;
// s = centre * (1 + 2 x + 2 y) * 0.2 (x, y the two pairs of the cross), alpha
// (s - 0.5) * vignette * fog. margin = (step, depth bias, soft map switch).
// SHADOW_VANILLA: the original, each test a single texel and each pair lerped
// with the weights of "frac(uv * 512)". Default (Luma): each test is a bilinear
// PCF at the map's real size (g_LumaShadow.xy; four point reads, the SM3
// stand-in for the dgVoodoo port's GatherRed) and a pair is averaged. The taps
// step in pixels of g_LumaKernel.xy (Fix High Resolution Effects' 720p unit, or
// c194.xy); the vanilla variant steps in c194 pixels, as the original.
// SHADOW_CENTRE_HALF_TEXEL (modelshadershadowrec00_bxxxx only): the centre test is read half a target pixel on, like the cross taps, as that
// blob does; the other variants read the centre at the exact UV.
#ifndef INCLUDES_SHADOW_RECEIVE_SM3_HLSLI
#define INCLUDES_SHADOW_RECEIVE_SM3_HLSLI

#include "Luma_Block_sm3.hlsli"

float4 g_TargetSize : register(c194);

#ifndef SHADOW_VANILLA
// 1 where the bilinear footprint at "uv" occludes "reference": a texel occludes
// when it is nearer the light (the map stores 1 - z/w)
float ShadowPCF(sampler2D map, float2 uv, float reference)
{
   const float2 map_size = g_LumaShadow.xy;
   const float2 position = uv * map_size - 0.5;
   const float2 weight = frac(position);
   const float2 corner = (floor(position) + 0.5) /
                         map_size; // centre of the footprint's (0, 0) texel
   const float2 unit = 1.0 / map_size;
   const float4 depths =
       float4(tex2Dlod(map, float4(corner, 0.0, 0.0)).x,
              tex2Dlod(map, float4(corner + float2(unit.x, 0.0), 0.0, 0.0)).x,
              tex2Dlod(map, float4(corner + float2(0.0, unit.y), 0.0, 0.0)).x,
              tex2Dlod(map, float4(corner + unit, 0.0, 0.0)).x);
   const float4 occluded =
       float4(depths > reference); // (0,0), (1,0), (0,1), (1,1)
   return lerp(lerp(occluded.x, occluded.y, weight.x),
               lerp(occluded.z, occluded.w, weight.x), weight.y);
}
#endif

// s for the shadow map position "shadow_position" (clip space) and the margin;
// kills where the original killed. "ndc" is for the vignette.
float ShadowReceive(float4 shadow_position, float3 margin, sampler2D map,
                    sampler2D soft_map, out float2 ndc)
{
   const float rcp_w = 1.0 / shadow_position.w;
   ndc = shadow_position.xy * rcp_w;
   clip(float4(shadow_position.w, ndc.x + 1.0, ndc.y + 1.0, 0.0));
   clip(float2(1.0 - ndc.x, 1.0 - ndc.y));
   const float2 uv = ndc * float2(0.5, -0.5) + 0.5;
   const float reference = (1.0 - shadow_position.z * rcp_w) + margin.y;
#ifdef SHADOW_VANILLA
   const float2 kernel_texel = 1.0 / g_TargetSize.xy;
#else
   const float2 kernel_texel = 1.0 / g_LumaKernel.xy;
#endif
   const float step =
       margin.x *
       kernel_texel.x; // both axes in the target's x pixels, as the original
   const float2 cross_uv =
       uv +
       0.5 * kernel_texel; // the original's D3D9 half pixel on the cross taps
   const float2 cross_offsets[4] = {float2(0.0, -step), float2(-step, 0.0),
                                    float2(0.0, step), float2(step, 0.0)};
#ifdef SHADOW_CENTRE_HALF_TEXEL
   const float2 centre_uv = cross_uv;
#else
   const float2 centre_uv = uv;
#endif
   float cross_term;
#ifdef SHADOW_VANILLA
   const float centre =
       (reference - tex2Dlod(map, float4(centre_uv, 0.0, 0.0)).x < 0.0) ? 1.0 : 0.0;
   [branch] if (margin.z >= 1.0)
   {
      float occluded[4];
      [unroll] for (int i = 0; i < 4; i++)
      {
         occluded[i] =
             (reference -
                  tex2Dlod(map, float4(cross_uv + cross_offsets[i], 0.0, 0.0)).x <
              0.0)
                 ? 1.0
                 : 0.0;
      }
      const float2 fraction = frac((ndc * float2(1.0, -1.0) + 1.0) * 512.0 + 0.5);
      const float x = lerp(occluded[1], occluded[3], fraction.x);
      const float y = lerp(occluded[0], occluded[2], fraction.y);
      cross_term = ((x + x) + 2.0 * y + 1.0) * 0.2;
   }
   else
   {
      cross_term = 0.0;
      [unroll] for (int i = 0; i < 4; i++)
      {
         cross_term +=
             tex2Dlod(soft_map, float4(cross_uv + cross_offsets[i], 0.0, 0.0)).w *
             0.25;
      }
   }
#else
   const float centre = ShadowPCF(map, centre_uv, reference);
   [branch] if (margin.z >= 1.0)
   {
      float cross_sum = 1.0;
      [unroll] for (int i = 0; i < 4; i++)
      {
         cross_sum += ShadowPCF(map, cross_uv + cross_offsets[i], reference);
      }
      cross_term = cross_sum * 0.2;
   }
   else
   {
      cross_term = 0.0;
      [unroll] for (int i = 0; i < 4; i++)
      {
         cross_term +=
             tex2Dlod(soft_map, float4(cross_uv + cross_offsets[i], 0.0, 0.0)).w *
             0.25;
      }
   }
#endif
   return centre * cross_term;
}

// The original's vignette over the shadow map's projection
float ShadowVignette(float2 ndc)
{
   const float distance_squared = dot(ndc, ndc);
   return saturate(1.0 - distance_squared * distance_squared);
}

#endif
