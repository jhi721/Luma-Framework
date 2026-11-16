// Bayonetta "modelshadershadowrec00_xxxxx" (raw ps_3_0 CRC32 0x1847C1F9 =
// dgVoodoo 2.87.5 0x98BD17D3), native D3D9 port: the shadow receive pass drawn
// over the scene, shadow in black with its alpha, 1 - z/w to target 1. See
// Includes/ShadowReceive_sm3.hlsli (SHADOW_VANILLA). g_Sampler0 s13 (material
// colour, alpha tested), s14 (shadow map, R32F, point), s15 (RGBA8 soft map);
// fognear c41, fogfar c42, margin c43, g_MatrialColor c44, g_TargetSize c194.
#include "Includes/ShadowReceive_sm3.hlsli"

sampler2D g_Sampler0 : register(s13);
sampler2D g_Sampler1 : register(s14);
sampler2D g_Sampler2 : register(s15);
float4 fognear : register(c41);
float4 fogfar : register(c42);
float4 margin : register(c43);
float4 g_MatrialColor : register(c44);

struct PSOutput
{
   float4 color : COLOR0;
   float4 depth : COLOR1;
};

PSOutput main(float2 uv : TEXCOORD0, float4 shadow_position : TEXCOORD1,
              float4 depth_position : TEXCOORD2,
              float4 fog_position : TEXCOORD3)
{
   clip(fog_position.z);
   float2 ndc;
   const float s =
       ShadowReceive(shadow_position, margin.xyz, g_Sampler1, g_Sampler2, ndc);
   clip(tex2D(g_Sampler0, uv).w - 0.5);
   clip(s - 0.5);
   const float fog =
       saturate((fog_position.w + fogfar.x) / (fogfar.x - fognear.x));
   PSOutput output;
   output.color = float4(
       0.0, 0.0, 0.0, ShadowVignette(ndc) * (s - 0.5) * fog * g_MatrialColor.w);
   output.depth = 1.0 - depth_position.z / depth_position.w;
   return output;
}
