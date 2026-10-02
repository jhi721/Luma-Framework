// The Witcher EE lit material (normal map, specular lookup, cube reflection; dgVoodoo 2.87.3 ps_5_0 0xEECADC4F), seen drawing the
// HUD's 3D wolf medallion after the post chain, straight into the canvas. Transcribed from the translated disassembly.
// The 8-bit canvas clamped its output to [0, 1]; on the upgraded fp16 canvas its lighting reached -0.96 and 1.86 around the medallion
// (Luma MCP readback, 2026-09-30): the output keeps the canvas' floor at 0 and lets the specular highlights exceed 1.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo's own constants: texture format masks (and/or pairs), alpha test reference at [8].z
}
cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 pixel shader constants, c<N> at cb4[N]
}

Texture2D<float4> t2 : register(t2);     // diffuse
Texture2D<float4> t3 : register(t3);     // normal map (alpha: reflection mask)
Texture2D<float4> t4 : register(t4);     // specular lookup
Texture2D<float4> t5 : register(t5);     // ambient/light map
TextureCube<float4> t22 : register(t22); // reflection

SamplerState s2_s : register(s2);
SamplerState s3_s : register(s3);
SamplerState s4_s : register(s4);
SamplerState s5_s : register(s5);
SamplerState s6_s : register(s6);

// dgVoodoo's texture format emulation after every sample
float4 ApplyFormatMask(float4 value, float4 and_mask, float4 or_mask)
{
   return asfloat((asuint(value) & asuint(and_mask)) | asuint(or_mask));
}

// rsq with dgVoodoo's guard: a zero vector stays zero instead of becoming inf
float3 NormalizeOrZero(float3 value)
{
   float inverse_length = rsqrt(dot(value, value));
   inverse_length = (asuint(inverse_length) != 0x7F800000u) ? inverse_length : 0.0;
   return value * inverse_length;
}

void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    centroid float4 v2 : COLOR0,
    centroid float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   const float4 diffuse = ApplyFormatMask(t2.Sample(s2_s, v5.xy), cb3[48], cb3[49]);
   const float3 eye = NormalizeOrZero(v7.xyz);
   float4 normal = ApplyFormatMask(t3.Sample(s3_s, v5.xy), cb3[50], cb3[51]);
   normal.xyz = normal.xyz * 2.0 - 1.0;
   const float3 light = NormalizeOrZero(v6.xyz);
   const float3 eye_reflection = normal.xyz * -(2.0 * dot(-eye, normal.xyz)) - eye;
   const float specular_dot = dot(eye_reflection, light);
   const float diffuse_dot = dot(normal.xyz, light);

   const float3 world_normal = normal.x * v9.xyz + normal.y * v10.xyz + normal.z * v11.xyz;
   const float3 view = NormalizeOrZero(saturate(v3.xyz) * 2.0 - 1.0);
   const float3 cube_direction = world_normal * -(2.0 * dot(-view, world_normal)) - view;

   const float4 specular_lookup = ApplyFormatMask(t4.Sample(s4_s, saturate(specular_dot).xx), cb3[52], cb3[53]);
   const float4 ambient = ApplyFormatMask(t5.Sample(s5_s, v8.xy), cb3[54], cb3[55]);
   float4 reflection = ApplyFormatMask(t22.Sample(s6_s, cube_direction), cb3[56], cb3[57]);

   const float3 specular = (normal.w * specular_lookup.x * v6.w) * cb4[8].xyz;
   const float3 lighting = saturate(v2.xyz) * ambient.xyz + (diffuse_dot * v6.w) * cb4[8].xyz;
   float3 color = lighting * diffuse.xyz + specular;
   reflection.xyz *= cb4[18].x * normal.w;
   color += (-cb4[16].x >= 0.0) ? 0.0 : reflection.xyz;

   const float alpha = cb4[17].w;
   if (asuint(cb3[8].z) >= uint(alpha * 255.0 + 0.0001))
   {
      discard;
   }
   // Negative lighting (N·L < 0) was floored by the unorm canvas; the highlights above 1 are kept for HDR
   o0 = float4(max(color, 0.0), saturate(alpha));
}
