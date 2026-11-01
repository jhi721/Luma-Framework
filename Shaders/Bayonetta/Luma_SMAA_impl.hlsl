// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, as Mass Effect 2007's (same dgVoodoo stack), on top of the game's MSAA (or alone with AA
// off). Run by main.cpp on the canvas right after the scene copy (ResetRender, already display mapped), before the UI; Display
// Composition runs after. Edges are detected on the gamma encoded canvas, the neighborhood blend filters it in linear light (below) and
// re-encodes. The predication mask comes from Luma_Bayonetta_DepthEdges.

// "SMAA_T2X" 1 is SMAA T2x (main.cpp's "Bayonetta SMAA T2x" passes, Mirror's Edge's design): the scene jitters by a quarter pixel
// diagonally in two phases, the blending weights take the phase's area texture subsamples, the neighborhood blend stays in linear
// light with the velocity length in alpha, and the resolve blends it with the previous frame reprojected by Luma's motion vectors.
// The output hook below runs at the resolve.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

#include "Includes/Common.hlsl"

cbuffer SmaaMetricsCB : register(b1)
{
   CB::SmaaMetrics smaa_metrics;
}

#if SMAA_T2X
#define SMAA_SUBSAMPLE_INDICES smaa_metrics.subsample_indices
#define SMAA_REPROJECTION      1
// The motion vector target ("mv.texture" in main.cpp) holds the UV delta from the current to the previous position. SMAA's velocity
// is the opposite.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#endif

#define SMAA_RT_METRICS smaa_metrics.rt_metrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE smaa_metrics.predication.x
// Predication is live (the compute shader's mask, scale 2), so both values below matter: the flat threshold is SCALE *
// SMAA_THRESHOLD = 0.10 and the silhouette one is flat * (1 - STRENGTH) = 0.05 (the ULTRA base). SMAA_PREDICATION_THRESHOLD 0.5
// doesn't depend on the scale, so tune the compute shader instead.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1

// SMAAEdgeDetection
// tex0 = colorTexGamma (gamma-encoded graded canvas)
// tex1 = predicationTex (edge-ness in [0,1]; a null texture reads 0, with scale 1 for the plain ULTRA threshold)

// SMAANeighborhoodBlending
// tex0 = colorTex (the same snapshot, filtered in linear light), tex1 = blendTex. Re-encode to the canvas' gamma; the alpha is
// not the canvas' (T2x's velocity length at the resolve): main.cpp writes RGB only.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);

#include "../Includes/SMAA_Passes.hlsl"
