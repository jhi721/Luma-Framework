// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "../Includes/PlatinumToneCurve.hlsl" // PlatinumToneCurve::FitTangent (the HDR continuation)
#include "../Includes/Reinhard.hlsl" // Reinhard::ReinhardPiecewise (hue reference)
#include "../Includes/MacLeodBoynton.hlsl" // MacLeodBoynton::HueOnlyBT2020
// clang-format on

// MGR:R final pass "FilterShaderGather" (shader2.dat, D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x726E6E78 without alpha test;
// Gather_0x562F84D7 / Gather_0xDD1B4599 are its GREATEREQUAL / GREATER alpha test translations). Render layer 0x62
// (FUN_00a3ab10), after FXAA, into the back buffer (the canvas) before the UI. Transcribed from the translated disassembly. Every
// value is gamma encoded (R'G'B', 1 = SDR white) as the game's 8-bit targets hold it: the 256x1 ColorTableTexture (t2, the spline
// the CPU writes every frame, one grey curve in R, G and B: FUN_00eac880) read per channel with the channel as U, saturation
// g_Saido (c186 = cb4[194]) around Y' of the curved colour, the g_MulColor gain (c184 = cb4[192], with the light pre-scale's
// inverse and the auto exposure: FUN_00eb50c0) and the vertical g_AddColor gradient (c185 = cb4[193]); then the bloom (t1) times
// g_BloomColor (c187 = cb4[195]) is added and everything multiplied by g_Brightness (c188 = cb4[196], the brightness option).
// HDR: the curve continued by its tangent above a pivot (PlatinumToneCurve), vanilla exact below it. Then the vanilla clip's hue turn (MacLeod-Boynton, hue only), the display map (a
// max-channel C-infinity rolloff to the peak) and the UI paper white pre-scale (UI_DRAW_TYPE 2), encoded back for the Core
// composition.

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
// and offsets its UV, g_NoiseColor (c190 = cb4[198]) its value and .w its strength. 2: the same with no texture on s3 (dgVoodoo's
// GatherNoise_0xD7D5CA35 translation reads (0, 0, 0, 1) there)
#ifndef GATHER_NOISE
#define GATHER_NOISE 0
#endif

float Curve(float x)
{
   return PlatinumToneCurve::Sample(t2, cb3[48], cb3[49], x);
}

float3 Curve(float3 color)
{
   return float3(Curve(color.r), Curve(color.g), Curve(color.b));
}

// Cubic B-spline in 4 bilinear taps (Sigg and Hadwiger, "Fast Third-Order Texture Filtering", as Bayonetta's bloom composite)
float4 SampleBloomBSpline(float2 uv)
{
   float2 size;
   t1.GetDimensions(size.x, size.y);
   const float2 grid = uv * size - 0.5;
   const float2 index = floor(grid);
   const float2 f = grid - index;
   const float2 g = 1.0 - f;
   const float2 w0 = g * g * g / 6.0;
   const float2 w1 = 2.0 / 3.0 - 0.5 * f * f * (2.0 - f);
   const float2 w2 = 2.0 / 3.0 - 0.5 * g * g * (2.0 - g);
   const float2 w3 = f * f * f / 6.0;
   const float2 g0 = w0 + w1;
   const float2 h0 = (index + w1 / g0 - 0.5) / size;
   const float2 h1 = (index + w3 / (w2 + w3) + 1.5) / size;
   const float4 t00 = ApplyDgvMask(t1.SampleLevel(s1_s, h0, 0.0), cb3[46], cb3[47]);
   const float4 t10 = ApplyDgvMask(t1.SampleLevel(s1_s, float2(h1.x, h0.y), 0.0), cb3[46], cb3[47]);
   const float4 t01 = ApplyDgvMask(t1.SampleLevel(s1_s, float2(h0.x, h1.y), 0.0), cb3[46], cb3[47]);
   const float4 t11 = ApplyDgvMask(t1.SampleLevel(s1_s, h1, 0.0), cb3[46], cb3[47]);
   return lerp(lerp(t11, t01, g0.x), lerp(t10, t00, g0.x), g0.y);
}

void main(DGV_MAIN_SIGNATURE)
{
   const float4 scene = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);
   // Luma Bloom's texture (main.cpp, CustomData2 1): a B-spline magnification, not the bilinear's creases
   float4 bloom;
   [branch] if (LumaData.CustomData2 == 1)
   {
      bloom = SampleBloomBSpline(v5.xy);
   }
   else
   {
      bloom = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]);
   }

   // The sampler clamps U, so this is the vanilla curve of the clipped scene
   float3 ungraded = saturate(scene.rgb);
   float3 curved = Curve(ungraded);
   [branch] if (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1)
   {
      ungraded = IsNaN_Strict(scene.rgb) ? 0.0 : max(scene.rgb, 0.0);
      // Every pivot is at least a window up, so the fit is skipped where no channel can pass it
      [branch] if (max3(ungraded) > PlatinumToneCurve::kWindow)
      {
         curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(t2, cb3[48], cb3[49]), ungraded, Curve(ungraded));
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
   // "Exposure" rides on the g_MulColor gain, where the vanilla auto exposure is (encoded: its linear scale through the 2.2 decode)
   const float3 gain = cb4[192].xyz * pow(LumaSettings.GameSettings.Exposure, 1.0 / DefaultGamma);
   const float3 graded = ((curved - luma) * saturation + curved) * gain + gradient;
   o0 = (float4(graded, 0.0) + bloom * cb4[195] * LumaSettings.GameSettings.BloomIntensity) * cb4[196];
#if GATHER_NOISE
#if GATHER_NOISE == 2
   const float4 noise = float4(0.0, 0.0, 0.0, 1.0);
#else
   const float4 noise = ApplyDgvMask(t3.Sample(s3_s, v5.xy * cb4[197].xy + cb4[197].zw), cb3[50], cb3[51]);
#endif
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
   // Vanilla clipped per channel in 8-bit (the scene before the curve, the canvas here), turning bright highlights toward the
   // primaries; the max-channel curve and map keep the unclipped hue. Restore only that hue turn, from a per-channel Reinhard
   // (ceiling 5, shoulder 1.5: identity below it, so the scene under 1.5 stays exact), the reference our RenoDX ports ship. Hue 1;
   // "Highlights Desaturation" is the chrominance strength, the share of the reference's whitening (0 = none, vanilla's at 1).
   // Before the map on purpose: it is hue neutral and contains any channel the turn lifts. Below the shoulder the reference is the
   // colour itself, so the solve (some 300 instructions) is skipped there.
   color = gamma_to_linear(color, GCT_MIRROR);
   const float3 color2020 = BT709_To_BT2020(color);
   [branch] if (max3(color2020) > 1.5)
   {
      color = BT2020_To_BT709(MacLeodBoynton::HueAndPurityEmulationBT2020(color2020, Reinhard::ReinhardPiecewise(color2020, 5.0, 1.5), 1.0,
                                                                          LumaSettings.GameSettings.HighlightsDesaturation));
   }
   // Contrast (a power around mid gray) and saturation, as Sunset Overdrive's DICE path: the map below doesn't take them
   const float contrast = LumaSettings.GameSettings.Contrast;
   [branch] if (contrast != 1.0)
   {
      color *= pow(max(GetLuminance(color, CS_BT709), FLT_MIN) / MidGray, contrast - 1.0);
   }
   const float saturationScale = LumaSettings.GameSettings.Saturation;
   [branch] if (saturationScale != 1.0)
   {
      color = Saturation(color, saturationScale, CS_BT709);
   }
   // Linear BT.709, 1 = paper white; the peak can't go below paper white (the rolloff needs it above its 0.18 anchor)
   const float paperWhite = max(GamePaperWhiteNits, 1.0);
   color = Tonemap::CInfinityRollOff::MaxChannel(color, max(PeakWhiteNits, paperWhite) / paperWhite, 0.18, kDisplayMapStrength);
#else
   // The vanilla clip, on the HDR canvas: only the UI paper white pre-scale the composition expects
   float3 color = gamma_to_linear(saturate(o0.rgb), GCT_MIRROR);
#endif
   color *= GetUIPaperWhitePreScale(GamePaperWhiteNits, UIPaperWhiteNits, true);
   o0.rgb = linear_to_gamma(color, GCT_MIRROR);
}
