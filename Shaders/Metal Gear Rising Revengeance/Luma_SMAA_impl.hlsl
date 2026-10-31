// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, Bayonetta's 1x form (same dgVoodoo stack), on top of the game's MSAA (or alone with it off).
// Run by main.cpp on the gather pass' snapshot (display mapped, gamma encoded), written back into the gather target before the UI.
// Edges are detected on the gamma snapshot, the neighborhood blend filters it in linear light and re-encodes. The predication mask
// comes from Luma_MGRR_DepthEdges.

#include "Includes/Common.hlsl"

cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics; // 1/W, 1/H, W, H
   // x = predication threshold scale: 2 with the predication mask, 1 with a null one (plain ULTRA threshold). yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication is live (CS mask, SCALE 2.0), so both values below are load-bearing: flat = SCALE * SMAA_THRESHOLD =
// 0.10, silhouette = flat * (1 - STRENGTH) = 0.05 (ULTRA base). SMAA_PREDICATION_THRESHOLD 0.5 is scale-free; tune the CS.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1

// SMAAEdgeDetection: tex0 = the gamma snapshot, tex1 = the predication edge-ness in [0,1] (null: reads 0, scale 1)
// SMAANeighborhoodBlending: tex0 = the same snapshot, filtered in linear light, tex1 = blendTex; re-encoded to the target's gamma
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);

#include "../Includes/SMAA_Passes.hlsl"
