// fx_track_blur (sh_ogre3_w64.par). See Includes/TrackBlur.hlsl.
#include "Includes/TrackBlur.hlsl"

void main(float4 v0 : COLOR0, float4 v1 : SV_Position, float4 v2 : TEXCOORD0, float4 v3 : TEXCOORD1, float4 v4 : TEXCOORD2, float4 v5 : TEXCOORD3, float4 v6 : TEXCOORD4, float4 v7 : TEXCOORD5, float2 v8 : TEXCOORD6, out float4 o0 : SV_Target0)
{
   const float4 taps[6] = {v2, v3, v4, v5, v6, v7};
   o0 = TrackBlur(v0.w, taps, v8);
}
