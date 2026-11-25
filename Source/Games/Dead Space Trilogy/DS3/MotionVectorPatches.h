#pragma once

#include <array>
#include <cstdint>

#include "..\..\..\Core\includes\motion_vector_patch.h"
#include "..\..\..\Core\includes\scene_motion_vectors.h"

namespace DS3
{
   // The game's rows for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
   // "MotionVectorPatch::DgVoodoo"). The second vertex shader run reads the previous frame's per-draw constants (vc4) from another slot.
   namespace MotionVectorPatches
   {
      using MotionVectorPatch::DgVoodoo::object_row_offset;
      using SceneMotionVectors::reactive_slot;

      // vc4 rows (c<N> at row N + 20). CTAB census of the 246 D3D9 vertex shaders: row_major matrices
      // (mul/mad chains over the rows, translation in the 4th row), as Dead Space 1/2. fxc packed each shader's registers itself, so one
      // name sits at different rows: the world view projection at c0, c4, c8 or c12, and scalar constants fill whole rows next to it (e.g.
      // ModelViewProj c0 + g_statusTint c4-c6 + g_playerStatus c7). No bone palette: skinning runs on the CPU, the skinned vertices arrive
      // in vertex buffers rewritten every frame.
      constexpr uint32_t model_view_projection_c12_row = object_row_offset + 12; // After Model c4 + ModelView c8 (38): lit materials
      constexpr uint32_t model_view_projection_c8_row = object_row_offset + 8;   // After ModelView c4 or Model c4 (33)
      constexpr uint32_t model_view_projection_c4_row = object_row_offset + 4;   // Alone, with ModelRot c8 or after Model c0 (35)
      constexpr uint32_t model_view_projection_c0_row = object_row_offset + 0;   // ModelViewProj or model2screen c0 (55)
      // w2s c8 + w2v c12 (60 billboards and particles, vertices in world space): the only pure camera, alpha blended
      constexpr uint32_t world_to_screen_row = object_row_offset + 8;
      constexpr uint32_t world_to_view_row = object_row_offset + 12;

      // No camera row filter (the camera sits in several rows); shadow and screen passes are told apart by their bound targets
      constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();

      // Core's "SceneMotionVectors" config, in priority order (the first kind whose rows a vertex shader reads wins). The packed scalars
      // make "reads 4 rows" meaningless here, so each kind's rows must also feed SV_Position ("position_rows"). The billboards' position
      // also goes through w2v (their camera facing corners): that tells them from the c8 world view projection shaders, and puts them
      // first. Checked offline against the CTAB names (229 / 246 right; the rest go to the w2s kind, whose rows Core compares with the
      // frame camera before any camera motion, so a wrong match there only loses the camera motion of an unmatched object: w2v c0 + v2s c4
      // particles, the actor velocity VS, 2 wind and 1 trail shaders). Every vertex shader with a world view projection gets a kind, so
      // every scene mesh draw is jittered.
      constexpr std::array<SceneMotionVectors::Camera, 5> cameras = {{
         {.type = SceneMotionVectors::CameraType::VIEW_PROJECTION, .row = world_to_screen_row, .row_vectors = true, .position_rows = true, .also_position_row = world_to_view_row},
         {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_c12_row, .row_vectors = true, .position_rows = true},
         {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_c8_row, .row_vectors = true, .position_rows = true},
         {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_c4_row, .row_vectors = true, .position_rows = true},
         {.type = SceneMotionVectors::CameraType::WORLD_VIEW_PROJECTION, .row = model_view_projection_c0_row, .row_vectors = true, .position_rows = true},
      }};
      // The tie-break transform between the draws sharing a key, the first of these rows the shader reads: c4 (Model or ModelView for the
      // c12 and c8 world view projection shaders, whose c0 is a single constant; ModelViewProj c4, the billboards' mWposToV), then c0
      // (Model, or the c0 world view projection's product: still per object, and a frame's camera step keeps the nearest candidate the
      // same object). ModelViewProj c0 with ModelRot or scalars at c4 take those: a weaker tie-break for them.
      constexpr std::array<SceneMotionVectors::TieBreakRow, 2> tie_break_rows = {{
         {.row = object_row_offset + 4, .row_vectors = true},
         {.row = object_row_offset + 0, .row_vectors = true},
      }};
      // No opaque draw reads a camera alone (w2s is in the alpha blended particles): "main.cpp" lays the engine's own view projection out
      // at w2s's rows when the scene opens
      constexpr SceneMotionVectors::Camera frame_camera = cameras[0];
      // CPU skinned characters: their opaque draws read last frame's vertices here, past the second run's resource slots (as Dead Space 2)
      constexpr uint32_t previous_vertices_slot = layout.previous_resources_slot + layout.resource_slots;
   } // namespace MotionVectorPatches
} // namespace DS3
