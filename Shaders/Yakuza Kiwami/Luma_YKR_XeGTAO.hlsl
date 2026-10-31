// XeGTAO for Yakuza Kiwami (D3D12): replaces the game's SSAO.
// Source: https://github.com/GameTechDev/XeGTAO
//
// Game specifics (see NOTES.md "XeGTAO"):
// - main.cpp ("DrawXeGTAO()") runs the 4 dispatches at fx_ssao_test 0x5F373FFF, at the scene depth's full resolution, and skips the game's chain
//   (fx_ssao_test, fx_ssao_temporal_filter 0xC806318D, ps_ssao_blur 0x2AA0CC2B x8). At ps_ssao_upsampling 0x18B4D161, "apply_ps" writes the final AO
//   in its place, into the alpha of the screen shadow mask (the materials' t13.w) as the game does (color write mask A, FUN_140864d90).
// - tex0 = an R32 copy of the scene depth (D32S8, standard Z), t1 = an RGBA8 copy of the depth prepass' view normals (ps_normal: n * 0.498 + 0.498),
//   both made at that draw.
// - The view space is the main camera's projection (the exe's camera slot 0, never jittered there), right-handed: XeGTAO's looks down +z.
// - No TAA: NoiseIndex is frozen at 0 and denoise runs twice. Under DLAA or SMAA T2x (which average it over frames) the noise moves every frame,
//   "temporal_pass_cs" accumulates the AO between the main pass and the denoise (both denoise passes kept, as MELE's), and GT-VBAO takes one
//   slice (main.cpp's "YKR GT-VBAO Temporal Main Pass CS"). With the history off (its weight 0) the temporal mode denoises once.
// - The radius is the game's: fx_ssao_test samples two rings in the tangent plane at view distance max(c.z * noise, c.w * view z), c_ps_user[8]
//   and [10] (the stage's, live), noise in [0, 1] (c.x on) or 1. XeGTAO spreads its steps over its radius as the noise spreads the game's, so its
//   radius is the outer extent of both rings (as the Remastered Collection's matches ASSAO's).

// Root constants, set by main.cpp
cbuffer LumaGTAO : register(b0)
{
   float2 ViewportPixelSizeRT; // 1 / the depth's size
   float2 DepthUnpackRT;       // The projection's _33 and _43: view z = _43 / (d + _33)
   float2 NDCToViewMulRT;      // (2 / _11, -2 / _22)
   float2 NDCToViewAddRT;      // ((_31 - 1) / _11, (1 + _32) / _22)
   float FinalValuePowerRT;    // Primary darkness dial
   float RadiusOverrideRT;     // > 0 overrides the game's radius (view units, meters)
   float DebugViewRT;          // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float NoiseIndexRT;         // The frame (mod 64) while a temporal anti-aliasing averages the noise, else 0
   float4 VanillaRadiusRT;     // The game's rings: c_ps_user[8].zw, c_ps_user[10].zw
   // "temporal_pass_cs" only
   row_major float4x4 ReprojectionRT; // Current clip space to the previous frame's, column vectors (the DLAA motion vectors')
   float2 JitterNDCRT;                // This frame's projection jitter, in the depth and not in the motion vectors
   float HistoryWeightRT;             // 0 restarts the history
   float ObjectMotionRT;              // 1 if t2 holds this frame's object motion vectors, else t2 is a stand-in
}

// The game's SSAO darkens far wider than its rings: each ring sample counts any depth in front of it as a full occluder, so its halos reach
// past the radius. Matched live on a street at night (AO alpha vs the game's, static pixels): 7.5x its radius (1.5 m near the camera) at power 1
// gives the game's darkening percentiles (total 1.07x, its darkest 10% at 0.79x: GTAO's softer contacts); an office scene fits it too, with
// another power (main.cpp picks the power per algorithm)
#define YKR_GTAO_RADIUS_SCALE             7.5
#define XE_GTAO_EFFECT_RADIUS(viewspaceZ) (RadiusOverrideRT > 0.0 ? RadiusOverrideRT : YKR_GTAO_RADIUS_SCALE * max(max(VanillaRadiusRT.x, VanillaRadiusRT.y * (viewspaceZ)), max(VanillaRadiusRT.z, VanillaRadiusRT.w * (viewspaceZ))))

#define VIEWPORT_PIXEL_SIZE               ViewportPixelSizeRT
#define NDC_TO_VIEW_MUL                   NDCToViewMulRT
#define NDC_TO_VIEW_ADD                   NDCToViewAddRT

#define XE_GTAO_FINAL_OUTPUT_TYPE         unorm float
#define XE_GTAO_ENCODE_FINAL(v)           (v)

// Sky (d = 1) stays finite: the far plane's view z
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   return DepthUnpackRT.y / min(screenDepth + DepthUnpackRT.x, -1e-7);
}

Texture2D<float4> tex1 : register(t1);

float3 XeGTAO_LoadViewspaceNormal(uint2 pixCoord)
{
   // fx_ssao's decode (x * 2.0079 - 1, normalized), then z flipped from the game's right-handed view space
   const float3 n = normalize(tex1.Load(int3(pixCoord, 0)).xyz * 2.007874 - 1.0);
   return float3(n.xy, -n.z);
}

// GT-VBAO's main pass ("gtvbao_main_pass_cs", "GT-VBAO Enable") on top of XeGTAO's, which it includes. Steps set its accuracy (fewer
// miss occluders: the AO comes out brighter), slices its noise, which nothing averages over time here (NoiseIndex 0): one slice is
// blotchy after the denoise (_tools/ykr/gtao_bench, a street frame vs 8 x 32). In game (4080 SUPER, 4K) over XeGTAO: 2 x 16 +2.6 ms, 2 x 8
// +1.0 to 1.9 ms
#ifndef GTVBAO_SLICE_COUNT
#define GTVBAO_SLICE_COUNT 2.0
#endif
#ifndef GTVBAO_STEP_COUNT
#define GTVBAO_STEP_COUNT 8.0
#endif
// The thickness behind each depth sample is VBAO's strength (thicker reaches GTAO's darkening), kept small as its paper advises: at power 1
// and YKR_GTAO_RADIUS_SCALE a sweep against the game's SSAO fit 0.2 x the radius (0.3 m near the camera) best over an office (total 1.06x,
// darkest 10% 0.83x) and a street at night (0.77x, 0.69x), see NOTES.md "Calibration"
#define GTVBAO_THICKNESS(effectRadius) (0.2 * (effectRadius))
#include "../Includes/GTVBAO.hlsl"
#include "../Includes/MotionVectorFill.hlsl"

// The AO history ("temporal_pass_cs"), between the main pass and the denoise, with the per frame noise under DLAA / SMAA T2x (main.cpp,
// "DrawXeGTAO()"), Mass Effect Legendary Edition's (Luma_MELE_XeGTAO.hlsl, measured in its NOTES.md: the AO's frame to frame change on
// still surfaces 0.0064 -> 0.0012): DLSS keeps much of the AO's noise in the lit scene as detail. The previous frame's AO is reprojected
// with the object motion vectors the patched prepass draws wrote (the camera's motion for every other pixel, as the DLAA motion vectors
// do later in the frame); each bilinear tap is rejected off the depth the previous frame should see there, the result clamped to this
// frame's 3x3 range (motion vectors miss an AO a moving occluder casts on a still surface), then blended.
static const float kYKR_HistoryDepthTolerance = 0.03; // Relative view Z mismatch rejecting a history tap

Texture2D<float2> history : register(t1);           // The previous frame's (AO, view Z)
Texture2D<float2> object_motion : register(t2);     // Previous minus current, UV, MOTION_VECTOR_MARKER where no patched draw wrote
Texture2D<float> scene_device_depth : register(t3); // The depth copy (the prefilter's tex0)
RWTexture2D<float2> history_output : register(u1);

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)] void temporal_pass_cs(uint2 dtid : SV_DispatchThreadID) {
   // tex0 = the main pass's AO term (.x) and packed edges (.y); ao_term_and_edges = the same for the denoise
   const uint2 size = uint2(round(1.0 / ViewportPixelSizeRT));
   if (any(dtid >= size))
      return;
   const float2 current = tex0.Load(int3(dtid, 0)).xy;
   const float device_depth = scene_device_depth.Load(int3(dtid, 0));
   const float view_z = XeGTAO_ScreenSpaceToViewSpaceDepth(device_depth);

   // Fed (NDC, depth, 1), the clip space w of the reprojection is the previous view Z over the current one
   const float4 current_clip = float4((dtid + 0.5) * ViewportPixelSizeRT * float2(2.0, -2.0) + float2(-1.0, 1.0) - JitterNDCRT, device_depth, 1.0);
   const float4 previous_clip = mul(ReprojectionRT, current_clip);
   const float expected_z = view_z * previous_clip.w;
   float2 motion = object_motion.Load(int3(dtid, 0));
   // Ordered compare: a NaN one gets the camera motion
   if (!(ObjectMotionRT != 0.0 && motion.x < MOTION_VECTOR_MARKER))
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
      const float weight = ((inside && abs(previous.y - expected_z) <= kYKR_HistoryDepthTolerance * abs(expected_z)) ? (bilinear.x * bilinear.y) : 0.0);
      history_ao += previous.x * weight;
      weight_sum += weight;
   }

   float ao = current.x;
   // A restarted history isn't read at all: a new texture holds anything, NaN included, which a 0 weight lerp would keep
   if (HistoryWeightRT > 0.0 && weight_sum > 0.25)
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

// At ps_ssao_upsampling, drawn with an alpha only write mask: the final AO (tex0, the depth's size, the target's)
float4 apply_ps(float4 pos : SV_Position) : SV_Target
{
   return tex0.Load(int3(pos.xy, 0)).xxxx;
}
