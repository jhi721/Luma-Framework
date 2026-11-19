// SMAA implementation for Saints Row IV. Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection, run right after the rl_hdr final composite on the gamma canvas (the swapchain), before the
// PostProcess 2 diffusion DoF and the UI. It adds to the game's MSAA rather than replacing an AA pass; DLSS/FSR replace it.
// The canvas holds gamma-encoded, display-mapped HDR, so values above 1 occur; edge detection runs on it, neighborhood blending
// filters it in linear light, then re-encodes.
// Predication uses plane-deviation edge-ness in [0,1] of the game's R24 scene depth (Luma_SR4_SMAAPredication); main.cpp passes a
// null texture and scale 1 (plain ULTRA) without it. No SMAAGather: SMAA point-samples the three neighbours.

#include "Includes/Common.hlsl"

// "SMAA_T2X" 1 is SMAA T2x (main.cpp's "SR4 SMAA T2x" passes). The scene jitters by a quarter pixel diagonally in two phases. The
// blending weights take the phase's area texture subsamples, the neighborhood blend stays in linear light with the velocity length in
// alpha, and the resolve blends it with the previous frame reprojected by the motion vectors. The output hook below runs at the resolve.
#ifndef SMAA_T2X
#define SMAA_T2X 0
#endif

#define SMAA_RT_METRICS float4(LumaSettings.SwapchainInvSize, LumaSettings.SwapchainSize)
#if SMAA_T2X
// The jitter phase's "SMAA.hlsl" @SUBSAMPLE_INDICES (main.cpp's CustomData4), 0 when the scene didn't jitter
#define SMAA_SUBSAMPLE_INDICES float4(LumaData.CustomData4.xxx, 0)
#define SMAA_REPROJECTION      1
// The motion vector target ("mv_texture" in main.cpp) holds the UV delta from the current to the previous position. SMAA's velocity is
// the opposite.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#endif
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION 1
// TW2's validated tuning: flagged silhouettes get plain ULTRA (2 x (1 - 0.5) x 0.05), the rest twice that.
#define SMAA_PREDICATION_SCALE     LumaData.CustomData3 // 2, or 1 (plain ULTRA) without depth
#define SMAA_PREDICATION_THRESHOLD 0.5                  // Midpoint of the [0,1] edge-ness signal
#define SMAA_PREDICATION_STRENGTH  0.5

// Edge detection: tex0 = colorTexGamma (gamma canvas snapshot), tex1 = predication edge-ness (may be null)
// Neighborhood blending: tex0 = colorTex (the same snapshot, filtered in linear light), tex1 = blendTex. Re-encode to the canvas'
// gamma.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR 1
#if SMAA_T2X
// At the resolve: tex3 = the composite snapshot (the colorTex above). The composite writes the scene's alpha; it is kept from there,
// never the velocity length the T2x frames hold in alpha.
Texture2D tex3 : register(t3);
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color = float4(linear_to_gamma(color.rgb, GCT_MIRROR), tex3.Load(int3(position.xy, 0)).a);
#else
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
#endif
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
