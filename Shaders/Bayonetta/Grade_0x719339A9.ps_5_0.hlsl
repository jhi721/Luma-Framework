// Bayonetta grade "filter00" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x719339A9), drawn once per frame, before the bloom.
// Transcribed from the translated disassembly. Every value is gamma encoded (R'G'B', 1 = SDR white), as the game displays its
// 8-bit targets: the 256x1 ColorTableTexture (t1, a spline the CPU writes every frame, the same grey curve in R, G and B) is
// read per channel with the channel as U, then saturation g_Saido (c193.xyz) around Y' of the curved colour, the
// g_MatrialColor gain (c192.xyz, which carries the auto exposure) and the vertical g_AddColor gradient (c194).
// HDR: the scene target is upgraded to fp16, so the lighting's values above 1 (clipped by the 8-bit target in vanilla) reach
// this pass. The curve is continued by its tangent above a pivot (PlatinumToneCurve), vanilla exact below it. The display map is
// the scene copy's (Copy_0x91A6C437).

#include "../Includes/DgVoodoo.hlsl"          // DGV_MAIN_SIGNATURE, ApplyDgvMask
#include "../Includes/PlatinumToneCurve.hlsl" // PlatinumToneCurve::FitTangent (the HDR continuation)
#include "Includes/Common.hlsl"

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] the scene's texture masks, [46]/[47] the curve's
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: c192 g_MatrialColor (.w alpha offset), c193 g_Saido (.w alpha scale), c194 g_AddColor (.w gradient slope)
}

Texture2D<float4> t0 : register(t0); // scene
Texture2D<float4> t1 : register(t1); // ColorTableTexture

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

float Curve(float x)
{
   return PlatinumToneCurve::Sample(t1, cb3[46], cb3[47], x);
}

float3 Curve(float3 color)
{
   return float3(Curve(color.r), Curve(color.g), Curve(color.b));
}

void main(DGV_MAIN_SIGNATURE)
{
   const float4 scene = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);

   // The sampler clamps U, so this is the vanilla curve of the clipped scene
   float3 curved = Curve(scene.rgb);
   const float vanillaLuma = dot(curved, kLumaWeights);
   // Alpha from the vanilla curve, clamped as by the 8-bit target: the bloom source (filter05) thresholds on it
   o0.a = saturate(scene.a * cb4[193].w + vanillaLuma - cb4[192].w);
   const float3 gradient = saturate((1.0 - v5.y * cb4[194].w) * cb4[194].xyz);

   const bool hdr = TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1;
   float luma = vanillaLuma;
   // The curve's input, as the curve reads it
   float3 color = saturate(scene.rgb);
   [branch] if (hdr)
   {
      color = IsNaN_Strict(scene.rgb) ? 0.0 : max(scene.rgb, 0.0);
      // Every pivot is at least a window up, so the fit is skipped where no channel can pass it
      [branch] if (max3(color) > PlatinumToneCurve::kWindow)
      {
         curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(t1, cb3[46], cb3[47]), color, curved);
         luma = dot(curved, kLumaWeights);
      }
   }

   // "Color Grading Intensity" fades the level's look: the curve toward identity, the saturation (an offset from the colour, 0 =
   // none) and the gradient toward none. The g_MatrialColor gain stays: it carries the auto exposure. Alpha keeps the vanilla curve.
   const float gradeIntensity = LumaSettings.GameSettings.ColorGradingIntensity;
   float3 saturation = cb4[193].xyz;
   float3 gradientTint = gradient;
   [branch] if (gradeIntensity != 1.0)
   {
      curved = lerp(color, curved, gradeIntensity);
      luma = dot(curved, kLumaWeights);
      saturation *= gradeIntensity;
      gradientTint *= gradeIntensity;
   }

   // "Exposure" rides on the g_MatrialColor gain, where the vanilla auto exposure is (encoded: its linear scale through the 2.2 decode)
   const float3 gain = cb4[192].xyz * pow(LumaSettings.GameSettings.Exposure, 1.0 / DefaultGamma);
   o0.rgb = ((curved - luma) * saturation + curved) * gain + gradientTint;
   // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
   o0.rgb = hdr ? max(o0.rgb, 0.0) : saturate(o0.rgb);
}
