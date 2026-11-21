// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA with colour edge detection, 1x or T2x (below), Vanquish's file. main.cpp ("RunSMAA") runs it in one of two places:
// MarkerPatch's SMAA slot (its frame copy, sRGB encoded: MarkerPatch read and wrote it through SRGBTEXTURE/SRGBWRITE) or the scene's
// end (gamma 2.2 encoded). "smaa_metrics.predication.y" picks the encoding: edges are detected on the encoded frame, while the
// neighborhood blend decodes it, filters in linear light and encodes it back. The predication mask comes from "Luma_DS2_DepthEdges.hlsl".

// "SMAA_T2X" 1 is SMAA T2x (main.cpp's "DS2 SMAA T2x" passes): the scene jitters by a quarter pixel diagonally in two phases, the
// blending weights take the phase's area texture subsamples, the neighborhood blend stays in linear light with the velocity length in
// alpha, and the resolve blends it with the previous frame reprojected by Luma's motion vectors. The output hook below runs at the
// resolve.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

#include "Includes/GameCBuffers.hlsl"

cbuffer SmaaMetricsCB : register(b1)
{
   CB::SmaaMetrics smaa_metrics;
}

#if SMAA_T2X
#define SMAA_SUBSAMPLE_INDICES smaa_metrics.subsample_indices
#define SMAA_REPROJECTION      1
// The motion vector target holds the UV delta from the current to the previous position. SMAA's velocity is the opposite.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#endif

#define SMAA_RT_METRICS smaa_metrics.rt_metrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE smaa_metrics.predication.x
// Predication is live (the depth edges mask, scale 2.0), so both values below matter: flat = SCALE * SMAA_THRESHOLD = 0.10 and
// silhouette = flat * (1 - STRENGTH) = 0.05 (the ULTRA base). "SMAA_PREDICATION_THRESHOLD" 0.5 is scale-free, so tune the mask itself.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// The predication signal above is a one-sided edge-ness, so we use morphological edge suppression in place of the local contrast
// adaptation (both in "SMAA_Passes.hlsl" and "SMAA.hlsl"). main.cpp also sets "SMAA_SMOOTH_U_SHAPES" 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1

#include "../Includes/Color.hlsl"

// SMAAEdgeDetection: tex0 = the encoded frame, tex1 = the predication edge-ness in [0,1] (null: reads 0, scale 1)
// SMAANeighborhoodBlending: tex0 = the same frame, filtered in linear light, tex1 = blendTex; encoded back as it came (a uniform
// branch). The alpha is blended as the color (with T2x the velocity length at the resolve).
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_DECODE(rgb)             (smaa_metrics.predication.y != 0.0 ? gamma_sRGB_to_linear(rgb, GCT_MIRROR) : gamma_to_linear(rgb, GCT_MIRROR))
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = (smaa_metrics.predication.y != 0.0 ? linear_to_sRGB_gamma(color.rgb, GCT_MIRROR) : linear_to_gamma(color.rgb, GCT_MIRROR));

#include "../Includes/SMAA_Passes.hlsl"
