// ps_down_sample (Y3R sh_ogre3_w64.par; the same bytecode is Y4R's ps_cubic): 2-tap average of the swapchain-sized glow
// source into the 512x256 bloom level. The source is fp16 now; vanilla clamped every texel to 1 before this average.
// See SampleGlowSource.
#include "Includes/Common.hlsl"

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = (SampleGlowSource(t0, s0_s, v1.xy + cb5[0].xy) + SampleGlowSource(t0, s0_s, v1.xy)) * 0.5;
}
