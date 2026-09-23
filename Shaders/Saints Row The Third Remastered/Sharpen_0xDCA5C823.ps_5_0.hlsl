#include "../Includes/RCAS.hlsl"
#include "Includes/Common.hlsl"

// anamorph_sharpen (display.ini "Sharpen"; the game skips the pass at 0). Vanilla sharpens the final image with an
// overlay blend of a luma high-pass, which assumes colours in [0,1] and inverts its sign above 1, so it breaks on the
// HDR image. Replaced by RCAS on the same input, keeping the vanilla strength and optional radial mask.
// ponytail: strength is fed to RCAS 1:1 (saturated); the Sharpen slider's range has not been measured in game.

cbuffer CB_CUSTOM : register(b10)
{
   float4 cb10[3]; // ANAMORPH_SHARPEN_PARAMS
}

#define ImageSize cb10[0].xy
#define UseMask   cb10[0].z
#define Strength  cb10[1].x

Texture2D<float4> BackBuffer : register(t0);
Texture2D<float2> UnusedMotionVectors : register(t1); // RCAS only reads it with dynamic sharpening, which is off

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   float strength = Strength;
   if (UseMask > 0.0)
   {
      // Vanilla radial mask: 1 at the centre falling to 0 at the corners.
      const float2 centered = uv * 2.0 - 1.0;
      const float edge = 1.0 - length(centered);
      const float low = min(edge, 0.5);
      const float high = max(edge, 0.5);
      strength *= (low * low + 2.0 * high - high * high) * 2.0 - 1.5;
   }

   // The input is gamma code with 1 = UI paper white (UI_DRAW_TYPE 2). RCAS's overshoot limiter assumes a peak of 1,
   // so in HDR it is normalized to the display peak in the same encoding.
   const float peak = LumaSettings.DisplayMode == 1 ? pow(LumaSettings.PeakWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0), 1.0 / DefaultGamma) : 1.0;
   const int2 maxPixel = int2(ImageSize) - 1;
   return float4(RCAS(int2(pos.xy), 0, maxPixel, saturate(strength), BackBuffer, UnusedMotionVectors, peak).rgb, 1.0);
}
