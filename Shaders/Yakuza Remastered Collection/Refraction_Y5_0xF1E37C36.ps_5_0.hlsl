// fx_refraction / fx_blood_floor_refraction (Y5R sh_devil_w64.par). See Includes/Refraction.hlsl.
#include "Includes/Refraction.hlsl"

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, float4 v2 : TEXCOORD1, float4 v3 : TEXCOORD2, float4 v4 : TEXCOORD3, float4 v5 : TEXCOORD4, float4 v6 : TEXCOORD5, float4 v7 : SV_Position, out float4 o0 : SV_Target0)
{
   o0 = Refraction(v0, v1, v2, v3.xyz, v4.xyz, v5.xyz);
}
