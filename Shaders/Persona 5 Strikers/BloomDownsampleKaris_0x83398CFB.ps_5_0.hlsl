// Bloom first downsample (0x83398CFB): mip 0 to 1, a Karis average with anti-flicker, else a box. See 0x7C59D221.

#define P5S_BLOOM_KARIS 1
#include "BloomDownsample_0x7C59D221.ps_5_0.hlsl"
