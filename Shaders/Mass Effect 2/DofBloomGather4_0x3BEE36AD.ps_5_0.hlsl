// Mass Effect 2 (2010) - UE3 DOFAndBloomGather, the QualityBloom=FALSE permutation: four taps, tap average 1/4.
// Body and rationale in Includes/DofBloomGather.hlsl; the sixteen-tap twin is DofBloomGather_0x565795ED.
#include "Includes/DofBloomGather.hlsl"

// Tap pairs are (vN.xy, vN.wz) for N = 5..6 - note the swapped second pair.
void main(ME2_MAIN_SIGNATURE)
{
   float4 sceneSum = (float4)0.0;
   float3 bloomSum = (float3)0.0;
   GatherTap(v5.xy, sceneSum, bloomSum);
   GatherTap(v5.wz, sceneSum, bloomSum);
   GatherTap(v6.xy, sceneSum, bloomSum);
   GatherTap(v6.wz, sceneSum, bloomSum);
   o0 = GatherTail(sceneSum, bloomSum, 1.0 / 4.0);
}
