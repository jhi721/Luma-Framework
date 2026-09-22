// ps_down_sample_2x4 (Y5R sh_devil_w64.par): 2-tap average of the swapchain-sized glow source into the 512x256 bloom
// level. The source is fp16 now; vanilla clamped every texel to 1 before this average. See SampleGlowSource.
#include "Includes/Common.hlsl"

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = SampleGlowSource(t0, s0_s, v0.zw) * 0.5 + SampleGlowSource(t0, s0_s, v0.xy) * 0.5;
}
