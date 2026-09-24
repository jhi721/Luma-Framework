#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
#include "../../../Source/Core/includes/shader_types.h"
#endif

namespace CB
{
// Mirrored from main.cpp. Exposure, ColorGradingIntensity, VignetteIntensity and FilmGrainIntensity act in SDR and
// HDR; Contrast, Saturation and HighlightsDesaturation only on the HDR display path; Dithering in both compose passes.
struct LumaGameSettings
{
   float RCASSharpness;          // 0 = off. RCAS on the scene in compose, before grain and GUI.
   float Exposure;               // 1 = vanilla. Multiplies the scene before the tone curve.
   float Contrast;               // 1 = vanilla. Multiplicative contrast around 18% mid-gray, before the display map.
   float Saturation;             // 1 = vanilla. BT.709-luminance saturation after the display map.
   float HighlightsDesaturation; // 0 = off. DICE highlight desaturation.
   float ColorGradingIntensity;  // 1 = vanilla. Blends the grade (TintColor.w saturation + LUT) with its input code.
   float VignetteIntensity;      // 1 = vanilla, 0 = none. Scales the tonemap's VignetteAmount.
   float FilmGrainIntensity;     // 1 = vanilla, 0 = off. Scales the compose's NoiseLevel.
   float Dithering;              // 0/1. Output dither in the compose passes; replaces the vanilla SDR Bayer dither.
   float HideGameplayUI;         // 0/1. The scene compose skips the GUI layer (menus keep it). Not saved.
};

struct LumaGameData
{
   float Dummy;
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
