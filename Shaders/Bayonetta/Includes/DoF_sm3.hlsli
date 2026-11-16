// Bayonetta DoF passes ("filter08_0" to "_4", 0xEA10C34E, 0xB959E4D7,
// 0x548C7346, 0xFC2A113C, 0xCB1D0471), native D3D9 ps_3_0 port of
// the former dgVoodoo mod's Includes/DoF.hlsl: the authored taps, weights and
// normalization of the SM3 originals, read at 1280x720 and stepped in pixels of
// a "kernel target size" (g_LumaKernel.xy: the 720p-scaled unit with Fix High
// Resolution Effects, else c194.xy). Gamma encoded R'G'B' colours, ZTexture an
// R32F depth. A gather tap's weight is its depth's circle of confusion (CoC;
// near and far ramps in g_Param), the centre's is 1, alpha is the centre's CoC
// (the blend into the scene). A tap averages the target over the bilinear
// footprint a 720p texel had (a tent of +-1 kernel pixel, half width
// "half_width" target pixels); with one kernel pixel per target pixel it is the
// original's single sample. Variants (fxc defines):
//   (none)                 dynamic footprint, 1..4 x 4 taps in a loop inside the 16 tap ring loop (2 loop levels)
//   rings are loops too (static slot count small, 2 loop levels) DOF_VANILLA
//   the original: kernel = c194, one tap per sample, all unrolled (equivalence
//   reference, no Luma constants read) DOF_FIXED_FOOTPRINT    no loops: a tap
//   is the single sample when the half width is <= 1, else a fixed 2x2 tent
//   (half width clamped to 2)
#ifndef INCLUDES_DOF_SM3_HLSLI
#define INCLUDES_DOF_SM3_HLSLI

#include "Luma_Block_sm3.hlsli"

sampler2D Texture0 : register(s0);
sampler2D ZTexture : register(s1); // point, R32F
float4 g_MatrialColor : register(c184);
float4 g_Param : register(c185); // near ramp x..y, far ramp z..w
float4 g_Param2
    : register(c186); // x = k (kernel scale), y = edge scale (modes 2 and 3)
float4 g_TargetSize : register(c194);

struct DoFContext
{
   float4 v0;
   float2 kernel_texel; // one kernel pixel in UV
   float2 texel;        // one target pixel in UV
   float taps;          // per axis
   float half_width;    // target pixels per kernel pixel (>= 1)
   float step;          // target pixels between taps
};

DoFContext MakeDoFContext(float4 v0)
{
   DoFContext context;
   context.v0 = v0;
#ifdef DOF_VANILLA
   const float2 kernel_size = g_TargetSize.xy;
#else
   const float2 kernel_size = g_LumaKernel.xy;
#endif
   context.kernel_texel = 1.0 / kernel_size;
   context.texel = 1.0 / g_TargetSize.xy;
   // Target pixels per kernel pixel: the half width of a kernel texel's bilinear
   // tent, covered by samples at most 2 px apart
   context.half_width = max(g_TargetSize.y / kernel_size.y, 1.0);
#if defined(DOF_FIXED_FOOTPRINT)
   context.half_width = min(context.half_width, 2.0);
#endif
   context.taps = clamp(ceil(context.half_width - 0.001), 1.0, 4.0);
   context.step = 2.0 * context.half_width / context.taps;
   return context;
}

// The colour a 720p tap at "uv" saw: the target averaged over a kernel texel's
// bilinear tent
float3 FootprintColor(DoFContext context, float2 uv)
{
#if defined(DOF_VANILLA)
   return tex2Dlod(Texture0, float4(uv, 0.0, 0.0)).rgb;
#elif defined(DOF_FIXED_FOOTPRINT)
   float3 color;
   [branch] if (context.taps < 1.5)
   {
      color = tex2Dlod(Texture0, float4(uv, 0.0, 0.0)).rgb;
   }
   else
   {
      // taps = 2: offsets +-step / 2 = +-half_width / 2, tent weight 0.5 each
      // axis, equal weights
      const float2 offset = 0.5 * context.step * context.texel;
      color =
          (tex2Dlod(Texture0, float4(uv - offset, 0.0, 0.0)).rgb +
           tex2Dlod(Texture0, float4(uv + float2(offset.x, -offset.y), 0.0, 0.0))
               .rgb +
           tex2Dlod(Texture0, float4(uv + float2(-offset.x, offset.y), 0.0, 0.0))
               .rgb +
           tex2Dlod(Texture0, float4(uv + offset, 0.0, 0.0)).rgb) *
          0.25;
   }
   return color;
#else
   float3 sum = 0.0;
   float weight_sum = 0.0;
   const float count = context.taps * context.taps;
   [loop] for (int t = 0; t < 16; t++)
   {
      if (t >= count)
         break;
      const float y = floor((t + 0.5) / context.taps);
      const float2 offset =
          (float2(t - y * context.taps, y) - (context.taps - 1.0) * 0.5) *
          context.step;
      const float2 tent = saturate(1.0 - abs(offset) / context.half_width);
      const float weight = tent.x * tent.y;
      sum +=
          weight *
          tex2Dlod(Texture0, float4(uv + offset * context.texel, 0.0, 0.0)).rgb;
      weight_sum += weight;
   }
   return sum / weight_sum;
#endif
}

// The depth term the CoC ramps over, "v0.w / (v0.z - depth + 1)", the depth
// read half a kernel texel on from "uv"
float DepthTerm(DoFContext context, float2 uv)
{
   const float depth =
       tex2Dlod(ZTexture, float4(uv + context.kernel_texel * 0.5, 0.0, 0.0)).x;
   return 1.0 / (context.v0.z - depth + 1.0) * context.v0.w;
}

// g_Param's near and far ramps
float CircleOfConfusion(float depth_term)
{
   const float2 ramp_rcp =
       float2(1.0 / (g_Param.y - g_Param.x), 1.0 / (g_Param.w - g_Param.z));
   const float2 ramps = saturate(depth_term * ramp_rcp - g_Param.xz * ramp_rcp);
   return max(ramps.x, ramps.y);
}

// (sum + centre) / (weights + 1): the centre's weight is 1, read half a kernel
// texel back as the game does
float3 ResolveGather(DoFContext context, float4 weighted)
{
   const float3 centre =
       FootprintColor(context, context.v0.xy - context.kernel_texel * 0.5);
   return (weighted.rgb + centre) / (weighted.a + 1.0);
}

// filter08_1/2/3's 16 taps at (+-3k, +-k), (+-k, +-3k), (+-3k, +-3k) and (+-1,
// +-1) kernel pixels: their CoC weighted sum (rgb, weight), their plain colour
// sum (filter08_3) and the extremes of the inner (+-1, +-1) taps' depth terms
// (filter08_3's edges)
struct DoFRing
{
   float4 weighted;
   float3 color_sum;
   float inner_max;
   float inner_min;
};

DoFRing GatherRing(DoFContext context, float k)
{
   DoFRing ring;
   ring.weighted = 0.0;
   ring.color_sum = 0.0;
   ring.inner_max = -3.4e38;
   ring.inner_min = 3.4e38;
   [loop] for (int n = 0; n < 16; n++)
   {
      const float corner = floor(n * 0.25 + 0.001);
      const float j = n - corner * 4.0;
      const float2 sign = float2((frac(corner * 0.5) > 0.25) ? 1.0 : -1.0,
                                 (corner > 1.5) ? 1.0 : -1.0);
      const float2 base = (j < 0.5)
                              ? float2(3.0 * k, k)
                              : ((j < 1.5) ? float2(k, 3.0 * k)
                                           : ((j < 2.5) ? float2(3.0 * k, 3.0 * k)
                                                        : float2(1.0, 1.0)));
      const float2 uv = context.v0.xy + sign * base * context.kernel_texel;
      const float depth_term = DepthTerm(context, uv);
      const float weight = CircleOfConfusion(depth_term);
      const float3 color = FootprintColor(context, uv);
      ring.weighted += float4(weight * color, weight);
      ring.color_sum += color;
      if (j > 2.5)
      {
         ring.inner_max = max(ring.inner_max, depth_term);
         ring.inner_min = min(ring.inner_min, depth_term);
      }
   }
   return ring;
}

#endif
