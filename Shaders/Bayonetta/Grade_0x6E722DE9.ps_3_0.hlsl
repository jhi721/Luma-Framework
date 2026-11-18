// Bayonetta grade "filter00" (raw D3D9 ps_3_0 blob, CRC32 0x6E722DE9 = dgVoodoo 2.87.5 0x719339A9), native D3D9 port.
// Vanilla math from the disassembly (all values transfer-encoded R'G'B', 1 = SDR white): ColorTableTexture (256x1, u = channel value) per
// channel, saturation g_Saido around Y' of the curved colour (0.2989/0.5866/0.1145), vertical g_AddColor gradient, * g_MatrialColor
// (which carries the auto exposure). Native register numbers; dgVoodoo's cb4 rows are +8 (cb4[192] = c184).
// Luma (g_LumaGrade, c208): .x "Exposure" multiplies the g_MatrialColor gain in both paths. With .z = 0 and .y = 1 the blob's math runs
// unchanged; otherwise the Luma path, the old D3D11 mod's Grade_0x719339A9: the table interpolated between smoothed texels
// (PlatinumToneCurve), in HDR (.z = 1, the scene target is fp16, values above 1 reach this pass) continued by its tangent above a pivot,
// "Color Grading Intensity" (.y) fading the curve, saturation and gradient, and the 8-bit target's clamps the fp16 one lost.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);
sampler2D ColorTableTexture : register(s1);
float4 g_MatrialColor : register(c184); // .w alpha offset
float4 g_Saido : register(c185);        // .xyz saturation offset, .w alpha scale
float4 g_AddColor : register(c186);     // .xyz tint, .w gradient slope

#include "Includes/PlatinumToneCurve_sm3.hlsli"

// The game's Y' weights, on encoded values
static const float3 kLumaWeights = float3(0.298911989, 0.586610973, 0.114478);

float Curve(float x)
{
   return tex2D(ColorTableTexture, float2(x, 0)).x;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float4 scene = tex2D(Texture, uv);
   const float3 gradient = saturate((1.0 - uv.y * g_AddColor.w) * g_AddColor.xyz);
   float4 result;
   const bool hdr = g_LumaGrade.z > 0.5;
   [branch] if (hdr || g_LumaGrade.y != 1.0)
   {
      // Sample() clamps its input as the sampler clamped U, so this is the curve of the clipped scene
      float3 curved = float3(PlatinumToneCurve::Sample(ColorTableTexture, scene.r), PlatinumToneCurve::Sample(ColorTableTexture, scene.g),
                             PlatinumToneCurve::Sample(ColorTableTexture, scene.b));
      const float vanilla_luma = dot(curved, kLumaWeights);
      // Alpha from the vanilla curve, clamped as by the 8-bit target: the bloom source (filter05) thresholds on it
      result.a = saturate(scene.a * g_Saido.w + vanilla_luma - g_MatrialColor.w);
      float luma = vanilla_luma;
      // The curve's input, as the curve reads it
      float3 color = saturate(scene.rgb);
      [branch] if (hdr)
      {
         // ps_3_0 has no asuint for IsNaN_Strict, and fxc folds a (x >= 0 ? x : 0) select into this max: NaN to 0 relies on the
         // hardware's max returning the non-NaN operand (D3D10+ rule, not guaranteed by D3D9); +inf is kept as in the D3D11 mod
         color = max(scene.rgb, 0.0);
         // Every pivot is at least a window up, so the fit is skipped where no channel can pass it
         [branch] if (max(color.r, max(color.g, color.b)) > PlatinumToneCurve::kWindow)
         {
            curved = PlatinumToneCurve::Apply(PlatinumToneCurve::FitTangent(ColorTableTexture), color, curved);
            luma = dot(curved, kLumaWeights);
         }
      }
      // "Color Grading Intensity" fades the level's look: the curve toward identity, the saturation (an offset from the colour, 0 = none)
      // and the gradient toward none. The g_MatrialColor gain stays: it carries the auto exposure. Alpha keeps the vanilla curve.
      float3 saturation = g_Saido.xyz;
      float3 gradient_tint = gradient;
      [branch] if (g_LumaGrade.y != 1.0)
      {
         curved = lerp(color, curved, g_LumaGrade.y);
         luma = dot(curved, kLumaWeights);
         saturation *= g_LumaGrade.y;
         gradient_tint *= g_LumaGrade.y;
      }
      result.rgb = ((curved - luma) * saturation + curved) * (g_MatrialColor.xyz * g_LumaGrade.x) + gradient_tint;
      // The 8-bit target floored the saturation's negatives and, in SDR, clipped the top
      result.rgb = hdr ? max(result.rgb, 0.0) : saturate(result.rgb);
      return result;
   }
   const float3 curved = float3(Curve(scene.r), Curve(scene.g), Curve(scene.b));
   const float luma = dot(curved, kLumaWeights);
   result.rgb = ((curved - luma) * g_Saido.xyz + curved) * (g_MatrialColor.xyz * g_LumaGrade.x) + gradient;
   result.a = scene.a * g_Saido.w + luma - g_MatrialColor.w;
   return result;
}
