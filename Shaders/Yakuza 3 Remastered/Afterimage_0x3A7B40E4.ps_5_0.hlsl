// ps_afterimage01 (sh_ogre3_w64.par): afterimage ghost, alpha = 1.6 * source alpha. Vanilla relied on the UNORM target
// clamping that source alpha to 1 before blending; on the fp16 chain it would extrapolate. Verbatim except the
// saturated output alpha (the alpha test still uses the unclamped value, as vanilla did).

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}
cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   float4 c = t0.Sample(s0_s, v1.xy);
   float4 r = (0.0 >= c.w) ? 0.0 : float4(c.xyz * cb5[0].xyz, dot(float4(0.5, 0.5, 0.5, 0.1), c.wwww));
   if (cb11[0].z > 0u && (r.w - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;
   o0 = float4(r.xyz, saturate(r.w));
}
