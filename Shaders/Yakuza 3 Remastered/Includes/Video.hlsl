// Shared output of the Sofdec video replacements (Video_0xC9782177, VideoLegacy_0xFD02F404).
#include "Common.hlsl"

// Video AutoHDR peak at full boost.
static const float VideoAutoHDRPeakNits = 250.0;

// Takes the gamma-space YUV->RGB result and returns it in the post-process space the UI stage expects.
float3 VideoToOutput(float3 color)
{
   // The YUV->RGB conversion overshoots [0,1]; the vanilla UNORM target clipped it.
   float3 lin = gamma_to_linear(saturate(color));
   // Boost 0 = peak at sRGB white, where PumboAutoHDR no-ops; it also no-ops in SDR, where the peak is paper white. Kept
   // light because the videos are low bitrate and compression artifacts blow up when pushed hard.
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      const float peakNits = lerp(sRGB_WhiteLevelNits, VideoAutoHDRPeakNits, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      lin = PumboAutoHDR(lin, peakNits, LumaSettings.GamePaperWhiteNits);
   }
#if UI_DRAW_TYPE >= 2
   // Drawn in the UI stage, so pre-scaled like the scene (see ColorCorrect.hlsl) to land at game paper white.
   lin *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return linear_to_gamma(lin);
}
