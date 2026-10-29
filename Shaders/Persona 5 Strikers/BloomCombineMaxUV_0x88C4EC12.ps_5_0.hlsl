// Bloom combine, 5 tap + g_vMaxUV clamp (0x88C4EC12). Disassembly diff vs 0x619045C8: only every tap UV, center included,
// goes through min(uv, g_vMaxUV.xy).

#define P5S_BLOOM_MAX_UV 1
#include "BloomCombine_0x619045C8.ps_5_0.hlsl"
