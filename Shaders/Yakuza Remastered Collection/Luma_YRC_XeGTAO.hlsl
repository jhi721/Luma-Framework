// XeGTAO adapted for Yakuza Remastered Collection (QLOC PC port, native D3D11): replaces the game's Intel ASSAO.
// Source: https://github.com/GameTechDev/XeGTAO
// Shares the depth-derived normals path with Luma_TW2_XeGTAO.hlsl; only the game-specific inputs differ.
//
// Game specifics (from the ASSAO disassembly and its live cb0):
// - The whole ASSAO chain (prepare 0x972BE5B5, depth mips 0x1DD919C4, generate 0x47BFF17F/0xD18E0D3F, blurs
//   0x8CE62D1E/0x15EEFFAF) is skipped. These 4 dispatches run in place of the prepare, at the depth's full resolution
//   (ASSAO's 4 half-res deinterleaved slices cover the same pixels); the apply 0x6A73BA10 then draws with
//   Luma_YRC_GTAOApply.hlsl in place of the native PS, keeping its multiply blend onto the scene mid material stream.
// - Depth = the prepare's t0: 4K r32 hardware depth, standard Z (sky = 1) in Y3R, reversed (sky = 0) in Y4R/Y5R.
// - Normals are generated from depth (XE_GTAO_GENERATE_NORMALS): ASSAO's own are depth-derived too, and the game has
//   no normal buffer.
// - NDC->view, depth unpack and the radius come from ASSAO's live cb0 (the prepare's own b0, rebound PS -> CS), so the
//   per-shot camera FOV (vertical 22.9..55 degrees measured) is this frame's.
// - No TAA: NoiseIndex is FROZEN at 0 (a frame index would make the pattern boil) and denoise runs twice.

// --- Game constant buffer: ASSAO's ASSAOConstants (main.cpp binds it at CS b0) ---

cbuffer AssaoCB0 : register(b0)
{
   // c0.xy ViewportPixelSize, c1.xy DepthUnpackConsts, c1.zw CameraTanHalfFOV, c2 NDCToViewMul/Add,
   // c5.x EffectRadius, c6.xy EffectFadeOutMul/Add, ... (Intel layout, unchanged by QLOC)
   float4 assao[15];
}

// --- Luma runtime knobs (set from main.cpp; live-tunable via DEV/TEST sliders, no recompile) ---
// b8: b9/b10 are the Luma data/settings cbuffers in this game, and no game shader binds b8. Mirrored by gtao_knobs_cb_slot.
cbuffer LumaGTAO : register(b8)
{
   float FinalValuePowerRT; // primary darkness dial (1.0)
   float RadiusOverrideRT;  // > 0 overrides EFFECT_RADIUS (view units, same as ASSAO's)
   float DebugViewRT;       // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float PaddingRT;
   float2 ViewportPixelSizeRT; // 1 / AO resolution (the depth's), set by main.cpp
   float2 Padding2RT;
}

#define XE_GTAO_GENERATE_NORMALS 1 // the game has no normal buffer (ASSAO derives its normals from depth too)

// Anchored to ASSAO's live EffectRadius (0.8 view units, measured constant), so the effective XeGTAO radius equals the native
// one. RadiusOverrideRT > 0 wins.
#define EFFECT_RADIUS (assao[5].x / RADIUS_MULTIPLIER)
// No occlusion floor (Intel's 0.03 floor exists for bent-normal packing, unused here). ASSAO's distance fade (measured 50 -> 300
// view units), part of the native look: no AO on far geometry.
#define XE_GTAO_ADJUST_VISIBILITY(visibility, viewspaceZ) visibility = lerp(1.0, visibility, saturate(viewspaceZ * assao[6].x + assao[6].y));

#define NoiseIndexRT                                      0

#define VIEWPORT_PIXEL_SIZE                               ViewportPixelSizeRT

// ASSAO's NDCToViewMul/Add use XeGTAO's own convention (both are Intel's): viewPos.xy = (uv * MUL + ADD) * viewZ.
#define NDC_TO_VIEW_MUL           assao[2].xy
#define NDC_TO_VIEW_ADD           assao[2].zw

#define XE_GTAO_FINAL_OUTPUT_TYPE unorm float // final AO (R8_UNORM), read by Luma_YRC_GTAOApply.hlsl
#define XE_GTAO_ENCODE_FINAL(v)   (v)

// ASSAO's unpack: viewZ = DepthUnpackConsts.x / (DepthUnpackConsts.y - d), measured (0.10001, 1.0001) in Y3R, and
// (-0.10001, -0.0001) in Y5R, (-0.100007, -0.000067) in Y4R (reversed Z): near 0.1, far 1000 / 1500. The divisor has
// the sign of DepthUnpackConsts.x; the guard keeps it (an infinite far plane) so viewZ stays positive.
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   const float divisor = assao[1].y - screenDepth;
   return assao[1].x / (abs(divisor) > 1e-6 ? divisor : (assao[1].x < 0.0 ? -1e-6 : 1e-6));
}

// tex0 = the prepare's t0, the hardware depth (prefilter)
#include "../Includes/XeGTAO.hlsl"
