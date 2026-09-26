// Katana engine bloom combine (Unity/Kino style): 5 taps of the bloom mip (1920x1080 in the 3840x2160 captures, center
// counted twice) / 6, times the scene exposure and g_vBloomInfo.w (intensity), blended additively (one + one) into the full resolution scene.
// The engine compiles a 2x2 family: this 5 tap filter or a 3x3 tent / 16 (P5S_BLOOM_TENT), each with or without every
// tap clamped to g_vMaxUV (P5S_BLOOM_MAX_UV); the other three corners include this file.
// Luma: scaled by "Bloom Intensity".
// clang-format off
#include "Includes/Common.hlsl"
#include "Includes/cbComposite.hlsl"
// clang-format on

#ifndef P5S_BLOOM_TENT
#define P5S_BLOOM_TENT 0
#endif
#ifndef P5S_BLOOM_MAX_UV
#define P5S_BLOOM_MAX_UV 0
#endif

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_tBloomMap : register(t1);
Texture2D<float4> g_tExposureScaleInfo : register(t2);

float3 SampleBloom(float2 uv)
{
#if P5S_BLOOM_MAX_UV
   uv = min(uv, g_vMaxUV.xy);
#endif
   return min(g_tBloomMap.SampleLevel(sampleLinear_s, uv, 0).rgb, FLT11_MAX);
}

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float2 offset = g_vBloomInfo.xy * g_vBloomInfo.z;
#if P5S_BLOOM_TENT
   float3 bloom = SampleBloom(v1) * 4.0;
   bloom += (SampleBloom(v1 + float2(offset.x, 0.0)) + SampleBloom(v1 - float2(offset.x, 0.0)) + SampleBloom(v1 + float2(0.0, offset.y)) + SampleBloom(v1 - float2(0.0, offset.y))) * 2.0;
   bloom += SampleBloom(v1 + offset) + SampleBloom(v1 - offset) + SampleBloom(v1 + float2(offset.x, -offset.y)) + SampleBloom(v1 + float2(-offset.x, offset.y));
   const float weightSum = 16.0;
#else
   float3 bloom = SampleBloom(v1 + offset) + SampleBloom(v1 + float2(-offset.x, offset.y));
   bloom += SampleBloom(v1) * 2.0;
   bloom += SampleBloom(v1 + float2(offset.x, 0.0)) + SampleBloom(v1 - float2(offset.x, 0.0));
   const float weightSum = 6.0;
#endif

   const float exposure = g_vCompositeInfo.z < 0.0 ? g_tExposureScaleInfo.Load(int3(1, 0, 0)).x : 1.0;
   o0.rgb = bloom * (exposure * g_vBloomInfo.w * LumaSettings.GameSettings.BloomIntensity / weightSum);
   o0.a = 1.0;
}
