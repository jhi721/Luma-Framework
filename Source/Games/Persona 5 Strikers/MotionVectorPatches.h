#pragma once

#include <bit>

#include "..\..\Core\includes\shader_patching.h"

// Motion vectors for a game that renders none, by patching its DXBC shaders (whole containers, as the signatures change, which
// Core's patch module can't do).
// Vertex shaders: the program runs a second time, reading the previous frame's $Globals (cb0: view projection, world matrix or bone
// palette) and resources from other slots, and only that run's SV_Position survives, as an extra output. The first run's SV_Position is copied
// to an extra output too, as outputs can't be read back.
// Pixel shaders: an extra render target gets the UV space delta from the current to the previous position.
namespace MotionVectorPatches
{
   // The cbuffer the second run reads from the previous frame's copy ($Globals), and that copy's slot
   constexpr uint32_t globals_slot = 0;
   constexpr uint32_t previous_globals_slot = 10;
   // The projection jitter for temporal upscalers, in NDC (c0.xy), added to the current position after its unjittered copy (so the
   // motion vectors never contain it)
   constexpr uint32_t jitter_slot = 9;
   // The second run reads the shader's resources (t0 to t15: wind and interaction buffers, ocean maps...) from these slots
   // instead, where the previous frame's copies go
   constexpr uint32_t resource_slots = 16;
   constexpr uint32_t previous_resources_slot = 64;
   // Past every register the game's shaders use (vertex outputs end at o12, and the pixel shaders' system value inputs follow their interpolants)
   constexpr uint32_t current_position_register = 30;
   constexpr uint32_t previous_position_register = 31;
   // The G-buffer writes 5 targets (the water 6), the forward redraws 1
   constexpr uint32_t target_slot = 6;
   constexpr char semantic_name[] = "LUMAMV";

   constexpr uint32_t FourCC(const char (&name)[5])
   {
      return uint32_t(uint8_t(name[0])) | (uint32_t(uint8_t(name[1])) << 8) | (uint32_t(uint8_t(name[2])) << 16) | (uint32_t(uint8_t(name[3])) << 24);
   }

   struct Chunk
   {
      uint32_t fourcc;
      std::vector<uint8_t> data;
   };

   inline bool ReadChunks(const uint8_t* code, size_t size, std::vector<Chunk>* chunks)
   {
      if (size < sizeof(Shader::DXBCHeader) || std::memcmp(code, "DXBC", 4) != 0)
         return false;
      const auto* header = reinterpret_cast<const Shader::DXBCHeader*>(code);
      if (header->file_size != size || sizeof(Shader::DXBCHeader) + size_t(header->chunk_count) * sizeof(uint32_t) > size)
         return false;
      for (uint32_t i = 0; i < header->chunk_count; i++)
      {
         const size_t offset = header->chunk_offsets[i];
         if (offset + 8 > size)
            return false;
         uint32_t fourcc, chunk_size;
         std::memcpy(&fourcc, code + offset, 4);
         std::memcpy(&chunk_size, code + offset + 4, 4);
         if (offset + 8 + chunk_size > size)
            return false;
         chunks->push_back({fourcc, std::vector<uint8_t>(code + offset + 8, code + offset + 8 + chunk_size)});
      }
      return true;
   }

   // A new container (sizes, offsets and checksum included)
   inline std::vector<uint8_t> WriteChunks(const std::vector<Chunk>& chunks)
   {
      size_t size = sizeof(Shader::DXBCHeader) + chunks.size() * sizeof(uint32_t);
      for (const Chunk& chunk : chunks)
         size += 8 + chunk.data.size();
      std::vector<uint8_t> code(size);
      auto* header = reinterpret_cast<Shader::DXBCHeader*>(code.data());
      std::memcpy(header->format_name, "DXBC", 4);
      header->version = 1;
      header->file_size = uint32_t(size);
      header->chunk_count = uint32_t(chunks.size());
      size_t offset = sizeof(Shader::DXBCHeader) + chunks.size() * sizeof(uint32_t);
      for (size_t i = 0; i < chunks.size(); i++)
      {
         header->chunk_offsets[i] = uint32_t(offset);
         const uint32_t chunk_size = uint32_t(chunks[i].data.size());
         std::memcpy(code.data() + offset, &chunks[i].fourcc, 4);
         std::memcpy(code.data() + offset + 4, &chunk_size, 4);
         std::memcpy(code.data() + offset + 8, chunks[i].data.data(), chunk_size);
         offset += 8 + chunk_size;
      }
      const Hash::MD5::Digest digest = Shader::CalcDXBCHash(code.data(), code.size());
      std::memcpy(header->hash, &digest.data, Shader::DXBCHeader::hash_size);
      return code;
   }

   // An ISGN/OSGN element (24 bytes each, names after them)
   struct SignatureElement
   {
      std::string name;
      uint32_t semantic_index;
      uint32_t system_value;
      uint32_t component_type;
      uint32_t reg;
      uint8_t mask;
      uint8_t rw_mask; // Components used (inputs) or never written (outputs)
   };

   inline bool ReadSignature(const std::vector<uint8_t>& data, std::vector<SignatureElement>* elements)
   {
      if (data.size() < 8)
         return false;
      uint32_t count, first;
      std::memcpy(&count, data.data(), 4);
      std::memcpy(&first, data.data() + 4, 4);
      if (first != 8 || 8 + size_t(count) * 24 > data.size())
         return false;
      for (uint32_t i = 0; i < count; i++)
      {
         const uint8_t* entry = data.data() + 8 + i * 24;
         uint32_t values[5];
         std::memcpy(values, entry, sizeof(values));
         const auto name_end = std::find(data.begin() + std::min<size_t>(values[0], data.size()), data.end(), uint8_t(0));
         if (values[0] >= data.size() || name_end == data.end())
            return false;
         elements->push_back({std::string(data.begin() + values[0], name_end), values[1], values[2], values[3], values[4], entry[20], entry[21]});
      }
      return true;
   }

   inline std::vector<uint8_t> WriteSignature(const std::vector<SignatureElement>& elements)
   {
      std::vector<uint8_t> data(8 + elements.size() * 24);
      const uint32_t header[2] = {uint32_t(elements.size()), 8};
      std::memcpy(data.data(), header, sizeof(header));
      std::unordered_map<std::string, uint32_t> name_offsets;
      for (size_t i = 0; i < elements.size(); i++)
      {
         const SignatureElement& element = elements[i];
         auto [name, added] = name_offsets.try_emplace(element.name, uint32_t(data.size()));
         if (added)
            data.insert(data.end(), element.name.c_str(), element.name.c_str() + element.name.size() + 1);
         const uint32_t values[5] = {name->second, element.semantic_index, element.system_value, element.component_type, element.reg};
         std::memcpy(data.data() + 8 + i * 24, values, sizeof(values));
         data[8 + i * 24 + 20] = element.mask;
         data[8 + i * 24 + 21] = element.rw_mask;
      }
      data.resize((data.size() + 3) & ~size_t(3), 0xAB); // fxc pads the names with 0xAB
      return data;
   }

   struct Instruction
   {
      size_t begin;
      size_t length;
      D3D10_SB_OPCODE_TYPE opcode;
   };

   // The SHEX/SHDR tokens split into instructions (lengths from the opcode tokens, or the second token for custom data), after the
   // version and length tokens. False if the lengths don't add up.
   inline bool SplitInstructions(const std::vector<uint32_t>& tokens, std::vector<Instruction>* instructions)
   {
      if (tokens.size() < 2 || tokens[1] != tokens.size())
         return false;
      size_t i = 2;
      while (i < tokens.size())
      {
         const D3D10_SB_OPCODE_TYPE opcode = DECODE_D3D10_SB_OPCODE_TYPE(tokens[i]);
         const size_t length = opcode == D3D10_SB_OPCODE_CUSTOMDATA ? (i + 1 < tokens.size() ? tokens[i + 1] : 0) : DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(tokens[i]);
         if (length == 0 || i + length > tokens.size())
            return false;
         instructions->push_back({i, length, opcode});
         i += length;
      }
      return true;
   }

   // Where an operand's register index sits (the first index of an immediate or immediate plus relative index), or none
   constexpr size_t no_index = SIZE_MAX;

   // Walks the operand at "i" (relative index operands included, after the operand that owns them), calling
   // "visit(operand_token_position, first_index_position)". Returns the position after the operand, or 0 for an encoding this
   // doesn't handle (64 bit immediates and indices).
   template <typename Visit>
   size_t WalkOperand(const std::vector<uint32_t>& tokens, size_t i, size_t end, Visit&& visit)
   {
      if (i >= end)
         return 0;
      const size_t token_position = i;
      const uint32_t token = tokens[i++];
      // Extended operand tokens (modifiers, min precision) chain through their top bit too
      for (uint32_t extended = token; DECODE_IS_D3D10_SB_OPERAND_EXTENDED(extended); extended = tokens[i++])
      {
         if (i >= end)
            return 0;
      }
      const D3D10_SB_OPERAND_TYPE type = DECODE_D3D10_SB_OPERAND_TYPE(token);
      if (type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64)
         return 0;
      if (type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32)
      {
         i += DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token) == D3D10_SB_OPERAND_4_COMPONENT ? 4 : 1;
         return i <= end ? i : 0;
      }
      size_t first_index_position = no_index;
      const uint32_t dimensions = DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(token);
      for (uint32_t dimension = 0; dimension < dimensions; dimension++)
      {
         const D3D10_SB_OPERAND_INDEX_REPRESENTATION representation = DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(dimension, token);
         if (representation == D3D10_SB_OPERAND_INDEX_IMMEDIATE32 || representation == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE)
         {
            if (dimension == 0)
               first_index_position = i;
            i++;
         }
         if (representation == D3D10_SB_OPERAND_INDEX_RELATIVE || representation == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE)
         {
            i = WalkOperand(tokens, i, end, visit);
            if (i == 0)
               return 0;
         }
         else if (representation != D3D10_SB_OPERAND_INDEX_IMMEDIATE32)
         {
            return 0;
         }
         if (i > end)
            return 0;
      }
      if (!visit(token_position, first_index_position))
         return 0;
      return i;
   }

   // Calls "visit" on every operand of a body instruction. False if its operands don't exactly fill it.
   template <typename Visit>
   bool WalkOperands(const std::vector<uint32_t>& tokens, const Instruction& instruction, Visit&& visit)
   {
      const size_t end = instruction.begin + instruction.length;
      size_t i = instruction.begin + 1;
      // Extended opcode tokens (sample offsets, resource dimension/return type)
      for (uint32_t extended = tokens[instruction.begin]; DECODE_IS_D3D10_SB_OPCODE_EXTENDED(extended); extended = tokens[i++])
      {
         if (i >= end)
            return false;
      }
      while (i < end)
      {
         i = WalkOperand(tokens, i, end, visit);
         if (i == 0)
            return false;
      }
      return i == end;
   }

   inline bool IsDeclaration(D3D10_SB_OPCODE_TYPE opcode)
   {
      return opcode == D3D10_SB_OPCODE_CUSTOMDATA || ShaderPatching::opcodes_dcl.contains(opcode);
   }

   // Operand tokens for the few instructions this writes
   constexpr uint32_t RegisterOperand(D3D10_SB_OPERAND_TYPE type)
   {
      return ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(type) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
   }
   constexpr uint32_t Destination(D3D10_SB_OPERAND_TYPE type, uint32_t mask)
   {
      return RegisterOperand(type) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask);
   }
   constexpr uint32_t Source(D3D10_SB_OPERAND_TYPE type, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
   {
      return RegisterOperand(type) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(x, y, z, w);
   }
   constexpr uint32_t mask_xy = D3D10_SB_OPERAND_4_COMPONENT_MASK_X | D3D10_SB_OPERAND_4_COMPONENT_MASK_Y;
   constexpr uint32_t mask_xyw = mask_xy | D3D10_SB_OPERAND_4_COMPONENT_MASK_W;

   // The version and length tokens and the declarations, with "added_temps" more temps ("first_temp" gets the first added one;
   // a temps declaration is appended if there was none), and the tokens "after(index)" returns inserted after each declaration,
   // so new declarations join their group, as fxc orders them.
   template <typename After>
   std::vector<uint32_t> CopyDeclarations(const std::vector<uint32_t>& tokens, const std::vector<Instruction>& instructions, size_t first_body, uint32_t added_temps, uint32_t* first_temp, After&& after)
   {
      std::vector<uint32_t> declarations(tokens.begin(), tokens.begin() + 2);
      bool has_temps = false;
      *first_temp = 0;
      for (size_t i = 0; i < first_body; i++)
      {
         const Instruction& instruction = instructions[i];
         const size_t position = declarations.size();
         declarations.insert(declarations.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
         if (instruction.opcode == D3D10_SB_OPCODE_DCL_TEMPS)
         {
            *first_temp = declarations[position + 1];
            declarations[position + 1] += added_temps;
            has_temps = true;
         }
         const std::vector<uint32_t> added = after(i);
         declarations.insert(declarations.end(), added.begin(), added.end());
      }
      if (!has_temps)
         declarations.insert(declarations.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_TEMPS) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(2), added_temps});
      return declarations;
   }

   // The index of the last declaration (before "first_body") with one of the opcodes, or "first_body" if none
   inline size_t FindLastDeclaration(const std::vector<Instruction>& instructions, size_t first_body, std::initializer_list<D3D10_SB_OPCODE_TYPE> opcodes)
   {
      size_t last = first_body;
      for (size_t i = 0; i < first_body; i++)
      {
         if (std::ranges::find(opcodes, instructions[i].opcode) != opcodes.end())
            last = i;
      }
      return last;
   }

   // The vertex shader with the second run added, or empty if it can't be patched (the draw then keeps the original shaders)
   inline std::vector<uint8_t> PatchVertexShader(const uint8_t* code, size_t size, std::string* error)
   {
      std::vector<Chunk> chunks;
      if (!ReadChunks(code, size, &chunks))
         return (*error = "container", std::vector<uint8_t>());
      Chunk* program = nullptr;
      Chunk* output_signature = nullptr;
      for (Chunk& chunk : chunks)
      {
         if (chunk.fourcc == FourCC("SHEX") || chunk.fourcc == FourCC("SHDR"))
            program = &chunk;
         else if (chunk.fourcc == FourCC("OSGN"))
            output_signature = &chunk;
      }
      std::vector<SignatureElement> outputs;
      if (!program || !output_signature || program->data.size() % 4 != 0 || !ReadSignature(output_signature->data, &outputs))
         return (*error = "chunks", std::vector<uint8_t>());
      const auto position = std::ranges::find_if(outputs, [](const SignatureElement& element)
         { return element.system_value == 1; }); // D3D_NAME_POSITION
      if (position == outputs.end() || std::ranges::any_of(outputs, [](const SignatureElement& element)
                                          { return element.reg >= current_position_register; }))
         return (*error = "outputs", std::vector<uint8_t>());
      const uint32_t position_register = position->reg;

      std::vector<uint32_t> tokens(program->data.size() / 4);
      std::memcpy(tokens.data(), program->data.data(), program->data.size());
      std::vector<Instruction> instructions;
      if (!SplitInstructions(tokens, &instructions))
         return (*error = "lengths", std::vector<uint8_t>());
      const size_t first_body = size_t(std::ranges::find_if(instructions, [](const Instruction& instruction)
                                          { return !IsDeclaration(instruction.opcode); }) -
                                       instructions.begin());
      if (first_body >= instructions.size() || instructions.back().opcode != D3D10_SB_OPCODE_RET || std::count_if(instructions.begin() + first_body, instructions.end(), [](const Instruction& instruction)
                                                                                                       { return instruction.opcode == D3D10_SB_OPCODE_RET; }) != 1)
         return (*error = "returns", std::vector<uint8_t>());

      // Declarations: the globals cbuffer again at the previous frame's slot (same size and immediate or dynamic indexing), each
      // resource again at the previous frame's slots, the two outputs, one more temp
      const auto is_resource_declaration = [](D3D10_SB_OPCODE_TYPE opcode)
      { return opcode == D3D10_SB_OPCODE_DCL_RESOURCE || opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW || opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED; };
      size_t globals_index = first_body;
      for (size_t i = 0; i < first_body; i++)
      {
         const Instruction& instruction = instructions[i];
         if (instruction.opcode == D3D10_SB_OPCODE_DCL_INDEX_RANGE || instruction.opcode == D3D11_SB_OPCODE_DCL_FUNCTION_BODY)
            return (*error = "declarations", std::vector<uint8_t>());
         // A 1D immediate register: operand token, then the slot
         if (is_resource_declaration(instruction.opcode) && (instruction.length < 3 || DECODE_D3D10_SB_OPERAND_TYPE(tokens[instruction.begin + 1]) != D3D10_SB_OPERAND_TYPE_RESOURCE || DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(tokens[instruction.begin + 1]) != D3D10_SB_OPERAND_INDEX_1D || tokens[instruction.begin + 2] >= resource_slots))
            return (*error = "resources", std::vector<uint8_t>());
         if (instruction.opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER && instruction.length == 4)
         {
            const uint32_t slot = tokens[instruction.begin + 2];
            if (slot == previous_globals_slot || slot == jitter_slot)
               return (*error = "slot taken", std::vector<uint8_t>());
            if (slot == globals_slot)
               globals_index = i;
         }
      }
      if (globals_index == first_body)
         return (*error = "no globals", std::vector<uint8_t>());
      const size_t last_output = FindLastDeclaration(instructions, first_body, {D3D10_SB_OPCODE_DCL_OUTPUT, D3D10_SB_OPCODE_DCL_OUTPUT_SIV, D3D10_SB_OPCODE_DCL_OUTPUT_SGV});
      uint32_t scratch;
      std::vector<uint32_t> declarations = CopyDeclarations(tokens, instructions, first_body, 1, &scratch, [&](size_t i)
         {
            std::vector<uint32_t> added;
            if (is_resource_declaration(instructions[i].opcode))
            {
               const Instruction& instruction = instructions[i];
               added.assign(tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
               added[2] += previous_resources_slot;
            }
            if (i == globals_index)
            {
               const Instruction& instruction = instructions[i];
               added.assign(tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
               added[2] = previous_globals_slot;
               // The jitter: one register, immediate indexing
               added.insert(added.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
               added[instruction.length] &= ~D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN_MASK;
               added[instruction.length + 2] = jitter_slot;
               added[instruction.length + 3] = 1;
            }
            if (i == last_output)
            {
               for (const uint32_t reg : {current_position_register, previous_position_register})
                  added.insert(added.end(), {ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(3), Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL), reg});
            }
            return added; });

      // The two runs. Outputs are only ever destinations in vertex shaders.
      std::vector<uint32_t> body;
      for (int run = 0; run < 2; run++)
      {
         const bool previous = run == 1;
         for (size_t i = first_body; i < instructions.size(); i++)
         {
            const Instruction& instruction = instructions[i];
            if (!previous && i + 1 == instructions.size())
               break; // The first run's ret
            if (instruction.opcode == D3D10_SB_OPCODE_LABEL || instruction.opcode == D3D10_SB_OPCODE_CALL || instruction.opcode == D3D10_SB_OPCODE_CALLC || instruction.opcode == D3D11_SB_OPCODE_INTERFACE_CALL || instruction.opcode == D3D10_SB_OPCODE_RETC || instruction.opcode == D3D10_SB_OPCODE_CUSTOMDATA)
               return (*error = "control flow", std::vector<uint8_t>());
            const size_t offset = body.size();
            body.insert(body.end(), tokens.begin() + instruction.begin, tokens.begin() + instruction.begin + instruction.length);
            bool supported = true;
            const bool walked = WalkOperands(tokens, instruction, [&](size_t token_position, size_t index_position)
               {
                  uint32_t& token = body[offset + token_position - instruction.begin];
                  const D3D10_SB_OPERAND_TYPE type = DECODE_D3D10_SB_OPERAND_TYPE(token);
                  if (type == D3D10_SB_OPERAND_TYPE_OUTPUT)
                  {
                     if (index_position == no_index || DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, token) != D3D10_SB_OPERAND_INDEX_IMMEDIATE32)
                        return supported = false;
                     uint32_t& index = body[offset + index_position - instruction.begin];
                     // SV_Position: into the scratch temp in the first run (copied to its output and ours after), into ours in the
                     // second. The second run's other outputs go to the scratch temp, dead.
                     if (previous && index == position_register)
                     {
                        index = previous_position_register;
                     }
                     else if (!previous && index != position_register)
                     {
                        return true;
                     }
                     else
                     {
                        token = (token & ~D3D10_SB_OPERAND_TYPE_MASK) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_TEMP);
                        index = scratch;
                     }
                  }
                  else if (previous && type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER && index_position != no_index)
                  {
                     uint32_t& index = body[offset + index_position - instruction.begin];
                     if (index == globals_slot)
                        index = previous_globals_slot;
                  }
                  else if (previous && type == D3D10_SB_OPERAND_TYPE_RESOURCE)
                  {
                     if (index_position == no_index || DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, token) != D3D10_SB_OPERAND_INDEX_IMMEDIATE32)
                        return supported = false;
                     body[offset + index_position - instruction.begin] += previous_resources_slot;
                  }
                  return true; });
            if (!walked || !supported)
               return (*error = "operands", std::vector<uint8_t>());
         }
         if (!previous)
         {
            // Our current position output (unjittered), then SV_Position with the jitter: xy += jitter * w
            constexpr uint32_t mov = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5);
            constexpr uint32_t jitter_operand = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(0, 1, 0, 0) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_2D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(1, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
            body.insert(body.end(), {
                                       mov,
                                       Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL),
                                       current_position_register,
                                       Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3),
                                       scratch,
                                       ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MAD) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(10),
                                       Destination(D3D10_SB_OPERAND_TYPE_TEMP, mask_xy),
                                       scratch,
                                       jitter_operand,
                                       jitter_slot,
                                       0,
                                       Source(D3D10_SB_OPERAND_TYPE_TEMP, 3, 3, 3, 3),
                                       scratch,
                                       Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 0, 0),
                                       scratch,
                                       mov,
                                       Destination(D3D10_SB_OPERAND_TYPE_OUTPUT, D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL),
                                       position_register,
                                       Source(D3D10_SB_OPERAND_TYPE_TEMP, 0, 1, 2, 3),
                                       scratch,
                                    });
         }
      }

      declarations.insert(declarations.end(), body.begin(), body.end());
      declarations[1] = uint32_t(declarations.size());
      program->data.resize(declarations.size() * 4);
      std::memcpy(program->data.data(), declarations.data(), program->data.size());

      SignatureElement element = {semantic_name, 0, 0, 3 /* float */, current_position_register, 0xF, 0};
      outputs.push_back(element);
      element.semantic_index = 1;
      element.reg = previous_position_register;
      outputs.push_back(element);
      output_signature->data = WriteSignature(outputs);
      return WriteChunks(chunks);
   }

   // The pixel shader with the motion vector target added, or empty if it can't be patched
   inline std::vector<uint8_t> PatchPixelShader(const uint8_t* code, size_t size, std::string* error)
   {
      std::vector<Chunk> chunks;
      if (!ReadChunks(code, size, &chunks))
         return (*error = "container", std::vector<uint8_t>());
      Chunk* program = nullptr;
      Chunk* input_signature = nullptr;
      Chunk* output_signature = nullptr;
      for (Chunk& chunk : chunks)
      {
         if (chunk.fourcc == FourCC("SHEX") || chunk.fourcc == FourCC("SHDR"))
            program = &chunk;
         else if (chunk.fourcc == FourCC("ISGN"))
            input_signature = &chunk;
         else if (chunk.fourcc == FourCC("OSGN"))
            output_signature = &chunk;
      }
      std::vector<SignatureElement> inputs, outputs;
      if (!program || !input_signature || !output_signature || program->data.size() % 4 != 0 || !ReadSignature(input_signature->data, &inputs) || !ReadSignature(output_signature->data, &outputs))
         return (*error = "chunks", std::vector<uint8_t>());
      // Targets are known by name, their system value is left undefined in the signature
      const auto is_target = [](const SignatureElement& element)
      { return _stricmp(element.name.c_str(), "SV_Target") == 0; };
      const auto target = std::ranges::find_if(outputs, is_target);
      if (target == outputs.end() || std::ranges::any_of(outputs, [&](const SignatureElement& element)
                                        { return is_target(element) && element.reg >= target_slot; }) ||
          std::ranges::any_of(inputs, [](const SignatureElement& element)
             { return element.reg >= current_position_register; }))
         return (*error = "signatures", std::vector<uint8_t>());

      std::vector<uint32_t> tokens(program->data.size() / 4);
      std::memcpy(tokens.data(), program->data.data(), program->data.size());
      std::vector<Instruction> instructions;
      if (!SplitInstructions(tokens, &instructions))
         return (*error = "lengths", std::vector<uint8_t>());
      const size_t first_body = size_t(std::ranges::find_if(instructions, [](const Instruction& instruction)
                                          { return !IsDeclaration(instruction.opcode); }) -
                                       instructions.begin());
      if (first_body >= instructions.size() || instructions.back().opcode != D3D10_SB_OPCODE_RET || std::ranges::any_of(instructions.begin() + first_body, instructions.end() - 1, [](const Instruction& instruction)
                                                                                                       { return instruction.opcode == D3D10_SB_OPCODE_RET || instruction.opcode == D3D10_SB_OPCODE_RETC; }))
         return (*error = "returns", std::vector<uint8_t>());

      // The two inputs after the last input declaration (or right before the outputs), the target after the last output
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

      // Before the final ret: (previous.xy / previous.w - current.xy / current.w) * (0.5, -0.5), UV space
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
      declarations[1] = uint32_t(declarations.size());
      program->data.resize(declarations.size() * 4);
      std::memcpy(program->data.data(), declarations.data(), program->data.size());

      // Signature masks are plain component bits (x = 1), unlike the operand token masks above
      inputs.push_back({semantic_name, 0, 0, 3, current_position_register, 0xF, 0xB});
      inputs.push_back({semantic_name, 1, 0, 3, previous_position_register, 0xF, 0xB});
      input_signature->data = WriteSignature(inputs);
      SignatureElement output = *target;
      output.semantic_index = target_slot;
      output.reg = target_slot;
      output.mask = 0x3;
      output.rw_mask = 0;
      outputs.push_back(output);
      output_signature->data = WriteSignature(outputs);
      return WriteChunks(chunks);
   }
} // namespace MotionVectorPatches
