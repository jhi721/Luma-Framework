#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"
#include "..\..\Core\includes\scene_motion_vectors.h"

// The game's slots for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"). The second vertex shader run reads the previous frame's per-draw constants (vc4) from another slot.
namespace MotionVectorPatches
{
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // The reactive masks, written by the scene's alpha blended draws (their pixel shaders patched with "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   // vc4 rows (c<N> at row N + 20; CTAB census of the 532 of 564 D3D9 vertex shaders that read a camera, 2.87.5 translation accepted
   // 320/326 offline). Column vector math, one row per clip space component. Camera kinds in priority order: a vertex shader may declare
   // several, and the first one whose rows it reads positions the vertex (the others feed varyings, normals or lighting).
   // g_WorldView c20 + g_Proj c4: non-skinned materials (102; 76 also read the world matrix c16 for lighting varyings)
   constexpr uint32_t world_view_row = object_row_offset + 20;
   // g_View c0 + g_Proj c4: skinned materials (130; the bones in c40 already carry the view) and camera relative effects. Also the frame
   // camera (the velocity, fill and material shaders): the projection stays in these rows for the whole scene.
   constexpr uint32_t view_row = object_row_offset + 0;
   constexpr uint32_t projection_row = object_row_offset + 4;
   // g_WorldViewProjMatrix c24: effects, particles and other non-skinned objects (236; scalar g_WorldViewProj in 2 c20+c4 shaders is not it)
   constexpr uint32_t world_view_projection_row = object_row_offset + 24;
   // g_ViewProjection c8, only when nothing above is read (26; 9 skinned, 8 with a world matrix c16). The 60 shaders that also read
   // c20+c4 or c0+c4 only write c8 * world to a varying.
   constexpr uint32_t view_projection_row = object_row_offset + 8;
   // Not a scene camera: g_ViewProjMatrix c54 (38) is the UI, font, movie and HUD shaders (world matrix c58).

   // The tie-break transforms: the first bone of g_WorldArray c40 (105 view space skinned shaders, 9 view projection ones), then the
   // world matrix c16
   constexpr uint32_t first_bone_row = object_row_offset + 40;
   constexpr uint32_t world_row = object_row_offset + 16;

   // No camera row filter (the camera sits in four rows); shadow and screen passes are told apart by their bound targets. 32 vertex
   // shaders fit no kind (shadow casters c28, blur and screen passes, outline, water simulation): jitter only.
   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();

   // Core's "SceneMotionVectors" config. The camera kinds in order (the first one a vertex shader reads wins). Every shader of the two
   // matrix kinds reads all 4 rows of both (CTAB and disassembly census), and a 1 row scalar g_WorldViewProj c24 must not pass as the
   // matrix: all 4 rows are required. The view projection kind (26 shaders, one skips its z row) is told by its first row, as
   // Bayonetta's. The tie-break rows in order, and the kind that sets the frame's camera (the skinned materials': their c0 and c4 rows are
   // the scene's; the world view shaders take the camera too, from the same buffer). g_ViewProjMatrix c54 has no kind: the UI, fonts and
   // movies are no scene draws.
   constexpr std::array<SceneMotionVectors::Camera, 4> cameras = {{
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_AND_PROJECTION, .row = world_view_row, .projection_row = projection_row},
      {.type = SceneMotionVectors::CameraType::VIEW_AND_PROJECTION, .row = view_row, .projection_row = projection_row},
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = world_view_projection_row},
      {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = view_projection_row, .rows_read = 1},
   }};
   constexpr std::array<SceneMotionVectors::TieBreakRow, 2> tie_break_rows = {{
      {.row = first_bone_row, .relative = true},
      {.row = world_row},
   }};
   constexpr SceneMotionVectors::Camera frame_camera = cameras[1];
} // namespace MotionVectorPatches
