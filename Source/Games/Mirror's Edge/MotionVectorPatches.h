#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// The game's slots for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same engine family and wrapper). The second vertex shader run reads the
// previous frame's per-draw constants (vc4: view projection, world matrix, bone palette) from another slot.
namespace MotionVectorPatches
{
   // vc4 (CTAB census of the RefShaderCache, NOTES.md): view projection c0-c3 in every scene vertex shader (not
   // translated), camera position c4, world matrix c5-c8 (skinned: bones c5-c229, world matrix c230-c233). Offline mvcheck PASS on
   // dgVoodoo 2.87.5 and 2.81.3.
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // The first vc4 row of the view projection (c0): vertex shaders that never read it (shadow depths, full screen passes) are refused
   constexpr uint32_t view_projection_row = object_row_offset;
   // The reactive masks, written by the scene's alpha blended draws (their pixel shaders patched with "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout(view_projection_row);
} // namespace MotionVectorPatches
