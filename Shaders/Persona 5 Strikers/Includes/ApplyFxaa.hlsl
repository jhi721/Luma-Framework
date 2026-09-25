// Katana engine PostEffect3 "ApplyFxaa*PS": optional FXAA over the composite's output, drawn into the swapchain. Never seen drawn;
// presumably used when the engine's own FXAA or its radial blur is on. The four FXAA types (ApplyFxaaPS, ApplyFxaaRepairPS,
// ApplyFxaaConsolePS, ApplyFxaaQualityPS) differ only in the antialiasing; this is their shared tail: the radial blur (it only exists
// here, not in the composite), the output power curve (g_vGammaCorrection) and the fade.

#include "cbComposite.hlsl"

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_tSceneMap : register(t0);

// 3Dmigoto declarations
#define cmp -

float3 ApplyRadialBlurGammaFade(float3 color, float2 uv)
{
   [branch] if (g_vRadialBlurInfo.x != 0.0)
   {
      // Taps towards the blur center, weighted by pow(1 - i / steps, g_vRadialBlurInfo.w), stopping once a tap weighs under 1/255 of the sum
      float2 blurStep = uv * 2.0 - 1.0 - g_vRadialBlurCenter.xy;
      blurStep = -blurStep * exp2(log2(dot(blurStep, blurStep)) * g_vRadialBlurInfo.y) * g_vRadialBlurInfo.x;
      const int steps = min(max(1, (int)length(g_vSceneTexSize.xy * blurStep)), (int)g_vRadialBlurInfo.z);
      blurStep /= (float)steps;
      float3 sum = color;
      float weightSum = 1.0;
      float2 tapUV = uv;
      [loop] for (int i = 1; i < steps; ++i)
      {
         tapUV += blurStep;
         const float weight = exp2(log2(saturate(1.0 - i / (float)steps)) * g_vRadialBlurInfo.w);
         sum += g_tSceneMap.SampleLevel(sampleLinear_s, tapUV, 0).rgb * weight;
         weightSum += weight;
         if (weight < weightSum / 255.0)
            break;
      }
      color = sum / weightSum;
   }
   if (!P5S_HDR_SCENE && g_vGammaCorrection.y != 1.0) // Luma: the output power curve only applies to SDR, Luma has its own
      color = exp2(log2(abs(color)) * g_vGammaCorrection.y);
   return color * g_vRadialBlurCenter.w;
}
