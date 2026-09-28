// Katana engine bloom mip chain (Unity/Kino style) passes, shared by their replacements and the Luma passes that add the iterations
// the game skips below the output resolution (see "Luma_P5S_Bloom.hlsl"). The chain is one mipped texture: each pass reads one mip
// (a single level view) and writes the next one down (downsamples) or blends into the next one up (upsamples, additive).
// Luma: the tap offsets come from the bound mip's size instead of "g_vMainTexSize", as under DLSS/FSR upscaling the chain is output
// resolution (identical without upscaling). They sample level 0 instead of g_vMainTexSize.w: the views have one level, which the
// vanilla level clamps to.

#include "Common.hlsl"
#include "cbBloom.hlsl"

#ifndef P5S_BLOOM_MAX_UV
#define P5S_BLOOM_MAX_UV 0
#endif

SamplerState sampleMain_s : register(s0);
Texture2D<float4> g_sMainInputMap : register(t0);

float2 GetBloomTexel()
{
   float2 size;
   g_sMainInputMap.GetDimensions(size.x, size.y);
   return 1.0 / size;
}

float3 SampleBloomMip(float2 uv)
{
#if P5S_BLOOM_MAX_UV
   uv = min(uv, g_vSampleScale.zw);
#endif
   return g_sMainInputMap.SampleLevel(sampleMain_s, uv, 0).rgb;
}

// 4 bilinear taps at the corners, averaged, or Karis averaged against fireflies
float3 BloomDownsample(float2 uv, bool karis)
{
   const float2 texel = GetBloomTexel();
   const float3 a = SampleBloomMip(uv - texel);
   const float3 b = SampleBloomMip(uv + float2(texel.x, -texel.y));
   const float3 c = SampleBloomMip(uv + float2(-texel.x, texel.y));
   const float3 d = SampleBloomMip(uv + texel);
   if (!karis)
      return (a + b + c + d) * 0.25;
   const float wa = P5S_KarisWeight(a), wb = P5S_KarisWeight(b), wc = P5S_KarisWeight(c), wd = P5S_KarisWeight(d);
   return (a * wa + b * wb + c * wc + d * wd) / (wa + wb + wc + wd);
}

// A tent tap, capped at the R11G11B10 float max
float3 TentTap(float2 uv)
{
   return min(SampleBloomMip(uv), FLT11_MAX);
}

// 3x3 tent / 16 at "sampleScale" texels
float3 BloomUpsample(float2 uv, float sampleScale)
{
   const float2 offset = GetBloomTexel() * sampleScale;
   float3 color = TentTap(uv) * 4.0;
   color += (TentTap(uv - float2(0.0, offset.y)) + TentTap(uv - float2(offset.x, 0.0)) + TentTap(uv + float2(offset.x, 0.0)) + TentTap(uv + float2(0.0, offset.y))) * 2.0;
   color += TentTap(uv - offset) + TentTap(uv + float2(offset.x, -offset.y)) + TentTap(uv + float2(-offset.x, offset.y)) + TentTap(uv + offset);
   return color / 16.0;
}

// The game's tent scale, or Luma's for the output resolution chain (see "ExtendBloom")
float GetBloomSampleScale()
{
   return LumaData.CustomData3 > 0.0 ? LumaData.CustomData3 : g_vSampleScale.x;
}
