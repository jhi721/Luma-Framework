// The Witcher 2 tonemap, STATIC BLOOM BRIGHT-PASS permutation (no GPU histogram; dgVoodoo -> ps_5_0, hash 0x6587B8D6; DX9
// cache md5 c46227f6). 0x00E31BF9 with the levels from PSC_LumRanges; hash from the offline map, never
// captured.
// Thin wrapper over the shared impl. See Luma_TW2_Tonemap.hlsl.
#define TM_BRIGHT_PASS 1
#define TM_STATIC      1
#include "Luma_TW2_Tonemap.hlsl"
