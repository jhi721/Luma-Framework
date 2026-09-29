// SMAA implementation for Medal of Honor (2010) (port of the shipped Airborne, BL2/TPS and Witcher 2 setups —
// same dgVoodoo DX9->11 stack).
// Reference: https://github.com/iryoku/smaa
//
// ULTRA preset + color edge detection. Unlike Airborne, this game DOES ship an anti-aliasing pass: EdgeAA, a
// luma-gradient directional blur fused into the tonemap itself (two of its four permutations). It is a blur, not
// a resolve, so it is suppressed while this runs — Luma_MOH_Tonemap.hlsl gates RunEdgeAA on the same
// GameSettings.SMAAEnable that drives this chain. Leaving both on stacks a blur under the SMAA pass.
//
// Runs from the post-draw callback on the TONEMAP pass, on the gamma canvas that pass just wrote — before the
// bloom chain and DoF, which is where antialiasing belongs, and long before the HUD is drawn on the same canvas.
// The canvas is GAMMA (POST_PROCESS_SPACE_TYPE=0) and carries display-mapped HDR values (>1 possible) — edge
// detection and neighborhood blending both work in gamma, which keeps 1px-thin dark features alive against a
// bright sky. The PS appends NO HDR/tonemap tail; the core Display Composition runs downstream.

#include "Includes/Common.hlsl"

// (1/W, 1/H, W, H) at output resolution — filled by the mod (see main.cpp RunPostTonemapSMAA).
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
// Predication is live: the depth-extract CS supplies the mask and the metrics CB advertises SCALE 2.0, so the two
// values below are load-bearing. SCALE falls back to 1.0 only when the mask is missing. The identities:
//  - flat threshold  = SCALE * SMAA_THRESHOLD           = 2.0 * 0.05       = 0.10 (rejects texture color-noise)
//  - silhouette thr  = SCALE * SMAA_THRESHOLD * (1-STR) = 2.0 * 0.05 * 0.5 = 0.05 (= plain ULTRA base; predication
//    only relaxes geometric edges back to normal sensitivity, never below).
//  - PREDICATION_THRESHOLD 0.5 needs no per-scene tuning because the CS hands SMAA an EDGE-NESS signal in [0,1]
//    (deviation from the local tangent plane), not a depth. The CS tolerance is the lever, not this value.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAAGather(tex, coord)     tex.Gather(LinearSampler, coord, 0)

// Edge detection: tex0 = colorTexGamma (gamma-encoded graded canvas)
// tex1 = predicationTex (edge-ness in [0,1]; null fallback -> reads 0, scale 1.0 = plain ULTRA threshold)
// Neighborhood blending: tex0 = colorTex (gamma copy), tex1 = blendTex. Blend in gamma (the buffer's space): keeps the bright HDR sky
// compressed so 1px-thin dark features survive the average (a linear blend erodes them). No HDR tail and no re-encode: the output stays
// in the gamma canvas' space, mid-pipeline before the HUD and the composition.
#include "../Includes/SMAA_Passes.hlsl"
