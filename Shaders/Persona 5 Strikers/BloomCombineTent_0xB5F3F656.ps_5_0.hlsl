// Persona 5 Strikers bloom combine, 3x3 tent permutation (hash 0xB5F3F656). Disassembly diff vs 0x619045C8:
// 9 taps weighted 4 (center) / 2 (edges) / 1 (corners) / 16 instead of the 5 tap / 6 filter; exposure and intensity are unchanged.

#define P5S_BLOOM_TENT 1
#include "BloomCombine_0x619045C8.ps_5_0.hlsl"
