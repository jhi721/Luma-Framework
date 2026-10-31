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
   float VideoAutoHDREnable;    // Video_0x43A3099D: 0/1. 1 = light PumboAutoHDR on the CRI movies (HDR only).
   float VideoAutoHDRBoost;     // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   float Dithering;             // 0/1. Animated triangular dither at output (Present_0x880A17D3) to break gradient banding.
   float ColorGradingIntensity; // Gather_0x726E6E78: 1 = vanilla, 0 = no curve, saturation or gradient
   float BloomIntensity;        // Gather_0x726E6E78 on the bloom it adds: 1 = vanilla, 0 = none
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

// SMAA's metrics (Luma_SMAA_impl.hlsl, VS+PS b1), written by "DrawSMAAOnScene"
struct SmaaMetrics
{
   float4 rt_metrics;        // (1/W, 1/H, W, H)
   float4 predication;       // x = predication threshold scale: 2 with predication, 1 with a null predication texture (plain ULTRA 0.05)
   float4 subsample_indices; // SMAA T2x's jitter phase "SMAA.hlsl" @SUBSAMPLE_INDICES, 0 when the scene didn't jitter (unread without T2x)
};
} // namespace CB

#ifdef __cplusplus
static_assert(sizeof(CB::SmaaMetrics) == 48);
#endif

#endif // LUMA_GAME_CB_STRUCTS
