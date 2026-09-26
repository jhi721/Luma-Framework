// Bloom combine, 3x3 tent (0xB5F3F656). Disassembly diff vs 0x619045C8: only 9 taps weighted 4 (center) / 2 (edges) /
// 1 (corners) / 16 instead of the 5 tap / 6 filter.

#define P5S_BLOOM_TENT 1
#include "BloomCombine_0x619045C8.ps_5_0.hlsl"
