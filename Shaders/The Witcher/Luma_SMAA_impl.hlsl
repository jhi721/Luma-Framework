// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, Mass Effect 2007's (BL2/TPS and Witcher 2 lineage, same dgVoodoo stack). ADDS antialiasing:
// the game ships none. Injected at the scene's end (its first post pass, see main.cpp "EndScene"), before the post effects and the
// UI. Edges are detected on the GAMMA canvas, the neighborhood blend filters it in linear light (below) and re-encodes.
// The predication mask comes from Luma_Witcher_DepthEdges.

#include "Includes/Common.hlsl"

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunPostFinalGradeSMAA).
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active, 1.0 with a null predication texture
   // (fallback) -> plain ULTRA threshold 0.05. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication is live (CS mask, SCALE 2.0), so both values below are load-bearing: flat = SCALE * SMAA_THRESHOLD =
// 0.10, silhouette = x (1-STRENGTH) = 0.05 (ULTRA base). SMAA_PREDICATION_THRESHOLD 0.5 is scale-free; tune the CS.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// Edge detection: tex0 = colorTexGamma (gamma-encoded graded canvas)
// tex1 = predicationTex (edge-ness in [0,1]; null fallback -> reads 0, scale 1.0 = plain ULTRA threshold)
// Neighborhood blending: tex0 = colorTex (the gamma snapshot), tex1 = blendTex. It averages the pixel with a neighbor through a
// bilinear fetch, which must happen in linear light: the fetch is done by hand, the footprint's texels decoded before weighting
// (clamped as the linear sampler). The same as the hardware bilinear of a linear copy, without the copy. An axis within the
// hardware's 8 bit filter weights of a texel center is on it: the interpolated coordinate's float error would otherwise weight in
// the neighbor a little, visibly so for an HDR one next to a dark pixel. On a texel center on both (every pixel that doesn't
// blend) the fetch is that texel.
float4 SampleGammaInLinear(Texture2D tex, float2 coord)
{
   const float2 nearest = round(coord * SMAA_RT_METRICS.zw - 0.5);
   const float2 texel = abs(coord * SMAA_RT_METRICS.zw - 0.5 - nearest) < 1.0 / 512.0 ? nearest : coord * SMAA_RT_METRICS.zw - 0.5;
   const int2 last = int2(SMAA_RT_METRICS.zw) - 1;
   float4 color;
   [branch] if (all(texel == nearest))
   {
      color = tex.Load(int3(clamp(int2(nearest), 0, last), 0));
      color.rgb = gamma_to_linear(color.rgb, GCT_MIRROR);
   }
   else
   {
      const float2 base = floor(texel);
      const float2 f = texel - base;
      float4 c[4];
      [unroll] for (uint i = 0; i < 4; i++)
      {
         c[i] = tex.Load(int3(clamp(int2(base) + int2(i & 1, i >> 1), 0, last), 0));
         c[i].rgb = gamma_to_linear(c[i].rgb, GCT_MIRROR);
      }
      color = lerp(lerp(c[0], c[1], f.x), lerp(c[2], c[3], f.x), f.y);
   }
   return color;
}
#define SMAA_NEIGHBORHOOD_SAMPLE(tex, coord) SampleGammaInLinear(tex, coord)
// Re-encode to the canvas' gamma.
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
