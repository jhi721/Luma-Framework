#pragma once

#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo's, as in Mass Effect 2007 (same wrapper).
namespace MotionVectorPatches
{
   // dgVoodoo mirrors every D3D9 vertex shader constant into vc4 (c<N> at cb4[N + 20]). REDengine's material vertex shaders
   // (CTAB census of CookedPC/shader.cache): LocalToWorld c0-c2 (3x4, column vectors),
   // WorldToScreen c4-c7, WorldToView c8-c10, the camera c16-c19, wind and time c35-c47, bones c70-c249 (3 rows each, world
   // space). The second run reads the previous frame's copy at b10. vc3 holds dgVoodoo's own per-device constants, the same in both
   // runs.
   constexpr uint32_t object_slot = 4;
   constexpr uint32_t object_row_offset = 20;
   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{object_slot, 10}};
   // WorldToScreen's first row (c4): vertex shaders that never read it (full screen passes, shadow and particle shaders with their
   // own projection) are refused
   constexpr uint32_t view_projection_row = object_row_offset + 4;
   // c0.xy: the upscaler's projection jitter in NDC, added to SV_Position after its unjittered copy, so motion vectors never contain it
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 at t64-t79: the vertex shaders that sample (the terrain's heightmap) read the previous frame's there
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the translated shaders use (vertex outputs end at o12), within SM4's 16 vertex outputs
   // (D3D10_VS_OUTPUT_REGISTER_COUNT), which dgVoodoo 2.81.3 emits
   constexpr uint32_t current_position_register = 14;
   constexpr uint32_t previous_position_register = 15;
   // Past the G-buffer's 4 targets (albedo, specular, normals, linear depth), within SM4's 8
   constexpr uint32_t target_slot = 4;
   // FSR's masks (x reactive, y transparency & composition), written by the scene's alpha blended draws (their pixel shaders
   // patched with Core's "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = 5;

   constexpr MotionVectorPatch::Layout layout = {object_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot, view_projection_row};
} // namespace MotionVectorPatches
