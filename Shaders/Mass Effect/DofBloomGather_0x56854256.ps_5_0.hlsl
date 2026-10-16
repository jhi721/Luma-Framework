// Mass Effect (2007) - UE3 DOFAndBloomGather, the QualityBloom=TRUE permutation: sixteen taps, output * 1/64. Body and
// rationale in Includes/DofBloomGather.hlsl; the four-tap twin is DofBloomGather4_0x28F8DB16.
#include "Includes/DofBloomGather.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). Tap pairs are (vN.xy,
// vN.wz) for N = 5..12 - note the swapped second pair.
void main(DGV_MAIN_SIGNATURE)
{
   float4 sum = GatherTap(v5.xy) + GatherTap(v5.wz);
   sum += GatherTap(v6.xy) + GatherTap(v6.wz);
   sum += GatherTap(v7.xy) + GatherTap(v7.wz);
   sum += GatherTap(v8.xy) + GatherTap(v8.wz);
   sum += GatherTap(v9.xy) + GatherTap(v9.wz);
   sum += GatherTap(v10.xy) + GatherTap(v10.wz);
   sum += GatherTap(v11.xy) + GatherTap(v11.wz);
   sum += GatherTap(v12.xy) + GatherTap(v12.wz);
   o0 = sum * (1.0 / 64.0);
}
