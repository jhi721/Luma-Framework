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
   constexpr char semantic_name[] = "LUMAMV";

   // For Core's vertex shader patch ("motion_vector_patch.h"; the pixel shader patches are this game's own, below)
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
      for (Chunk& chunk : shader->chunks)
      {
         if (chunk.fourcc == FourCC("SHEX") || chunk.fourcc == FourCC("SHDR"))
            shader->program = &chunk;
         else if (chunk.fourcc == FourCC("ISGN"))
            shader->input_signature = &chunk;
         else if (chunk.fourcc == FourCC("OSGN"))
            shader->output_signature = &chunk;
      }
      if (!shader->program || !shader->input_signature || !shader->output_signature || shader->program->data.size() % 4 != 0 || !ReadSignature(shader->input_signature->data, &shader->inputs) || !ReadSignature(shader->output_signature->data, &shader->outputs))
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

   // A target output like the game's, at "slot" with the "mask" components (signature masks are plain component bits, x = 1,
   // unlike operand token masks)
   inline SignatureElement AddedTarget(const PixelShader& shader, uint32_t slot, uint8_t mask)
   {
      SignatureElement output = shader.target;
      output.semantic_index = slot;
      output.reg = slot;
      output.mask = mask;
      output.rw_mask = 0;
      return output;
   }

   // The pixel shader with the motion vector target, or empty if unpatchable
   inline std::vector<uint8_t> PatchPixelShader(const uint8_t* code, size_t size, std::string* error)
   {
      PixelShader shader;
      if (!ReadPixelShader(code, size, &shader, error))
         return {};
      auto& [chunks, program, input_signature, output_signature, inputs, outputs, target, tokens, instructions, first_body] = shader;

      // The two inputs after the last input (or before the outputs), the target after the last output
      const size_t first_output = size_t(std::ranges::find_if(instructions.begin(), instructions.begin() + first_body, [](const Instruction& instruction)
                                            { return instruction.opcode == D3D10_SB_OPCODE_DCL_OUTPUT; }) -
                                         instructions.begin());
      if (first_output == first_body || first_output == 0)
         return (*error = "no outputs", std::vector<uint8_t>());
      size_t last_input = FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_INPUT_PS, D3D10_SB_OPCODE_DCL_INPUT_PS_SGV, D3D10_SB_OPCODE_DCL_INPUT_PS_SIV});
      if (last_input == first_body)
         last_input = first_output - 1;
      const size_t last_output = FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_OUTPUT});
      uint32_t temp;
      std::vector<uint32_t> declarations = CopyDeclarations(tokens, instructions, first_body, 2, &temp, [&](size_t i)
         {
            std::vector<uint32_t> added;
            if (i == last_input)
            {
               for (const uint32_t reg : {current_position_register, previous_position_register})
                  added.insert(added.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_INPUT_PS) | ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(D3D10_SB_INTERPOLATION_LINEAR) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(3), Destination(D3D10_SB_OPERAND_TYPE_INPUT, mask_xyw), reg});
            }
            if (i == last_output)
               added.insert(added.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(3), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, mask_xy), target_slot});
            return added; });

      // Before the final ret, in UV space: (previous.xy / previous.w - current.xy / current.w) * (0.5, -0.5)
      const uint32_t current = temp;
      const uint32_t previous = temp + 1;
      constexpr uint32_t div = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DIV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(7);
      const std::vector<uint32_t> motion_vector = {
         div, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy), current, Source(D3D10_SB_OPERAND_TYPE_INPUT, 0, 1, 0, 0), current_position_register, Source(D3D10_SB_OPERAND_TYPE_INPUT, 3, 3, 3, 3), current_position_register,
         div, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy), previous, Source(D3D10_SB_OPERAND_TYPE_INPUT, 0, 1, 0, 0), previous_position_register, Source(D3D10_SB_OPERAND_TYPE_INPUT, 3, 3, 3, 3), previous_position_register,
         ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_ADD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(8), Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy), current, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0), previous, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0) | ENCODE_D3D10_SB_OPERAND_EXTENDED(true), ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(D3D10_SB_OPERAND_MODIFIER_NEG), current,
         ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, mask_xy), target_slot, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0), current,
         ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(-0.5f), 0, 0};
      const size_t last = instructions.back().begin;
      declarations.insert(declarations.end(), tokens.begin() + instructions[first_body].begin, tokens.begin() + last);
      declarations.insert(declarations.end(), motion_vector.begin(), motion_vector.end());
      declarations.insert(declarations.end(), tokens.begin() + last, tokens.end());
      WriteProgram(&declarations, program);

      // Signature masks are plain component bits (x = 1), unlike the operand token masks above
      inputs.push_back({semantic_name, 0, 0, 3, current_position_register, 0xF, 0xB});
      inputs.push_back({semantic_name, 1, 0, 3, previous_position_register, 0xF, 0xB});
      input_signature->data = WriteSignature(inputs);
      outputs.push_back(AddedTarget(shader, target_slot, 0x3));
      output_signature->data = WriteSignature(outputs);
      return WriteChunks(chunks);
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

      outputs.push_back(AddedTarget(shader, reactive_slot, 0x3));
      output_signature->data = WriteSignature(outputs);
      return WriteChunks(chunks);
   }
} // namespace MotionVectorPatches
