// SMAA for Mirror's Edge. Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection, run right after TdToneMapping on its fp16 gamma target (main.cpp RunPostTonemapSMAA), before
// TdMotionBlur and the HUD. Edge detection reads the ENCODED post-tonemap signal, the domain its thresholds are tuned in;
// neighborhood blending filters that same snapshot in linear light (SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR) and this file re-encodes the
// blended result, so what goes downstream is still the tonemap's gamma output. Predication = plane-deviation edge-ness from the
// linear depth in the scene's alpha (Luma_ME_DepthExtract), null texture + scale 1.0 as the fallback. Borderlands 2's file, except the output alpha (1 here).

// "SMAA_T2X" 1 is SMAA T2x (main.cpp's "ME SMAA T2x" passes, on the upscalers' motion vectors). The scene jitters by a quarter pixel diagonally in two phases.
// The blending weights take the phase's area texture subsamples, the neighborhood blend stays in linear light with the velocity
// length in alpha, and the resolve blends it with the previous frame reprojected by the motion vectors. The output hook below runs
// at the resolve.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunPostTonemapSMAA).
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active (edge-ness texture bound) -> frame-wide
   // threshold 0.10 on flats; 1.0 with a null predication texture (fallback) -> plain ULTRA threshold 0.05. yzw unused.
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
// Predication budget:
//  - flat threshold  = SCALE * SMAA_THRESHOLD           = 2.0 * 0.05       = 0.10 (rejects cel-shade texture noise)
//  - silhouette thr  = SCALE * SMAA_THRESHOLD * (1-STR) = 2.0 * 0.05 * 0.5 = 0.05 (= plain ULTRA base; predication
//    only relaxes geometric edges back to base sensitivity, never below).
// THRESHOLD is 0.5 because Luma_ME_DepthExtract.hlsl feeds a unitless edge-ness in [0,1], not a depth: the
// half-way point simply means "the extract called this a silhouette". Calibrate its tolerance, not this number.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
// Color.hlsl only for the decode and re-encode helpers. Neither it (it includes only Math.hlsl) nor the self-contained SMAA.hlsl
// has a DEVELOPMENT/TEST conditional, so every entry point stays byte-identical across the two define sets.
#include "../Includes/Color.hlsl"

// Edge detection: tex0 = colorTexGamma (the gamma-encoded LDR snapshot)
// tex1 = predicationTex (plane-deviation edge-ness; null fallback -> reads 0, scale 1.0 = plain ULTRA threshold)
// Neighborhood blending: tex0 = colorTex, the same snapshot, decoded before its bilinear weights (the blend averages, which must
// happen in linear light); tex1 = blendTex. Re-encode with the tonemap's own linear_to_gamma, so both sides move together if
// DefaultGamma ever does; GCT_MIRROR brings the dither's negative half at black back out unclamped. One encode only: the RTV is
// never an SRGB view. Alpha is 1, as the tonemap writes it, never T2x's velocity length. No HDR tail.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color = float4(linear_to_gamma(color.rgb, GCT_MIRROR), 1.0);
#include "../Includes/SMAA_Passes.hlsl"
