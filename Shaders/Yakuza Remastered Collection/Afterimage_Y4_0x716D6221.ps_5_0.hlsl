// fx_afterimage01 (Y4R sh_soul_w64.par, in place of Y3R's ps_afterimage01): afterimage ghost, texture * vertex color.
// Vanilla relied on the UNORM target clamping the output alpha to 1 before blending; on the fp16 chain it would
// extrapolate. Verbatim except the saturated output alpha (the alpha test still uses the unclamped value, as vanilla did).
#include "Includes/Common.hlsl"

cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, float4 v2 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   float4 r = t0.Sample(s0_s, v1.xy) * v2;
   YRC_AlphaTest(r.w, cb11[0].z);
   o0 = float4(r.xyz, saturate(r.w));
}
