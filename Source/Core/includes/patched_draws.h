#pragma once

#include <d3d11_1.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <memory>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <include/reshade.hpp>
#include <source/com_ptr.hpp>

// Game draws redone with patched shaders (the motion vector ones, "motion_vector_patch.h"): swapping the shaders in and out, and
// uploading the extra constants they read.
namespace PatchedDraws
{
   // A patched shader left bound after its draw, with the game's shader it replaced (see "BindPatchedShader")
   template <typename T>
   struct BoundShader
   {
      T* patched = nullptr;
      com_ptr<T> game;
   };

   template <typename T>
   com_ptr<T> GetBoundShader(ID3D11DeviceContext* native_device_context)
   {
      com_ptr<T> shader;
      if constexpr (std::is_same_v<T, ID3D11VertexShader>)
         native_device_context->VSGetShader(&shader, nullptr, nullptr);
      else
         native_device_context->PSGetShader(&shader, nullptr, nullptr);
      return shader;
   }

   template <typename T>
   void SetBoundShader(ID3D11DeviceContext* native_device_context, T* shader)
   {
      if constexpr (std::is_same_v<T, ID3D11VertexShader>)
         native_device_context->VSSetShader(shader, nullptr, 0);
      else
         native_device_context->PSSetShader(shader, nullptr, 0);
   }

   // Binds a patched shader and leaves it bound after the draw (set directly, bypassing Core's state tracking). If the game hasn't
   // bound another since, the next draw has the same original shader: it binds it again (a no-op) or puts the game's back first.
   template <typename T>
   void BindPatchedShader(ID3D11DeviceContext* native_device_context, T* patched, BoundShader<T>* bound)
   {
      com_ptr<T> current = GetBoundShader<T>(native_device_context);
      if (current.get() == patched)
         return;
      bound->game = std::move(current);
      bound->patched = patched;
      SetBoundShader(native_device_context, patched);
   }

   // Puts the game's shader back where a patched one is still bound, before a draw that must not use it
   template <typename T>
   void RestoreGameShader(ID3D11DeviceContext* native_device_context, BoundShader<T>* bound)
   {
      if (!bound->patched)
         return;
      if (GetBoundShader<T>(native_device_context).get() == bound->patched)
         SetBoundShader(native_device_context, bound->game.get());
      bound->patched = nullptr;
      bound->game.reset();
   }

   // A dynamic constant buffer holding "data", (re)created at its size
   inline bool WriteDynamicConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, com_ptr<ID3D11Buffer>* buffer, const void* data, UINT size)
   {
      D3D11_BUFFER_DESC desc = {};
      if (*buffer)
         (*buffer)->GetDesc(&desc);
      if (desc.ByteWidth != size)
      {
         buffer->reset();
         desc = {size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
         native_device->CreateBuffer(&desc, nullptr, &(*buffer));
      }
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (!*buffer || FAILED(native_device_context->Map(buffer->get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         return false;
      std::memcpy(mapped.pData, data, size);
      native_device_context->Unmap(buffer->get(), 0);
      return true;
   }

   // The previous frame's per-draw constants a patched draw reads: in a ring when the device supports constant buffer offsets (one map
   // for all of a draw's buffers, no renaming), else in a dynamic buffer per slot, renamed at every upload
   struct PreviousConstants
   {
      static constexpr UINT ring_size = 4 << 20;
      com_ptr<ID3D11Buffer> ring;
      ID3D11DeviceContext1* ring_context = nullptr; // Not referenced: the immediate context lives as long as the device
      UINT ring_offset = 0;
      bool checked = false;
      std::vector<com_ptr<ID3D11Buffer>> buffers; // Without the ring, one per slot

      // Binds each "slots" pair's previous slot: its upload, or where there is none (null), its current buffer (zero motion).
      // "log_tag" names the game in the log line saying which of the two ways it uses. "read_sizes" (optional, 0 = all): the bytes of
      // each upload the shader reads ("DXBC::ConstantBufferBytes"), the ring gets only those.
      void Bind(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, std::span<const std::pair<uint32_t, uint32_t>> slots,
         std::span<const std::vector<uint8_t>* const> uploads, std::span<ID3D11Buffer* const> current, const char* log_tag, std::span<const UINT> read_sizes = {})
      {
         if (!std::exchange(checked, true))
         {
            D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
            com_ptr<ID3D11DeviceContext1> context1;
            const D3D11_BUFFER_DESC desc = {ring_size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE};
            if (SUCCEEDED(native_device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options))) && options.ConstantBufferOffsetting &&
                options.MapNoOverwriteOnDynamicConstantBuffer && SUCCEEDED(native_device_context->QueryInterface(&context1)) &&
                SUCCEEDED(native_device->CreateBuffer(&desc, nullptr, &ring)))
            {
               ring_context = context1.get();
               ring_offset = ring_size; // The first write discards
            }
            reshade::log::message(reshade::log::level::info,
               std::format("[{} MV] previous constants {}", log_tag, ring ? "in a ring (constant buffer offsets)" : "in renamed buffers (no constant buffer offsets)").c_str());
         }

         constexpr size_t max_slots = 8;
         const size_t count = (std::min)(slots.size(), max_slots);
         assert(slots.size() <= max_slots && uploads.size() >= count && current.size() >= count);
         ID3D11Buffer* buffers_to_bind[max_slots];
         UINT bytes[max_slots] = {}, sizes[max_slots] = {};
         UINT total = 0;
         for (size_t i = 0; i < count; i++)
         {
            buffers_to_bind[i] = current[i];
            if (!uploads[i])
               continue;
            bytes[i] = UINT(uploads[i]->size());
            if (i < read_sizes.size() && read_sizes[i] != 0)
               bytes[i] = (std::min)(bytes[i], read_sizes[i]);
            // "FirstConstant" and "NumConstants" are multiples of 16 constants (256 bytes)
            total += sizes[i] = (bytes[i] + 255u) & ~255u;
         }
         if (ring && total != 0 && total <= ring_size)
         {
            const bool restart = ring_offset + total > ring_size;
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(native_device_context->Map(ring.get(), 0, restart ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
            {
               UINT offset = restart ? 0u : ring_offset;
               UINT first_constants[max_slots], constant_counts[max_slots];
               for (size_t i = 0; i < count; i++)
               {
                  if (!uploads[i])
                     continue;
                  std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, uploads[i]->data(), bytes[i]);
                  first_constants[i] = offset / 16;
                  constant_counts[i] = sizes[i] / 16;
                  offset += sizes[i];
               }
               native_device_context->Unmap(ring.get(), 0);
               ring_offset = offset;
               ID3D11Buffer* const ring_buffer = ring.get();
               for (size_t i = 0; i < count; i++)
               {
                  if (uploads[i])
                     ring_context->VSSetConstantBuffers1(slots[i].second, 1, &ring_buffer, &first_constants[i], &constant_counts[i]);
                  else
                     native_device_context->VSSetConstantBuffers(slots[i].second, 1, &buffers_to_bind[i]);
               }
               return;
            }
         }
         buffers.resize(count);
         for (size_t i = 0; i < count; i++)
         {
            if (uploads[i] && WriteDynamicConstants(native_device, native_device_context, std::addressof(buffers[i]), uploads[i]->data(), UINT(uploads[i]->size())))
               buffers_to_bind[i] = buffers[i].get();
            native_device_context->VSSetConstantBuffers(slots[i].second, 1, &buffers_to_bind[i]);
         }
      }
   };

   // A development check of a frame's motion vector objects by draw key: those with another object's "translation" but other
   // constants. Their previous frame match is arbitrary (a tie-break translation that isn't per object, e.g. a bone row); 0 when
   // the tie-break works.
   template <typename Object, typename SameConstants>
   uint32_t CountTieBreakCollisions(const std::unordered_map<uint64_t, std::vector<Object>>& objects_by_key, SameConstants same_constants)
   {
      uint32_t collisions = 0;
      for (const auto& [key, objects] : objects_by_key)
      {
         for (size_t i = 0; i < objects.size(); i++)
         {
            for (size_t j = 0; j < objects.size(); j++)
            {
               if (i != j && objects[i].translation == objects[j].translation && !same_constants(objects[i], objects[j]))
               {
                  collisions++;
                  break;
               }
            }
         }
      }
      return collisions;
   }

   // Constant copies (shared, null until a buffer's first upload) holding the same bytes, for "CountTieBreakCollisions"
   inline bool SameBytes(const std::shared_ptr<const std::vector<uint8_t>>& a, const std::shared_ptr<const std::vector<uint8_t>>& b)
   {
      return a == b || (a && b && *a == *b);
   }
} // namespace PatchedDraws
