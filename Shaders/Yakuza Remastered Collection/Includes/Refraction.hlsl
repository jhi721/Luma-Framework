// fx_refraction == fx_blood_floor_refraction (byte-identical), the same math in every game (Y5R moves SV_Position last):
// glass, water and blood pools sample the scene copy (t2) through a normal-mapped screen offset. Vanilla ends with
// mul_sat, which clips the refracted HDR scene at 1. Verbatim except max(0) there (the alpha was already saturated).

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

float4 Refraction(float4 color, float2 uv, float4 screenPosition, float3 tangentX, float3 tangentY, float3 tangentZ)
{
   float4 diffuse = t0.Sample(s0_s, uv);
   float alpha = saturate(diffuse.w * color.w);
   if (cb11[0].z > 0u && (alpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;

   float3 n;
   n.xy = t1.Sample(s1_s, uv).xy * 2.0 - 1.0;
   n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
   float3 tangentSpace = float3(dot(tangentX, n), dot(tangentY, n), dot(tangentZ, n));
   float2 offset = tangentSpace.xy * rsqrt(dot(tangentSpace, tangentSpace));
   float2 refractedUV = (-offset * cb4[0].x + screenPosition.xy) / screenPosition.w;
   float3 refracted = t2.Sample(s2_s, refractedUV).xyz;
   return float4(max(0.0, diffuse.xyz * refracted * color.xyz), alpha);
}
