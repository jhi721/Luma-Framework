// Bayonetta grade variant "filter04" (raw D3D9 ps_3_0 blob, CRC32 0x373A4BB7 = dgVoodoo 2.87.5 0xB7A08D4A), native D3D9 port.
// Vanilla math from the disassembly (transfer-encoded): ColorTableTexture read at Y' of the scene, colour scaled by curve(Y')/Y',
// saturation g_Saido around the curve value, * g_MatrialColor; alpha = g_MatrialColor.w. 1/Y' is an unguarded rcp as in the blob.
// Luma (g_LumaGrade, c208): as in Grade_0x6E722DE9, .x "Exposure" on the g_MatrialColor gain in both paths, the blob's math with
// .z = 0 and .y = 1, else the old D3D11 mod's GradeLuma_0xB7A08D4A: the smoothed curve on Y', its tangent continuation in HDR (.z),
// "Color Grading Intensity" (.y) and the 8-bit target's clamps.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);
sampler2D ColorTableTexture : register(s1);
float4 g_MatrialColor : register(c184); // .w alpha
float4 g_Saido : register(c185);

#include "Includes/PlatinumToneCurve_sm3.hlsli"

// The game's Y' weights, on encoded values
static const float3 kLumaWeights = float3(0.298911989, 0.586610973, 0.114478);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float3 scene = tex2D(Texture, uv).rgb;
   float4 result;
   const bool hdr = g_LumaGrade.z > 0.5;
   [branch] if (hdr || g_LumaGrade.y != 1.0)
   {
      // SDR: Y' of the scene as the 8-bit target clipped it. HDR: NaN to 0 through max as in Grade_0x6E722DE9, +inf kept
      const float3 color = hdr ? max(scene, 0.0) : saturate(scene);
      const float luma = dot(color, kLumaWeights);
      float curved = PlatinumToneCurve::Sample(ColorTableTexture, luma);
      [branch] if (hdr && luma > PlatinumToneCurve::kWindow)
      {
         curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(ColorTableTexture), luma, curved);
      }
      // "Color Grading Intensity" fades the curve toward identity and the saturation (an offset from the colour) toward none; the
      // g_MatrialColor gain (auto exposure) stays
      float3 saturation = g_Saido.xyz;
      [branch] if (g_LumaGrade.y != 1.0)
      {
         curved = lerp(luma, curved, g_LumaGrade.y);
         saturation *= g_LumaGrade.y;
      }
      // Y' = 0 only for black (non-negative colour, positive weights): toned 0 instead of the blob's 0 * inf
      const float3 toned = color * curved * (luma > 0.0 ? rcp(luma) : 0.0);
      result.rgb = ((toned - curved) * saturation + toned) * (g_MatrialColor.xyz * g_LumaGrade.x);
      // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
      result.rgb = hdr ? max(result.rgb, 0.0) : saturate(result.rgb);
      result.a = saturate(g_MatrialColor.w);
      return result;
   }
   const float luma = dot(scene, kLumaWeights);
   const float curved = tex2D(ColorTableTexture, float2(luma, 0)).x;
   const float3 toned = scene * curved * rcp(luma);
   result.rgb = ((toned - curved) * g_Saido.xyz + toned) * (g_MatrialColor.xyz * g_LumaGrade.x);
   result.a = g_MatrialColor.w;
   return result;
}
