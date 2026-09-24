#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// The Luma Bloom prefilter and glow_pass0 targets: main.cpp creates them, Luma_YRC_Bloom/GlowGain.hlsl address them.
#define YRC_GLOW_PREFILTER_WIDTH  1024
#define YRC_GLOW_PREFILTER_HEIGHT 512

// The Y5R material tone curve's tangent continuation: main.cpp patches it in (PatchY5MaterialToneCurve), Common.hlsl maps
// it back (YRC_Y5VanillaMaterialCurve). The slope is exp(-pivot). The curve is 1 - exp(-u) in linear light, concave with no
// inflection, so any pivot is valid and a lower one is brighter: 0.18, the pivot RenoDX Breath of the Wild uses on the same
// curve (0.165 of white).
#define YRC_Y5_MATERIAL_CURVE_PIVOT 0.18f
#define YRC_Y5_MATERIAL_CURVE_SLOPE 0.835270211f

// Mirrors c++ name spaces.
namespace CB
{
// Define the game specific cbuffer settings here. They don't need 4 bytes alignment.
struct LumaGameSettings
{
   float VideoAutoHDREnable; // 0/1. Light AutoHDR on the Sofdec videos (HDR only).
   float VideoAutoHDRBoost;  // 0..1 highlight-expansion strength. 0 = peak at sRGB white (off).
   float Dithering;          // 0/1. One output code value of noise on the final image (8-bit SDR, 10-bit PQ HDR).
   float BloomIntensity;     // Luma Bloom strength (1 = vanilla, 0 = none).
   float Exposure;           // 1 = vanilla. Linear multiplier on the graded HDR color, before the display map.
   float Contrast;           // 1 = vanilla. Power around 18% mid-gray on the graded HDR color, before the display map.
   float Saturation;         // 1 = vanilla. Luminance-relative saturation multiplier on the final HDR color.
   float HighlightDechroma;  // 0 = off (default). DICE highlight desaturation above a third of peak.
};

// Define the game specific cbuffer (instance/pass) data here
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
