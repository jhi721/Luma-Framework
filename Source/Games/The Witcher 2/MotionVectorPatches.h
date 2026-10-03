#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo's ("MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same wrapper).
namespace MotionVectorPatches
{
   // REDengine's material vertex shaders (CTAB census of CookedPC/shader.cache): LocalToWorld c0-c2 (3x4, column vectors),
   // WorldToScreen c4-c7, WorldToView c8-c10, the camera c16-c19, wind and time c35-c47, bones c70-c249 (3 rows each, world
   // space). The terrain's vertex shaders sample its heightmap: the second run reads the previous frame's at t64.
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_resources_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::resource_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // WorldToScreen's first row (c4): vertex shaders that never read it (full screen passes, shadow and particle shaders with their
   // own projection) are refused
   constexpr uint32_t view_projection_row = object_row_offset + 4;
   // Past the G-buffer's 4 targets (albedo, specular, normals, linear depth): FSR's masks (x reactive, y transparency &
   // composition), written by the scene's alpha blended draws (their pixel shaders patched with Core's "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout(view_projection_row);
} // namespace MotionVectorPatches
