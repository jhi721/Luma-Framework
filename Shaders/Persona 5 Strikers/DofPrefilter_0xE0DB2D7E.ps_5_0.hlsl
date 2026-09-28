// Katana engine DOF prefilter: each half resolution pixel loads its 2x2 scene and depth block, computes the CoC per texel and
// writes their CoC and Karis weighted color (o0) and CoC (o1). Two variants (P5S_DOF_PREFILTER_HIGHLIGHTS 1 and 2,
// 0xD65ABD25 and 0x3D7CAD40, which include this file) also write a bokeh highlight mask (o2: strength by CoC where the exposed
// weighted RGB is flat and above a CoC based threshold, that value, near/far).
// Luma: under DLSS/FSR upscaling the scene is the output resolution upscaler output while the depth stays at render resolution, so
// depth texels are found by scaling the scene texel centers by the depth / scene size ratio (1:1 without upscaling, as vanilla).
// clang-format off
#include "Includes/Common.hlsl"
#include "Includes/cbDof.hlsl"
// clang-format on

#ifndef P5S_DOF_PREFILTER_HIGHLIGHTS
#define P5S_DOF_PREFILTER_HIGHLIGHTS 0
#endif

Texture2D<float4> g_t4MainMap : register(t0);
Texture2D<float> g_t1DepthMap : register(t1);
Texture2D<float4> g_tExposureScaleInfo : register(t2);

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, float2 w1 : TEXCOORD1, out float3 o0 : SV_Target0, out float o1 : SV_Target1
#if P5S_DOF_PREFILTER_HIGHLIGHTS
          ,
          out float3 o2 : SV_Target2
#endif
)
{
   const int2 pixel = int2(v0.xy) << 1;
   float2 sceneSize, depthSize;
   g_t4MainMap.GetDimensions(sceneSize.x, sceneSize.y);
   g_t1DepthMap.GetDimensions(depthSize.x, depthSize.y);
   const float2 depthScale = depthSize / sceneSize;
   const float maxCoc = P5S_DOF_PREFILTER_HIGHLIGHTS ? g_vDofInfo2.y : g_vDofInfo1.x;

   static const int2 offsets[4] = {int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1)};
   float4 coc, weights;
   float3 colors[4];
   [unroll] for (int i = 0; i < 4; i++)
   {
      const int2 texel = pixel + offsets[i];
      const float z = 0.01 / (g_t1DepthMap.Load(int3((texel + 0.5) * depthScale, 0)) * g_vD2Z_Z2D.x + g_vD2Z_Z2D.y);
      coc[i] = (z - g_vDofInfo0.x) * g_vDofInfo0.y / z;
      colors[i] = g_t4MainMap.Load(int3(texel, 0)).rgb;
      weights[i] = saturate(abs(clamp(coc[i], -maxCoc, maxCoc)) / maxCoc) * P5S_KarisWeight(colors[i]);
   }
   const float weightSum = weights.x + weights.y + weights.z + weights.w;
   weights = weightSum < 0.0001 ? 0.25 : weights / weightSum;
   o0 = colors[0] * weights.x + colors[1] * weights.y + colors[2] * weights.z + colors[3] * weights.w;
   o1 = dot(coc, weights);

#if P5S_DOF_PREFILTER_HIGHLIGHTS
   const float exposure = g_vDofInfo0.w < 0.0 ? g_tExposureScaleInfo.Load(int3(0, 0, 0)).x : (g_vDofInfo0.w > 0.0 ? g_vDofInfo0.w : 1.0);
   float weightedRgb = exposure * dot(o0, P5S_PostWeights);
#if P5S_DOF_PREFILTER_HIGHLIGHTS == 2
   weightedRgb = max(weightedRgb, 0.0001);
   const float level = saturate((log2(weightedRgb) + 6.0) * 0.083333);
   const float threshold = min(0.029630 / abs(o1), 1.0) * g_vDofInfo2.x;
#else
   const float level = (log2(weightedRgb) + 6.0) * 0.083333;
   const float threshold = saturate(g_vDofInfo0.z * 16.0 / abs(o1)) * g_vDofInfo2.x;
#endif
   const float gradient = abs(ddx_coarse(level)) + abs(ddy_coarse(level));
   o2.x = (g_vDofInfo2.y < abs(o1) && threshold < weightedRgb) ? (gradient <= 0.5 ? abs(o1) : 0.0) / g_vDofInfo2.y : 0.0;
   o2.y = o2.x > 0.0 ? weightedRgb : 0.0;
   o2.z = o1 > 0.0 ? 1.0 : 0.0;
#endif
}
