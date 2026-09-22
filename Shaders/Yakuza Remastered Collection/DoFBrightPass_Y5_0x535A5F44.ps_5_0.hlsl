// ps_focus_blur_pass1_2_blur1 (Y5R sh_devil_w64.par): bokeh bright-pass of the menu DoF, a 16-tap mean thresholded by
// add_sat(-cb5[0].z) and scaled by cb5[0].w, kept only where every channel is non-zero. Its 256x256 source and target were
// 8-bit UNORM in vanilla and are fp16 now, so the taps are clamped per texel and the output is saturated. Otherwise verbatim.
#include "Includes/Common.hlsl"

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : TEXCOORD0, float4 v1 : TEXCOORD1, float4 v2 : TEXCOORD2, float4 v3 : TEXCOORD3, float4 v4 : TEXCOORD4, float4 v5 : TEXCOORD5, float4 v6 : TEXCOORD6, float4 v7 : TEXCOORD7, out float4 o0 : SV_Target0)
{
   const float4 taps[8] = {v0, v1, v2, v3, v4, v5, v6, v7};
   float3 mean = 0.0;
   for (int i = 0; i < 8; i++)
      mean += (SampleSaturatedBilinear(t0, s0_s, taps[i].zw).rgb + SampleSaturatedBilinear(t0, s0_s, taps[i].xy).rgb) * 0.0625;
   const float3 c = saturate(mean - cb5[0].z) * cb5[0].w;
   o0 = saturate(float4(c, 1.0) * ((c.r * c.g * c.b != 0.0) ? 1.0 : 0.0));
}
