// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/ShadowReceive.hlsl"
// clang-format on

// Bayonetta "modelshadershadowfixed" (dgVoodoo 2.87.5 -> ps_5_0 0xCDA8ACDD), the shadow receive pass of "Includes/ShadowReceive.hlsl"
// with the fixed function constants: no depth target, the material alpha scaled, red the fog colour times 1e-6.
// g_Sampler0 s0 (colour, alpha tested), g_Sampler1 s1 (the shadow map, R32F), g_Sampler2 s2 (RGBA8 map); g_MatrialColor c184 [192],
// fog c185 [193], fognear c186 [194], fogfar c187 [195], margin c188 [196].

Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1);
Texture2D<float4> t2 : register(t2);

SamplerState s0 : register(s0);
SamplerState s1 : register(s1);
SamplerState s2 : register(s2);

void main(DGV_MAIN_SIGNATURE)
{
   if (v8.z < 0.0)
      discard;
   float2 ndc;
   const float shadow = ShadowReceive(v6, cb4[196], t1, s1, cb3[46], cb3[47], t2, s2, cb3[48], cb3[49], ndc);
   if (ApplyDgvMask(t0.Sample(s0, v5.xy), cb3[44], cb3[45]).w * cb4[192].w < 0.5 || shadow < 0.5)
      discard;
   const float fog = saturate((v8.w + cb4[195].x) * DgVoodooRcp(cb4[195].x - cb4[194].x));
   o0 = float4(cb4[193].x * 1e-6, 0.0, 0.0, ShadowVignette(ndc) * (shadow - 0.5) * fog);
}
