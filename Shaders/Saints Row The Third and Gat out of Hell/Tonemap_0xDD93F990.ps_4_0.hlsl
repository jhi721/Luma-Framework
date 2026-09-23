// rl_hdr_13: final composite with bloom, the 1.49 shoulder and the colour LUT (PostProcess 1/2).
#include "Luma_SR3_Tonemap.hlsl"

void main(float4 pos : SV_Position, float2 uv : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = SR3_TonemapLUT(uv);
}
