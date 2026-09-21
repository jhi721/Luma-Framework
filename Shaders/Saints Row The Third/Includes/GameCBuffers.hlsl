#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User settings, mirrored to c++ (defaults / ImGui in main.cpp) and read through LumaSettings.GameSettings.
struct LumaGameSettings
{
   float Dithering;             // 0/1. Animated triangular dither on the rl_hdr finals' output (HDR path).
   float VideoAutoHDREnable;    // 0/1. Light AutoHDR on Bink videos (Video_0xC917553E); no-op in SDR.
   float VideoAutoHDRBoost;     // 0..1 highlight-expansion strength. 0 = peak at paper white (off).
   float BloomIntensity;        // 1 = vanilla. Scales the finals' bloom term, native or Luma.
   float Exposure;              // 1 = vanilla. Scales the finals' tinted scene and bloom.
   float Saturation;            // 1 = vanilla. HDR path only.
   float ColorGradingIntensity; // 1 = vanilla LUT, 0 = its identity (the plain shoulder).
   float Contrast;              // 1 = vanilla. Multiplicative contrast around 18% mid-gray before the display map. HDR path only.
   float HighlightDechroma;     // 0 = off. DICE highlight desaturation: sources above a third of peak fade toward white. HDR path only.
};

// Game specific cbuffer (instance/pass) data.
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
