// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection for Yakuza Kiwami, drawn by Core's D3D12 "DrawSMAA()" in place of the game's FXAA (main.cpp),
// on the post-ccr canvas (gamma 2.2, fp16, 1.0 = UI paper white, > 1 in HDR). Edges are detected on the gamma canvas, the
// neighborhood blend filters it in linear light and re-encodes. The predication mask comes from Luma_YKR_SMAAPredication.

// For "gamma_to_linear" and "linear_to_gamma" only: Luma's cbuffers aren't in this pass' root signature, nothing here reads them
#include "Includes/Common.hlsl"

// "DrawSMAA()": the target's metrics, then main.cpp's constants
cbuffer SMAAConstants : register(b1)
{
   float4 smaa_rt_metrics;
   float4 smaa_predication; // Scale, strength: (2, 0.5) with the depth's edges, (1, 0) = plain ULTRA without them; z = T2x subsample index (0 = none, 1/2 = the jitter phases)
}

// "SMAA_T2X" 1 is SMAA T2x (main.cpp, without DLSS): the scene jitters by a quarter pixel diagonally in two phases, the blending weights take the
// phase's area texture subsamples, the neighborhood blend keeps the velocity length in alpha and Luma_YKR_SMAAT2xResolve blends it with the previous frame.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

#if SMAA_T2X
#define SMAA_SUBSAMPLE_INDICES float4(smaa_predication.zzz, 0)
#define SMAA_REPROJECTION      1
// The motion vector target holds the UV delta from the current to the previous position (unjittered). SMAA's velocity is the opposite.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#endif

#define SMAA_RT_METRICS smaa_rt_metrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION 1
// TW2's validated tuning: flat = SCALE * SMAA_THRESHOLD = 0.10, silhouette = flat * (1 - STRENGTH) = 0.05 (ULTRA base).
// SMAA_PREDICATION_THRESHOLD 0.5 is the midpoint of the [0,1] edge-ness signal.
#define SMAA_PREDICATION_SCALE     smaa_predication.x
#define SMAA_PREDICATION_STRENGTH  smaa_predication.y
#define SMAA_PREDICATION_THRESHOLD 0.5

// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1

// SMAAEdgeDetection: tex0 = colorTexGamma (the canvas), tex1 = predicationTex (edge-ness in [0,1])
// SMAANeighborhoodBlending: tex0 = the same canvas, filtered in linear light, tex1 = blendTex. Re-encoded to the canvas' gamma.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);

#include "../Includes/SMAA_Passes.hlsl"
