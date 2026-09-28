#ifndef LUMA_ME3_FX_MATERIAL
#define LUMA_ME3_FX_MATERIAL

// Feedback material effects drawn after the uber (fullscreen, scene at t0/s0, c<N> = cb4[N+8]). Vanilla ran them on the
// 8-bit canvas: scene samples <= 1 in, result clamped out. The replacements restore both clamps and pass the scene's HDR
// excess through with the weight vanilla gives the scene.

// clang-format off
#include "Common.hlsl"
#include "GameBindings.hlsl"
// clang-format on

Texture2D<float4> Scene : register(t0);
SamplerState SceneSampler : register(s0);
Texture2D<float4> Tex1 : register(t1); // per material: noise, mask or streaks
Texture2D<float4> Tex2 : register(t2);
Texture2D<float4> Tex3 : register(t3);
Texture2D<float4> Tex4 : register(t4);
SamplerState Sampler1 : register(s1);
SamplerState Sampler2 : register(s2);
SamplerState Sampler3 : register(s3);
SamplerState Sampler4 : register(s4);

// A [0,1] quad position through ScreenPositionScaleBias (c1).
float2 FX_ScreenUV(float2 uv)
{
   return (uv * float2(2.0, -2.0) + float2(-1.0, 1.0)) * PsConstants[9].xy + PsConstants[9].wz;
}
// UE3's zoom factor from a material row: ((row - 0.25) * scale + 0.25) * 4.
float FX_Zoom(float row, float scale)
{
   return ((row - 0.25) * scale + 0.25) * 4.0;
}
float2 FX_ZoomUV(float2 uv, float zoom)
{
   return FX_ScreenUV(zoom * (uv - 0.5) + 0.5);
}
float3 FX_SceneHDR(float2 screenUV)
{
   return ApplyDgvMask(Scene.Sample(SceneSampler, screenUV), 0).xyz;
}
float3 FX_Scene(float2 screenUV)
{
   return saturate(FX_SceneHDR(screenUV));
}
float FX_InvW(float4 v10)
{
   return abs(v10.w) > 0.0 ? rcp(v10.w) : FLT_MAX;
}
// The pixel's own scene position (perspective-correct) and colour, unclamped.
float2 FX_PixelUV(float4 v10)
{
   return (FX_InvW(v10) * v10.xy) * PsConstants[9].xy + PsConstants[9].wz;
}
float3 FX_PixelSceneHDR(float4 v10)
{
   return FX_SceneHDR(FX_PixelUV(v10));
}
float FX_Alpha(float4 v10)
{
   return PsConstants[10].x * FX_InvW(v10) + PsConstants[10].y;
}
// UE3 glow: c + 20 k |c|^5.
float3 FX_Glow(float3 c, float k)
{
   const float3 a = max(abs(c), 1e-6);
   return a * a * a * a * a * k * 20.0 + c;
}
// Glow on near-black scene: saturate((hi c)^hiPow) * (1 - (lo c)^8)^outPow. ~0 for c >= 1/lo, so HDR input is safe.
float3 FX_DarkBand(float3 c, float hi, float hiPow, float lo, float outPow)
{
   const float3 l = max(abs(c * lo), 1e-6);
   return saturate(pow(max(abs(c * hi), 1e-6), hiPow)) * pow(max(1.0 - pow(l, 8.0), 1e-6), outPow);
}
// Vanilla's UNORM output clamp, plus the HDR excess its clamped inputs dropped (the caller weights it as vanilla does).
float4 FX_Output(float3 vanilla, float3 excess, float alpha)
{
   return float4(saturate(vanilla) + excess, alpha);
}

// Adrenaline Rush / Krogan Berserk: rotated noisy mask tint, chromatic zoom split, whiteout 5 (m + 0.015)^2. Shared rows:
// tint c3, noise offset c4, zoom c5.w / c6 / c6.w / c7.x; per-material: noise scale, tint scale, final weight.
float4 FX_Whiteout(float2 uv, float4 v10, float noiseScale, float tintScale, float weightScale, float3 zoomBase, float blendScale, bool blendSqrt)
{
   const float2 centred = uv - 0.5;
   float2 maskUV = float2(dot(float2(0.070737, -0.997495), centred), dot(float2(0.997495, 0.070737), centred)) + 0.5;
   maskUV += noiseScale * ApplyDgvMask(Tex1.Sample(Sampler1, uv * 3.0 + PsConstants[12].xy), 1).xy;
   const float4 mask = ApplyDgvMask(Tex2.Sample(Sampler2, maskUV), 2);
   const float2 ndc = uv * 2.0 - 1.0;
   const float r2 = dot(ndc, ndc);
   const float edge = r2 - 1e-6 >= 0.0 ? saturate(exp2(log2(r2) * 0.2)) : 0.0;
   const float inner = r2 - 1e-6 >= 0.0 ? saturate(exp2(log2(r2) * 0.1)) : 0.0;
   const float3 tint = mask.x * (1.0 - inner) * min(mask.y * mask.y * 200.0, 2.0) * PsConstants[11].xyz;

   const float3 zoom = (PsConstants[13].w * (PsConstants[14].xyz + zoomBase) - zoomBase) * 4.0;
   const float3 sampleR = FX_Scene(FX_ZoomUV(uv, zoom.x));
   const float3 split = lerp(sampleR, lerp(FX_Scene(FX_ZoomUV(uv, zoom.y)), FX_Scene(FX_ZoomUV(uv, zoom.z)), PsConstants[14].w), PsConstants[15].x);

   const float2 d = uv * blendScale - blendScale * 0.5;
   const float r3 = dot(d, d);
   const float blend = r3 - 1e-6 >= 0.0 ? saturate(blendSqrt ? sqrt(r3) : r3 * r3) : 0.0;
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);

   const float3 mixed = tintScale * tint + lerp(scene, split, blend);
   const float weight = weightScale * edge;
   return FX_Output(weight * ((mixed + 0.015) * (mixed + 0.015) * 5.0 - scene) + scene + PsConstants[8].xyz, (1.0 - weight) * (sceneHDR - scene), FX_Alpha(v10));
}

#endif // LUMA_ME3_FX_MATERIAL
