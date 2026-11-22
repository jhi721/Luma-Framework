#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User controls, drawn in DrawImGuiSettings (main.cpp). The display map ones are read where the scene ends (Includes/DisplayMap.hlsl).
struct LumaGameSettings
{
   float Exposure;               // 1 = vanilla. Multiplier on the scene in linear, before the display map (SDR too, before its clip).
   float Contrast;               // 1 = vanilla. Power around mid gray on BT.709 relative luminance, before the HDR display map.
   float Saturation;             // 1 = vanilla. Scale of the RGB residual from BT.709 relative luminance ("Saturation()": lerp(Y, rgb, s)), before the HDR display map.
   float HighlightsDesaturation; // 0 = off. MacLeod-Boynton purity weight of the hue reference's whitening.
   float ColorGradingIntensity;  // 1 = vanilla, 0 = no LUT. Scales the uber post's LUT amount (read by the patched uber, "WrapUberLut" in UberLutPatch.h).
   float Dithering;              // 0/1. Output dither against banding (8 bit SDR, 10 bit PQ HDR).
   float VideoAutoHDREnable;     // 0/1. PumboAutoHDR on the video logs (Video_0xD49CB77E.ps_5_0.hlsl).
   float VideoAutoHDRBoost;      // 0..1. Video peak = lerp(1, 250 / 80, boost) x the game paper white (PumboAutoHDR's nits are relative to 80), at most the display peak.
   float GammaRampExponent;      // 1 = vanilla default. Not a user control: the game's brightness slider (its SetGammaRamp, hooked in main.cpp), on the scene canvas.
};

// Define the game specific cbuffer (instance/pass) data here
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};

// The motion vector fill's constants (Luma_DS2_MotionVectorFill.hlsl, b0), written by "FillMotionVectors"
struct MotionVectorFillConstants
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's (column vectors)
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled; // 0/1
   float depth_scale;      // 1 / the projection's z scale: the engine's custom Z range puts infinity at ~0.99, the upscalers expect 1
   float2 padding;
};

// SMAA's metrics (Luma_SMAA_impl.hlsl, VS+PS b1), written by "RunSMAA"
struct SmaaMetrics
{
   float4 rt_metrics;        // (1/W, 1/H, W, H) of the antialiased frame
   float4 predication;       // x = predication threshold scale (2 with predication, 1 without); y = the frame's encoding: 1 sRGB, 0 gamma 2.2
   float4 subsample_indices; // SMAA T2x's jitter phase "SMAA.hlsl" @SUBSAMPLE_INDICES, 0 when the scene didn't jitter (unread without T2x)
};

// The SMAA predication signal's constants (Luma_DS2_DepthEdges.hlsl, b0)
struct DepthEdgesConstants
{
   float tolerance;   // Plane deviation counted as a full edge, as a fraction of view depth
   float depth_scale; // 1 / the projection's A (see "MotionVectorFillConstants")
   float near_plane;  // View depth = near / (1 - device depth * depth_scale)
   float padding;
};
} // namespace CB

#ifdef __cplusplus
static_assert(sizeof(CB::MotionVectorFillConstants) == 96);
static_assert(sizeof(CB::SmaaMetrics) == 48);
static_assert(sizeof(CB::DepthEdgesConstants) == 16);
#endif

#endif // LUMA_GAME_CB_STRUCTS
