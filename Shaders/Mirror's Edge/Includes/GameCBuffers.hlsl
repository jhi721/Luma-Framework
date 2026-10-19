#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User controls, drawn in DrawImGuiSettings (main.cpp). Read in TdToneMapping_0x1A760388.ps_5_0.hlsl unless noted.
// Exposure, ColorGradingIntensity and Dithering act on the vanilla SDR path too, the other grade fields on the HDR path only.
struct LumaGameSettings
{
   float Exposure;               // 1 = vanilla. Multiplier on the scene, before the grade.
   float Contrast;               // 1 = vanilla. PsychoV31 contrast (its cone response power) in the HDR display map.
   float Saturation;             // 1 = vanilla. PsychoV31 saturation (its purity) in the HDR display map.
   float HighlightsDesaturation; // 0 = off. PsychoV31 highlights desaturation (its "dechroma") in the HDR display map.
   float ColorGradingIntensity;  // 1 = vanilla, 0 = neutral. Strength of the per-channel color curves.
   float Dithering;              // 0/1. Output dither against banding (8 bit SDR, 10 bit PQ HDR).
   float VideoAutoHDREnable;     // 0/1. PumboAutoHDR on the Bink passes (Video_0xE41621CF.ps_5_0.hlsl).
   float VideoAutoHDRBoost;      // 0..1. Video peak = lerp(SDR white, 250 nits, boost).
};

struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};

// The motion vector fill's constants (Luma_ME_MotionVectorFill.hlsl, b0), written by "FillMotionVectors"
struct MotionVectorFillConstants
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 depth_from_view;          // The projection's depth row: device depth = x + y / view depth
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled; // 0/1
   float padding;
};

// SMAA's metrics (Luma_SMAA_impl.hlsl, VS+PS b1), written by "RunPostTonemapSMAA"
struct SmaaMetrics
{
   float4 rt_metrics;        // (1/W, 1/H, W, H) at output resolution
   float4 predication;       // x = predication threshold scale: 2 with predication, 1 with a null predication texture (plain ULTRA 0.05)
   float4 subsample_indices; // SMAA T2x's jitter phase "SMAA.hlsl" @SUBSAMPLE_INDICES, 0 when the scene didn't jitter (unread without T2x)
};

// The SMAA predication signal's constants (Luma_ME_DepthExtract.hlsl, b0)
struct DepthExtractConstants
{
   float tolerance; // Plane deviation counted as a full edge, as a fraction of view depth
   float3 padding;
};
} // namespace CB

#ifdef __cplusplus
static_assert(sizeof(CB::MotionVectorFillConstants) == 96); // The HLSL cbuffers' sizes
static_assert(sizeof(CB::SmaaMetrics) == 48);
static_assert(sizeof(CB::DepthExtractConstants) == 16);
#endif

#endif // LUMA_GAME_CB_STRUCTS
