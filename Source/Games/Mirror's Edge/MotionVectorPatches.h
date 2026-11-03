#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"
#include "..\..\Core\includes\scene_motion_vectors.h"

// The game's slots for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same engine family and wrapper). The second vertex shader run reads the
// previous frame's per-draw constants (vc4: view projection, world matrix, bone palette) from another slot.
namespace MotionVectorPatches
{
   // vc4 (CTAB census of the RefShaderCache, NOTES.md): view projection c0-c3 in every scene vertex shader (not
   // translated), camera position c4, world matrix c5-c8 (skinned: bones c5-c229, world matrix c230-c233). Row vector matrices
   // (UE3's). Offline mvcheck PASS on dgVoodoo 2.87.5 and 2.81.3.
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using SceneMotionVectors::object_slot;
   using SceneMotionVectors::reactive_slot;
   using SceneMotionVectors::target_slot;
   // The first vc4 row of the view projection (c0): vertex shaders that never read it (shadow depths, full screen passes) are refused
   constexpr uint32_t view_projection_row = object_row_offset;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout(view_projection_row);
   // Core's "SceneMotionVectors" config: one camera kind (the layout already requires its first row; the camera position c4 moves with
   // it, for camera facing vertices), and the LocalToWorld tie-break
   // rows the shader reads (VS CTAB census, _tools/mirrors_edge/NOTES.md): dgVoodoo uploads every D3D9 register, so a row it doesn't
   // read holds another draw's data (The Witcher 2). Skinned first: their bones cover c5-c8. Terrain (InvTerrainSize_SectionBase)
   // holds it at c10-c13, sphere fog volumes at c11-c14; neither reads c5-c9.
   constexpr std::array<SceneMotionVectors::Camera, 1> cameras = {{
      {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = view_projection_row, .rows_read = 1, .row_vectors = true, .camera_rows = 5},
   }};
   constexpr std::array<SceneMotionVectors::TieBreakRow, 4> tie_break_rows = {{
      {.row = object_row_offset + 230, .row_vectors = true},
      {.row = object_row_offset + 5, .row_vectors = true},
      {.row = object_row_offset + 10, .row_vectors = true},
      {.row = object_row_offset + 11, .row_vectors = true},
   }};
   constexpr SceneMotionVectors::Camera frame_camera = cameras[0];
} // namespace MotionVectorPatches
