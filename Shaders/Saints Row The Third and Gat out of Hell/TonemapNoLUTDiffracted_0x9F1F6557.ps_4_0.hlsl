// rl_hdr_06 [final_no_lut_diffracted]: rl_hdr_10 with the Signal_noise channel split.
#include "Luma_GOOH_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 texcoord0 : TEXCOORD0, float4 texcoord1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   o0 = GOOH_TonemapFinal(texcoord0, texcoord1, true, false);
}
