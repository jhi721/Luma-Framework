// FilterShaderGatherNoise drawn with no texture on s3 (dgVoodoo 2.87.6, without alpha test): dgVoodoo drops the noise sample for
// (0, 0, 0, 1). Never seen in game; hashed offline (dgVoodoo's translation with s3 unbound). GatherNoise_0xB3BB0D35 /
// GatherNoise_0xFF4AFD3B are its GREATEREQUAL / GREATER alpha test forms.
#define GATHER_NOISE 2
#include "Gather_0x726E6E78.ps_5_0.hlsl"
