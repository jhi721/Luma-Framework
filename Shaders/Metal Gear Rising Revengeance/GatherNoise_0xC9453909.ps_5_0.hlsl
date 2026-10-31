// FilterShaderGatherNoise as Steam006's PC Fix patches it ("DisableNoiseFilter"; dgVoodoo 2.87.6, without alpha test): the noise
// texld's destination moves from r2 to r3, so the noise term reads r2's bloom sample instead (the frame times 1 + w * (bloom * color
// - 1)). Its intent is no noise: the noise-free FilterShaderGather body. Seen in game, and reproduced offline (dgVoodoo's translation of
// the D3D9 blob with that one register patched); GatherNoise_0x0CDD2BDB / GatherNoise_0x252B2AE7 are its GREATEREQUAL / GREATER alpha test forms.
#include "Gather_0x726E6E78.ps_5_0.hlsl"
