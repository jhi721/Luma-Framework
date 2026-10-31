// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/PsychoV31.hlsl" // PsychoVDisplayMap
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// MGR:R final pass "FilterShaderGather" (shader2.dat, D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x726E6E78 without alpha test;
// Gather_0x562F84D7 / Gather_0xDD1B4599 are its GREATEREQUAL / GREATER alpha test translations). Render layer 0x62
// (FUN_00a3ab10), after FXAA, into the back buffer (the canvas) before the UI. Transcribed from the translated disassembly. Every
// value is gamma encoded (R'G'B', 1 = SDR white) as the game's 8-bit targets hold it: the 256x1 ColorTableTexture (t2, the spline
// the CPU writes every frame, one grey curve in R, G and B: FUN_00eac880) read per channel with the channel as U, saturation
// g_Saido (c186 = cb4[194]) around Y' of the curved colour, the g_MulColor gain (c184 = cb4[192], with the light pre-scale's
// inverse and the auto exposure: FUN_00eb50c0) and the vertical g_AddColor gradient (c185 = cb4[193]); then the bloom (t1) times
// g_BloomColor (c187 = cb4[195]) is added and everything multiplied by g_Brightness (c188 = cb4[196], the brightness option).
// HDR: the grade's curve as Bayonetta's (the same engine code): one max-channel scalar compresses values above the shoulder into
// the curve's [0,1] domain and is divided back out, so every pixel below the shoulder stays vanilla exact. Then the display map
// (PsychoV31 to the peak) and the UI paper white pre-scale (UI_DRAW_TYPE 2), encoded back for the Core composition.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks, [46]/[47] t1's, [48]/[49] t2's, [8].z the alpha test reference (uint)
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0); // scene
Texture2D<float4> t1 : register(t1); // bloom
Texture2D<float4> t2 : register(t2); // ColorTableTexture
Texture2D<float4> t3 : register(t3); // NoiseTexture (GATHER_NOISE)

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);
SamplerState s3_s : register(s3);

// 0 none, 1 GREATEREQUAL (Gather_0x562F84D7), 2 GREATER (Gather_0xDD1B4599)
#ifndef GATHER_ALPHA_TEST
#define GATHER_ALPHA_TEST 0
#endif
// 1: "FilterShaderGatherNoise" (GatherNoise_0xCE6E1597), the same pass times a noise texture: g_NoiseOffset (c189 = cb4[197]) scales
// and offsets its UV, g_NoiseColor (c190 = cb4[198]) its value and .w its strength
#ifndef GATHER_NOISE
#define GATHER_NOISE 0
#endif

float Curve(float x)
{
   return ApplyDgvMask(t2.Sample(s2_s, float2(x, 0.0)), cb3[48], cb3[49]).x;
}

float3 Curve(float3 color)
{
   return float3(Curve(color.r), Curve(color.g), Curve(color.b));
}

void main(DGV_MAIN_SIGNATURE)
{
   const float4 scene = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);
   const float4 bloom = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]);

   // The sampler clamps U, so this is the vanilla curve of the clipped scene
   float3 ungraded = saturate(scene.rgb);
   float3 curved = Curve(scene.rgb);
   [branch] if (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1)
   {
      ungraded = IsNaN_Strict(scene.rgb) ? 0.0 : max(scene.rgb, 0.0);
      const float peak = max3(ungraded);
      // The max channel is what the curve's domain must hold
      if (peak > kCurveShoulder)
      {
         const float scale = CurveDomainScale(peak);
         curved = Curve(ungraded * scale) / scale;
      }
   }
   // "Color Grading Intensity" fades the level's look (Bayonetta's): the curve toward identity, the saturation (an offset from the
   // colour, 0 = none) and the gradient toward none. The g_MulColor gain stays: it carries the auto exposure.
   const float gradeIntensity = LumaSettings.GameSettings.ColorGradingIntensity;
   float3 saturation = cb4[194].xyz;
   float3 gradient = saturate((1.0 - v5.y * cb4[193].w) * cb4[193].xyz);
   [branch] if (gradeIntensity != 1.0)
   {
      curved = lerp(ungraded, curved, gradeIntensity);
      saturation *= gradeIntensity;
      gradient *= gradeIntensity;
   }
   const float luma = dot(curved, kLumaWeights);
   const float3 graded = ((curved - luma) * saturation + curved) * cb4[192].xyz + gradient;
   o0 = (float4(graded, 0.0) + bloom * cb4[195] * LumaSettings.GameSettings.BloomIntensity) * cb4[196];
#if GATHER_NOISE
   const float4 noise = ApplyDgvMask(t3.Sample(s3_s, v5.xy * cb4[197].xy + cb4[197].zw), cb3[50], cb3[51]);
   o0 *= 1.0 + cb4[198].w * (noise * cb4[198] - 1.0);
#endif

#if GATHER_ALPHA_TEST
   const uint alpha = uint(o0.a * 255.0 + 0.0001);
   const uint reference = asuint(cb3[8].z);
#if GATHER_ALPHA_TEST == 1
   if (alpha < reference)
#else
   if (alpha <= reference)
#endif
   {
      discard;
   }
#endif

   // SDR: the 8-bit canvas clipped the scene before the UI blended on it
   if (LumaSettings.DisplayMode != 1)
   {
      o0 = saturate(o0);
      return;
   }
   o0.a = saturate(o0.a);
#if TONEMAP_TYPE >= 1
   // The 8-bit targets floored the saturation's negatives
   float3 color = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
   color = PsychoVDisplayMap(gamma_to_linear(color, GCT_MIRROR), LumaSettings.GameSettings.Contrast, LumaSettings.GameSettings.Saturation, LumaSettings.GameSettings.HighlightsDesaturation);
#else
   // The vanilla clip, on the HDR canvas: only the UI paper white pre-scale the composition expects
   float3 color = gamma_to_linear(saturate(o0.rgb), GCT_MIRROR);
#endif
   color *= GetUIPaperWhitePreScale(GamePaperWhiteNits, UIPaperWhiteNits, true);
   o0.rgb = linear_to_gamma(color, GCT_MIRROR);
}
