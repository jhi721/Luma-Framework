#pragma once

#include <bit>
#include <utility>

#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's
// "motion_vector_patch.h"; this file holds the game's slots and registers).
// Vertex shaders run twice: the second run reads the previous frame's per-draw constants (vc2: view projection, world matrix, tree
// wind; vc3: bone palette) and resources from other slots, and only its SV_Position is kept, as an extra output. The first run's
// SV_Position is also copied to an extra output, as outputs can't be read back.
// Pixel shaders write the UV space delta from the current to the previous position to an extra target.
namespace MotionVectorPatches
{
   // Per-draw constant buffers (vc2 object/camera, vc3 bones), and the slots of the previous frame's copies the second run reads.
   // Every other VS constant that moves vertices is per frame or per material (vc0, vc1, vc5, vc8: particles, impostors, rain).
   constexpr uint32_t object_slot = 2;
   constexpr std::pair<uint32_t, uint32_t> previous_slots[] = {{object_slot, 10}, {3, 11}};
   // The jitter buffer. c0.xy: the upscaler's projection jitter in NDC, added to SV_Position after its unjittered copy, so motion
   // vectors never contain it; c0.zw: the render sub-rect's share of the target (see "PatchScreenUVPixelShader").
   constexpr uint32_t jitter_slot = 9;
   // The second run reads t0-t15 at t64-t79, bound to the current resources (no motion vector VS reads one that changes per frame)
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the game uses (vertex outputs end at o10, pixel inputs at v10), within SM4's 16 vertex outputs
   // (D3D10_VS_OUTPUT_REGISTER_COUNT)
   constexpr uint32_t current_position_register = 14;
   constexpr uint32_t previous_position_register = 15;
   // Past the game's targets (the G-buffer writes 2-3, the material pass 1, only 2 shaders reach o3), within SM4's 8
   constexpr uint32_t target_slot = 4;

   // For Core's patches ("motion_vector_patch.h")
   constexpr MotionVectorPatch::Layout layout = {object_slot, previous_slots, jitter_slot, resource_slots, previous_resources_slot,
      current_position_register, previous_position_register, target_slot};

   using namespace DXBC; // The container toolkit (Core's dxbc.h)

   // Render scale (sub-rect rendering): a pixel shader whose screen texture reads address the full target gets them scaled to
   // the sub-rect by the jitter buffer's share (cb9[0].zw, see "Luma_SR4_SubRectQuad.hlsl"):
   // - one that makes its screen UV from NDC itself, "mad rX.xy, rY.xyxx, l(0.5, -0.5, ..), l(0.5, 0.5, ..)", gets
   //   "mul rX.xy, rX.xyxx, cb9[0].zwzz" after it;
   // - every read of the "texture_slots" (a bit per t slot, none: no_texture) at a register coordinate reads at
   //   "mul rNew.xy, coordinate, cb9[0].zwzz";
   // - the stipple DSF (material pass, "IR_Stipple_Pattern_Offset"): "mad rB.xyzw, cb4[9].xyxy, l(0.9, ..), rA.xzxz" makes the
   //   viewport UV plus 0.9 texel, the next instruction the texel from rB ("IR_Pixel_Steps.zw", the render size under the sub-rect).
   //   rB is then only compared with the samples' UVs over the full texture as distance weights, all zero from sub-rect UVs (black),
   //   so after that texel instruction rB = (rB - 0.9 * cb4[9].xy) * cb9[0].zw + 0.9 * cb4[9].xy.
   // Empty if nothing matched ("no screen uv") or unpatchable (e.g. no constant buffer declaration to copy for the jitter buffer's).
   constexpr uint32_t no_texture = 0;
   inline std::vector<uint8_t> PatchScreenUVPixelShader(const uint8_t* code, size_t size, uint32_t texture_slots, std::string* error)
   {
      std::vector<Chunk> chunks;
      if (!ReadChunks(code, size, &chunks))
         return (*error = "container", std::vector<uint8_t>());
      Chunk* const program = FindChunk(&chunks, FourCC("SHEX"), FourCC("SHDR"));
      if (!program)
         return (*error = "chunks", std::vector<uint8_t>());
      std::vector<uint32_t> tokens;
      std::vector<Instruction> instructions;
      size_t first_body;
      if (!ReadProgram(*program, &tokens, &instructions, &first_body))
         return (*error = "lengths", std::vector<uint8_t>());
      const size_t constant_buffer = FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER});
      if (constant_buffer == first_body || instructions[constant_buffer].length != 4)
         return (*error = "no constant buffer", std::vector<uint8_t>());
      for (size_t i = 0; i < first_body; i++)
      {
         if (instructions[i].opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER && tokens[instructions[i].begin + 2] == jitter_slot)
            return (*error = "slot taken", std::vector<uint8_t>());
      }

      constexpr uint32_t immediate = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      const auto is_ndc_to_uv = [&](const Instruction& instruction)
      {
         const uint32_t* t = tokens.data() + instruction.begin;
         return instruction.length == 15 && t[0] == (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(15)) &&
                t[1] == Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy) && t[3] == Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0) &&
                t[5] == immediate && t[6] == std::bit_cast<uint32_t>(0.5f) && t[7] == std::bit_cast<uint32_t>(-0.5f) &&
                t[10] == immediate && t[11] == std::bit_cast<uint32_t>(0.5f) && t[12] == std::bit_cast<uint32_t>(0.5f);
      };
      // A texture read (sample*, ld, gather4) of one of the "texture_slots": the position of its coordinate operand (2 tokens, a 1D
      // register without modifiers), else 0
      const auto screen_texture_coordinate = [&](const Instruction& instruction) -> size_t
      {
         if (texture_slots == no_texture || (instruction.opcode != D3D10_SB_OPCODE_SAMPLE && instruction.opcode != D3D10_SB_OPCODE_SAMPLE_L && instruction.opcode != D3D10_SB_OPCODE_SAMPLE_B && instruction.opcode != D3D10_SB_OPCODE_SAMPLE_D && instruction.opcode != D3D10_SB_OPCODE_SAMPLE_C && instruction.opcode != D3D10_SB_OPCODE_SAMPLE_C_LZ && instruction.opcode != D3D10_SB_OPCODE_LD && instruction.opcode != D3D10_1_SB_OPCODE_GATHER4))
            return 0;
         std::vector<size_t> operands;
         if (!WalkOperands(tokens, instruction, [&](size_t token_position, size_t)
                {
                   operands.push_back(token_position);
                   return true; }) ||
             operands.size() < 3)
            return 0;
         const uint32_t coordinate = tokens[operands[1]], resource = tokens[operands[2]];
         const D3D10_SB_OPERAND_TYPE coordinate_type = DECODE_D3D10_SB_OPERAND_TYPE(coordinate);
         if (DECODE_D3D10_SB_OPERAND_TYPE(resource) != D3D10_SB_OPERAND_TYPE_RESOURCE || tokens[operands[2] + 1] >= 32 || (texture_slots & (1u << tokens[operands[2] + 1])) == 0 || (coordinate_type != D3D10_SB_OPERAND_TYPE_INPUT && coordinate_type != D3D10_SB_OPERAND_TYPE_TEMP) || DECODE_IS_D3D10_SB_OPERAND_EXTENDED(coordinate) || DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(coordinate) != D3D10_SB_OPERAND_INDEX_1D || operands[2] != operands[1] + 2)
            return 0;
         return operands[1];
      };
      constexpr uint32_t mad = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(13);
      constexpr uint32_t mask_xyzw = D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
      constexpr uint32_t bias = std::bit_cast<uint32_t>(0.9f);
      const auto is_stipple_uv = [&](const Instruction& instruction)
      {
         const uint32_t* t = tokens.data() + instruction.begin;
         return instruction.length == 13 && t[0] == mad && t[1] == Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xyzw) && DECODE_D3D10_SB_OPERAND_TYPE(t[3]) == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER &&
                t[4] == 4 && t[5] == 9 && t[6] == immediate && t[7] == bias && t[8] == bias && t[9] == bias && t[10] == bias;
      };
      const auto cb9_swizzle = [](uint32_t x, uint32_t y, uint32_t z, uint32_t w)
      { return ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(x, y, z, w) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_2D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(1, D3D10_SB_OPERAND_INDEX_IMMEDIATE32); };
      const uint32_t cb9_zw = cb9_swizzle(2, 3, 2, 2);
      constexpr uint32_t mul = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MUL) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(8);
      uint32_t scaled;
      std::vector<uint32_t> patched = CopyDeclarations(tokens, instructions, first_body, texture_slots == no_texture ? 0 : 1, &scaled, [&](size_t i)
         {
            std::vector<uint32_t> added;
            if (i == constant_buffer)
            {
               const Instruction& instruction = instructions[i];
               added.assign(tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
               added[0] &= ~D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN_MASK;
               added[2] = jitter_slot;
               added[3] = 1;
            }
            return added; });
      bool found = false;
      for (size_t i = first_body; i < instructions.size(); i++)
      {
         const Instruction& instruction = instructions[i];
         if (const size_t coordinate = screen_texture_coordinate(instruction))
         {
            patched.insert(patched.end(), {mul, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy), scaled, tokens[coordinate], tokens[coordinate + 1], cb9_zw, jitter_slot, 0});
            const size_t position = patched.size() + (coordinate - instruction.begin);
            patched.insert(patched.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
            patched[position] = Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0);
            patched[position + 1] = scaled;
            found = true;
            continue;
         }
         patched.insert(patched.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
         if (is_stipple_uv(instruction) && i + 1 < instructions.size())
         {
            // The texel instruction reads rB first ("mad rT.xy, rB.zwzz, cb4[9].zwzz, ..")
            const Instruction& texel = instructions[i + 1];
            const uint32_t uv = tokens[instruction.begin + 2];
            if (texel.opcode != D3D10_SB_OPCODE_MAD || texel.length < 5 || DECODE_D3D10_SB_OPERAND_TYPE(tokens[texel.begin + 3]) != D3D10_SB_OPERAND_TYPE_TEMP || tokens[texel.begin + 4] != uv)
               continue;
            patched.insert(patched.end(), tokens.begin() + texel.begin, tokens.begin() + texel.begin + texel.length);
            const uint32_t* c9 = tokens.data() + instruction.begin + 3;
            constexpr uint32_t negative_bias = std::bit_cast<uint32_t>(-0.9f);
            patched.insert(patched.end(), {mad, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xyzw), uv, c9[0], c9[1], c9[2], immediate, negative_bias, negative_bias, negative_bias, negative_bias, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), uv,
                                             mul, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xyzw), uv, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), uv, cb9_swizzle(2, 3, 2, 3), jitter_slot, 0,
                                             mad, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xyzw), uv, c9[0], c9[1], c9[2], immediate, bias, bias, bias, bias, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), uv});
            i++;
            found = true;
            continue;
         }
         if (!is_ndc_to_uv(instruction))
            continue;
         const uint32_t uv = tokens[instruction.begin + 2];
         patched.insert(patched.end(), {mul, Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy), uv, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0), uv, cb9_zw, jitter_slot, 0});
         found = true;
      }
      if (!found)
         return (*error = "no screen uv", std::vector<uint8_t>());
      WriteProgram(&patched, program);
      return WriteChunks(chunks);
   }
} // namespace MotionVectorPatches
