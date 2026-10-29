// Bloom first downsample with every tap clamped to g_vMaxUV (0xC27D2B8E): see 0x7C59D221.

#define P5S_BLOOM_KARIS  1
#define P5S_BLOOM_MAX_UV 1
#include "BloomDownsample_0x7C59D221.ps_5_0.hlsl"
