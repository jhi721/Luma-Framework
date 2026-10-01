#pragma once

#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// The game's slots and registers for Core's DXBC motion vector patch (described in "motion_vector_patch.h"), ported from
// Saints Row IV Re-Elected. The second vertex shader run reads the previous frame's per-draw constants (vc4: view projection,
// world matrix, bone palette) from another slot.
namespace MotionVectorPatches
{
   // dgVoodoo mirrors every D3D9 vertex shader constant into vc4 (c<N> at cb4[N + 20]): view projection c0-c3, camera position c4,
   // world matrix c5-c8 (skinned: bones c5-c229, world matrix c230-c233). The second run reads the previous frame's copy at b10.
   // vc3 holds dgVoodoo's own per-device constants (viewport, texture coordinate masks), the same in both runs.
   constexpr uint32_t object_slot = 4;
   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{object_slot, 10}};
   // The first vc4 row of the view projection (c0). Vertex shaders that never read it (shadow and LocalToView projections, the 29 of
   // 420 in the shader cache that don't place vertices with it) are refused: they don't draw the scene's camera view.
   constexpr uint32_t view_projection_row = 20;
   // The jitter buffer (see "MotionVectorPatch::Layout")
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 at t64-t79 (no translated vertex shader declares a resource)
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the translated shaders use (vertex outputs end at o12, pixel inputs at v12), within SM4's 16 vertex outputs
   // (D3D10_VS_OUTPUT_REGISTER_COUNT), which dgVoodoo 2.81.3 emits
   constexpr uint32_t current_position_register = 14;
   constexpr uint32_t previous_position_register = 15;
   // Past D3D9's 4 simultaneous targets (every dumped pixel shader writes o0 only), within SM4's 8
   constexpr uint32_t target_slot = 4;
   // FSR's reactive mask, written by the scene's alpha blended draws (their pixel shaders patched with "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = 5;

   // For Core's patches ("motion_vector_patch.h"; its pixel shader patch only on the shaders "ReadPixelShader" accepts)
   constexpr MotionVectorPatch::Layout layout = {object_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot, view_projection_row};

   // The pixel shader with the motion vector target, or empty if unpatchable
   inline std::vector<uint8_t> PatchPixelShader(const uint8_t* code, size_t size, std::string* error)
   {
      MotionVectorPatch::PixelShader shader;
      return MotionVectorPatch::ReadPixelShader(code, size, layout, &shader, error) ? MotionVectorPatch::PatchPixelShader(code, size, layout, error) : std::vector<uint8_t>();
   }
} // namespace MotionVectorPatches
