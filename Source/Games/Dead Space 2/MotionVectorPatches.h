#pragma once

#include <array>
#include <cstdint>

#include "..\..\Core\includes\motion_vector_patch.h"
#include "..\..\Core\includes\scene_motion_vectors.h"

// The game's rows for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"). The second vertex shader run reads the previous frame's per-draw constants (vc4) from another slot.
namespace MotionVectorPatches
{
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using SceneMotionVectors::reactive_slot;

   // vc4 rows (c<N> at row N + 20). CTAB census of the 186 D3D9 vertex shaders: fxc packed each shader's registers itself, so one
   // name sits at different rows. Row vector matrices (mul/mad chains over the rows, translation in the 4th row). No bone palette:
   // skinning runs on the CPU (EARSGeom_SkinVerts), the skinned vertices arrive in vertex buffers rewritten every frame.
   // ModelViewProj c4 with Model c0 (32 + MarkerPatch's velocity VS): lit materials, and the tie-break world matrix
   constexpr uint32_t model_view_projection_after_model_row = object_row_offset + 4;
   constexpr uint32_t model_row = object_row_offset + 0;
   // ModelViewProj / model2screen c0 (59 + 17, some with ModelRot c4-c6 or model2world c5, never all 4 rows of c4)
   constexpr uint32_t model_view_projection_row = object_row_offset + 0;
   // w2s c8 (40 particle shaders, vertices in world space): the only pure camera, alpha blended and never depth writing
   constexpr uint32_t world_to_screen_row = object_row_offset + 8;
   // Not told apart: w2v c0 + v2s c4 (13 particle shaders, "ib" emitter constants) read the same rows as Model c0 + ModelViewProj c4,
   // so they classify as the latter (alpha blended: blended motion vectors, the reactive mask where unmatched).
   // vPosScaleTranslationVS (16) and the 6 without constants are screen passes.

   // No camera row filter (the camera sits in three rows); shadow and screen passes are told apart by their bound targets
   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();

   // Core's "SceneMotionVectors" config, in priority order (the first kind whose rows a vertex shader reads wins): all 4 rows of c4
   // first, as the c0 world view projection shaders read at most 3 of them.
   constexpr std::array<SceneMotionVectors::Camera, 3> cameras = {{
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_after_model_row, .row_vectors = true},
      {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_row, .row_vectors = true},
      {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = world_to_screen_row, .row_vectors = true},
   }};
   // The tie-break transform: Model c0. A c0 world view projection shader reads its product there instead (model2world c5 can't come
   // first: c5 is inside a c4 ModelViewProj): still per object, and a frame's camera step keeps the nearest candidate the same object.
   constexpr std::array<SceneMotionVectors::TieBreakRow, 1> tie_break_rows = {{
      {.row = model_row, .row_vectors = true},
   }};
   // No opaque draw reads a camera alone (w2s is in the alpha blended particles), and most read only their ModelViewProj c0 (the G-buffer
   // and the material apply): "main.cpp" lays the engine's own view projection (gtGlobals) out at w2s's rows when the scene opens
   constexpr SceneMotionVectors::Camera frame_camera = cameras[2];
   // CPU skinned characters: their opaque draws read last frame's vertices here, past the second run's resource slots (as Mirror's Edge)
   constexpr uint32_t previous_vertices_slot = layout.previous_resources_slot + layout.resource_slots;
} // namespace MotionVectorPatches
