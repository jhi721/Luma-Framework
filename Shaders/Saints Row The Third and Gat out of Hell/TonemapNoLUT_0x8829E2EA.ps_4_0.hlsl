// rl_hdr_09: final composite with bloom and the 1.49 shoulder but no colour LUT.
#include "Luma_SR3_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 uv : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = SR3_TonemapNoLUT(uv);
}
