#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo 2.87.3's ("MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same
// wrapper); validated offline on every dumped and generated vertex shader.
namespace MotionVectorPatches
{
   // D3DX assigns the vc4 registers per effect ("VertexShaderRegisters.h"), so no row filters the vertex shaders that project with
   // the camera. No translated vertex shader declares a resource.
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_resources_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::resource_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();
} // namespace MotionVectorPatches
