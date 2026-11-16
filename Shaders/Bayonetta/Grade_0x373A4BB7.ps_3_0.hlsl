// Bayonetta grade variant "filter04" (raw D3D9 ps_3_0 blob, CRC32 0x373A4BB7 = dgVoodoo 2.87.5 0xB7A08D4A), native D3D9 spike port.
// Vanilla math from the disassembly (gamma encoded): ColorTableTexture read at Y' of the scene, colour scaled by curve(Y')/Y',
// saturation g_Saido around the curve value, * g_MatrialColor; alpha = g_MatrialColor.w. 1/Y' is an unguarded rcp as in the blob.
// Luma injection: g_Luma.x (c208, 1 = vanilla) multiplies the g_MatrialColor gain.

sampler2D Texture : register(s0);
sampler2D ColorTableTexture : register(s1);
float4 g_MatrialColor : register(c184); // .w alpha
float4 g_Saido : register(c185);
float4 g_Luma : register(c208); // Luma injection base (PS c208..c223)

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float3 scene = tex2D(Texture, uv).rgb;
   const float luma = dot(scene, float3(0.298911989, 0.586610973, 0.114478));
   const float curved = tex2D(ColorTableTexture, float2(luma, 0)).x;
   const float3 toned = scene * curved * rcp(luma);
   float4 result;
   result.rgb = ((toned - curved) * g_Saido.xyz + toned) * (g_MatrialColor.xyz * g_Luma.x);
   result.a = g_MatrialColor.w;
   return result;
}
