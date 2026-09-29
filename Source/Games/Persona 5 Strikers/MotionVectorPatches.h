#pragma once

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's
// "motion_vector_patch.h"; this file holds the game's slots and registers).
// Vertex shaders run twice: the second run reads the previous frame's $Globals (cb0: view projection, world matrix or bone palette)
// and resources from other slots, and only its SV_Position is kept, as an extra output. The first run's SV_Position is also copied to
// an extra output, as outputs can't be read back.
// Pixel shaders write the UV space delta from the current to the previous position to an extra target.
namespace MotionVectorPatches
{
   // $Globals, and the slot of the previous frame's copy the second run reads
   constexpr uint32_t globals_slot = 0;
   constexpr uint32_t previous_globals_slot = 10;
   // Upscaler projection jitter in NDC (c0.xy), added to SV_Position after its unjittered copy, so motion vectors never contain it
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 (wind and interaction buffers, ocean maps...) from these slots, which hold the previous frame's copies
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the game uses (vertex outputs end at o12; pixel shader system value inputs follow the interpolants)
   constexpr uint32_t current_position_register = 30;
   constexpr uint32_t previous_position_register = 31;
   // The G-buffer writes 5 targets (the water 6), the forward redraws 1
   constexpr uint32_t target_slot = 6;

   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{globals_slot, previous_globals_slot}};
   // For Core's patches ("motion_vector_patch.h"): the jitter buffer is declared after $Globals
   constexpr MotionVectorPatch::Layout layout = {globals_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot};
} // namespace MotionVectorPatches
