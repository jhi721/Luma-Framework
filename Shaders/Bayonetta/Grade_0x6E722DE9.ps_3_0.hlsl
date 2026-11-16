// Bayonetta grade "filter00" (raw D3D9 ps_3_0 blob, CRC32 0x6E722DE9 = dgVoodoo 2.87.5 0x719339A9), native D3D9 spike port.
// Vanilla math from the disassembly (all values gamma encoded R'G'B'): ColorTableTexture (256x1, u = channel value) per channel,
// saturation g_Saido around Y' of the curved colour (0.2989/0.5866/0.1145), vertical g_AddColor gradient, * g_MatrialColor.
// Luma injection: g_Luma.x (c208, 1 = vanilla) multiplies the g_MatrialColor gain (the "Exposure" slot of the dgVoodoo grade).
// Native register numbers; dgVoodoo's cb4 rows are +8 (cb4[192] = c184).
// GRADE_HDR=1 selects the HDR viability probe (compile-only, not a verified port) instead of the vanilla port.

#if GRADE_HDR
// Viability probe, NOT a verified port: filter00 (CRC32 0x6E722DE9) with the HDR path of Shaders/Bayonetta/Grade_0x719339A9 (Platinum tone
// curve tangent continuation, grade intensity, exposure), on the game's unchanged global Common.hlsl and the ps_3_0 shim of PlatinumToneCurve.
// Needs the scene RT in fp16 (values above 1 reach this pass). Injected block (PS c208..c209): luma[0] = (exposure, grade intensity, hdr 0/1, 0).
#include "Includes/Common.hlsl"
#include "Includes/PlatinumToneCurve_sm3.hlsli"

sampler2D Texture : register(s0);
sampler2D ColorTableTexture : register(s1);
float4 g_MatrialColor : register(c184);
float4 g_Saido : register(c185);
float4 g_AddColor : register(c186);
float4 g_Luma[2] : register(c208);

float Curve(float x)
{
   return tex2D(ColorTableTexture, float2(x, 0)).x;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float4 scene = tex2D(Texture, uv);
   float3 curved = float3(Curve(scene.r), Curve(scene.g), Curve(scene.b));
   const float vanilla_luma = dot(curved, kLumaWeights);
   float4 result;
   result.a = saturate(scene.a * g_Saido.w + vanilla_luma - g_MatrialColor.w);
   const float3 gradient = saturate((1.0 - uv.y * g_AddColor.w) * g_AddColor.xyz);
   float luma = vanilla_luma;
   float3 color = saturate(scene.rgb);
   [branch] if (g_Luma[0].z > 0.5)
   {
      color = max(scene.rgb, 0.0); // ps_3_0: IsNaN_Strict (asuint) does not compile; max() drops NaN on most GPUs, not guaranteed;
      [branch] if (max3(color) > PlatinumToneCurve::kWindow)
      {
         curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(ColorTableTexture), color, curved);
         luma = dot(curved, kLumaWeights);
      }
   }
   float3 saturation = g_Saido.xyz;
   float3 gradient_tint = gradient;
   [branch] if (g_Luma[0].y != 1.0)
   {
      curved = lerp(color, curved, g_Luma[0].y);
      luma = dot(curved, kLumaWeights);
      saturation *= g_Luma[0].y;
      gradient_tint *= g_Luma[0].y;
   }
   const float3 gain = g_MatrialColor.xyz * pow(g_Luma[0].x, 1.0 / DefaultGamma);
   result.rgb = ((curved - luma) * saturation + curved) * gain + gradient_tint;
   result.rgb = g_Luma[0].z > 0.5 ? max(result.rgb, 0.0) : saturate(result.rgb);
   return result;
}
#else

sampler2D Texture : register(s0);
sampler2D ColorTableTexture : register(s1);
float4 g_MatrialColor : register(c184); // .w alpha offset
float4 g_Saido : register(c185);        // .xyz saturation offset, .w alpha scale
float4 g_AddColor : register(c186);     // .xyz tint, .w gradient slope
float4 g_Luma : register(c208);         // Luma injection base (PS c208..c223)

float Curve(float x)
{
   return tex2D(ColorTableTexture, float2(x, 0)).x;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float4 scene = tex2D(Texture, uv);
   const float3 curved = float3(Curve(scene.r), Curve(scene.g), Curve(scene.b));
   const float luma = dot(curved, float3(0.298911989, 0.586610973, 0.114478));
   const float3 gradient = saturate((1.0 - uv.y * g_AddColor.w) * g_AddColor.xyz);
   float4 result;
   result.rgb = ((curved - luma) * g_Saido.xyz + curved) * (g_MatrialColor.xyz * g_Luma.x) + gradient;
   result.a = scene.a * g_Saido.w + luma - g_MatrialColor.w;
   return result;
}
#endif
