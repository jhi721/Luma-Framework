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
   float ColorGradingIntensity;  // Grade_0x719339A9 / GradeLuma_0xB7A08D4A: 1 = vanilla, 0 = no curve, saturation or gradient
   float BloomIntensity;         // Copy_0x91A6C437 on the bloom composite: 1 = vanilla, 0 = none
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};

// XeGTAO's runtime knobs (Luma_Bayonetta_XeGTAO.hlsl, b9), written by "RunXeGTAO"
struct GTAOKnobs
{
   float final_value_power;    // Darkness dial (higher = darker)
   float depth_scale;          // View depth divisor (game units -> ~meters)
   float radius_override;      // > 0 overrides EFFECT_RADIUS (view units after depth_scale)
   float debug_view;           // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl), shown alone by Luma_Bayonetta_XeGTAOApply.hlsl
   float2 viewport_pixel_size; // 1 / the depth's size
   float noise_index;          // XeGTAO's temporal noise: frame % 64 while DLSS/FSR accumulates the AO, else 0
   float camera_affine_row;    // b4's first row of the prepass draw's affine view transform (3 rows)
   float camera_matrix_row;    // b4's first row of the same transform followed by the projection (4 rows)
   float3 padding;
};

// The motion vector fill's constants (Luma_Bayonetta_MotionVectorFill.hlsl, b0), written by "FillMotionVectors"
struct MotionVectorFillConstants
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled; // 0/1
   float3 padding;
};

// SMAA's metrics (Luma_SMAA_impl.hlsl, VS+PS b1), written by "DrawSMAAOnCanvas"
struct SmaaMetrics
{
   float4 rt_metrics;        // (1/W, 1/H, W, H) at output resolution
   float4 predication;       // x = predication threshold scale: 2 with predication, 1 with a null predication texture (plain ULTRA 0.05)
   float4 subsample_indices; // SMAA T2x's jitter phase "SMAA.hlsl" @SUBSAMPLE_INDICES, 0 when the scene didn't jitter (unread without T2x)
};

// The SMAA predication signal's constants (Luma_Bayonetta_DepthEdges.hlsl, b0)
struct DepthEdgesConstants
{
   float tolerance;     // Plane deviation counted as a full edge, as a fraction of view depth
   float2 scene_origin; // The scene rect's origin on the canvas
   float padding;
};
} // namespace CB

#ifdef __cplusplus
static_assert(sizeof(CB::GTAOKnobs) == 48); // The HLSL cbuffers' sizes
static_assert(sizeof(CB::MotionVectorFillConstants) == 96);
static_assert(sizeof(CB::SmaaMetrics) == 48);
static_assert(sizeof(CB::DepthEdgesConstants) == 16);
#endif

#endif // LUMA_GAME_CB_STRUCTS
