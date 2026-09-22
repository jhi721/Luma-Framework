// rl_hdr_10 [final_no_lut]: rl_hdr_09 without the colour LUT, ending in pow(1/2.2).
#include "Luma_SR4_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 texcoord0 : TEXCOORD0, float4 texcoord1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   o0 = SR4_TonemapFinal(texcoord0, texcoord1, false, false);
}
