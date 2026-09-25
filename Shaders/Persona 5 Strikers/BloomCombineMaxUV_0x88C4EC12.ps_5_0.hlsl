// Persona 5 Strikers bloom combine, 5 tap + g_vMaxUV clamp permutation (hash 0x88C4EC12). Disassembly diff vs
// 0x619045C8: every tap UV, the center included, goes through min(uv, g_vMaxUV.xy); the filter is unchanged.

#define P5S_BLOOM_MAX_UV 1
#include "BloomCombine_0x619045C8.ps_5_0.hlsl"
