#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User-facing grade controls, drawn in DrawImGuiSettings (main.cpp) and read in Luma_MOH_Tonemap.hlsl.
// All apply only on the HDR tonemap path.
struct LumaGameSettings
{
   float Exposure;          // exposure multiplier (1 = vanilla). Applied scene-referred, before the vanilla grade.
   float Saturation;        // 1 = vanilla. Saturation multiplier on the final HDR color (lerp against luminance).
   float HighlightDechroma; // 0 = off (default; keep color, only mandatory gamut desat applies); higher = bright sources fade to white sooner.
   float Contrast;          // 1 = vanilla. Slope contrast around 18% mid-gray on the final HDR color.
   float Dithering;         // 0/1 toggle. Animated triangular dither at output to break gradient banding.
   // hueStrength / chrominanceStrength of the vanilla highlight emulation (RestoreHueAndChrominance). Read ONLY
   // by the legacy DevSetting04 A/B in Luma_MOH_Tonemap.hlsl; the shipped max-channel path preserves the vanilla
   // clip's hue by construction and needs neither. Both fields go when that A/B is settled.
   float HighlightsHueStrength; // DEV A/B only
   float HighlightsHueChroma;   // DEV A/B only
   // Luma HDR bloom (ENABLE_BLOOM). One switch swaps one bloom for the other: the tonemap adds the Luma pyramid
   // in linear scene light, and the game's own bright pass + composite stand down.
   float LumaBloomEnable; // 0/1. 1 = Luma pyramid, 0 = the engine's gamma-space glow (also the TONEMAP_TYPE 0 path)
   float BloomIntensity;  // 1 = the pyramid's natural strength, 0 = none. Luma bloom only
   float BloomThreshold;  // linear SCENE brightness where the glow starts. Scene units, NOT paper white
   // Read by the tonemap to stand the engine's own EdgeAA down: it is a directional blur, and stacking it under
   // the SMAA pass only smears. Everything else about SMAA lives C++-side (main.cpp) and in its own cbuffers.
   float SMAAEnable; // 0/1
   // Appended, never reordered: this struct is a C++/HLSL ABI mirror.
};

// Game specific cbuffer (instance/pass) data.
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
