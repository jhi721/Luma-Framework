// rl_hdr_05 [no_tonemapping]: the final composite with post processing off (PostProcess 0).
#include "Luma_SR4_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 texcoord0 : TEXCOORD0, float4 texcoord1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   o0 = SR4_TonemapNoPost(texcoord0, texcoord1);
}
