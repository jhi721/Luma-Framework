// The Witcher 2 final grade, NO-FXAA + NO-VIGNETTE permutation (dgVoodoo -> ps_5_0, hash 0xBABBFFAD): the
// engine drops the vignette stage entirely in this variant. Identical by disassembly diff
// vs 0xCF3B72A9: byte-for-byte the same shader minus the t2/s2 mask sample, cb3[48..49] fixup and the
// cb4[66..67] weight/color lerp — everything from the colour balance through the split toning is identical, and
// the output alpha likewise carries the scene alpha.

#define LUMA_TW2_NO_FXAA_PERM     1
#define LUMA_TW2_NO_VIGNETTE_PERM 1
#include "FinalGrade_0xDE5CF9CD.ps_5_0.hlsl"
