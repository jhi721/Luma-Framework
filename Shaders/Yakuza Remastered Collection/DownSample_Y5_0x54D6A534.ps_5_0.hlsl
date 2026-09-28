// ps_down_sample_2x4 (Y5R sh_devil_w64.par): 2-tap average of the scene-sized glow source into the 512x256 bloom
// level. The source is fp16 now and is drawn by the materials with the extended tone curve: every texel goes back through
// the vanilla curve, as vanilla stored it, before this average (flagged draws only, as in SampleGlowSource).
#include "Includes/Common.hlsl"

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = SampleGlowSource(t0, s0_s, v0.zw, true) * 0.5 + SampleGlowSource(t0, s0_s, v0.xy, true) * 0.5;
}
