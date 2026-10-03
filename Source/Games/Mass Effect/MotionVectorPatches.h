#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// The game's slots for Core's DXBC motion vector patch (described in "motion_vector_patch.h"; dgVoodoo's registers in
// "MotionVectorPatch::DgVoodoo"), ported from Saints Row IV Re-Elected. The second vertex shader run reads the previous frame's
// per-draw constants (vc4: view projection, world matrix, bone palette) from another slot.
namespace MotionVectorPatches
{
   // vc4: view projection c0-c3, camera position c4, world matrix c5-c8 (skinned: bones c5-c229, world matrix c230-c233). No
   // translated vertex shader declares a resource. Every dumped pixel shader writes o0 only.
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_resources_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::resource_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;
   // The first vc4 row of the view projection (c0). Vertex shaders that never read it (shadow and LocalToView projections, the 29 of
   // 420 in the shader cache that don't place vertices with it) are refused: they don't draw the scene's camera view.
   constexpr uint32_t view_projection_row = object_row_offset;
   // FSR's reactive mask, written by the scene's alpha blended draws (their pixel shaders patched with "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   // For Core's patches ("motion_vector_patch.h"; its pixel shader patch with "targets_only")
   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout(view_projection_row);
} // namespace MotionVectorPatches
