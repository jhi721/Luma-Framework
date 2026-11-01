#pragma once

#include <array>
#include <cstdint>

#include "..\..\Core\includes\motion_vector_patch.h"
#include "..\..\Core\includes\scene_motion_vectors.h"

// The game's slots and rows for Core's DXBC motion vector patch (see "motion_vector_patch.h"; dgVoodoo's registers are in
// "MotionVectorPatch::DgVoodoo"). The second vertex shader run reads the previous frame's per-draw constants (vc4) from another slot.
// Bayonetta's layout (the same engine), plus MGR:R's view and projection kinds.
namespace MotionVectorPatches
{
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using SceneMotionVectors::reactive_slot;
   using SceneMotionVectors::target_slot;

   // vc4 rows (c<N> at row N + 20; CTAB census of the 501 vertex shaders of shader.dat and shader2.dat).
   // Column vector math, one row per clip space component; the per-object matrices are 3x4, translation in .w. No vertex shader reads
   // both c8 and c24.
   // g_ViewProjection / g_ViewProjMatrix / g_ViewProj c8: materials (47 skinned by g_WorldArray, 100 static in world space, 9 with a
   // world matrix c16)
   constexpr uint32_t view_projection_row = object_row_offset + 8;
   // g_WorldViewProjMatrix / g_WorldViewProj c24: effects, particles and non-skinned objects (95)
   constexpr uint32_t world_view_projection_row = object_row_offset + 24;
   // g_View c0 + g_Proj c4 (8 with all 4 rows of both: the depth prepass' skinned modelshaderzmap*; the 3 with a 3 row c0 are the
   // motion blur's velocity passes) and g_WorldView c20 + g_Proj c4 (2: its static modelshaderzmapscr*). g_Tmp c4 (22, with c24) and
   // g_ProjMtx c4 (2 rows) are no projection.
   constexpr uint32_t view_row = object_row_offset + 0;
   constexpr uint32_t projection_row = object_row_offset + 4;
   constexpr uint32_t world_view_row = object_row_offset + 20;
   // Not a scene camera: g_ShadowViewProj c28 (shadow casters) and g_ViewProjMatrix c54 (the UI)

   // The tie-break transforms: the first bone of g_WorldArray c40 (skinned, world space), then the world matrix c16
   constexpr uint32_t first_bone_row = object_row_offset + 40;
   constexpr uint32_t world_row = object_row_offset + 16;

   // No camera row filter (the camera sits in three rows); shadow and screen passes are told apart by their bound targets
   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();

   // Core's "SceneMotionVectors" config. The camera kinds in order (the first one a vertex shader reads wins): the two matrix kinds need
   // all 4 rows of both, the view projection ones only their first (Bayonetta's). The tie-break rows in order, and the kind that sets
   // the frame's camera: the depth prepass' (MGR:R's), the only opaque draws accepted (the G-buffer's c8 draws have several targets).
   // Its world view shaders take the camera too, from the same rows.
   constexpr std::array<SceneMotionVectors::Camera, 4> cameras = {{
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_AND_PROJECTION, .row = world_view_row, .projection_row = projection_row},
      {.type = SceneMotionVectors::CameraType::VIEW_AND_PROJECTION, .row = view_row, .projection_row = projection_row},
      {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = view_projection_row, .rows_read = 1},
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = world_view_projection_row, .rows_read = 1},
   }};
   constexpr std::array<SceneMotionVectors::TieBreakRow, 2> tie_break_rows = {{
      {.row = first_bone_row, .relative = true},
      {.row = world_row},
   }};
   constexpr SceneMotionVectors::Camera frame_camera = cameras[1];
} // namespace MotionVectorPatches
