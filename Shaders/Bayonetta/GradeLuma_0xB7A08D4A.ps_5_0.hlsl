// Bayonetta grade "filter04" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0xB7A08D4A), filter00's variant with the curve on Y' only.
// Transcribed from the translated disassembly, everything gamma encoded (R'G'B', 1 = SDR white): Y' of the scene (the game's
// 0.298912/0.586611/0.114478 on encoded values) reads the 256x1 ColorTableTexture, the colour is scaled by curve(Y')/Y' (dgVoodoo's
// guarded reciprocal), then saturation g_Saido (c193.xyz) around the curve value and the g_MatrialColor gain (c192.xyz, the auto
// exposure); alpha is c192.w.
// HDR: as in Grade_0x719339A9, the curve continued by its tangent above a pivot (PlatinumToneCurve), here on Y' (the curve's input);
// vanilla exact below the pivot.

#include "../Includes/DgVoodoo.hlsl"          // DGV_MAIN_SIGNATURE, ApplyDgvMask, DgVoodooRcp
#include "../Includes/PlatinumToneCurve.hlsl" // PlatinumToneCurve::FitTangent (the HDR continuation)
#include "Includes/Common.hlsl"

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] the scene's texture masks, [46]/[47] the curve's
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: c192 g_MatrialColor (.w alpha), c193 g_Saido
}

Texture2D<float4> t0 : register(t0); // scene
Texture2D<float4> t1 : register(t1); // ColorTableTexture

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

float Curve(float x)
{
   return PlatinumToneCurve::Sample(t1, cb3[46], cb3[47], x);
}

void main(DGV_MAIN_SIGNATURE)
{
   const float3 scene = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).rgb;
   const bool hdr = TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1;
   // SDR: Y' of the scene as the 8-bit target clipped it
   const float3 color = hdr ? (IsNaN_Strict(scene) ? 0.0 : max(scene, 0.0)) : saturate(scene);
   const float luma = dot(color, kLumaWeights);
   float curved = Curve(luma);
   [branch] if (hdr && luma > PlatinumToneCurve::kWindow)
   {
      curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(t1, cb3[46], cb3[47]), luma, curved);
   }
   // "Color Grading Intensity" fades the curve toward identity and the saturation (an offset from the colour) toward none, as in
   // Grade_0x719339A9; the g_MatrialColor gain (auto exposure) stays
   const float gradeIntensity = LumaSettings.GameSettings.ColorGradingIntensity;
   float3 saturation = cb4[193].xyz;
   [branch] if (gradeIntensity != 1.0)
   {
      curved = lerp(luma, curved, gradeIntensity);
      saturation *= gradeIntensity;
   }
   const float3 toned = color * curved * DgVoodooRcp(luma);
   // "Exposure" on the g_MatrialColor gain, as in Grade_0x719339A9
   o0.rgb = ((toned - curved) * saturation + toned) * cb4[192].xyz * pow(LumaSettings.GameSettings.Exposure, 1.0 / DefaultGamma);
   // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
   o0.rgb = hdr ? max(o0.rgb, 0.0) : saturate(o0.rgb);
   o0.a = saturate(cb4[192].w);
}
