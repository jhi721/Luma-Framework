// SMAA implementation for Saints Row: The Third Remastered, ported from Saints Row: The Third. Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection, in place of the game's FXAA (pbr_fxaa 0xD928AE8D): it reads the tonemap output and
// writes FXAA's render target, before compose adds grain and the GUI (RCAS Sharpness then runs in compose).
// The input is gamma code and carries display-mapped HDR values, so >1 is possible; edge detection works in gamma,
// neighborhood blending filters the same texture in linear light and re-encodes.
// Predication uses plane-deviation edge-ness in [0,1] of the game's R24 scene depth (Luma_SRTTR_SMAAPredication); main.cpp
// passes a null texture and scale 1 (plain ULTRA) without it. No SMAAGather: SMAA point-samples the three neighbours.

#include "Includes/Common.hlsl"

#define SMAA_RT_METRICS float4(LumaSettings.SwapchainInvSize, LumaSettings.SwapchainSize)
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION 1
// TW2's validated tuning: flagged silhouettes get plain ULTRA (2 x (1 - 0.5) x 0.05), the rest twice that.
#define SMAA_PREDICATION_SCALE     LumaData.CustomData3 // 2, or 1 (plain ULTRA) without depth
#define SMAA_PREDICATION_THRESHOLD 0.5                  // Midpoint of the [0,1] edge-ness signal
#define SMAA_PREDICATION_STRENGTH  0.5

// Edge detection: tex0 = colorTexGamma (FXAA's input), tex1 = predication edge-ness (may be null)
// Neighborhood blending: tex0 = colorTex (the same input, filtered in linear light), tex1 = blendTex. Re-encode to the canvas' gamma.
#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
#include "../Includes/SMAA_Passes.hlsl"
