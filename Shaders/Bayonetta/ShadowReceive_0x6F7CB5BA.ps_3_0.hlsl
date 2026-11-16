// Bayonetta "modelshadershadowfixed" (raw ps_3_0 CRC32 0x6F7CB5BA =
// dgVoodoo 2.87.5 0xCDA8ACDD), native D3D9 port: the shadow receive pass with
// the fixed function constants: no depth target, the material alpha scaled in
// the alpha test, red the fog colour times 1e-6. See
// Includes/ShadowReceive_sm3.hlsli. g_Sampler0 s0 (colour, alpha tested), s1
// (shadow map, R32F), s2 (RGBA8 soft map); g_MatrialColor c184, fog c185,
// fognear c186, fogfar c187, margin c188, g_TargetSize c194.
#include "Includes/ShadowReceive_sm3.hlsli"

sampler2D g_Sampler0 : register(s0);
sampler2D g_Sampler1 : register(s1);
sampler2D g_Sampler2 : register(s2);
float4 g_MatrialColor : register(c184);
float4 fog : register(c185);
float4 fognear : register(c186);
float4 fogfar : register(c187);
float4 margin : register(c188);

float4 main(float2 uv : TEXCOORD0, float4 shadow_position : TEXCOORD1,
            float4 fog_position : TEXCOORD3)
    : COLOR0
{
   clip(fog_position.z);
   float2 ndc;
   const float s =
       ShadowReceive(shadow_position, margin.xyz, g_Sampler1, g_Sampler2, ndc);
   clip(tex2D(g_Sampler0, uv).w * g_MatrialColor.w - 0.5);
   clip(s - 0.5);
   const float fog_amount =
       saturate((fog_position.w + fogfar.x) / (fogfar.x - fognear.x));
   return float4(fog.x * 1e-6, 0.0, 0.0,
                 ShadowVignette(ndc) * (s - 0.5) * fog_amount);
}
