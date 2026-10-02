// The Witcher 2 final grade, NO-FXAA permutation (dgVoodoo -> ps_5_0, hash 0xCF3B72A9): what the engine runs
// with the game's Anti-aliasing setting turned off. Identical to 0xDE5CF9CD from
// the colour balance onward (vShadow offset cb4[62], vMidtone log2/pow/exp2 cb4[61], vHighlight gain cb4[60],
// split toning cb4[68..71], vignette t2 + cb4[66..67]); the FXAA neighbourhood is replaced by a single
// scene tap at v5.xy, and the output alpha carries the scene alpha instead of 0.

#define LUMA_TW2_NO_FXAA_PERM 1
#include "FinalGrade_0xDE5CF9CD.ps_5_0.hlsl"
