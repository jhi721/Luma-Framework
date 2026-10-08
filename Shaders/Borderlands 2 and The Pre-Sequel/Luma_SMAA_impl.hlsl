// SMAA for Borderlands 2 / The Pre-Sequel. Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection, run POST-tonemap on the gamma LDR (main.cpp RunPostTonemapSMAA) so it cannot
// perturb the DoF composited inside the tonemap; the native FXAA is cancelled while SMAA is on (main.cpp's FXAA
// override). Edge detection reads the ENCODED post-tonemap signal, the domain its thresholds are
// tuned in; neighborhood blending filters that same snapshot in linear light (SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR) and
// this file re-encodes the blended result, so what goes back downstream is still the game's gamma 2.2 LDR. The PS
// appends no HDR tail (Display Composition does paper-white + scRGB). Predication = plane-deviation edge-ness
// built from the scene-color .a depth (Luma_BL2TPS_DepthExtract), null texture + scale 1.0 as the fallback.

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunPostTonemapSMAA).
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active (edge-ness texture bound) -> frame-wide
   // threshold 0.10 on flats; 1.0 with a null predication texture (fallback) -> plain ULTRA threshold 0.05. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication budget:
//  - flat threshold  = SCALE * SMAA_THRESHOLD           = 2.0 * 0.05       = 0.10 (rejects cel-shade texture noise)
//  - silhouette thr  = SCALE * SMAA_THRESHOLD * (1-STR) = 2.0 * 0.05 * 0.5 = 0.05 (= plain ULTRA base; predication
//    only relaxes geometric edges back to base sensitivity, never below).
// THRESHOLD is 0.5 because Luma_BL2TPS_DepthExtract.hlsl feeds a unitless edge-ness in [0,1], not a depth: the
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
// never an SRGB view. Alpha is left as the blend produced it (the tonemap writes o0.w = 0). No HDR tail.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
#include "../Includes/SMAA_Passes.hlsl"
