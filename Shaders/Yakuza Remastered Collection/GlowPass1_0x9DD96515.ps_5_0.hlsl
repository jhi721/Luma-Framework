// ps_glow_pass1 (sh_ogre3_w64.par): separable 6-tap bloom blur. Vanilla wrote 8-bit UNORM targets. The 512x256 level is
// now fp16 (for the DoF), so the output is saturated to keep the vanilla bloom bounded. Otherwise verbatim.

cbuffer cb5 : register(b5)
{
   float4 cb5[4];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : TEXCOORD0, float4 v1 : TEXCOORD1, float4 v2 : TEXCOORD2, out float4 o0 : SV_Target0)
{
   float4 r = t0.Sample(s0_s, v0.zw) * cb5[2];
   r += t0.Sample(s0_s, v0.xy) * cb5[3];
   r += t0.Sample(s0_s, v1.xy) * cb5[1];
   r += t0.Sample(s0_s, v1.zw) * cb5[1];
   r += t0.Sample(s0_s, v2.xy) * cb5[2];
   r += t0.Sample(s0_s, v2.zw) * cb5[3];
   o0 = saturate(r);
}
