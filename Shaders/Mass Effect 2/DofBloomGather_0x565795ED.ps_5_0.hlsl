// Mass Effect 2 (2010) - UE3 DOFAndBloomGather, the QualityBloom=TRUE permutation: sixteen taps, tap average 1/16.
// Body and rationale in Includes/DofBloomGather.hlsl; the four-tap twin is DofBloomGather4_0x3BEE36AD.
#include "Includes/DofBloomGather.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). Tap pairs are (vN.xy, vN.wz) for N = 5..12 - note the swapped second pair.
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
   float4 sceneSum = (float4)0.0;
   float3 bloomSum = (float3)0.0;
   GatherTap(v5.xy, sceneSum, bloomSum);
   GatherTap(v5.wz, sceneSum, bloomSum);
   GatherTap(v6.xy, sceneSum, bloomSum);
   GatherTap(v6.wz, sceneSum, bloomSum);
   GatherTap(v7.xy, sceneSum, bloomSum);
   GatherTap(v7.wz, sceneSum, bloomSum);
   GatherTap(v8.xy, sceneSum, bloomSum);
   GatherTap(v8.wz, sceneSum, bloomSum);
   GatherTap(v9.xy, sceneSum, bloomSum);
   GatherTap(v9.wz, sceneSum, bloomSum);
   GatherTap(v10.xy, sceneSum, bloomSum);
   GatherTap(v10.wz, sceneSum, bloomSum);
   GatherTap(v11.xy, sceneSum, bloomSum);
   GatherTap(v11.wz, sceneSum, bloomSum);
   GatherTap(v12.xy, sceneSum, bloomSum);
   GatherTap(v12.wz, sceneSum, bloomSum);
   o0 = GatherTail(sceneSum, bloomSum, 1.0 / 16.0);
}
