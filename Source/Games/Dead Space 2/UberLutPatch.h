#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <vector>

#include "..\..\Core\includes\dxbc.h"

// The uber post's LUT tail, in all 517 permutations (1551 dgVoodoo translations, checked offline): "mad c, x, 0.96875, 0.015625" ->
// the 32^3 LUT(s) (1 sample in 1536 translations, a weighted blend of 2 to 6 in 15) -> "mad o.xyz, k, (lut - x), x" with k a cb4
// component (LUT amount). x is gamma encoded; the LUT's CLAMP addressing clips it to [0, 1], so vanilla lost everything above 1.
// Patched to MELE's max-channel wrap with a hard knee at 1:
//   V = saturate(x) + k * (LUT(saturate(x)) - saturate(x))   the vanilla grade of the clipped color
//   m = max(1, max3(x)),  G = p + k * (LUT(p) - p) with p = x / m   the grade of the color scaled into the cube
//   o = V * (Y(decode(G * m)) / Y(decode(V)))^(1/2.2)   decode = gamma 2.2, Y = BT.709 relative luminance
// The output keeps the vanilla RGB ratios and takes the brightness of the scaled-back grade. Where max3(x) <= 1, G and V run the same
// instructions on the same values, so o = V bit exactly.
// G runs a copy of the tail, from the coordinate mad to the lerp, on renamed temps, before the original; the temps it reads before
// writing (x, or the value x was scaled from) start as their original / m. Matched by structure (the coordinate mad's immediates, and
// a cb4-weighted mad whose addend is that coordinate's source); null for any other shader.
inline std::unique_ptr<std::byte[]> WrapUberLut(const std::byte* code, size_t* size)
{
   if (*size % sizeof(uint32_t) != 0)
      return nullptr;
   DXBC::Chunk program{.fourcc = DXBC::FourCC("SHEX"), .data = std::vector<uint8_t>(8 + *size)};
   const uint32_t header[2] = {0, uint32_t(program.data.size() / sizeof(uint32_t))};
   std::memcpy(program.data.data(), header, sizeof(header));
   std::memcpy(program.data.data() + sizeof(header), code, *size);
   std::vector<uint32_t> tokens;
   std::vector<DXBC::Instruction> instructions;
   size_t first_body = 0;
   if (!DXBC::ReadProgram(program, &tokens, &instructions, &first_body))
      return nullptr;

   // A mad's 4 operands as [begin, end) token ranges (operands[4] = the instruction's end); false for other encodings
   const auto read_mad = [&](const DXBC::Instruction& instruction, std::array<size_t, 5>* operands)
   {
      if (instruction.opcode != D3D10_SB_OPCODE_MAD || DECODE_IS_D3D10_SB_OPCODE_EXTENDED(tokens[instruction.begin]) || DECODE_IS_D3D10_SB_INSTRUCTION_SATURATE_ENABLED(tokens[instruction.begin]))
         return false;
      const size_t end = instruction.begin + instruction.length;
      size_t i = instruction.begin + 1;
      for (size_t operand = 0; operand < 4; operand++)
      {
         (*operands)[operand] = i;
         i = DXBC::WalkOperand(tokens, i, end, [](size_t, size_t)
            { return true; });
         if (i == 0)
            return false;
      }
      (*operands)[4] = i;
      return i == end;
   };
   constexpr uint32_t temp_xyzw = DXBC::Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3);
   // The temp register of a plain "rN.xyzw" source, or UINT32_MAX
   const auto temp_source = [&](size_t begin, size_t end)
   {
      return (end - begin == 2 && tokens[begin] == temp_xyzw) ? tokens[begin + 1] : UINT32_MAX;
   };
   const auto immediate4 = [&](size_t begin, size_t end, float value)
   {
      // fxc also sets the (unused) swizzle bits on immediates, so match type and component count only
      const uint32_t token = tokens[begin];
      const uint32_t bits = std::bit_cast<uint32_t>(value);
      return end - begin == 5 && DECODE_D3D10_SB_OPERAND_TYPE(token) == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 && DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token) == D3D10_SB_OPERAND_4_COMPONENT && !DECODE_IS_D3D10_SB_OPERAND_EXTENDED(token) && std::all_of(tokens.begin() + begin + 1, tokens.begin() + end, [bits](uint32_t token)
                                                                                                                                                                                                                                                { return token == bits; });
   };

   uint32_t x = UINT32_MAX;
   size_t coordinate = first_body;
   size_t lerp = first_body;
   std::array<size_t, 5> lerp_operands = {};
   for (size_t i = first_body; i < instructions.size(); i++)
   {
      std::array<size_t, 5> operands{};
      if (!read_mad(instructions[i], &operands))
         continue;
      if (x == UINT32_MAX)
      {
         if (immediate4(operands[2], operands[3], 0.96875f) && immediate4(operands[3], operands[4], 0.015625f))
         {
            x = temp_source(operands[1], operands[2]);
            coordinate = i;
         }
         continue;
      }
      constexpr uint32_t mask_xyz = D3D10_SB_OPERAND_4_COMPONENT_MASK_X | D3D10_SB_OPERAND_4_COMPONENT_MASK_Y | D3D10_SB_OPERAND_4_COMPONENT_MASK_Z;
      const uint32_t destination = tokens[operands[0]];
      const uint32_t weight = tokens[operands[1]];
      const bool lerp_shape = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(destination) == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE && DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(destination) == mask_xyz &&
                              DECODE_D3D10_SB_OPERAND_TYPE(weight) == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER && !DECODE_IS_D3D10_SB_OPERAND_EXTENDED(weight) &&
                              temp_source(operands[3], operands[4]) == x;
      if (!lerp_shape)
         continue;
      if (lerp != first_body)
         return nullptr; // Two candidates: not the known tail
      lerp = i;
      lerp_operands = operands;
   }
   if (lerp == first_body)
      return nullptr;

   // The tail's temps, in the order they appear (their index among the added temps), and those read before being written
   std::map<uint32_t, uint32_t> renamed;
   std::vector<uint32_t> live_ins;
   std::set<uint32_t> written;
   const auto read = [&](uint32_t temp)
   {
      if (!written.contains(temp) && renamed.try_emplace(temp, uint32_t(renamed.size())).second)
         live_ins.push_back(temp);
   };
   const auto visit_reads = [&](size_t token_position, size_t index_position)
   {
      if (DECODE_D3D10_SB_OPERAND_TYPE(tokens[token_position]) == D3D10_SB_OPERAND_TYPE_TEMP)
         read(tokens[index_position]);
      return true;
   };
   for (size_t i = coordinate; i < lerp; i++)
   {
      // The first operand is the destination: a temp, as nothing in the tail has side effects
      uint32_t destination = UINT32_MAX;
      std::vector<uint32_t> sources;
      bool first = true;
      const bool readable = DXBC::WalkOperands(tokens, instructions[i], [&](size_t token_position, size_t index_position)
         {
            const bool temp = DECODE_D3D10_SB_OPERAND_TYPE(tokens[token_position]) == D3D10_SB_OPERAND_TYPE_TEMP;
            if (first)
            {
               first = false;
               if (!temp || DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(tokens[token_position]) != D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)
                  return false;
               destination = tokens[index_position];
            }
            else if (temp)
            {
               sources.push_back(tokens[index_position]);
            }
            return true; });
      if (!readable)
         return nullptr;
      std::ranges::for_each(sources, read);
      written.insert(destination);
      renamed.try_emplace(destination, uint32_t(renamed.size()));
   }
   for (size_t operand = 1; operand < 4; operand++)
   {
      if (DXBC::WalkOperand(tokens, lerp_operands[operand], lerp_operands[operand + 1], visit_reads) != lerp_operands[operand + 1])
         return nullptr;
   }

   uint32_t first_added = 0;
   std::vector<uint32_t> patched = DXBC::CopyDeclarations(tokens, instructions, first_body, uint32_t(renamed.size()) + 4, &first_added, [](size_t)
      { return std::vector<uint32_t>(); });
   // m in .x, 1 / m in .y | V | scratch | G, then Y(decode(G * m)) in .x
   const uint32_t scale = first_added + uint32_t(renamed.size());
   const uint32_t graded = scale + 1;
   const uint32_t scratch = scale + 2;
   const uint32_t wrapped = scale + 3;

   constexpr uint32_t x_mask = D3D10_SB_OPERAND_4_COMPONENT_MASK_X;
   constexpr uint32_t xyz_mask = x_mask | D3D10_SB_OPERAND_4_COMPONENT_MASK_Y | D3D10_SB_OPERAND_4_COMPONENT_MASK_Z;
   using Operand = std::vector<uint32_t>;
   const auto emit = [&](D3D10_SB_OPCODE_TYPE opcode, std::initializer_list<Operand> operands, bool saturate = false)
   {
      size_t length = 1;
      for (const Operand& operand : operands)
         length += operand.size();
      patched.push_back(DXBC::Opcode(opcode, uint32_t(length), saturate));
      for (const Operand& operand : operands)
         patched.append_range(operand);
   };
   const auto destination = [](uint32_t temp, uint32_t mask)
   {
      return Operand{DXBC::Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask), temp};
   };
   const auto source = [](uint32_t temp, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
   {
      return Operand{DXBC::Source(D3D10_SB_OPERAND_TYPE_TEMP, x, y, z, w), temp};
   };
   const auto xyzw = [&](uint32_t temp)
   {
      return source(temp, 0, 1, 2, 3);
   };
   const auto negated = [](Operand operand)
   {
      operand[0] |= ENCODE_D3D10_SB_OPERAND_EXTENDED(true);
      operand.insert(operand.begin() + 1, ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(D3D10_SB_OPERAND_MODIFIER_NEG));
      return operand;
   };
   const auto immediate = [](float x, float y, float z, float w)
   {
      return Operand{ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_IMMEDIATE32), std::bit_cast<uint32_t>(x), std::bit_cast<uint32_t>(y), std::bit_cast<uint32_t>(z), std::bit_cast<uint32_t>(w)};
   };
   const auto splat = [&](float value)
   {
      return immediate(value, value, value, value);
   };
   const auto lerp_operand = [&](size_t operand)
   {
      return Operand(tokens.begin() + lerp_operands[operand], tokens.begin() + lerp_operands[operand + 1]);
   };
   // Copies tokens [begin, end) of the tail with the temps of the operands from "operands" on renamed (all read by the first pass)
   const auto append_renamed = [&](size_t begin, size_t operands, size_t end)
   {
      const size_t base = patched.size();
      patched.insert(patched.end(), tokens.begin() + begin, tokens.begin() + end);
      for (size_t i = operands; i != 0 && i < end;)
      {
         i = DXBC::WalkOperand(tokens, i, end, [&](size_t token_position, size_t index_position)
            {
               if (DECODE_D3D10_SB_OPERAND_TYPE(tokens[token_position]) == D3D10_SB_OPERAND_TYPE_TEMP)
                  patched[base + index_position - begin] = first_added + renamed.at(tokens[index_position]);
               return true; });
      }
   };
   // Relative luminance of the gamma 2.2 decoded "temp".xyz, into "temp".x
   const auto decoded_luminance = [&](uint32_t temp)
   {
      emit(D3D10_SB_OPCODE_MAX, {destination(temp, xyz_mask), xyzw(temp), splat(0.f)});
      emit(D3D10_SB_OPCODE_LOG, {destination(temp, xyz_mask), xyzw(temp)});
      emit(D3D10_SB_OPCODE_MUL, {destination(temp, xyz_mask), xyzw(temp), splat(2.2f)});
      emit(D3D10_SB_OPCODE_EXP, {destination(temp, xyz_mask), xyzw(temp)});
      emit(D3D10_SB_OPCODE_DP3, {destination(temp, x_mask), xyzw(temp), immediate(0.2126f, 0.7152f, 0.0722f, 0.f)});
      // Added to both luminances: their ratio stays exactly 1 when they're equal, and finite at black
      emit(D3D10_SB_OPCODE_ADD, {destination(temp, x_mask), source(temp, 0, 0, 0, 0), splat(1e-10f)});
   };

   for (size_t i = first_body; i < instructions.size(); i++)
   {
      const DXBC::Instruction& instruction = instructions[i];
      if (i == coordinate)
      {
         // m = max(1, max3(x)), 1 / m
         emit(D3D10_SB_OPCODE_MAX, {destination(scale, x_mask), source(x, 0, 0, 0, 0), source(x, 1, 1, 1, 1)});
         emit(D3D10_SB_OPCODE_MAX, {destination(scale, x_mask), source(scale, 0, 0, 0, 0), source(x, 2, 2, 2, 2)});
         emit(D3D10_SB_OPCODE_MAX, {destination(scale, x_mask), source(scale, 0, 0, 0, 0), splat(1.f)});
         emit(D3D10_SB_OPCODE_DIV, {destination(scale, D3D10_SB_OPERAND_4_COMPONENT_MASK_Y), splat(1.f), source(scale, 0, 0, 0, 0)});
         for (uint32_t temp : live_ins)
         {
            emit(D3D10_SB_OPCODE_MUL, {destination(first_added + renamed.at(temp), D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL), xyzw(temp), source(scale, 1, 1, 1, 1)});
         }
         // The tail on p = x / m: G = p + k * (LUT(p) - p)
         for (size_t j = coordinate; j < lerp; j++)
         {
            const size_t begin = instructions[j].begin;
            size_t operands = begin + 1;
            for (uint32_t extended = tokens[begin]; DECODE_IS_D3D10_SB_OPCODE_EXTENDED(extended); extended = tokens[operands++])
            {
            }
            append_renamed(begin, operands, begin + instructions[j].length);
         }
         patched.insert(patched.end(), {DXBC::Opcode(D3D10_SB_OPCODE_MAD, uint32_t(3 + lerp_operands[4] - lerp_operands[1])), DXBC::Destination(D3D10_SB_OPERAND_TYPE_TEMP, xyz_mask), wrapped});
         append_renamed(lerp_operands[1], lerp_operands[1], lerp_operands[4]);
         emit(D3D10_SB_OPCODE_MUL, {destination(wrapped, xyz_mask), xyzw(wrapped), source(scale, 0, 0, 0, 0)});
         decoded_luminance(wrapped);
      }
      if (i != lerp)
      {
         patched.insert(patched.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
         continue;
      }
      // The vanilla lerp L = x + k * (LUT(saturate(x)) - x), then V = L + (1 - k) * e with e = saturate(x) - x
      emit(D3D10_SB_OPCODE_MAD, {destination(graded, xyz_mask), lerp_operand(1), lerp_operand(2), lerp_operand(3)});
      emit(D3D10_SB_OPCODE_MOV, {destination(scratch, xyz_mask), xyzw(x)}, true);
      emit(D3D10_SB_OPCODE_ADD, {destination(scratch, xyz_mask), xyzw(scratch), negated(xyzw(x))});
      emit(D3D10_SB_OPCODE_ADD, {destination(graded, xyz_mask), xyzw(graded), xyzw(scratch)});
      emit(D3D10_SB_OPCODE_MAD, {destination(graded, xyz_mask), negated(lerp_operand(1)), xyzw(scratch), xyzw(graded)});
      // o = V * (Y(decode(G * m)) / Y(decode(V)))^(1/2.2)
      emit(D3D10_SB_OPCODE_MOV, {destination(scratch, xyz_mask), xyzw(graded)});
      decoded_luminance(scratch);
      emit(D3D10_SB_OPCODE_DIV, {destination(scratch, x_mask), source(wrapped, 0, 0, 0, 0), source(scratch, 0, 0, 0, 0)});
      emit(D3D10_SB_OPCODE_LOG, {destination(scratch, x_mask), source(scratch, 0, 0, 0, 0)});
      emit(D3D10_SB_OPCODE_MUL, {destination(scratch, x_mask), source(scratch, 0, 0, 0, 0), splat(1.f / 2.2f)});
      emit(D3D10_SB_OPCODE_EXP, {destination(scratch, x_mask), source(scratch, 0, 0, 0, 0)});
      emit(D3D10_SB_OPCODE_MUL, {lerp_operand(0), xyzw(graded), source(scratch, 0, 0, 0, 0)});
   }

   *size = (patched.size() - 2) * sizeof(uint32_t);
   auto new_code = std::make_unique<std::byte[]>(*size);
   std::memcpy(new_code.get(), patched.data() + 2, *size);
   return new_code;
}
