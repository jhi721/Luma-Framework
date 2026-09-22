// fx_track_blur (sh_ogre3_w64.par): 12-tap motion trail over the scene. Vanilla ends with add_sat(+0.2), which clips
// the HDR scene inside the trail. Verbatim except max(0) there.

cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
Texture2D<float4> t0 : register(t0); // scene
Texture2D<float4> t1 : register(t1); // trail mask (alpha)

void main(float4 v0 : COLOR0, float4 v1 : SV_Position, float4 v2 : TEXCOORD0, float4 v3 : TEXCOORD1, float4 v4 : TEXCOORD2, float4 v5 : TEXCOORD3, float4 v6 : TEXCOORD4, float4 v7 : TEXCOORD5, float2 v8 : TEXCOORD6, out float4 o0 : SV_Target0)
{
   float alpha = t1.Sample(s1_s, v8.xy).w * v0.w;
   o0.w = alpha;
   if (cb11[0].z > 0u && (alpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;

   float3 c = t0.Sample(s0_s, v2.zw).xyz * 0.133;
   c = t0.Sample(s0_s, v2.xy).xyz * 0.143 + c;
   c = t0.Sample(s0_s, v3.xy).xyz * 0.123 + c;
   c = t0.Sample(s0_s, v3.zw).xyz * 0.113 + c;
   c = t0.Sample(s0_s, v4.xy).xyz * 0.103 + c;
   c = t0.Sample(s0_s, v4.zw).xyz * 0.093 + c;
   c = t0.Sample(s0_s, v5.xy).xyz * 0.073 + c;
   c = t0.Sample(s0_s, v5.zw).xyz * 0.063 + c;
   c = t0.Sample(s0_s, v6.xy).xyz * 0.053 + c;
   c = t0.Sample(s0_s, v6.zw).xyz * 0.043 + c;
   c = t0.Sample(s0_s, v7.xy).xyz * 0.033 + c;
   c = t0.Sample(s0_s, v7.zw).xyz * 0.023 + c;
   o0.xyz = max(0.0, c + 0.2);
}
