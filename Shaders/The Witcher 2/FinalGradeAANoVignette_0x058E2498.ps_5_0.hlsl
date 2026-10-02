// The Witcher 2 final grade, FXAA + NO-VIGNETTE permutation (dgVoodoo -> ps_5_0, hash 0x058E2498): the fourth
// corner of the 2x2 matrix the engine compiles (FXAA in/out x vignette in/out). Disassembly diff vs
// 0xDE5CF9CD: byte-for-byte the same shader minus the t2/s2 mask sample, the cb3[48..49] fixup and the
// cb4[66..67] weight/color lerp; the FXAA block, the grade tail and the o0.w = 0 write are unchanged.

#define LUMA_TW2_NO_VIGNETTE_PERM 1
#include "FinalGrade_0xDE5CF9CD.ps_5_0.hlsl"
