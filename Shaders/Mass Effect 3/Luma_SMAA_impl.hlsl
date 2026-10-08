// Reference: https://github.com/iryoku/smaa

// SMAA ULTRA + colour edge detection, copied from ME1 2007 (same dgVoodoo stack). REPLACES the game's FXAA 3 pass
// 0x243EAC75: reads its input canvas, writes its render target, before the HUD; Display Composition runs after. Edges
// are detected on the GAMMA canvas, the neighborhood blend runs in linear light (Luma_ME3_SMAALinearize) and
// re-encodes. The predication mask comes from Luma_ME3_DepthExtract.

#include "Includes/Common.hlsl"

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunSMAA).
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2.0 when predication is active, 1.0 with a null predication texture
   // (fallback) -> plain ULTRA threshold 0.05. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication is live (CS mask, SCALE 2.0), so both values below are load-bearing: flat = SCALE * SMAA_THRESHOLD =
// 0.10, silhouette = x (1-STRENGTH) = 0.05 (ULTRA base). SMAA_PREDICATION_THRESHOLD 0.5 is scale-free; tune the CS.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// Edge detection: tex0 = colorTexGamma (gamma-encoded graded canvas)
// tex1 = predicationTex (edge-ness in [0,1]; null fallback -> reads 0, scale 1.0 = plain ULTRA threshold)
// Neighborhood blending: tex0 = colorTex (linear copy), tex1 = blendTex. Re-encode to the canvas' gamma.
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
// The predication signal above is a one-sided edge-ness; morphological edge suppression in place of the local contrast
// adaptation (both in SMAA_Passes.hlsl / SMAA.hlsl). main.cpp also sets SMAA_SMOOTH_U_SHAPES 0.
#define SMAA_PREDICATION_EDGENESS           1
#define SMAA_MORPHOLOGICAL_EDGE_SUPPRESSION 1
#include "../Includes/SMAA_Passes.hlsl"
