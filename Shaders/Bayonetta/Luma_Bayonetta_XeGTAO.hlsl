// XeGTAO adapted for Bayonetta (Platinum Engine DX9 -> dgVoodoo2 -> DX11): replaces the game's SSAO ("filtershaderssao", PS 0x40AF18CC
// on dgVoodoo 2.87.5, 0xC32759B1 on 2.81.3). Source: https://github.com/GameTechDev/XeGTAO
//
// Bayonetta specifics:
// - Input is the SSAO draw's t2 (R32F "1 - z/w" from the depth prepass) to the prefilter. The normals come from that depth, not from
//   the SSAO draw's t1 (the prepass' "normalize(normal * modelview) * 0.5 + 0.5"): those vertex normals are smoothed across flat walls
//   and corners, and the main pass' horizons, measured against them, darkened wide bands of flat walls that slid with the camera. The
//   final visibility (R8_UNORM) is applied by "Luma_Bayonetta_XeGTAOApply.hlsl", drawn in place of the game's SSAO shader.
// - No projection reaches the SSAO draw: b4 is a copy of the vertex constants (dgVoodoo's cb4, D3D9 c(n) = row n + 20) of a depth
//   prepass draw of the frame (main.cpp). Every prepass vertex shader has an affine view transform (3 rows) and the same transform
//   followed by the projection (4 rows): "Rmodelview" c20 + "Rmodelviewproj" c24, or "view" c0 + "g_ViewProjection" c8. The projection
//   is the second times the inverse of the first.
// - Noise: frozen at 0 without an upscaler (nothing accumulates: a moving pattern would boil), frame % 64 under an upscaler, which
//   accumulates the lit scene the AO multiplies. There the AO history ("temporal_pass_cs") runs before the denoise.

#define XE_GTAO_GENERATE_NORMALS 1
#include "../Includes/RenoDX/math.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer PrepassVertexConstants : register(b4)
{
   float4 vc4[276];
}

// Mirrored by "GTAO_KNOBS_CB_SLOT" (main.cpp)
cbuffer LumaGTAO : register(b9)
{
   CB::GTAOKnobs gtao_knobs;
}
// The names the shared XeGTAO.hlsl reads
#define FinalValuePowerRT gtao_knobs.final_value_power
#define DepthScaleRT      gtao_knobs.depth_scale
#define RadiusOverrideRT  gtao_knobs.radius_override
#define DebugViewRT       gtao_knobs.debug_view
#define NoiseIndexRT      gtao_knobs.noise_index

// User configurable
//

#ifndef EFFECT_RADIUS
#define EFFECT_RADIUS 0.5 // Default 0.5. RadiusOverrideRT > 0 wins.
#endif

#ifndef EFFECT_FALLOFF_RANGE
#define EFFECT_FALLOFF_RANGE 0.615 // Default 0.615
#endif

//

// The terms of the projection (D3D9 column vectors: clip = P * view) that rebuild the view position: P00, P02, P11, P12, P22, P23,
// and P32's sign (+1 when the view looks down +z, -1 down -z). P02, P12 and P22 come multiplied by that sign, which turns the view
// into one whose depth is positive ahead of the camera, as XeGTAO's (z and the normals' z flip with it).
struct ProjectionTerms
{
   float2 scale;         // P00, P11
   float2 offset;        // P02, P12 (signed)
   float2 depth;         // P22 (signed), P23
   float view_direction; // P32's sign
};

ProjectionTerms GetProjectionTerms()
{
   const uint affine_row = uint(gtao_knobs.camera_affine_row);
   const uint matrix_row = uint(gtao_knobs.camera_matrix_row);
   const float3x3 rotation = float3x3(vc4[affine_row].xyz, vc4[affine_row + 1].xyz, vc4[affine_row + 2].xyz);
   const float3 translation = float3(vc4[affine_row].w, vc4[affine_row + 1].w, vc4[affine_row + 2].w);
   // The affine transform's inverse: (R^-1, -R^-1 t). A row of P is the same row of the 4x4 times it.
   const float3x3 inverse_rotation = renodx::math::Invert3x3(rotation);
   const float3 inverse_translation = -mul(inverse_rotation, translation);
   float4 rows[4];
   [unroll] for (uint i = 0; i < 4; i++)
   {
      const float4 row = vc4[matrix_row + i];
      rows[i] = float4(mul(row.xyz, inverse_rotation), dot(row.xyz, inverse_translation) + row.w);
   }
   ProjectionTerms terms;
   terms.view_direction = (rows[3].z < 0.0) ? -1.0 : 1.0;
   terms.scale = float2(rows[0].x, rows[1].y);
   terms.offset = float2(rows[0].z, rows[1].z) * terms.view_direction;
   terms.depth = float2(rows[2].z * terms.view_direction, rows[2].w);
   return terms;
}

#define VIEWPORT_PIXEL_SIZE gtao_knobs.viewport_pixel_size

// ndc = (uv.x * 2 - 1, 1 - uv.y * 2); view x / depth = (ndc.x - P02) / P00, view y / depth = (ndc.y - P12) / P11
#define NDC_TO_VIEW_MUL (float2(2.0, -2.0) / GetProjectionTerms().scale)
#define NDC_TO_VIEW_ADD ((float2(-1.0, 1.0) - GetProjectionTerms().offset) / GetProjectionTerms().scale)

// The prepass stores 1 - z/w. With the depth positive ahead: z/w = P22 + P23 / depth.
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   const ProjectionTerms terms = GetProjectionTerms();
   const float device_depth = 1.0 - screenDepth;
   const float view_depth = terms.depth.y / (device_depth - terms.depth.x);
   return max(0.0, view_depth) / max(1e-3, DepthScaleRT);
}

#define XE_GTAO_ADJUST_VISIBILITY(visibility, viewspaceZ) visibility = max(0.03, visibility); // A visible surface can't be fully occluded
#define XE_GTAO_FINAL_OUTPUT_TYPE                         unorm float
#define XE_GTAO_ENCODE_FINAL(v)                           (v)

// tex0 = the SSAO draw's t2, the R32F "1 - z/w" depth.
// GT-VBAO's main pass ("gtvbao_main_pass_cs", "GT-VBAO Enable") on top of XeGTAO's, which it includes, as MELE's. Steps and
// thickness follow Yakuza Kiwami's calibration, not yet measured against the game's SSAO. Slices: YK's and MELE's 2, as they run at
// one AO pixel per scene pixel too, and 1 under the history (main.cpp defines it). The denoise runs twice either way, as MELE keeps
// for GT-VBAO's coarser bitmask noise.
#ifndef GTVBAO_SLICE_COUNT
#define GTVBAO_SLICE_COUNT 2.0
#endif
#ifndef GTVBAO_STEP_COUNT
#define GTVBAO_STEP_COUNT 8.0
#endif
#define GTVBAO_THICKNESS(effectRadius) (0.2 * (effectRadius))
// Core's "FAST Noise" (Shaders/Global/Textures/FAST: 64 layers of 128x128 RGBA, spatio-temporal noise for a 1 pixel gaussian and a
// temporal filter) in place of the R2 sequence, a layer per frame, with the history (main.cpp defines it), as MELE and FF7R
#if GTVBAO_FAST_NOISE
Texture2DArray<float4> fast_noise : register(t4);
#define GTVBAO_NOISE(pixCoord) fast_noise.Load(int4((pixCoord) & 127u, uint(NoiseIndexRT) & 63u, 0)).xyz
#endif
#include "../Includes/GTVBAO.hlsl"
// The AO history ("temporal_pass_cs") under an upscaler (main.cpp, "RunXeGTAO"), from the prepass' "1 - z/w" at t3 (saturate drops a NaN,
// as the motion vector fill's)
#define XE_GTAO_TEMPORAL_DEVICE_DEPTH(depth) saturate(1.0 - (depth))
#include "../Includes/XeGTAOTemporal.hlsl"
