#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// Define the game specific cbuffer settings here. They don't need 4 bytes alignment.
struct LumaGameSettings
{
   float Dithering;          // 0/1. Animated triangular dither at output (Present_0x880A17D3) to break gradient banding.
   float VideoAutoHDREnable; // Video_0x72C37F0F: 0/1. 1 = light PumboAutoHDR on the Bink movie pass (HDR only).
   float VideoAutoHDRBoost;  // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
};

// Define the game specific cbuffer (instance/pass) data here
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
