#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
#include "../../../Source/Core/includes/shader_types.h"
#endif

namespace CB
{
struct LumaGameSettings
{
   float Dithering;  // 0/1
   float SMAAEnable; // 0/1
   float RCASSharpness;
   float VignetteIntensity;      // 1 = vanilla, 0 = none
   float LensFlareIntensity;     // 1 = vanilla, 0 = off
   float Exposure;               // 1 = vanilla. Scene multiplier before the LUT (SDR + HDR)
   float ColorGradingIntensity;  // 1 = vanilla, 0 = the LUT's gray tone curve only, by luminance (SDR + HDR)
   float Saturation;             // 1 = vanilla. After the display map, HDR only
   float HighlightsDesaturation; // 0 = off. DICE highlight desaturation, HDR only
   float BloomIntensity;         // 1 = vanilla, 0 = none. Scales the bloom combine (SDR + HDR)
   float UncapBloom;             // 0/1. 1 = the bloom prefilter's 5.0 source cap removed
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
