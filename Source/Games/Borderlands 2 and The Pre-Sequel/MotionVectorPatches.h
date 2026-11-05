#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo's ("MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same wrapper).
namespace MotionVectorPatches
{
   // UE3 (Common.usf, CTAB census of the RefShaderCache in NOTES.md): translated view projection c0-c3, camera position c4,
   // PreViewTranslation c5, LocalToWorld c6-c9 (skinned: bones c6-c230, LocalToWorld c231-c234).
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_resources_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::resource_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // The first vc4 row of the view projection (c0): vertex shaders that never read it don't draw the scene's camera view, and are
   // refused
   constexpr uint32_t view_projection_row = object_row_offset;
   // FSR's masks (x reactive, y transparency & composition), written by the scene's alpha blended draws (their pixel shaders
   // patched with Core's "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout(view_projection_row);
} // namespace MotionVectorPatches
