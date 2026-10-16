// Medal of Honor (2010) — tonemap + 3D LUT, permutation with BOTH the engine's EdgeAA and motion blur/DoF on.
// cb4[12] serves double duty here (EdgeAA tap offsets and the MB offset scale/bias), depth linearisation is in
// cb4[10], jitter frequency in cb4[13], grade tail in cb4[14]. See Luma_MOH_Tonemap.hlsl.
#define MOH_EDGE_AA     1
#define MOH_MOTION_BLUR 1
#define MOH_GRADE_REG   14
#include "Luma_MOH_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// only TEXCOORD0, the scene UV in v5.
void main(DGV_MAIN_SIGNATURE)
{
   o0 = RunMOHTonemap(v5.xy);
}
