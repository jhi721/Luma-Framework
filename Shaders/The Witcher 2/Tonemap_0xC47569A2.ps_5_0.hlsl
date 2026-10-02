// The Witcher 2 tonemap, STATIC EXPOSURE permutation (no GPU histogram; dgVoodoo -> ps_5_0, hash 0xC47569A2; DX9 cache md5
// 3366c0e4). 0x91348C0F with the levels from PSC_LumRanges; hash from the offline map, never captured.
// Thin wrapper over the shared impl. See Luma_TW2_Tonemap.hlsl.
#define TM_STATIC 1
#include "Luma_TW2_Tonemap.hlsl"
