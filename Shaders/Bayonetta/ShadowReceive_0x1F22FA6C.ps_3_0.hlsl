// Bayonetta "modelshadershadowrec00_bxxxx" (raw ps_3_0 CRC32 0x1F22FA6C = dgVoodoo 2.87.5 0x45E82A5F): the skinned meshes' twin of 0x1847C1F9,
// same code and registers except one thing, found by comparing the disassemblies: the centre shadow test is read half a target pixel on
// (like the cross taps), not at the exact UV. The dgVoodoo port included the xxxxx source for this hash and so dropped that offset.
#define SHADOW_CENTRE_HALF_TEXEL
#include "ShadowReceive_0x1847C1F9.ps_3_0.hlsl"
