#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
struct LumaGameSettings
{
   float Dithering;              // 0/1. Animated triangular dither at output (Present_0x880A17D3) to break gradient banding.
   float VideoAutoHDREnable;     // Video_0xC60649FD: 0/1. 1 = light PumboAutoHDR on the Sofdec movies (HDR only).
   float VideoAutoHDRBoost;      // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   float Contrast;               // HDR (PsychoV31 in Copy_0x91A6C437), 1 = vanilla
   float Saturation;             // HDR, 1 = vanilla
   float HighlightsDesaturation; // HDR (PsychoV "dechroma"), 0 = off
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
