#pragma once

#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's
// "motion_vector_patch.h"; this file holds the game's slots and registers).
// Vertex shaders run twice: the second run reads the previous frame's per-draw constants (UE3's b0 $Globals: LocalToWorld, material
// parameters; b1 VSOffsetConstants: ViewProjectionMatrix, CameraPosition; b3 VSBoneConstants: the bone palette) from other slots, and
// only its SV_Position is kept, as an extra output. The first run's SV_Position is also copied to an extra output, as outputs can't be
// read back. Pixel shaders write the UV space delta from the current to the previous position to an extra target.
// Borderlands GOTY Enhanced's patch (same UE3 slots). Tested offline on each game's full shader cache, vertex shaders equal on WARP
// stream-out with no jitter mismatch (the rest have no b1: shadow depth, screen quads), pixel shaders created: ME1 18723 of 20278 VS,
// 78186 of 78188 PS; ME2 27593 of 29867 VS, 108562 of 108564 PS; ME3 28349 of 30293 VS, 150326 of 150328 PS.
namespace MotionVectorPatches
{
   // The camera buffer every projecting vertex shader declares: the jitter buffer's declaration goes after it. A shader without it
   // (shadow depth with its own ProjectionMatrix, screen quads) is refused.
   constexpr uint32_t object_slot = 1;
   // Per-draw constant buffers (b0 object, b1 camera, b3 bones), and the slots of the previous frame's copies the second run reads
   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{0, 10}, {object_slot, 8}, {3, 11}};
   // The jitter buffer. c0.xy: the upscaler's projection jitter in NDC, added to SV_Position after its unjittered copy, so motion
   // vectors never contain it.
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 at t64-t79 (no game vertex shader reads a resource: unused here, kept for the generic patch)
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the game uses (vertex outputs end at o11, pixel inputs at v11), within SM5's 32
   constexpr uint32_t current_position_register = 30;
   constexpr uint32_t previous_position_register = 31;
   // Past the game's targets (pixel shaders write o0-o2)
   constexpr uint32_t target_slot = 4;

   // For Core's patches ("motion_vector_patch.h")
   constexpr MotionVectorPatch::Layout layout = {object_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot};
} // namespace MotionVectorPatches
