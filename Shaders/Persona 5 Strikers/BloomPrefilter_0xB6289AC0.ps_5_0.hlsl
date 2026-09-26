// Katana engine bloom prefilter (Unity/Kino style): exposure, each tap capped at 5 (scaled by its max channel in max channel mode, else
// clamped per channel), an optional 5 tap median
// (anti-flicker), then the quadratic soft knee threshold. The first downsample after it is a Karis average.
// Luma: the scene samples are clamped >= 0, as the scene is upgraded from R11G11B10_FLOAT.
// clang-format off
#include "Includes/Common.hlsl"
// clang-format on

cbuffer cbBloomLoop : register(b3)
{
   float4 g_vMainTexSize : packoffset(c0); // Texel size in .xy
}

cbuffer cbBloomMain : register(b4)
{
   float4 g_vBloomInfo0 : packoffset(c0); // Threshold curve: threshold - knee, 2 * knee, 0.25 / knee, threshold
   float4 g_vBloomInfo1 : packoffset(c1); // Exposure (< 0 = auto exposure), offset in texels, anti-flicker, threshold on the max channel (else weighted RGB)
}

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_tSceneMap : register(t0);
Texture2D<float4> g_tExposureScaleInfo : register(t1);

static const float3 ThresholdWeights = float3(0.222015, 0.706655, 0.071330);
static const float SourceCap = 5.0;

float3 Tap(float2 uv, float exposure)
{
   float3 color = clamp(g_tSceneMap.SampleLevel(sampleLinear_s, uv, 0).rgb, 0.0, FLT11_MAX) * exposure; // Luma: >= 0 too
   const float maxChannel = max3(color);
   if (g_vBloomInfo1.w > 0.0 && maxChannel > SourceCap)
      color *= SourceCap / maxChannel;
   return min(color, SourceCap);
}

float3 Median(float3 a, float3 b, float3 c)
{
   return a + b + c - min3(a, b, c) - max3(a, b, c);
}

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float exposure = g_vBloomInfo1.x < 0.0 ? g_tExposureScaleInfo.Load(int3(0, 0, 0)).x : (g_vBloomInfo1.x > 0.0 ? g_vBloomInfo1.x : 1.0);

   float3 color;
   if (g_vBloomInfo1.z > 0.0) // Anti-flicker: 5 tap median
   {
      const float2 texel = g_vMainTexSize.xy;
      const float2 uv = v1 + texel * g_vBloomInfo1.y;
      color = Median(Tap(uv - float2(texel.x, 0.0), exposure), Tap(uv, exposure), Tap(uv + float2(texel.x, 0.0), exposure));
      color = Median(Tap(uv - float2(0.0, texel.y), exposure), color, Tap(uv + float2(0.0, texel.y), exposure));
   }
   else
   {
      color = Tap(v1, exposure);
   }

   const float thresholdInput = g_vBloomInfo1.w > 0.0 ? max3(color) : dot(color, ThresholdWeights);
   float rq = clamp(thresholdInput - g_vBloomInfo0.x, 0.0, g_vBloomInfo0.y);
   rq = g_vBloomInfo0.z * rq * rq;
   o0.rgb = color * (max(rq, thresholdInput - g_vBloomInfo0.w) / max(thresholdInput, 1e-5));
   o0.a = 1.0;
}
