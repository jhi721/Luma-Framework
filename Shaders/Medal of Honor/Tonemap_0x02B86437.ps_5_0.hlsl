// Medal of Honor (2010) — tonemap + 3D LUT, permutation with BOTH the engine's EdgeAA and motion blur/DoF on.
// cb4[12] serves double duty here (EdgeAA tap offsets and the MB offset scale/bias), depth linearisation is in
// cb4[10], jitter frequency in cb4[13], grade tail in cb4[14]. See Luma_MOH_Tonemap.hlsl.
#define MOH_EDGE_AA     1
#define MOH_MOTION_BLUR 1
#define MOH_GRADE_REG   14
#include "Luma_MOH_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER. This pass reads
// only TEXCOORD0, the scene UV in v5.
void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   o0 = RunMOHTonemap(v5.xy);
}
