// rl_hdr_08: final composite with post processing off (PostProcess 0).
#include "Luma_SR3_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 uv : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = SR3_TonemapNoPost(uv);
}
