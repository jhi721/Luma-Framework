// ps_render_fb, every game: the screen fade/tint, the last pass before the present blit. Vanilla decodes with a
// 2.2 power, multiplies by the tint in linear light and re-encodes: pow(pow(x, 2.2) * tint, 1/2.2). On the fp16 chain
// its input can exceed 1 or carry small negatives (bicubic resample ringing), and pow() of a negative is NaN, so the
// transfer mirrors the sign.
#include "Common.hlsl"

#ifndef RENDER_FB_SCALE_AND_GAMMA
#define RENDER_FB_SCALE_AND_GAMMA 0
#endif

cbuffer cb5 : register(b5)
{
   float4 cb5[1];
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

float4 RenderFB(float2 uv)
{
   float4 result;
   float4 color = t0.Sample(s0_s, uv);
#if RENDER_FB_SCALE_AND_GAMMA
   // Y5R: a uniform scale (x) and a power (z), both on linear light.
   float3 linearColor = safePow(gamma_to_linear(color.rgb, GCT_MIRROR) * cb5[0].x, cb5[0].z);
#else
   float3 linearColor = gamma_to_linear(color.rgb, GCT_MIRROR) * cb5[0].rgb;
#endif
   // Vanilla's input never passed 1, so a brightening tint (a scale or power above 1, e.g. Y5R's hit flashes) never passed
   // white. The HDR input reaches the peak already, and nothing tonemaps after this pass: the brightest channel is held at
   // the peak (the composition scales this image by UIPaperWhite), keeping the hue.
   if (LumaSettings.DisplayMode != 0)
   {
      const float peak = LumaSettings.PeakWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
      linearColor *= min(1.0, peak / max(max3(linearColor), 1e-6));
   }
   result.rgb = linear_to_gamma(linearColor, GCT_MIRROR);
   result.a = color.a;

   // Anti-banding dither on the last pass (after CAS and the HUD), one step of the output quantizer: the 8-bit code in
   // SDR, 10-bit BT.2020 PQ in HDR. The composition scales this image by UIPaperWhite, so that is the PQ scale.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(result.rgb, uv, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(result.rgb, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         result.rgb = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
   return result;
}
