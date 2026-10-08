// SMAA implementation for Persona 5 Strikers. Reference: https://github.com/iryoku/smaa
// ULTRA preset + color edge detection on the composite's canvas (swapchain, upscaled canvas, or its render resolution target, which
// the game then stretches) before the UI, replacing the game's FXAA (which runs after the UI). main.cpp passes the
// canvas size in LumaData.CustomData1/2.
// The canvas is linear (1 = UI paper white, HDR above), so edge detection reads a gamma copy (Luma_P5S_SMAAEncode); neighborhood
// blending reads the linear copy and re-encodes to gamma for Luma_P5S_SMAAFinalize (RCAS), or writes the canvas.
// Predication reads the depth edge-ness of Luma_P5S_SMAAPredication; without it main.cpp passes null and scale 1 (plain ULTRA).
// No SMAAGather: SMAA point-samples the center and its left and top neighbours.

#include "Includes/Common.hlsl"

#define SMAA_CANVAS_SIZE float2(LumaData.CustomData1, LumaData.CustomData2)
#define SMAA_RT_METRICS  float4(1.0 / SMAA_CANVAS_SIZE, SMAA_CANVAS_SIZE)
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION 1
// TW2's validated tuning: flagged silhouettes get plain ULTRA (2 x (1 - 0.5) x 0.05), the rest twice that.
#define SMAA_PREDICATION_SCALE     LumaData.CustomData3 // 2, or 1 (plain ULTRA) without depth
#define SMAA_PREDICATION_THRESHOLD 0.5                  // Midpoint of the [0,1] edge-ness signal
#define SMAA_PREDICATION_STRENGTH  0.5

// Edge detection: tex0 = colorTexGamma (gamma copy of the canvas), tex1 = predication edge-ness (may be null)
// Neighborhood blending: tex0 = colorTex (linear copy), tex1 = blendTex. With RCAS, gamma encoded for the finalize pass; without, this
// writes the canvas, so it applies the finalize pass's dither.
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position)              \
   [branch] if (LumaSettings.GameSettings.RCASSharpness > 0.0) \
       color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);     \
   else P5S_DitherOutput(color.rgb, position.xy / SMAA_CANVAS_SIZE);
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
