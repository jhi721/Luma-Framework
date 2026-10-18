// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, as Mass Effect 2007's (same dgVoodoo stack), on top of the game's MSAA (or alone with AA
// off). Run by main.cpp on the canvas right after the scene copy (ResetRender, already display mapped), before the UI; Display
// Composition runs after. Edges are detected on the GAMMA canvas, the neighborhood blend filters it in linear light (below) and
// re-encodes. The predication mask comes from Luma_Bayonetta_DepthEdges.

// "SMAA_T2X" 1 is SMAA T2x (main.cpp's "Bayonetta SMAA T2x" passes, Mirror's Edge's design): the scene jitters by a quarter pixel
// diagonally in two phases, the blending weights take the phase's area texture subsamples, the neighborhood blend stays in linear
// light with the velocity length in alpha, and the resolve blends it with the previous frame reprojected by Luma's motion vectors.
// The output hook below runs at the resolve.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

#include "Includes/Common.hlsl"

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp "DrawSMAAOnCanvas").
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active, 1.0 with a null predication texture
   // (fallback) -> plain ULTRA threshold 0.05. yzw unused.
   float4 SmaaPredication;
#if SMAA_T2X
   float4 SmaaSubsampleIndices; // The jitter phase's "SMAA.hlsl" @SUBSAMPLE_INDICES, 0 when the scene didn't jitter
#endif
}

#if SMAA_T2X
#define SMAA_SUBSAMPLE_INDICES SmaaSubsampleIndices
#define SMAA_REPROJECTION      1
// The motion vector target ("mv_texture" in main.cpp) holds the UV delta from the current to the previous position. SMAA's velocity
// is the opposite.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#endif

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

// SMAAEdgeDetection
// tex0 = colorTexGamma (gamma-encoded graded canvas)
// tex1 = predicationTex (edge-ness in [0,1]; null fallback -> reads 0, scale 1.0 = plain ULTRA threshold)

// SMAANeighborhoodBlending
// tex0 = colorTex (the same snapshot, filtered in linear light), tex1 = blendTex. Re-encode to the canvas' gamma; the alpha is
// not the canvas' (T2x's velocity length at the resolve): main.cpp writes RGB only.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);

#include "../Includes/SMAA_Passes.hlsl"
