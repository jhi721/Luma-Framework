// Bayonetta grade "filter00" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x719339A9), drawn once per frame, before the bloom.
// Transcribed from the translated disassembly. Every value is gamma encoded (R'G'B', 1 = SDR white), as the game displays its
// 8-bit targets: the 256x1 ColorTableTexture (t1, a spline the CPU writes every frame, the same grey curve in R, G and B) is
// read per channel with the channel as U, then saturation g_Saido (c193.xyz) around Y' of the curved colour, the
// g_MatrialColor gain (c192.xyz, which carries the auto exposure) and the vertical g_AddColor gradient (c194).
// HDR: the scene target is upgraded to fp16, so the lighting's values above 1 (clipped by the 8-bit target in vanilla) reach
// this pass. One max-channel scalar compresses the colour into the curve's [0,1] domain (identity below the shoulder), the
// vanilla curve runs on it and the scalar is divided back out of its output: a scalar cannot move a channel ratio, and every
// pixel below the shoulder stays vanilla exact. The display map is the scene copy's (Copy_0x91A6C437).

#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
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
   return ApplyDgvMask(t1.Sample(s1_s, float2(x, 0.0)), cb3[46], cb3[47]).x;
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
   [branch] if (hdr)
   {
      const float3 color = IsNaN_Strict(scene.rgb) ? 0.0 : max(scene.rgb, 0.0);
      const float peak = max3(color);
      // The max channel is what the curve's domain must hold
      if (peak > kCurveShoulder)
      {
         const float scale = CurveDomainScale(peak);
         curved = Curve(color * scale) / scale;
         luma = dot(curved, kLumaWeights);
      }
   }

   o0.rgb = ((curved - luma) * cb4[193].xyz + curved) * cb4[192].xyz + gradient;
   // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
   o0.rgb = hdr ? max(o0.rgb, 0.0) : saturate(o0.rgb);
}
