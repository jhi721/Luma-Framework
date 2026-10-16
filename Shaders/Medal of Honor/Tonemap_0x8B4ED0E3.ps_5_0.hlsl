// Medal of Honor (2010) — tonemap + 3D LUT, permutation with the engine's EdgeAA ON and motion blur/DoF off.
// EdgeAA tap offsets in cb4[12], grade tail in cb4[13]. See Luma_MOH_Tonemap.hlsl.
#define MOH_EDGE_AA     1
#define MOH_MOTION_BLUR 0
#define MOH_GRADE_REG   13
#include "Luma_MOH_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// only TEXCOORD0, the scene UV in v5.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = RunMOHTonemap(v5.xy);
}
