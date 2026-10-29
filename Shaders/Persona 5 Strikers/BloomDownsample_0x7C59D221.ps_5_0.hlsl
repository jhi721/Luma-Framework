// Katana engine bloom downsample (0x7C59D221): 4 tap box into the next mip down. See "Includes/BloomSample.hlsl". Its first
// downsample (0x83398CFB) is a Karis average with anti-flicker (P5S_BLOOM_KARIS); 0xC9FB7E01 and 0xC27D2B8E are their g_vMaxUV
// clamped twins. They include this file.
#include "Includes/BloomSample.hlsl"

#ifndef P5S_BLOOM_KARIS
#define P5S_BLOOM_KARIS 0
#endif

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = float4(BloomDownsample(v1, P5S_BLOOM_KARIS && g_vBloomInfo1.z > 0.0), 1.0);
}
