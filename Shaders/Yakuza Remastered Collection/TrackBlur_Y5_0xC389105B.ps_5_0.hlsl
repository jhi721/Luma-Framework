// fx_track_blur (Y5R sh_devil_w64.par). See Includes/TrackBlur.hlsl.
#include "Includes/TrackBlur.hlsl"

void main(float4 v0 : COLOR0, float4 v1 : TEXCOORD0, float4 v2 : TEXCOORD1, float4 v3 : TEXCOORD2, float4 v4 : TEXCOORD3, float4 v5 : TEXCOORD4, float4 v6 : TEXCOORD5, float2 v7 : TEXCOORD6, float4 v8 : SV_Position, out float4 o0 : SV_Target0)
{
   const float4 taps[6] = {v1, v2, v3, v4, v5, v6};
   o0 = TrackBlur(v0.w, taps, v7);
}
