#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"
#include "..\..\Core\includes\motion_vector_patch.h"

// Motion vectors for a game that renders none, by patching whole DXBC containers (Core's "motion_vector_patch.h"; this file holds
// the game's slots). Vertex shaders run twice: the second run reads the previous frame's per-draw constants from another slot, and
// only its SV_Position is kept, as an extra output. Pixel shaders write the UV space delta from the current to the previous
// position to an extra target. The slots are dgVoodoo's ("MotionVectorPatch::DgVoodoo"), as in Mass Effect 2007 (same wrapper).
namespace MotionVectorPatches
{
   // The D3D9 compiler places each shader's parameters itself (cooked CTAB census, NOTES.md "DLAA groundwork"): ViewProjectionMatrix
   // and LocalToWorld sit at c4/c0 (static meshes), c229/c225 (GPU skin), c13/c5, c17/c5 (terrain, decals) and more. So no row
   // filters the vertex shaders (unlike Mass Effect 2007's "view_projection_row"): each one's matrices are found in its bytecode
   // ("MatrixRegisters").
   using MotionVectorPatch::DgVoodoo::jitter_slot;
   using MotionVectorPatch::DgVoodoo::object_row_offset;
   using MotionVectorPatch::DgVoodoo::object_slot;
   using MotionVectorPatch::DgVoodoo::previous_slots;
   using MotionVectorPatch::DgVoodoo::target_slot;

   // FSR's masks (x reactive, y transparency & composition), written by the scene's alpha blended draws (their pixel shaders patched
   // with Core's "PatchPixelShaderReactive")
   constexpr uint32_t reactive_slot = target_slot + 1;

   constexpr MotionVectorPatch::Layout layout = MotionVectorPatch::DgVoodoo::MakeLayout();

   // The D3D9 registers (vc4 row - "object_row_offset") of the 4x4 matrices the vertex shader multiplies a vector by: rows N to N+3
   // each times component x, y, z, w of one register, by immediate index (the translated "mul r1, v0.yyyy, cb4[N+1]; mad ...
   // cb4[N], v0.xxxx; ..."). Row vector matrices, as UE3 uploads them. Relatively indexed rows (bone palettes) don't count, nor
   // does a skinned shader's LocalToWorld (its translation row is added, not multiplied). Ascending; empty if the bytecode can't be
   // read.
   inline std::vector<uint32_t> MatrixRegisters(const uint8_t* code, size_t size)
   {
      std::vector<uint32_t> registers;
      std::vector<DXBC::Chunk> chunks;
      if (!DXBC::ReadChunks(code, size, &chunks))
         return registers;
      const DXBC::Chunk* const program = DXBC::FindChunk(&chunks, DXBC::FourCC("SHEX"), DXBC::FourCC("SHDR"));
      std::vector<uint32_t> tokens;
      std::vector<DXBC::Instruction> instructions;
      size_t first_body;
      if (!program || !DXBC::ReadProgram(*program, &tokens, &instructions, &first_body))
         return registers;

      // Per broadcast register (type and index): the rows multiplied by each of its components, as row * 4 + component
      std::map<uint64_t, std::vector<uint32_t>> products;
      for (size_t i = first_body; i < instructions.size(); i++)
      {
         const DXBC::Instruction& instruction = instructions[i];
         if (instruction.opcode != D3D10_SB_OPCODE_MUL && instruction.opcode != D3D10_SB_OPCODE_MAD)
            continue;
         uint32_t row = UINT32_MAX;
         uint64_t broadcast = UINT64_MAX;
         uint32_t component = 0;
         bool destination = true, single = true;
         DXBC::WalkOperands(tokens, instruction, [&](size_t token_position, size_t index_position)
            {
               const uint32_t token = tokens[token_position];
               if (std::exchange(destination, false))
                  return true;
               const D3D10_SB_OPERAND_TYPE type = DECODE_D3D10_SB_OPERAND_TYPE(token);
               if (type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
               {
                  const bool immediate = DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(token) == D3D10_SB_OPERAND_INDEX_2D && DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(1, token) == D3D10_SB_OPERAND_INDEX_IMMEDIATE32;
                  if (immediate && index_position != DXBC::no_index && tokens[index_position] == object_slot && row == UINT32_MAX)
                  {
                     row = tokens[index_position + 1];
                  }
                  else
                  {
                     single = false;
                  }
                  return true;
               }
               if ((type != D3D10_SB_OPERAND_TYPE_TEMP && type != D3D10_SB_OPERAND_TYPE_INPUT) || index_position == DXBC::no_index ||
                   DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token) != D3D10_SB_OPERAND_4_COMPONENT)
                  return true;
               // One component, replicated (".yyyy") or selected
               uint32_t selected = 0;
               switch (DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(token))
               {
               case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE:
                  selected = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(token);
                  break;
               case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE:
                  selected = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(token, 0);
                  for (uint32_t c = 1; c < 4; c++)
                  {
                     if (DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(token, c) != selected)
                        return true;
                  }
                  break;
               default:
                  return true;
               }
               if (broadcast != UINT64_MAX)
               {
                  single = false;
               }
               broadcast = (uint64_t(type) << 32) | tokens[index_position];
               component = selected;
               return true; });
         if (single && row != UINT32_MAX && broadcast != UINT64_MAX && row >= object_row_offset)
         {
            products[broadcast].push_back(row * 4 + component);
         }
      }
      for (auto& [broadcast, rows] : products)
      {
         std::ranges::sort(rows);
         for (const uint32_t first : rows)
         {
            if (first % 4 == 0 && std::ranges::all_of(std::initializer_list<uint32_t>{1, 2, 3}, [&](uint32_t k)
                                     { return std::ranges::binary_search(rows, first + k * 4 + k); }))
            {
               registers.push_back(first / 4 - object_row_offset);
            }
         }
      }
      std::ranges::sort(registers);
      registers.erase(std::unique(registers.begin(), registers.end()), registers.end());
      return registers;
   }
} // namespace MotionVectorPatches
