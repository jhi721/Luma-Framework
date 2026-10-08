// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, ported from BL2/TPS and Witcher 2 (same dgVoodoo stack). ADDS antialiasing: the
// game ships none. Injected after the gamma pass 0x17CE0932, before the HUD; Display Composition runs after. Edges are
// detected on the GAMMA canvas, the neighborhood blend filters it in linear light (below) and re-encodes.
// The predication mask comes from Luma_ME1_DepthExtract.

#include "Includes/Common.hlsl"

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunPostFinalGradeSMAA).
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active, 1.0 with a null predication texture
   // (fallback) -> plain ULTRA threshold 0.05. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication is live (CS mask, SCALE 2.0), so both values below are load-bearing: flat = SCALE * SMAA_THRESHOLD =
// 0.10, silhouette = x (1-STRENGTH) = 0.05 (ULTRA base). SMAA_PREDICATION_THRESHOLD 0.5 is scale-free; tune the CS.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// Edge detection: tex0 = the gamma canvas snapshot, tex1 = the predication edge-ness in [0,1] (null: reads 0, with scale
// 1.0 the plain ULTRA threshold). Neighborhood blending: tex0 = the same snapshot (see the header), tex1 = blend weights.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
