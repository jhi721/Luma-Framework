// XeGTAO adapted for Saints Row: The Third (2011, native D3D11): replaces the game's singleframe SSAO. Saints Row: Gat out
// of Hell ships the same calculate, blur and apply shaders byte-for-byte.
// Source: https://github.com/GameTechDev/XeGTAO
//
// SR3 specifics (DevKit SSAO snapshots and the DEV vc0 readout):
// - Only rl_ssao_singleframe_calculate (PS 0x624BF56D, SSAO_Level 2/3) is replaced: the first of its 4 draws (one RGBA
//   channel each) runs these 4 dispatches at the half-res target size, CopyResource'd into the game's
//   target (r16g16b16a16_float; in the Saints Row IV engine r8g8b8a8_unorm until Luma upgrades it), and the other 3 are skipped. Its blur (level 3) and apply (max(1 - avg(rgba), 0.05), multiplied into lighting) stay vanilla, so the
//   output is AO AMOUNT (0 = open) in all four channels.
// - Depth = the calculate's t14 (full-res r24 hardware depth, standard Z), normals = its t13 (full-res r16g16_unorm
//   Lambert azimuthal view-space normals), both read at the full-res pixel of each target pixel (DepthInputScaleRT).
// - NDC->view and depth unpack from the game's live vc0 (the calculate's own b0, rebound PS -> CS), so the animated
//   FOV is this frame's: ssao_inv_proj rows c1-c4, view = (ndc.x*c1 + ndc.y*c2 + d*c3 + c4) / w, i.e.
//   viewZ = 1 / (c3.w*d + c4.w), view.xy = ndc.xy * (c1.x, c2.y) * viewZ (y up, +z forward).
// - No TAA: NoiseIndex is FROZEN at 0 (a frame index would make the pattern boil) and denoise runs twice.

// --- Game constant buffer: the SSAO calculate's vc0 (main.cpp binds it at CS b0) ---

cbuffer GameVC0 : register(b0)
{
   float4 vc0[19]; // c0.x ssao_fade_parameter, c1-c4 ssao_inv_proj rows, c5.xy ssao_projection_scales, ...
}

// --- Luma runtime knobs (set from main.cpp; live-tunable via DEV sliders, no recompile) ---
// b9, not b11: core's DrawBloom owns b11 for its own constants. Mirrored by gtao_knobs_cb_slot.
cbuffer LumaGTAO : register(b9)
{
   float FinalValuePowerRT;    // primary darkness dial
   float DepthInputScaleRT;    // full-res depth/normals pixels per AO target pixel (2 = half res)
   float RadiusOverrideRT;     // > 0 overrides EFFECT_RADIUS (metres)
   float DebugViewRT;          // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float2 ViewportPixelSizeRT; // 1 / AO target resolution, set by main.cpp
   float2 PaddingRT;
}

#define NORMAL_Z_SIGN 1.0 // the game's decoded normal as is; -1 flips z if the Normals debug view proves the convention inverted

// Intel default, metres (near plane 0.15). The native AO reaches only 5-10 cm near the camera and grows with distance through a
// 1.5%-of-screen floor, so there is no radius to anchor to; with FinalValuePowerRT 2.2 this matched the native coverage and mean
// darkening in a lit interior. RadiusOverrideRT > 0 wins.
#define EFFECT_RADIUS 0.5

// Each target pixel takes the top-left full-res pixel of its block, the one the native first draw reads.
#define XE_GTAO_DEPTH_LOAD_SCALE uint(DepthInputScaleRT)

#define NoiseIndexRT             0

#define VIEWPORT_PIXEL_SIZE      ViewportPixelSizeRT

// Transcribed from the native calculate: ndc = (uv.x*2-1, 1-2*uv.y), view.xy = ndc * (c1.x, c2.y) * viewZ.
// Expressed as the XeGTAO mul/add pair over raw uv: viewPos.xy = (uv * MUL + ADD) * viewZ.
#define NDC_TO_VIEW_MUL (float2(2.0, -2.0) * float2(vc0[1].x, vc0[2].y))
#define NDC_TO_VIEW_ADD (float2(-1.0, 1.0) * float2(vc0[1].x, vc0[2].y))

// Copy source for the game's rgba16f AO target: AO amount in all four channels. The apply multiplies the scene by 1 - amount, so
// the scene shows v (floored at 0.05).
#define XE_GTAO_FINAL_OUTPUT_TYPE float4
#define XE_GTAO_ENCODE_FINAL(v)   (1.0 - (v))

// Hardware depth -> view z through ssao_inv_proj (near 0.15 at d = 0, ~15000 at d = 1; units are metres).
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   return rcp(max(vc0[3].w * screenDepth + vc0[4].w, 1e-6));
}

Texture2D<float2> normals : register(t1); // the calculate's t13, full res

// The game's Lambert azimuthal decode (rl_ssao_singleframe_calculate): e = t*2-1, f = |e|^2, n = (e * 2*sqrt(1-f), 2f-1), in the
// same view space as the positions.
float3 XeGTAO_LoadViewspaceNormal(uint2 pixCoord)
{
   const float2 e = normals.Load(int3(pixCoord * XE_GTAO_DEPTH_LOAD_SCALE, 0)) * 2.0 - 1.0;
   const float f = min(dot(e, e), 1.0);
   return float3(e * (2.0 * sqrt(1.0 - f)), (2.0 * f - 1.0) * NORMAL_Z_SIGN);
}

#include "../Includes/XeGTAO.hlsl"
