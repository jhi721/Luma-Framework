// Mass Effect 2 (2010) - UE3 DOFAndBloomGather, the QualityBloom=TRUE permutation: sixteen taps, tap average 1/16.
// Body and rationale in Includes/DofBloomGather.hlsl; the four-tap twin is DofBloomGather4_0x3BEE36AD.
#include "Includes/DofBloomGather.hlsl"

// Tap pairs are (vN.xy, vN.wz) for N = 5..12 - note the swapped second pair.
void main(ME2_MAIN_SIGNATURE)
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
