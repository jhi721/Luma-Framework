// SMAA implementation for Yakuza Remastered Collection (copied from Saints Row: The Third). Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection, in place of the game's CMAA2 or FXAA, on the post-ccr canvas (2.2 gamma, fp16,
// 1.0 = UI paper white, >1 possible). Edge detection works in gamma, neighborhood blending in linear light
// (Luma_YRC_SMAALinearize) and re-encodes.
// Predication uses plane-deviation edge-ness in [0,1] of the game's R32 scene depth (Luma_YRC_SMAAPredication); main.cpp
// passes a null texture and scale 1 (plain ULTRA) without it. No SMAAGather: SMAA point-samples the three neighbours.

#include "Includes/Common.hlsl"

#define SMAA_RT_METRICS float4(LumaSettings.SwapchainInvSize, LumaSettings.SwapchainSize)
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION 1
// TW2's validated tuning: flagged silhouettes get plain ULTRA (2 x (1 - 0.5) x 0.05), the rest twice that.
#define SMAA_PREDICATION_SCALE     LumaData.CustomData3 // 2, or 1 (plain ULTRA) without depth
#define SMAA_PREDICATION_THRESHOLD 0.5                  // Midpoint of the [0,1] edge-ness signal
#define SMAA_PREDICATION_STRENGTH  0.5

// Edge detection: tex0 = colorTexGamma (gamma canvas snapshot), tex1 = predication edge-ness (may be null)
// Neighborhood blending: tex0 = colorTex (linear copy), tex1 = blendTex. Re-encode to the canvas' gamma.
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
