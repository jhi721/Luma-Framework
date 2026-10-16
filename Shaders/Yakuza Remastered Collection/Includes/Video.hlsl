// Shared output of the Sofdec video replacements (Video_0xC9782177, VideoLegacy_0xFD02F404, VideoH264_0xB09E517F).
#include "Common.hlsl"

// Takes the gamma-space Y'CbCr->R'G'B' result and returns it in the post-process space the UI stage expects.
float3 VideoToOutput(float3 color)
{
   // The Y'CbCr->R'G'B' conversion overshoots [0,1]; the vanilla UNORM target clipped it.
   float3 lin = gamma_to_linear(saturate(color));
   // No-op in SDR, where the peak is paper white.
   lin = VideoAutoHDR(lin, LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   // Drawn in the UI stage, so encoded like the scene to land at game paper white.
   return YRC_EncodeOutput(lin);
}
