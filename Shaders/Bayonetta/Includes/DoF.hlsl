#ifndef BAYONETTA_DOF_HLSL
#define BAYONETTA_DOF_HLSL

#include "../../Includes/DgVoodoo.hlsl"

// Bayonetta's DoF passes ("filter08_0" to "_4", picked by FUN_00449c40's mode), transcribed from the SM3 sources and their dgVoodoo
// translations: the authored taps, weights and normalization, read as the game read them at 1280x720. A gather tap's weight is its
// own depth's circle of confusion (near and far ramps in g_Param), the centre's is 1, and alpha is the centre's (the blend into the
// scene). docs/DoF-Best-Practices.md (DOF-1, DOF-2, DOF-13) for the choices. The taps step
// in pixels of "kernel_target_size": the target's size, or with "Fix High Resolution Effects" its size at 720p (b10, see
// "BindPixelKernelTargetSize"). Scaled that way the original read one bilinear sample per tap of a target 3x finer at 4K, so sharp
// edges came out as offset copies; here a tap averages the target over the bilinear footprint a 720p texel had (a tent of +-1 kernel
// pixel), and with one kernel pixel per target pixel (the fix off, or 720p) it is the original's single sample.
// D3D9 c(n) = cb4[n + 8].

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo's texture format emulation masks: t0 at 44/45, t1 at 46/47
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: g_MatrialColor c184 [192], g_Param c185 [193], g_Param2 c186 [194], g_TargetSize c194 [202]
}

cbuffer TargetSize : register(b10)
{
   float2 kernel_target_size; // g_TargetSize as the taps step it (see above)
}

Texture2D<float4> t0 : register(t0); // The scene
Texture2D<float4> t1 : register(t1); // ZTexture (R32F)

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1); // Point

struct DoFContext
{
   float4 v5;
   float2 kernel_texel;  // One kernel pixel in UV
   float2 texel;         // One target pixel in UV
   int footprint_taps;   // Per axis, 1 when a kernel pixel is a target pixel
   float footprint_step; // In target pixels
   float footprint_half_width;
};

DoFContext MakeDoFContext(float4 v5)
{
   DoFContext context;
   context.v5 = v5;
   context.kernel_texel = float2(DgVoodooRcp(kernel_target_size.x), DgVoodooRcp(kernel_target_size.y));
   context.texel = float2(DgVoodooRcp(cb4[202].x), DgVoodooRcp(cb4[202].y));
   // Target pixels per kernel pixel: the half width of a kernel texel's bilinear tent, covered by samples at most 2 px apart (each
   // sample is itself bilinear)
   const float kernel_pixel = cb4[202].y * DgVoodooRcp(kernel_target_size.y);
   context.footprint_half_width = max(kernel_pixel, 1.0);
   context.footprint_taps = clamp(int(ceil(context.footprint_half_width - 0.001)), 1, 4);
   context.footprint_step = 2.0 * context.footprint_half_width / context.footprint_taps;
   return context;
}

// The colour a 720p tap at "uv" saw: the target averaged over a kernel texel's bilinear tent
float3 FootprintColor(DoFContext context, float2 uv)
{
   float3 sum = 0.0;
   float weight_sum = 0.0;
   [loop] for (int y = 0; y < context.footprint_taps; y++)
   {
      [loop] for (int x = 0; x < context.footprint_taps; x++)
      {
         const float2 offset = (float2(x, y) - (context.footprint_taps - 1) * 0.5) * context.footprint_step;
         const float2 tent = saturate(1.0 - abs(offset) / context.footprint_half_width);
         const float weight = tent.x * tent.y;
         sum += weight * ApplyDgvMask(t0.SampleLevel(s0_s, uv + offset * context.texel, 0), cb3[44], cb3[45]).rgb;
         weight_sum += weight;
      }
   }
   return sum / weight_sum;
}

// The depth term the circle of confusion ramps over, "v5.w / (v5.z - depth + 1)", the depth read half a kernel texel on from "uv"
float DepthTerm(DoFContext context, float2 uv)
{
   const float depth = ApplyDgvMask(t1.SampleLevel(s1_s, uv + context.kernel_texel * 0.5, 0), cb3[46], cb3[47]).x;
   return DgVoodooRcp(context.v5.z - depth + 1.0) * context.v5.w;
}

// g_Param's near and far ramps
float CircleOfConfusion(float depth_term)
{
   const float2 ramp_rcp = float2(DgVoodooRcp(cb4[193].y - cb4[193].x), DgVoodooRcp(cb4[193].w - cb4[193].z));
   const float2 ramps = saturate(depth_term * ramp_rcp - cb4[193].xz * ramp_rcp);
   return max(ramps.x, ramps.y);
}

// (sum + centre) / (weights + 1): the centre's weight is 1, read half a kernel texel back as the game does
float3 ResolveGather(DoFContext context, float4 weighted)
{
   const float3 centre = FootprintColor(context, context.v5.xy - context.kernel_texel * 0.5);
   return (weighted.rgb + centre) * DgVoodooRcp(weighted.a + 1.0);
}

// filter08_1/2/3's 16 taps at (+-3k, +-k), (+-k, +-3k), (+-3k, +-3k) and (+-1, +-1) kernel pixels: their CoC weighted sum (rgb,
// weight), their plain colour sum (filter08_3) and the inner (+-1, +-1) taps' depth terms (filter08_3's edges)
struct DoFRing
{
   float4 weighted;
   float3 color_sum;
   float4 inner_depth_terms;
};

DoFRing GatherRing(DoFContext context, float k)
{
   DoFRing ring = (DoFRing)0;
   [unroll] for (int i = 0; i < 4; i++)
   {
      const float2 sign = float2((i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0);
      const float2 offsets[4] = {sign * float2(3.0 * k, k), sign * float2(k, 3.0 * k), sign * 3.0 * k, sign};
      [unroll] for (int j = 0; j < 4; j++)
      {
         const float2 uv = context.v5.xy + offsets[j] * context.kernel_texel;
         const float depth_term = DepthTerm(context, uv);
         const float weight = CircleOfConfusion(depth_term);
         const float3 color = FootprintColor(context, uv);
         ring.weighted += float4(weight * color, weight);
         ring.color_sum += color;
         if (j == 3)
         {
            ring.inner_depth_terms[i] = depth_term;
         }
      }
   }
   return ring;
}

#endif // BAYONETTA_DOF_HLSL
