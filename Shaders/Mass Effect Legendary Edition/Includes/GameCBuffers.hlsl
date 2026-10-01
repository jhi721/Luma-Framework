#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// Expose HLSL-compatible types to C++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors the C++ namespace.
namespace CB
{
// C++/HLSL ABI for user controls and per-draw video classification. Preserve field order and alignment.
struct LumaGameSettings
{
   float Exposure;           // 1 = vanilla. Scene exposure multiplier, scene-referred / pre-grade.
   float Saturation;         // 1 = vanilla. Saturation multiplier (lerp from relative luminance) on the final HDR color, after DICE.
   float HighlightDechroma;  // 0 = off (default). DICE HighlightsDesaturation: lerps toward relative luminance, ramped on the max channel from a third of peak to peak, only once the channel average passes that shoulder.
   float Contrast;           // 1 = vanilla. Multiplicative contrast around 18% mid-gray, applied before the display map.
   float VignetteIntensity;  // 1 = vanilla. Scales the game's vignette darkening (0 = none; the native white-point tint stays).
   float FilmGrainIntensity; // 1 = vanilla. Scales the game's film grain (0 = off).
   float BloomIntensity;     // Written by C++: Bloom Intensity slider * live native BloomScale (0 = no bloom); 1 while Luma bloom is off.
   float BloomThreshold;     // = native bright-pass cb0.y (per-scene artist dial), captured live; 1.2 = ME1LE vanilla default until first readback.
   float Dithering;          // 0/1 toggle. Animated triangular output dither in HDR and SDR.
   float VideoAutoHDREnable; // 0/1 toggle. 1 = expand Bink movie highlights into HDR, 0 = vanilla SDR videos (no expansion).
   float VideoAutoHDRBoost;  // 0..1. Bink highlight range relative to UI white: 0 = 1x/no-op, 1 = up to 3.125x. Default 0.5.
   float VideoOnSwapchain;   // Set by C++ per Bink draw: 1 = it targets a swapchain back buffer, 0 = any other target.
};

// Game-specific per-pass cbuffer data.
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
