#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"
#include "..\..\Core\includes\scene_motion_vectors.h"

// The game's slots for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"). The second vertex shader run reads the previous frame's per-draw constants (vc4: view projection or
// world view projection, world matrix, bone palette) from another slot.
namespace MotionVectorPatches
{
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // The reactive masks, written by the scene's alpha blended draws (their pixel shaders patched with "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   // vc4 rows (c<N> at row N + 20; census of the 84 vertex shaders of the 2.87.5 in-game dump). No vertex
   // shader reads both cameras. Matrices are column vector math (one row per clip space component); the per-object ones are 3x4,
   // translation in .w.
   // g_ViewProjection c8: materials (39 skinned, 8 static)
   constexpr uint32_t view_projection_row = object_row_offset + 8;
   // g_WorldViewProjMatrix c24: effects and non-skinned objects (32)
   constexpr uint32_t world_view_projection_row = object_row_offset + 24;
   // The tie-break transforms: world matrix c16 (static view projection draws), c20 (12 of the world view projection ones; the other 20
   // have none) and the first bone of g_WorldArray c40 (skinned, world space)
   constexpr uint32_t world_row = object_row_offset + 16;
   constexpr uint32_t effect_world_row = object_row_offset + 20;
   constexpr uint32_t first_bone_row = object_row_offset + 40;

   // No view projection row filter: c8 alone refuses the world view projection shaders (71 of 144 offline); shadow and screen passes
   // are told apart by their bound targets instead
   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();
   // Core's "SceneMotionVectors" config. The camera kinds in order (a position uses all 4 rows of a product, but only the first is
   // required: the kinds were told apart by it), the tie-break rows in order, and the kind that sets the frame's camera.
   constexpr std::array<SceneMotionVectors::Camera, 2> cameras = {{
      {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = view_projection_row, .rows_read = 1},
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = world_view_projection_row, .rows_read = 1},
   }};
   constexpr std::array<SceneMotionVectors::TieBreakRow, 3> tie_break_rows = {{
      {.row = first_bone_row, .relative = true},
      {.row = world_row},
      {.row = effect_world_row},
   }};
   constexpr SceneMotionVectors::Camera frame_camera = cameras[0];
} // namespace MotionVectorPatches
