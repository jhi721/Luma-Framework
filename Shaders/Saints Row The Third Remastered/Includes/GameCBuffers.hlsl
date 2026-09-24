#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
#include "../../../Source/Core/includes/shader_types.h"
#endif

namespace CB
{
// Mirrored from main.cpp. Exposure and ColorGradingIntensity act in SDR and HDR; Contrast, Saturation and
// HighlightsDesaturation only on the HDR display path; Dithering in both compose passes.
struct LumaGameSettings
{
   float Exposure;               // 1 = vanilla. Multiplies the scene before the tone curve.
   float Contrast;               // 1 = vanilla. Multiplicative contrast around 18% mid-gray, before the display map.
   float Saturation;             // 1 = vanilla. BT.709-luminance saturation after the display map.
   float HighlightsDesaturation; // 0 = off. DICE highlight desaturation.
   float ColorGradingIntensity;  // 1 = vanilla. Blends the grade (TintColor.w saturation + LUT) with its input code.
   float Dithering;              // 0/1. Output dither in the compose passes.
   float VideoAutoHDREnable;     // 0/1. AutoHDR on the Bink videos (no-op in SDR).
   float VideoAutoHDRBoost;      // 0-1. Video AutoHDR peak, from SDR white to its maximum.
   float HideGameplayUI;         // 0/1. The scene compose skips the GUI layer (menus keep it). Not saved.
};

struct LumaGameData
{
   float Dummy;
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
