#pragma once

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include "..\..\Core\includes\dxbc.h"

namespace OutputClamp
{
   // The clamp the game's vanilla UNORM targets applied to a pixel shader's outputs before blending, which the fp16 mirrors drop:
   // o0's color floored at 0 (its highlights above 1 are the HDR), its alpha (the blend weight: above 1
   // the SRC_ALPHA / INV_SRC_ALPHA blend Aurora draws everything with pushes the color below 0) and the other targets (the depth of
   // field CoC) saturated. The outputs are written to temps, clamped into the outputs before the final ret (outputs can't be read).
   // False if the program can't be patched.
   inline bool PatchOutputClamp(DXBC::Chunk* program)
   {
      using namespace DXBC;
      std::vector<uint32_t> tokens;
      std::vector<Instruction> instructions;
      size_t first_body;
      // dgVoodoo's SM5 translations of the game's pixel shaders; its own blits, clears and present are SM4
      if (!ReadProgram(*program, &tokens, &instructions, &first_body) || DECODE_D3D10_SB_TOKENIZED_PROGRAM_TYPE(tokens[0]) != D3D10_SB_PIXEL_SHADER || DECODE_D3D10_SB_TOKENIZED_PROGRAM_MAJOR_VERSION(tokens[0]) != 5 ||
          first_body >= instructions.size() || instructions.back().opcode != D3D10_SB_OPCODE_RET)
         return false;
      if (std::ranges::any_of(instructions.begin() + first_body, instructions.end() - 1, [](const Instruction& instruction)
             { return instruction.opcode == D3D10_SB_OPCODE_RET || instruction.opcode == D3D10_SB_OPCODE_RETC; }))
         return false;
      // The color targets and their declared components (oDepth has its own operand type: the depth test clamps it)
      std::vector<std::pair<uint32_t, uint32_t>> outputs; // Register, component mask
      for (size_t i = 0; i < first_body; i++)
      {
         const Instruction& instruction = instructions[i];
         if (instruction.opcode == D3D10_SB_OPCODE_DCL_OUTPUT && instruction.length == 3 && DECODE_D3D10_SB_OPERAND_TYPE(tokens[instruction.begin + 1]) == D3D10_SB_OPERAND_TYPE_OUTPUT)
            outputs.push_back({tokens[instruction.begin + 2], DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(tokens[instruction.begin + 1])});
      }
      if (outputs.empty())
         return false;
      uint32_t first_temp;
      std::vector<uint32_t> patched = CopyDeclarations(tokens, instructions, first_body, uint32_t(outputs.size()), &first_temp, [](size_t)
         { return std::vector<uint32_t>(); });
      for (size_t i = first_body; i + 1 < instructions.size(); i++)
      {
         const Instruction& instruction = instructions[i];
         const size_t offset = patched.size();
         patched.insert(patched.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
         bool supported = true;
         const bool walked = WalkOperands(tokens, instruction, [&](size_t token_position, size_t index_position)
            {
               uint32_t& token = patched[offset + token_position - instruction.begin];
               if (DECODE_D3D10_SB_OPERAND_TYPE(token) != D3D10_SB_OPERAND_TYPE_OUTPUT)
                  return true;
               const auto output = index_position == no_index ? outputs.end() : std::ranges::find(outputs, tokens[index_position], &std::pair<uint32_t, uint32_t>::first);
               if (output == outputs.end() || DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, token) != D3D10_SB_OPERAND_INDEX_IMMEDIATE32)
                  return supported = false;
               token = (token & ~D3D10_SB_OPERAND_TYPE_MASK) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_TEMP);
               patched[offset + index_position - instruction.begin] = first_temp + uint32_t(output - outputs.begin());
               return true; });
         if (!walked || !supported)
            return false;
      }
      constexpr uint32_t mask_rgb = D3D10_SB_OPERAND_4_COMPONENT_MASK_X | D3D10_SB_OPERAND_4_COMPONENT_MASK_Y | D3D10_SB_OPERAND_4_COMPONENT_MASK_Z;
      constexpr uint32_t immediate = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      for (size_t k = 0; k < outputs.size(); k++)
      {
         const auto [reg, mask] = outputs[k];
         const uint32_t temp = first_temp + uint32_t(k);
         const uint32_t floored = reg == 0 ? (mask & mask_rgb) : 0;
         const uint32_t saturated = mask & ~floored;
         if (floored != 0)
            patched.insert(patched.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAX) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, floored), reg,
                                             Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), temp, immediate, 0, 0, 0, 0});
         if (saturated != 0)
            patched.insert(patched.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5),
                                             Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, saturated), reg, Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3), temp});
      }
      patched.insert(patched.end(), tokens.begin() + instructions.back().begin, tokens.end());
      WriteProgram(&patched, program);
      return true;
   }

   // A container with "PatchOutputClamp" applied, empty if it isn't a game pixel shader or can't be patched
   inline std::vector<uint8_t> ClampPixelShader(const uint8_t* code, size_t size)
   {
      std::vector<DXBC::Chunk> chunks;
      if (!DXBC::ReadChunks(code, size, &chunks))
         return {};
      DXBC::Chunk* const program = DXBC::FindChunk(&chunks, DXBC::FourCC("SHEX"), DXBC::FourCC("SHDR"));
      if (!program || !PatchOutputClamp(program))
         return {};
      return DXBC::WriteChunks(chunks);
   }
} // namespace OutputClamp
