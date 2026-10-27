// XeGTAO replacement for the trilogy-wide NVIDIA HBAO+ chain, ported from the Borderlands GOTY Enhanced mod.
// Source: https://github.com/GameTechDev/XeGTAO
//
// MELE-specific contracts shared by all three games:
// - XeGTAO runs over the whole AO target (half the output at any render scale) and writes visibility to blur u0, the game's final
//   R8_UNORM AO target; GT-VBAO runs over the rendered share of the scene depth, one AO pixel per scene pixel, into main.cpp's own
//   R8_UNORM target instead. The apply shader 0x2E826C0F (replaced: it scales the scene UV onto the whole target) reads either and
//   blends dst*src_color into the fp16 scene.
// - Inherit cb0 HBAO+ $Globals and cb2 CSOffsetConstants; layouts come from live disassembly of
//   0x80212FD6/0x06D92B08 and retain standard GFSDK offsets.
// - Depth input = the output sized r24_unorm_x8 scene depth (the depth and normals downsample 0xA75E6C32's t0), read with explicit
//   Loads (see XeGTAO_PrefilterDepths16x16) at DepthLoadScaleRT scene pixels per AO pixel: 1 for GT-VBAO; for XeGTAO 2 at native and
//   1 at 50% render scale, each AO pixel loading the scene pixel nearest its center (fractional at most render scales).
// - Normals = that downsample's t1 (FullSizedNormalsTexture, view-space xy in RGBA8; HBAO+'s ViewNormalTex is its half size copy),
//   read at the same scale; z is reconstructed locally.
// - Noise: frozen at 0 without an upscaler (the game has no TAA: a frame index would make the pattern boil), and denoise runs
//   twice. With DLSS/FSR (they accumulate the lit scene the AO multiplies into) it cycles frame % 64 and denoise runs once, as
//   Intel's XeGTAO.h advises with TAA (NoiseIndexRT, set by main.cpp). The quality default is Very High.
// - View Z is divided by DepthScaleRT (default 50) to approximate the meter-scale range XeGTAO expects.

cbuffer _Globals : register(b0) // NVIDIA GFSDK_SSAO $Globals.
{
   float RadiusToScreen;        // Offset:   0
   float NegInvR2;              // Offset:   4; -1/R^2 in native view units.
   float NDotVBias;             // Offset:   8
   float2 InvFullResolution;    // Offset:  16; inverse AO resolution.
   float2 InvQuarterResolution; // Offset:  24
   int2 FullResOffset;          // Offset:  32
   int2 QuarterResOffset;       // Offset:  40
   float AOMultiplier;          // Offset:  48
   float PowExponent;           // Offset:  52; native blur darkness control.
   float4 ProjInfo;             // Offset:  64; view xy = (uv*xy + zw) * viewZ.
   float2 Float2Offset;         // Offset:  80
   float4 Jitter;               // Offset:  96
   int ArrayOffset;             // Offset: 112
   float4 JitterCS[8];          // Offset: 128
}

cbuffer CSOffsetConstants : register(b2)
{
   column_major float4x4 ViewProjectionMatrixCS; // Offset:   0; [i] = clip row i over the translated world (see "MELE_NDCToView")
   float4 CameraPositionCS;                      // Offset:  64
   float4 ScreenPositionScaleBiasCS;             // Offset:  80
   float4 MinZ_MaxZRatioCS;                      // Offset:  96; viewZ = 1 / (d * z - w), non-reverse Z.
   float4 DynamicScaleCS;                        // Offset: 112
}

// Runtime parameters uploaded by main.cpp in b11.
cbuffer LumaGTAO : register(b11)
{
   float FinalValuePowerRT; // Darkness control calibrated to native AO.
   float DepthScaleRT;      // View-Z divisor from UE3 units to approximate meters.
   float RadiusOverrideRT;  // Positive values override EFFECT_RADIUS after depth scaling.
   float DebugViewRT;       // DEVELOPMENT / TEST debug view (legend in Includes/XeGTAO.hlsl)
   float NoiseIndexRT;      // frame % 64 with DLSS/FSR, 0 otherwise (see the header).
   float2 PixelSizeRT;      // 1 / the AO target's size.
   float PaddingRT;
   float2 DepthLoadScaleRT; // Scene depth and normal pixels per AO pixel: the rendered size over the AO target's.
}

#include "Includes/Common.hlsl"

// Compile-time defaults; runtime b11 overrides the exposed controls.

#ifndef EFFECT_RADIUS
#define EFFECT_RADIUS 0.6 // Native ME1LE radius: 30 UE3 units / DepthScale 50; ME2LE/ME3LE's 48 units arrive as RadiusOverrideRT, which wins.
#endif

#ifndef EFFECT_FALLOFF_RANGE
#define EFFECT_FALLOFF_RANGE 0.005 // Hard falloff matches native contact AO; Intel default 0.615 is softer.
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 1.5 // Default 2.0
#endif

// Packed normals' z sign; view-space normals face the camera.
#ifndef NORMAL_Z_SIGN
#define NORMAL_Z_SIGN (-1.0)
#endif

// The AO target spans the rendered area at any render scale (its depth is loaded from the rendered share of the scene depth), so its
// UVs are the rendered area's. InvFullResolution is the AO allocation's, as the untouched HBAO+ fills only its rendered share.
#define VIEWPORT_PIXEL_SIZE                PixelSizeRT
#define XE_GTAO_DEPTH_LOAD_COORD(pixCoord) uint2(((pixCoord) + 0.5) * DepthLoadScaleRT)

// The NDC-to-view pair over the rendered area's UVs, from the camera rather than HBAO+'s ProjInfo: the exe builds that from the
// horizontal FOV clamped to [75, 90] degrees for both axes (an integer aspect), so at 16:9 its view space is about 1.1x too wide and
// 1.95x too tall (ME3, NOTES.md "Native AO"). Row 3 of ViewProjectionMatrixCS is the view's forward axis; rows 0/1 are
// P11 * right + P31 * forward and P22 * up + P32 * forward, P31/P32 being the upscaler jitter.
float4 MELE_NDCToView()
{
   const float3 forward = ViewProjectionMatrixCS[3].xyz;
   const float forward_length_sq = dot(forward, forward);
   const float p31 = dot(ViewProjectionMatrixCS[0].xyz, forward) / forward_length_sq;
   const float p32 = dot(ViewProjectionMatrixCS[1].xyz, forward) / forward_length_sq;
   const float p11 = length(ViewProjectionMatrixCS[0].xyz - p31 * forward);
   const float p22 = length(ViewProjectionMatrixCS[1].xyz - p32 * forward);
   return float4(2.0 / p11, -2.0 / p22, (-1.0 - p31) / p11, (1.0 - p32) / p22);
}
#define NDC_TO_VIEW_MUL MELE_NDCToView().xy
#define NDC_TO_VIEW_ADD MELE_NDCToView().zw

// Convert non-reverse D24 through native MinZ_MaxZRatioCS, then scale UE3 units into XeGTAO's expected range.
// Without scaling, its mip and falloff assumptions cause broad over-occlusion.
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   float viewZ = 1.0 / max(1e-7, screenDepth * MinZ_MaxZRatioCS.z - MinZ_MaxZRatioCS.w);
   return max(0.0, viewZ) / max(1e-3, DepthScaleRT);
}

#define XE_GTAO_ADJUST_VISIBILITY(visibility, viewspaceZ) visibility = max(0.03, visibility); // A visible surface cannot be fully occluded.
#define XE_GTAO_FINAL_OUTPUT_TYPE                         unorm float                         // Native final R8_UNORM AO.
#define XE_GTAO_ENCODE_FINAL(v)                           (v)

Texture2D tex1 : register(t1);

float3 XeGTAO_LoadViewspaceNormal(uint2 pixCoord)
{
   // Decode packed view-space xy (the scene's normals, read at the depth's scale) and reconstruct unit z. Camera-facing normals use
   // negative z.
   float2 nxy = tex1.Load(int3(XE_GTAO_DEPTH_LOAD_COORD(pixCoord), 0)).xy * 2.0 - 1.0;
   float3 viewspaceNormal;
   viewspaceNormal.xy = nxy;
   viewspaceNormal.z = NORMAL_Z_SIGN * sqrt(saturate(1.0 - dot(nxy, nxy)));
   return normalize(viewspaceNormal);
}

// tex0 = the output sized R24_UNORM_X8 scene depth captured at the downsample (prefilter).
// GT-VBAO's main pass ("gtvbao_main_pass_cs", "GT-VBAO Enable") on top of XeGTAO's, which it includes. Steps and thickness follow
// Yakuza Kiwami's calibration (Luma_YKR_XeGTAO.hlsl), not yet measured against MELE's HBAO+. Slices: YK's 2, as it runs at one AO
// pixel per scene pixel too (main.cpp, "RunAO"); 1 under the history ("temporal_pass_cs", main.cpp defines it), as YK and FF7R take
// under a temporal filter. main.cpp keeps both denoise passes under DLSS/FSR too. Measurements in the game's NOTES.md ("GT-VBAO at
// full resolution, AO history, FAST noise").
#ifndef GTVBAO_SLICE_COUNT
#define GTVBAO_SLICE_COUNT 2.0
#endif
#ifndef GTVBAO_STEP_COUNT
#define GTVBAO_STEP_COUNT 8.0
#endif
#define GTVBAO_THICKNESS(effectRadius) (0.2 * (effectRadius))
// Core's "FAST Noise" (Shaders/Global/Textures/FAST: 64 layers of 128x128 RGBA, filter adapted spatio-temporal noise for a 1 pixel
// gaussian and a temporal filter) in place of the R2 sequence, a layer per frame, with the history (main.cpp defines it), as FF7R
// does under its temporal AO. XeGTAO's README advises against blue noise moved per frame (overlaps); FAST layers are made not to.
#if GTVBAO_FAST_NOISE
Texture2DArray<float4> fast_noise : register(t4);
#define GTVBAO_NOISE(pixCoord) fast_noise.Load(int4((pixCoord) & 127u, uint(NoiseIndexRT) & 63u, 0)).xyz
#endif
#include "../Includes/GTVBAO.hlsl"
#include "../Includes/MotionVectorFill.hlsl"

// The AO history ("temporal_pass_cs"), between the main pass and the denoise, with the per frame noise under DLSS/FSR (main.cpp,
// "RunAO"): Intel's XeGTAO leaves the temporal part to the TAA, but DLSS/FSR keep much of the AO's noise in the lit scene as detail.
// The previous frame's AO is reprojected with the upscaler's motion vectors (the camera's motion for the pixels no patched draw wrote
// yet: the fill runs after this), loaded like the depth at DepthLoadScaleRT; each bilinear tap is rejected off the depth the previous
// frame should see there, the result clamped to this frame's 3x3 range (motion vectors miss an AO a moving occluder casts on a still
// surface), then blended.
cbuffer LumaGTAOTemporal : register(b10)
{
   row_major float4x4 ReprojectionRT; // Current clip space to the previous frame's (main.cpp, "GetCameraReprojectionConstants")
   float2 JitterNDCRT;                // This frame's projection jitter, in the depth and not in the motion vectors
   float HistoryWeightRT;             // 0 restarts the history
   float TemporalPaddingRT;
}

static const float kMELE_HistoryDepthTolerance = 0.03; // Relative view Z mismatch rejecting a history tap

Texture2D<float2> history : register(t1);           // The previous frame's (AO, view Z)
Texture2D<float2> motion_vectors : register(t2);    // Previous minus current, UV of the scene viewport, unjittered
Texture2D<float> scene_device_depth : register(t3); // tex0 of the prefilter
RWTexture2D<float2> history_output : register(u1);

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)] void temporal_pass_cs(uint2 dtid : SV_DispatchThreadID) {
   // tex0 = the main pass's AO term (.x) and packed edges (.y); ao_term_and_edges = the same for the denoise
   const uint2 size = uint2(round(1.0 / PixelSizeRT));
   if (any(dtid >= size))
      return;
   const float2 current = tex0.Load(int3(dtid, 0)).xy;
   const float device_depth = scene_device_depth.Load(int3(XE_GTAO_DEPTH_LOAD_COORD(dtid), 0));
   const float view_z = XeGTAO_ScreenSpaceToViewSpaceDepth(device_depth);

   // Fed (NDC, depth, 1), the clip space w of the reprojection is the previous view Z over the current one
   const float4 current_clip = float4((dtid + 0.5) * PixelSizeRT * float2(2.0, -2.0) + float2(-1.0, 1.0) - JitterNDCRT, device_depth, 1.0);
   const float4 previous_clip = mul(ReprojectionRT, current_clip);
   const float expected_z = view_z * previous_clip.w;
   float2 motion = motion_vectors.Load(int3(XE_GTAO_DEPTH_LOAD_COORD(dtid), 0));
   if (!(motion.x < MOTION_VECTOR_MARKER))
   {
      motion = CameraMotion(dtid, size, ReprojectionRT, JitterNDCRT, device_depth);
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
      const float weight = ((inside && abs(previous.y - expected_z) <= kMELE_HistoryDepthTolerance * expected_z) ? (bilinear.x * bilinear.y) : 0.0);
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
      ao = lerp(current.x, clamp(history_ao / weight_sum, neighborhood_min, neighborhood_max), HistoryWeightRT);
   }
   ao_term_and_edges[dtid] = float2(ao, current.y);
   history_output[dtid] = float2(ao, view_z);
}
