// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/ShadowReceive.hlsl"
// clang-format on

// Bayonetta "modelshadershadowrec00_xxxxx" (dgVoodoo 2.87.5 -> ps_5_0 0x98BD17D3), the shadow receive pass drawn over the scene: the
// shadow in black with its alpha, and depth (1 - z/w) to target 1. See "Includes/ShadowReceive.hlsl".
// g_Sampler0 s13 (the material's colour, its alpha tested), g_Sampler1 s14 (the shadow map, R32F), g_Sampler2 s15 (RGBA8 map);
// fognear c41 [49], fogfar c42 [50], margin c43 [51], g_MatrialColor c44 [52].

Texture2D<float4> t13 : register(t13);
Texture2D<float4> t14 : register(t14);
Texture2D<float4> t15 : register(t15);

SamplerState s13 : register(s13);
SamplerState s14 : register(s14); // Point
SamplerState s15 : register(s15);

void main(DGV_MAIN_SIGNATURE, out float4 o1 : SV_TARGET1)
{
   if (v8.z < 0.0)
      discard;
   float2 ndc;
   const float shadow = ShadowReceive(v6, cb4[51], t14, s14, cb3[72], cb3[73], t15, s15, cb3[74], cb3[75], ndc);
   if (ApplyDgvMask(t13.Sample(s13, v5.xy), cb3[70], cb3[71]).w < 0.5 || shadow < 0.5)
      discard;
   const float fog = saturate((v8.w + cb4[50].x) * DgVoodooRcp(cb4[50].x - cb4[49].x));
   o0 = float4(0.0, 0.0, 0.0, ShadowVignette(ndc) * (shadow - 0.5) * fog * cb4[52].w);
   o1 = 1.0 - v7.z * DgVoodooRcp(v7.w);
}
