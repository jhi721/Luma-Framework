// Mass Effect (2007) - UE3 DOFAndBloomGather, the QualityBloom=FALSE permutation: four taps, output * 1/16. Body and
// rationale in Includes/DofBloomGather.hlsl; the sixteen-tap twin is DofBloomGather_0x56854256.
#include "Includes/DofBloomGather.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). Tap pairs are (vN.xy,
// vN.wz) for N = 5..6 - note the swapped second pair.
void main(DGV_MAIN_SIGNATURE)
{
   float4 sum = GatherTap(v5.xy) + GatherTap(v5.wz);
   sum += GatherTap(v6.xy) + GatherTap(v6.wz);
   o0 = sum * (1.0 / 16.0);
}
