// SMAA implementation for Mass Effect: Andromeda (replaces the game's FXAA pass).
// Reference: https://github.com/iryoku/smaa

#include "../Includes/Common.hlsl"

// (1/W, 1/H, W, H) of the FXAA target (it follows the in-game Resolution Scale), see "ReplaceFXAAWithSMAA"
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 with the predication mask, 1.0 without one (plain ULTRA threshold 0.05). yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// With the mask (scale 2.0) both values below are load-bearing: flat = 2 * SMAA_THRESHOLD = 0.10, silhouette = x (1 - STRENGTH)
// = 0.05 (the ULTRA base). The mask is normalized to [0,1], so tune its tolerance ("SMAA Predication Tolerance"), not the threshold.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
