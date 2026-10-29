// Bloom combine, 3x3 tent + g_vMaxUV clamp (0x4CE014A2): 0xB5F3F656 with every tap UV clamped as in 0x88C4EC12.

#define P5S_BLOOM_TENT   1
#define P5S_BLOOM_MAX_UV 1
#include "BloomCombine_0x619045C8.ps_5_0.hlsl"
