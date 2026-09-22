#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// The Luma Bloom prefilter and glow_pass0 targets: main.cpp creates them, Luma_YRC_Bloom/GlowGain.hlsl address them.
#define YRC_GLOW_PREFILTER_WIDTH  1024
#define YRC_GLOW_PREFILTER_HEIGHT 512

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
};

// Define the game specific cbuffer (instance/pass) data here
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
