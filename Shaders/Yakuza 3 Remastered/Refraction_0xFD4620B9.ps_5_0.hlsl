// fx_refraction (sh_ogre3_w64.par): glass/water refraction that samples the scene copy (t2) through a distorted screen
// position. Vanilla ends with mul_sat, which clips the refracted HDR scene at 1. Verbatim except max(0) there.

cbuffer cb4 : register(b4)
{
   float4 cb4[1];
}
cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);
Texture2D<float4> t0 : register(t0); // diffuse
Texture2D<float4> t1 : register(t1); // normal map
Texture2D<float4> t2 : register(t2); // scene copy

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, float4 v2 : SV_Position, float4 v3 : TEXCOORD1, float4 v4 : TEXCOORD2, float4 v5 : TEXCOORD3, float4 v6 : TEXCOORD4, float4 v7 : TEXCOORD5, out float4 o0 : SV_Target0)
{
   float4 diffuse = t0.Sample(s0_s, v1.xy);
   float alpha = saturate(diffuse.w * v0.w);
   o0.w = alpha;
   if (cb11[0].z > 0u && (alpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;

   float3 n;
   n.xy = t1.Sample(s1_s, v1.xy).xy * 2.0 - 1.0;
   n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
   float3 tangentSpace = float3(dot(v4.xyz, n), dot(v5.xyz, n), dot(v6.xyz, n));
   float2 offset = tangentSpace.xy * rsqrt(dot(tangentSpace, tangentSpace));
   float2 uv = (-offset * cb4[0].x + v3.xy) / v3.w;
   float3 refracted = t2.Sample(s2_s, uv).xyz;
   o0.xyz = max(0.0, diffuse.xyz * refracted * v0.xyz);
}
