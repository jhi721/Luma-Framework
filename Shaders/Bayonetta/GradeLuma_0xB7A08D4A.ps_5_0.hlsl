// Bayonetta grade "filter04" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0xB7A08D4A), filter00's variant with the curve on Y' only.
// Transcribed from the translated disassembly, everything gamma encoded (R'G'B', 1 = SDR white): Y' of the scene (the game's
// 0.298912/0.586611/0.114478 on encoded values) reads the 256x1 ColorTableTexture, the colour is scaled by curve(Y')/Y' (dgVoodoo's
// guarded reciprocal), then saturation g_Saido (c193.xyz) around the curve value and the g_MatrialColor gain (c192.xyz, the auto
// exposure); alpha is c192.w.
// HDR: as in Grade_0x719339A9, one scalar compresses the scene into the curve's [0,1] domain, here on Y' (the curve's input): the
// vanilla chain runs on it and the scalar is divided back out. Below the shoulder every pixel stays vanilla exact.

#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask, DgVoodooRcp
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
   return ApplyDgvMask(t1.Sample(s1_s, float2(x, 0.0)), cb3[46], cb3[47]).x;
}

void main(DGV_MAIN_SIGNATURE)
{
   const float3 scene = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).rgb;
   const bool hdr = TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1;
   // SDR: Y' of the scene as the 8-bit target clipped it
   float3 color = saturate(scene);
   float scale = 1.0;
   [branch] if (hdr)
   {
      color = IsNaN_Strict(scene) ? 0.0 : max(scene, 0.0);
      scale = CurveDomainScale(dot(color, kLumaWeights));
   }
   // The compression is a scalar, so it cancels out of color/luma: only the curve's input moves
   const float luma = dot(color, kLumaWeights) * scale;
   float curved = Curve(luma);
   // "Color Grading Intensity" fades the curve toward identity and the saturation (an offset from the colour) toward none, as in
   // Grade_0x719339A9; the g_MatrialColor gain (auto exposure) stays
   const float gradeIntensity = LumaSettings.GameSettings.ColorGradingIntensity;
   float3 saturation = cb4[193].xyz;
   [branch] if (gradeIntensity != 1.0)
   {
      curved = lerp(luma, curved, gradeIntensity);
      saturation *= gradeIntensity;
   }
   const float3 toned = color * scale * curved * DgVoodooRcp(luma);
   o0.rgb = ((toned - curved) * saturation + toned) / scale * cb4[192].xyz;
   // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
   o0.rgb = hdr ? max(o0.rgb, 0.0) : saturate(o0.rgb);
   o0.a = saturate(cb4[192].w);
}
