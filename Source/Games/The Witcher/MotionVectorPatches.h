#pragma once

#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo 2.87.3's, as in Mass Effect 2007 (same wrapper); validated offline on every
// dumped and generated vertex shader.
namespace MotionVectorPatches
{
   // dgVoodoo mirrors every D3D9 vertex shader constant into vc4 (c<N> at cb4[N + 20]). D3DX assigns the registers per effect
   // ("VertexShaderRegisters.h"), so no row filters the vertex shaders that project with the camera. The second run reads the
   // previous frame's copy at b10. vc3 holds dgVoodoo's own per-device constants, the same in both runs.
   constexpr uint32_t object_slot = 4;
   constexpr uint32_t object_row_offset = 20;
   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{object_slot, 10}};
   // c0.xy: the upscaler's projection jitter in NDC, added to SV_Position after its unjittered copy, so motion vectors never contain it
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 at t64-t79 (no translated vertex shader declares a resource)
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the translated shaders use, within SM4's 16 vertex outputs
   constexpr uint32_t current_position_register = 14;
   constexpr uint32_t previous_position_register = 15;
   // Past D3D9's 4 simultaneous targets, within SM4's 8
   constexpr uint32_t target_slot = 4;

   constexpr MotionVectorPatch::Layout layout = {object_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot};
} // namespace MotionVectorPatches
