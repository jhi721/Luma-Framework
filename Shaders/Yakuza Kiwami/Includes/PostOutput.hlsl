#ifndef SRC_YKR_POST_OUTPUT_HLSL
#define SRC_YKR_POST_OUTPUT_HLSL

// The last passes after the grade: render_fb (the brightness/fade and white balance, then the present blit) and the Sofdec videos.
// Both replacements are the decompiled originals verbatim, with their input and output wrapped here.
#include "Common.hlsl"

// render_fb decodes with log2/pow: on the fp16 chain a stray negative would become a NaN, the UNORM input never carried one
float4 YKR_RenderFBInput(float4 color)
{
   return max(color, 0.0);
}

// Vanilla's input never passed 1, so a brightening fade or "Screen Brightness" never passed white. The HDR input reaches the peak
// already and nothing tonemaps after this pass: the brightest channel is held at the peak (the composition scales this image by
// UIPaperWhite), keeping the hue. Then the anti-banding dither, one step of the output quantizer (8-bit SDR, 10-bit PQ HDR).
float4 YKR_RenderFBOutput(float4 result, float2 uv)
{
   float3 linear_color = gamma_to_linear(result.rgb, GCT_MIRROR);
   if (LumaSettings.DisplayMode != 0)
   {
      const float peak = LumaSettings.PeakWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
      linear_color *= min(1.0, peak / max(max3(linear_color), 1e-6));
   }
   result.rgb = linear_to_gamma(linear_color, GCT_MIRROR);
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(result.rgb, uv, true, LumaSettings.DisplayMode != 0, LumaSettings.UIPaperWhiteNits);
   }
   return result;
}

// The videos' gamma-space Y'CbCr->R'G'B' result -> the post-process space the HUD stage expects. Drawn in the HUD stage, so
// encoded like the scene to land at game paper white.
float4 YKR_VideoOutput(float4 color)
{
   // The Y'CbCr->R'G'B' conversion overshoots [0,1]; the vanilla UNORM target clipped it.
   float3 linear_color = gamma_to_linear(saturate(color.rgb));
   // No-op in SDR, where the peak is paper white.
   linear_color = VideoAutoHDR(linear_color, LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   return float4(YKR_EncodeOutput(linear_color), color.a);
}

#endif // SRC_YKR_POST_OUTPUT_HLSL
