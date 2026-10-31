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
   float VideoAutoHDREnable;     // Video_0x43A3099D: 0/1. 1 = light PumboAutoHDR on the CRI movies (HDR only).
   float VideoAutoHDRBoost;      // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   float Dithering;              // 0/1. Animated triangular dither at output (Present_0x880A17D3) to break gradient banding.
   float Contrast;               // HDR (PsychoV31 in Gather_0x726E6E78), 1 = vanilla
   float Saturation;             // HDR, 1 = vanilla
   float HighlightsDesaturation; // HDR (PsychoV "dechroma"), 0 = off
   float ColorGradingIntensity;  // Gather_0x726E6E78: 1 = vanilla, 0 = no curve, saturation or gradient
   float BloomIntensity;         // Gather_0x726E6E78 on the bloom it adds: 1 = vanilla, 0 = none
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};

// The motion vector fill's constants (Luma_MGRR_MotionVectorFill.hlsl, b0), written by "DrawUpscaler"
struct MotionVectorFillConstants
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's (column vectors)
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled; // 0/1
   float3 padding;
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
