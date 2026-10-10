#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User controls, drawn in DrawImGuiSettings (main.cpp). Read in TdToneMapping_0x1A760388.ps_5_0.hlsl unless noted.
// Exposure, ColorGradingIntensity and Dithering act on the vanilla SDR path too, the other grade fields on the HDR path only.
struct LumaGameSettings
{
   float Exposure;               // 1 = vanilla. Multiplier on the scene, before the grade.
   float Contrast;               // 1 = vanilla. Around 18% mid-gray, before the display map.
   float Saturation;             // 1 = vanilla. After the display map.
   float HighlightsDesaturation; // 0 = off. DICE highlight desaturation above a third of peak.
   float ColorGradingIntensity;  // 1 = vanilla, 0 = neutral. Strength of the per-channel color curves.
   float Dithering;              // 0/1. Output dither against banding (8 bit SDR, 10 bit PQ HDR).
   float VideoAutoHDREnable;     // 0/1. PumboAutoHDR on the Bink passes (Video_0xE41621CF.ps_5_0.hlsl).
   float VideoAutoHDRBoost;      // 0..1. Video peak = lerp(SDR white, 250 nits, boost).
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
