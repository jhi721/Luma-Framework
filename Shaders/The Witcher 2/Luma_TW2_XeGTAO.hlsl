// XeGTAO adapted for The Witcher 2 EE (REDengine DX9 -> dgVoodoo2 -> DX11) — replaces the game's HBAO.
// Source: https://github.com/GameTechDev/XeGTAO
//
// TW2 specifics:
// - Only the native generator draw (PS 0x3FEEC0F7 / VS 0x5D9D0449, half-res HBAO) is replaced, by the 4
//   XeGTAO dispatches; the result is CopyResource'd into the game's AO target, which is created without a
//   UAV bind, hence the copy through our own texture. Downstream reads only .x: pack 0x953119B5 (replaced,
//   AOPack_0x953119B5) -> ping-pong 0xC131C40D x2 -> blur 0xD01CBD13 x2 -> apply 0x5C63E1C2.
// - Depth input = the generator's own t0: r32_float half-res, ALREADY LINEAR view-space depth, so no
//   unpack, only the DepthScaleRT unit rescale. Below native render scale it is the G-buffer's full size linear depth instead, read
//   at depth_load_scale pixels per working pixel, and XeGTAO fills the whole AO target (half the output) rather than the scene's
//   share of it; the replaced pack (AOPack_0x953119B5) scales its AO UV onto it.
// - Normals are generated from depth in-place (XE_GTAO_GENERATE_NORMALS) using the same NDC->view
//   convention as the native HBAO, so the space stays self-consistent.
// - NDC->view from the game's cb4 (copied PS->CS by main.cpp):
//   viewRay.xy = (uv.x*2-1, 1-2*uv.y) * cb4[9].zw; viewPos = viewRay * depth, viewPos.z = +depth
// - Temporal noise only while DLSS/FSR accumulates the scene (NoiseIndex = frame % 64, SR3R's pattern); without an upscaler
//   it is frozen at 0 (nothing accumulates, a moving pattern would boil). Denoise runs twice either way.
// - Viewport size comes through the Luma knobs CB at b9, not from game constants.

// --- Game constant buffer (bound by the game at the hooked draw; main.cpp copies PS b4 -> CS b4) ---

cbuffer GameCB4 : register(b4)
{
   float4 cb4[236]; // [9].zw = tan-half-FOV NDC->view scale (see header); layout as captured, do not trim
}

// --- Luma runtime knobs (set from main.cpp; live-tunable via DEV sliders, no recompile) ---
// b9, NOT b11 (which the sibling MELE/BL GOTY ports use): core's DrawBloom owns b11 for its own constants,
// so keeping the AO knobs off that slot costs nothing and avoids a clash. Mirrored by GTAO_KNOBS_CB_SLOT (main.cpp).
#include "Includes/GameCBuffers.hlsl"

cbuffer LumaGTAO : register(b9)
{
   CB::GTAOKnobs gtao_knobs;
}
// The names the shared XeGTAO.hlsl reads
#define FinalValuePowerRT   gtao_knobs.final_value_power
#define DepthScaleRT        gtao_knobs.depth_scale
#define RadiusOverrideRT    gtao_knobs.radius_override
#define DebugViewRT         gtao_knobs.debug_view
#define ViewportPixelSizeRT gtao_knobs.viewport_pixel_size

// User configurable
//

#ifndef XE_GTAO_GENERATE_NORMALS
#define XE_GTAO_GENERATE_NORMALS 1 // TW2: no normal buffer captured at this point of the frame; derive from depth (a real normal source exists later in the frame — see 0x53AB3429 — but not here)
#endif

// The game's own HBAO radius, live: cb4[11] = (R, R^2, 1/R, H/W) in view units (meters), set per environment (3 m at the Flotsam gate
// and in its tavern, 1.18 m where the first probe was taken). XeGTAO multiplies EFFECT_RADIUS by RADIUS_MULTIPLIER, so it is divided
// out. The native kernel cap below still bounds it in pixels. RadiusOverrideRT > 0 wins.
#ifndef EFFECT_RADIUS
#define EFFECT_RADIUS (cb4[11].x / RADIUS_MULTIPLIER)
#endif

#ifndef EFFECT_FALLOFF_RANGE
#define EFFECT_FALLOFF_RANGE 0.005 // hard-edge falloff (full sample weight up to the radius edge) = crisp contact AO, repo-standard for HBAO-native games; Intel default 0.615 is softer
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 1.5 // Default 2.0
#endif

//

// Capped at the native HBAO kernel, cb4[16].y pixels of the full size AO target, converted to working pixels by work_share (see
// GameCBuffers.hlsl): < 1 on the scene's share alone, whose fewer pixels each cover more of the view (the radius would grow by
// 1 / work_share, larger halos at 50%); 2 for GT-VBAO at native, one pixel per scene pixel; 1 on the whole target.
#define NATIVE_KERNEL_PIXELS                        (cb4[16].y * gtao_knobs.work_share.x)
#define XE_GTAO_MAIN_PASS_EFFECT_RADIUS(viewspaceZ) min(XeGTAO_EffectRadius(), NATIVE_KERNEL_PIXELS * viewspaceZ * NDC_TO_VIEW_MUL_X_PIXEL_SIZE.x)

#define NoiseIndexRT                                gtao_knobs.noise_index
#define XE_GTAO_DEPTH_LOAD_SCALE                    gtao_knobs.depth_load_scale

#define VIEWPORT_PIXEL_SIZE                         ViewportPixelSizeRT

// Transcribed from the native HBAO PS (0x3FEEC0F7): ndc = (uv.x*2-1, 1-2*uv.y), viewRay = ndc * cb4[9].zw.
// Expressed as the XeGTAO mul/add pair over raw uv: viewPos.xy = (uv * MUL + ADD) * viewZ.
// XeGTAO's working textures span the rendered area at any render scale (the scene's share of the AO target, or the whole target
// from the full size depth; main.cpp), so cb4[9] (the full view's) applies as is.
#define NDC_TO_VIEW_MUL (float2(2.0, -2.0) * cb4[9].zw)
#define NDC_TO_VIEW_ADD (float2(-1.0, 1.0) * cb4[9].zw)

// TW2's depth texture is ALREADY linear view-space depth (the native HBAO scales its NDC ray by the raw
// sample) — no projection unpack. DepthScaleRT only rescales the game units into the ~meter range XeGTAO's
// Intel-tuned constants (radius/falloff/mip offsets) expect.
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   return max(0.0, screenDepth) / max(1e-3, DepthScaleRT);
}

#if TW2_GTAO_OWN_TARGET
// GT-VBAO's own R8_UNORM target, one AO pixel per scene pixel, which the pack reads in place of the game's (main.cpp, "RunXeGTAO")
#define XE_GTAO_FINAL_OUTPUT_TYPE unorm float
#define XE_GTAO_ENCODE_FINAL(v)   (v)
#else
// Plain float4 (not unorm): the copy-source texture matches the game AO RT's ACTUAL format, unorm8 (or rgba16_float should an
// upgrade ever catch it) — float4 UAV writes are valid against both.
#define XE_GTAO_FINAL_OUTPUT_TYPE float4
// Vanilla generator writes (AO, viewZ, 0, 0); the downstream pack pass 0x953119B5 reads only .x from
// this target (it re-reads depth from the depth texture), so .yzw can stay 0.
#define XE_GTAO_ENCODE_FINAL(v)   float4(v, 0.0, 0.0, 0.0)
#endif

// GT-VBAO's main pass ("gtvbao_main_pass_cs", "GT-VBAO Enable") on top of XeGTAO's prefilter and denoise, from the G-buffer's full
// size linear depth over the rendered area, one AO pixel per scene pixel (main.cpp, "RunXeGTAO"). MELE's settings: 2 slices, 1 under
// the AO history ("temporal_pass_cs", main.cpp defines it) with Core's FAST noise, 8 steps; but a 0.4 radius back face, not MELE's 0.2:
// against the native HBAO, 0.2 left the darkest 1% at 0.46x (XeGTAO 0.57x), 0.4 gives 0.53x with the power at 1.6 (main.cpp,
// "g_gtvbao_final_value_power"), the same midtones as XeGTAO at 2.2.
#ifndef GTVBAO_SLICE_COUNT
#define GTVBAO_SLICE_COUNT 2.0
#endif
#ifndef GTVBAO_STEP_COUNT
#define GTVBAO_STEP_COUNT 8.0
#endif
#define GTVBAO_THICKNESS(effectRadius) (0.4 * (effectRadius))
// Core's "FAST Noise" (Shaders/Global/Textures/FAST: 64 layers of 128x128 RGBA, filter adapted spatio-temporal noise), a layer per frame
#if GTVBAO_FAST_NOISE
Texture2DArray<float4> fast_noise : register(t4);
#define GTVBAO_NOISE(pixCoord) fast_noise.Load(int4((pixCoord) & 127u, uint(NoiseIndexRT) & 63u, 0)).xyz
#endif

// tex0 = the hooked HBAO draw's t0, the game's half-res r32_float linear view-space depth (below native render scale, and for GT-VBAO,
// the G-buffer's full size linear depth instead, see the header)
#include "../Includes/GTVBAO.hlsl"
#include "../Includes/MotionVectorFill.hlsl"

// The AO history ("temporal_pass_cs", MELE's), between the main pass and the denoise under DLSS/FSR (main.cpp, "RunXeGTAO"): the
// previous frame's AO is reprojected with the upscaler's motion vectors (the camera's motion for the pixels no patched draw wrote: the
// fill runs at the scene's end, after this), each bilinear tap rejected off the view depth the previous frame should see there, the
// result clamped to this frame's 3x3 range (motion vectors miss an AO a moving occluder casts on a still surface), then blended.
cbuffer LumaGTAOTemporal : register(b10)
{
   CB::GTAOTemporal gtao_temporal;
}

static const float kTW2_HistoryDepthTolerance = 0.03; // Relative view Z mismatch rejecting a history tap

Texture2D<float2> history : register(t1);         // The previous frame's (AO, view Z)
Texture2D<float2> motion_vectors : register(t2);  // Previous minus current, UV of the scene viewport, unjittered
Texture2D<float> scene_view_depth : register(t3); // tex0 of the prefilter: the linear view depth
RWTexture2D<float2> history_output : register(u1);

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)] void temporal_pass_cs(uint2 dtid : SV_DispatchThreadID) {
   // tex0 = the main pass's AO term (.x) and packed edges (.y); ao_term_and_edges = the same for the denoise
   const uint2 size = uint2(round(1.0 / ViewportPixelSizeRT));
   if (any(dtid >= size))
      return;
   const float2 current = tex0.Load(int3(dtid, 0)).xy;
   const float view_z = scene_view_depth.Load(int3(XE_GTAO_DEPTH_LOAD_COORD(dtid), 0));
   // As the motion vector fill: the cleared sky has no depth and goes to the far plane
   const float device_depth = (view_z > 0.0 ? saturate(gtao_temporal.depth_from_view.x + gtao_temporal.depth_from_view.y / view_z) : 1.0);

   // Fed (NDC, depth, 1), the clip space w of the reprojection is the previous view Z over the current one
   const float4 current_clip = float4((dtid + 0.5) * ViewportPixelSizeRT * float2(2.0, -2.0) + float2(-1.0, 1.0) - gtao_temporal.jitter_ndc, device_depth, 1.0);
   const float4 previous_clip = mul(gtao_temporal.reprojection, current_clip);
   const float expected_z = view_z * previous_clip.w;
   float2 motion = motion_vectors.Load(int3(XE_GTAO_DEPTH_LOAD_COORD(dtid), 0));
   if (!(motion.x < MOTION_VECTOR_MARKER))
   {
      motion = CameraMotion(dtid, size, gtao_temporal.reprojection, gtao_temporal.jitter_ndc, device_depth);
   }

   const float2 previous_pixel = (dtid + 0.5) + motion * size - 0.5;
   const int2 base = int2(floor(previous_pixel));
   const float2 f = previous_pixel - base;
   float history_ao = 0.0;
   float weight_sum = 0.0;
   [unroll] for (uint i = 0; i < 4; i++)
   {
      const int2 tap = base + int2(i & 1, i >> 1);
      const float2 bilinear = float2(((i & 1) ? f.x : (1.0 - f.x)), ((i >> 1) ? f.y : (1.0 - f.y)));
      const float2 previous = history.Load(int3(tap, 0));
      const bool inside = all(tap >= 0) && all(tap < int2(size));
      const float weight = ((inside && abs(previous.y - expected_z) <= kTW2_HistoryDepthTolerance * expected_z) ? (bilinear.x * bilinear.y) : 0.0);
      history_ao += previous.x * weight;
      weight_sum += weight;
   }

   float ao = current.x;
   if (weight_sum > 0.25)
   {
      float neighborhood_min = current.x, neighborhood_max = current.x;
      [unroll] for (int y = -1; y <= 1; y++)
      {
         [unroll] for (int x = -1; x <= 1; x++)
         {
            if (x == 0 && y == 0)
               continue;
            const float neighbor = tex0.Load(int3(clamp(int2(dtid) + int2(x, y), 0, int2(size) - 1), 0)).x;
            neighborhood_min = min(neighborhood_min, neighbor);
            neighborhood_max = max(neighborhood_max, neighbor);
         }
      }
      ao = lerp(current.x, clamp(history_ao / weight_sum, neighborhood_min, neighborhood_max), gtao_temporal.history_weight);
   }
   ao_term_and_edges[dtid] = float2(ao, current.y);
   history_output[dtid] = float2(ao, view_z);
}
