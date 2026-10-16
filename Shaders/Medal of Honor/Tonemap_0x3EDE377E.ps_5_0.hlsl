// Medal of Honor (2010) — tonemap + 3D LUT, permutation with EdgeAA and motion blur/DoF both OFF.
// Grade tail in cb4[12]. See Luma_MOH_Tonemap.hlsl for the implementation and the permutation table.
#define MOH_EDGE_AA     0
#define MOH_MOTION_BLUR 0
#define MOH_GRADE_REG   12
#include "Luma_MOH_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// only TEXCOORD0, the scene UV in v5.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = RunMOHTonemap(v5.xy);
}
