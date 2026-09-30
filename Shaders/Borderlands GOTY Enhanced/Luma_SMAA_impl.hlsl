// SMAA (https://github.com/iryoku/smaa) in place of the game's compute FXAA resolve: ULTRA preset, color edge detection and depth
// predication. The input is a snapshot of the fp16 swapchain, gamma-encoded (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white), so
// highlights run past 1. Edge detection reads it as stored; blending filters it in linear light and re-encodes.

#include "../Includes/Common.hlsl"

// Written by main.cpp's FXAA resolve replacement. SmaaRtMetrics: (1/W, 1/H, W, H) at output resolution.
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2 with a valid same-size scene depth (raises the off-edge threshold against noise, lowered back
   // on geometry edges); 1 when it is missing or mismatched, which with the null predication texture is the plain ULTRA threshold
   // (0.05) rather than a frame-wide doubled 0.10 that under-detects edges. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication budget:
//  - flat threshold       = SCALE * SMAA_THRESHOLD                  = 2 * 0.05       = 0.10 (rejects texture colour noise)
//  - silhouette threshold = SCALE * SMAA_THRESHOLD * (1 - STRENGTH) = 2 * 0.05 * 0.5 = 0.05, the plain ULTRA base: predication only
//    relaxes geometric edges back to base sensitivity, never below.
// THRESHOLD is 0.5 because Luma_BL_DepthExtract.hlsl feeds a unitless edge-ness in [0,1], not a depth: half-way means "the extract
// called this a silhouette". Calibrate its tolerance, not this number.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// Edge detection: tex0 = colorTexGamma (stored scene snapshot)
// tex1 = predicationTex (plane-deviation edge mask)
// Neighborhood blending: tex0 = colorTex (the same snapshot, filtered in linear light), tex1 = blendTex. Re-encode to the canvas'
// gamma.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
#include "../Includes/SMAA_Passes.hlsl"
