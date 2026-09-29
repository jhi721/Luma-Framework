#pragma once

#include <bit>
#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's
// "motion_vector_patch.h"; this file holds the game's slots and registers).
// Vertex shaders run twice: the second run reads the previous frame's per-draw constants (vc4: view projection, world matrix, bone
// palette) from another slot, and only its SV_Position is kept, as an extra output. The first run's SV_Position is also copied to an
// extra output, as outputs can't be read back. Ported from Saints Row IV Re-Elected.
// Pixel shaders write the UV space delta from the current to the previous position to an extra target.
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
   // The jitter buffer. c0.xy: the upscaler's projection jitter in NDC, added to SV_Position after its unjittered copy, so motion
   // vectors never contain it.
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

   using namespace DXBC; // The container toolkit (Core's dxbc.h)

   // A pixel shader's container, signatures and program, checked for the patches below: game targets only, below "target_slot"
   // (depth writers, dgVoodoo's depth restores, refuse: a target declared after oDepth), inputs below the added ones, one final ret
   struct PixelShader
   {
      std::vector<Chunk> chunks;
      Chunk* program = nullptr;
      Chunk* input_signature = nullptr;
      Chunk* output_signature = nullptr;
      std::vector<SignatureElement> inputs, outputs;
      SignatureElement target;
      std::vector<uint32_t> tokens;
      std::vector<Instruction> instructions;
      size_t first_body = 0;
   };

   inline bool ReadPixelShader(const uint8_t* code, size_t size, PixelShader* shader, std::string* error)
   {
      if (!ReadChunks(code, size, &shader->chunks))
         return (*error = "container", false);
      shader->program = FindChunk(&shader->chunks, FourCC("SHEX"), FourCC("SHDR"));
      shader->input_signature = FindChunk(&shader->chunks, FourCC("ISGN"));
      shader->output_signature = FindChunk(&shader->chunks, FourCC("OSGN"));
      if (!shader->program || !shader->input_signature || !shader->output_signature || !ReadSignature(shader->input_signature->data, &shader->inputs) || !ReadSignature(shader->output_signature->data, &shader->outputs))
         return (*error = "chunks", false);
      // Targets are known by name, their system value is left undefined in the signature
      const auto is_target = [](const SignatureElement& element)
      { return _stricmp(element.name.c_str(), "SV_Target") == 0; };
      const auto target = std::ranges::find_if(shader->outputs, is_target);
      if (target == shader->outputs.end() || std::ranges::any_of(shader->outputs, [&](const SignatureElement& element)
                                                { return !is_target(element) || element.reg >= target_slot; }) ||
          std::ranges::any_of(shader->inputs, [](const SignatureElement& element)
             { return element.reg >= current_position_register; }))
         return (*error = "signatures", false);
      shader->target = *target;

      if (!ReadProgram(*shader->program, &shader->tokens, &shader->instructions, &shader->first_body))
         return (*error = "lengths", false);
      const auto& instructions = shader->instructions;
      if (shader->first_body >= instructions.size() || instructions.back().opcode != D3D10_SB_OPCODE_RET || std::ranges::any_of(instructions.begin() + shader->first_body, instructions.end() - 1, [](const Instruction& instruction)
                                                                                                               { return instruction.opcode == D3D10_SB_OPCODE_RET || instruction.opcode == D3D10_SB_OPCODE_RETC; }))
         return (*error = "returns", false);
      return true;
   }

   // The pixel shader with the motion vector target, or empty if unpatchable
   inline std::vector<uint8_t> PatchPixelShader(const uint8_t* code, size_t size, std::string* error)
   {
      PixelShader shader;
      return ReadPixelShader(code, size, &shader, error) ? MotionVectorPatch::PatchPixelShader(code, size, layout, error) : std::vector<uint8_t>();
   }

   // The pixel shader with the mask target: the color goes to a temp, copied to the target before the final ret, then the draw's
   // mask to "reactive_slot": "additive" (blended ONE onto the scene) x = reactivity max(r, g, b) * a (the color part tonemapped
   // x / (1 + x) so HDR stays within 0-1) and y = 0 (an unwritten component is undefined; the max blend keeps what another draw
   // wrote), else (blended INV_SRC_ALPHA) x and y = a, reactivity (thresholded by the fill: water, fountains) and transparency &
   // composition (faint glass and haze). Empty if unpatchable.
   inline std::vector<uint8_t> PatchPixelShaderReactive(const uint8_t* code, size_t size, bool additive, std::string* error)
   {
      PixelShader shader;
      if (!ReadPixelShader(code, size, &shader, error))
         return {};
      auto& [chunks, program, input_signature, output_signature, inputs, outputs, target, tokens, instructions, first_body] = shader;
      if (outputs.size() != 1)
         return (*error = "targets", std::vector<uint8_t>());
      const size_t last_output = FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_OUTPUT});
      if (last_output == first_body)
         return (*error = "no outputs", std::vector<uint8_t>());
      uint32_t temp;
      std::vector<uint32_t> declarations = CopyDeclarations(tokens, instructions, first_body, 2, &temp, [&](size_t i)
         {
            std::vector<uint32_t> added;
            if (i == last_output)
               added.insert(added.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(3), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, mask_xy), reactive_slot});
            return added; });

      // The body with the target retyped to the color temp (outputs are only ever destinations)
      const uint32_t color = temp;
      const uint32_t scratch = temp + 1;
      for (size_t i = first_body; i + 1 < instructions.size(); i++)
      {
         const Instruction& instruction = instructions[i];
         const size_t offset = declarations.size();
         declarations.insert(declarations.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
         bool supported = true;
         const bool walked = WalkOperands(tokens, instruction, [&](size_t token_position, size_t index_position)
            {
               uint32_t& token = declarations[offset + token_position - instruction.begin];
               if (DECODE_D3D10_SB_OPERAND_TYPE(token) != D3D10_SB_OPERAND_TYPE_OUTPUT)
                  return true;
               if (index_position == no_index || DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, token) != D3D10_SB_OPERAND_INDEX_IMMEDIATE32 || tokens[index_position] != target.reg)
                  return supported = false;
               token = (token & ~D3D10_SB_OPERAND_TYPE_MASK) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_TEMP);
               declarations[offset + index_position - instruction.begin] = color;
               return true; });
         if (!walked || !supported)
            return (*error = "operands", std::vector<uint8_t>());
      }

      constexpr uint32_t mask_x = D3D10_SB_OPERAND_4_COMPONENT_MASK_X;
      constexpr uint32_t mov = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5);
      constexpr uint32_t max = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAX) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(7);
      constexpr uint32_t immediate = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      constexpr uint32_t one = std::bit_cast<uint32_t>(1.f);
      std::vector<uint32_t> reactive = {mov, Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL), target.reg, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), color};
      if (additive)
      {
         // o5.y = 0
         reactive.insert(reactive.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(8), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_Y), reactive_slot, immediate, 0, 0, 0, 0});
         // scratch.x = max(r, g, b, 0), scratch.y = scratch.x + 1, o5.x = saturate(scratch.x / scratch.y * a)
         reactive.insert(reactive.end(), {
                                            max,
                                            Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_x),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            color,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 1, 1, 1, 1),
                                            color,
                                            max,
                                            Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_x),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 2, 2, 2, 2),
                                            color,
                                            ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAX) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10),
                                            Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_x),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            scratch,
                                            immediate,
                                            0,
                                            0,
                                            0,
                                            0,
                                            ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_ADD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10),
                                            Destination(D3D10_SB_OPERAND_TYPE_TEMP, D3D10_SB_OPERAND_4_COMPONENT_MASK_Y),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            scratch,
                                            immediate,
                                            one,
                                            one,
                                            one,
                                            one,
                                            ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DIV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(7),
                                            Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_x),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 1, 1, 1, 1),
                                            scratch,
                                            ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(7),
                                            Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, mask_x),
                                            reactive_slot,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 0, 0, 0),
                                            scratch,
                                            Source(D3D10_SB_OPERAND_TYPE_TEMP, 3, 3, 3, 3),
                                            color,
                                         });
      }
      else
      {
         reactive.insert(reactive.end(), {mov | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, mask_xy), reactive_slot, Source(D3D10_SB_OPERAND_TYPE_TEMP, 3, 3, 3, 3), color});
      }
      declarations.insert(declarations.end(), reactive.begin(), reactive.end());
      declarations.insert(declarations.end(), tokens.begin() + instructions.back().begin, tokens.end());
      WriteProgram(&declarations, program);

      // A target like the game's (signature masks are plain component bits, x = 1, unlike operand token masks)
      SignatureElement output = target;
      output.semantic_index = reactive_slot;
      output.reg = reactive_slot;
      output.mask = 0x3;
      output.rw_mask = 0;
      outputs.push_back(output);
      output_signature->data = WriteSignature(outputs);
      return WriteChunks(chunks);
   }
} // namespace MotionVectorPatches
