// rl_hdr_09 [final]: the final composite with bloom, the 1.49 shoulder and the colour LUT (PostProcess 1/2).
#include "Luma_GOOH_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 texcoord0 : TEXCOORD0, float4 texcoord1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   o0 = GOOH_TonemapFinal(texcoord0, texcoord1, false, true);
}
