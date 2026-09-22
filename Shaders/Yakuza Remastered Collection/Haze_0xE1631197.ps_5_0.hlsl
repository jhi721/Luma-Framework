// ps_haze (sh_ogre3_w64.par): heat haze tint whose blend alpha is the mean of the sampled color. On the vanilla UNORM
// target that alpha could never exceed 1; with the fp16 scene it can, and alpha blending would extrapolate. Verbatim
// except the saturated alpha (identical for inputs in [0,1]).

cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   float3 c = t0.Sample(s0_s, v1.xy).xyz;
   float alpha = (c.y + c.x + c.z) * 0.333333;
   o0.w = saturate(alpha);
   if (cb11[0].z > 0u && (alpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;
   o0.xyz = v0.xyz;
}
