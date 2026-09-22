// fx_refraction / fx_blood_floor_refraction (Y3R sh_ogre3_w64.par, same hash in Y4R). See Includes/Refraction.hlsl.
#include "Includes/Refraction.hlsl"

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, float4 v2 : SV_Position, float4 v3 : TEXCOORD1, float4 v4 : TEXCOORD2, float4 v5 : TEXCOORD3, float4 v6 : TEXCOORD4, float4 v7 : TEXCOORD5, out float4 o0 : SV_Target0)
{
   o0 = Refraction(v0, v1, v3, v4.xyz, v5.xyz, v6.xyz);
}
