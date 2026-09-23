#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
#include "../../../Source/Core/includes/shader_types.h"
#endif

namespace CB
{
// Mirrored from main.cpp. Exposure acts in SDR and HDR (tonemap input); Saturation and HighlightsDesaturation only on
// the HDR display path; Dithering in both compose passes.
struct LumaGameSettings
{
   float Exposure;               // 1 = vanilla. Multiplies the scene before the tone curve.
   float Saturation;             // 1 = vanilla. BT.709-luminance saturation after the display map.
   float HighlightsDesaturation; // 0 = off. DICE highlight desaturation.
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
