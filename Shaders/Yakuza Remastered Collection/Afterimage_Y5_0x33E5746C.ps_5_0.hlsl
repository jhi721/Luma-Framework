// fx_afterimage01 (Y5R sh_devil_w64.par): the Y4R shader with its inputs reordered. See Afterimage_Y4_0x716D6221.

cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float2 v0 : TEXCOORD0, float4 v1 : TEXCOORD1, float4 v2 : SV_Position, out float4 o0 : SV_Target0)
{
   float4 r = t0.Sample(s0_s, v0.xy) * v1;
   if (cb11[0].z > 0u && (r.w - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;
   o0 = float4(r.xyz, saturate(r.w));
}
