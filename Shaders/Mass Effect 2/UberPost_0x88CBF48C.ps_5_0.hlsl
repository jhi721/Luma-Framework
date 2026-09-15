// Mass Effect 2 (2010) - UE3 UberPostProcessBlend, TONEMAP-LESS permutation: no curve at all, the grade's own
// saturate is the only compression. DIAG-confirmed live in the early/menu scene. See Luma_ME2_Tonemap.hlsl.
#include "Luma_ME2_Tonemap.hlsl"

// dgVoodoo's fixed interpolator layout: EVERY entry must be declared, in order, even the unread ones - VS->PS
// linkage is by REGISTER, so dropping one shifts every later TEXCOORD.
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
   // Alpha stays at the vanilla 0: SMAA predication reads the game's own fp16 scene, captured at this draw's t0, so
   // nothing needs depth here - and an Unreal-unit depth in an fp16 canvas alpha is a side channel no blend expects.
   o0 = float4(RunME2Uber(v5.xy, v6.xy), 0.0);
}
