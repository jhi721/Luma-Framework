#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
#include "../../../Source/Core/includes/shader_types.h"
#endif

namespace CB
{
// User controls, drawn in DrawImGuiSettings (main.cpp) and read by the ME3 shaders. A C++/HLSL ABI: append,
// never reorder.
struct LumaGameSettings
{
   float Exposure;           // 1 = vanilla. Scene-referred multiplier, pre-grade.
   float Saturation;         // 1 = vanilla. Luminance-relative lerp on the final HDR colour, after the display map.
   float HighlightDechroma;  // 0 = off (default). DICE HighlightsDesaturation: max channel ramps toward white from a third of peak.
   float Contrast;           // 1 = vanilla. Multiplicative contrast around 18% mid-gray, before the display map.
   float VignetteIntensity;  // 1 = vanilla radial darkening, 0 = flat. The blue-tinted white point stays either way.
   float FilmGrainIntensity; // 1 = vanilla grain amplitude, 0 = off. Only the grain perms read it.
   float Dithering;          // 0/1. Animated triangular output dither.
   float VideoAutoHDREnable; // 0/1. Light PumboAutoHDR on Bink movies (Video_0xE41621CF). 0 = flat SDR at paper white.
   float VideoAutoHDRBoost;  // 0..1 highlight strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   float BloomIntensity;     // 1 = vanilla strength. Scales the Luma pyramid only.
   float LumaBloomEnable;    // 0/1. 1 = the Luma HDR pyramid REPLACES the game's bloom (the gather replacement stops writing it).
   float BloomThreshold;     // Luma_Bloom_impl.hlsl: linear scene brightness where bloom starts. 1.0 = the game's bright-pass.
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
