#ifndef LUMA_ME2_FX_MATERIAL
#define LUMA_ME2_FX_MATERIAL

// Feedback material effects drawn between the uber and the BioSceneEffect material (bMergePostUber), on the gamma
// canvas. Vanilla ran them on an 8-bit canvas: scene samples <= 1 in, result clamped out. The replacements restore both
// clamps and pass the scene's HDR excess through with the linear weight vanilla gives it. SM3 c<N> = cb4[N+8].

// clang-format off
#include "Common.hlsl"
#include "GameBindings.hlsl"
// clang-format on

#define FXC(n) PsConstants[(n) + 8]

Texture2D<float4> Scene : register(t0);
SamplerState SceneSampler : register(s0);
Texture2D<float4> Tex1 : register(t1); // per material: noise, masks, flash textures
Texture2D<float4> Tex2 : register(t2);
Texture2D<float4> Tex3 : register(t3);
Texture2D<float4> Tex4 : register(t4);
Texture2D<float4> Tex5 : register(t5);
Texture2D<float4> Tex6 : register(t6);
SamplerState Sampler1 : register(s1);
SamplerState Sampler2 : register(s2);
SamplerState Sampler3 : register(s3);
SamplerState Sampler4 : register(s4);
SamplerState Sampler5 : register(s5);
SamplerState Sampler6 : register(s6);

// A fetch plus the dgVoodoo mask pair of its sampler slot (cb3[44 + 2N], cb3[45 + 2N]).
float4 FX_Fetch(Texture2D<float4> t, SamplerState s, float2 uv, uint slot)
{
   return ApplyDgvMask(t.Sample(s, uv), DgvConstants[44 + 2 * slot], DgvConstants[45 + 2 * slot]);
}

// A [0,1] quad position through ScreenPositionScaleBias (c1).
float2 FX_ScreenUV(float2 uv)
{
   return (uv * float2(2.0, -2.0) + float2(-1.0, 1.0)) * FXC(1).xy + FXC(1).wz;
}
float3 FX_SceneHDR(float2 screenUV)
{
   return FX_Fetch(Scene, SceneSampler, screenUV, 0).xyz;
}
float3 FX_Scene(float2 screenUV)
{
   return saturate(FX_SceneHDR(screenUV));
}
// The pixel's own scene position (perspective-correct TEXCOORD5).
float2 FX_PixelUV(float4 screenPos)
{
   return (rcp(screenPos.w) * screenPos.xy) * FXC(1).xy + FXC(1).wz;
}
float3 FX_Desaturate(float3 c, float amount)
{
   return lerp(c, dot(c, float3(0.3, 0.59, 0.11)), amount);
}
// Vanilla's UNORM output clamp, then the HDR excess its clamped inputs dropped, at vanilla's linear weight.
float4 FX_Output(float3 vanilla, float3 excess, float alpha)
{
   return float4(saturate(vanilla) + excess, alpha);
}

// Lightning / Explosion: three scaled taps mixed, a squared glow blended over the pixel, then desaturated. Rows from
// `r`: taps (r+2,r+3) (r+4,r+5) (r,r+1), mixes r+6/r+7, gain r+8, glow r+9, blend r+10, desaturation r+11.
float4 FX_SquareFlash(float2 uv, float4 screenPos, uint r, float3 squareScale)
{
   const float3 tapA = FX_SceneHDR(FX_ScreenUV(FXC(r + 2).x * uv + FXC(r + 3).x));
   const float3 tapB = FX_SceneHDR(FX_ScreenUV(FXC(r + 4).x * uv + FXC(r + 5).x));
   const float3 tapC = FX_SceneHDR(FX_ScreenUV(FXC(r).x * uv + FXC(r + 1).x));
   const float3 mixHDR = lerp(tapC, lerp(tapA, tapB, FXC(r + 6).x), FXC(r + 7).x) * FXC(r + 8).x;
   const float3 mix = lerp(saturate(tapC), lerp(saturate(tapA), saturate(tapB), FXC(r + 6).x), FXC(r + 7).x) * FXC(r + 8).x;
   const float3 s = mix * squareScale;
   const float3 glow = FXC(r + 9).x * (s * s - mix) + mix;
   const float3 sceneHDR = FX_SceneHDR(FX_PixelUV(screenPos));
   const float3 scene = saturate(sceneHDR);
   const float k = FXC(r + 10).x;
   const float3 excess = (1.0 - k) * (sceneHDR - scene) + k * (1.0 - FXC(r + 9).x) * (mixHDR - mix);
   return FX_Output(FX_Desaturate(lerp(scene, glow, k), FXC(r + 11).x) + FXC(0).xyz, FX_Desaturate(excess, FXC(r + 11).x), screenPos.w);
}

// dgVoodoo's fixed interpolator layout (linkage is by register): every entry point declares all 13.
#define FX_MAIN_SIGNATURE \
   float4 v0 : SV_POSITION0, float4 v1 : TEXCOORD8, float4 v2 : COLOR0, float4 v3 : COLOR1, float4 v4 : TEXCOORD9, float4 v5 : TEXCOORD0, float4 v6 : TEXCOORD1, float4 v7 : TEXCOORD2, float4 v8 : TEXCOORD3, float4 v9 : TEXCOORD4, float4 v10 : TEXCOORD5, float4 v11 : TEXCOORD6, float4 v12 : TEXCOORD7, out float4 o0 : SV_TARGET0

#endif // LUMA_ME2_FX_MATERIAL
