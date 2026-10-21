#pragma once

// D3D12 readbacks for the dev MCP ("luma_read_resource", "luma_read_cbuffer"), see "docs/DX12-Support-Plan.md" A4. A readback is a GPU copy
// recorded into the game's command list at the target draw, into a READBACK buffer that is read once that list ran.
// D3D12 has no state queries and Core doesn't track resource states (ReShade's "barrier" event drops subresources), so only views whose state
// at the draw is certain are copied: render targets and unordered access views right after the draw, a depth or stencil plane after a draw
// whose pipeline writes it, constant buffers in UPLOAD heaps (always GENERIC_READ). The barriers are legacy ones: textures a game manages
// with enhanced barrier layouts are outside their interop rules.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <d3d12.h>
#include <include/reshade.hpp>

#include <source/com_ptr.hpp>

namespace D3D12Readback
{
   enum class Kind : uint8_t
   {
      RenderTarget,
      UnorderedAccess,
      Depth,
      Stencil,
      ConstantBuffer,
   };

   // The command a draw event stands for, re-issued to copy its output after it. "ExecuteIndirect()" comes without its command signature,
   // and ReShade passes "DispatchMesh()" with X as Y (its "D3D12GraphicsCommandList::DispatchMesh"): those can't be re-issued.
   enum class Command : uint8_t
   {
      Draw,
      DrawIndexed,
      Dispatch,
      Other,
   };

   // The draw a readback copies: the "instance"th draw since the last present with one of "pipelines", recorded on "thread" (any if none)
   struct Target
   {
      std::vector<uint64_t> pipelines;
      std::thread::id thread;
      int32_t instance = 0;
      Kind kind = Kind::RenderTarget;
      uint32_t slot = 0; // The render target slot, or the packed UAV or constant buffer entry (as the frame capture lists them)
   };

   struct Copy
   {
      enum class Status : uint8_t
      {
         Recorded,  // In a command list not executed yet
         Executed,  // Its list went to "queue". ReShade unlocks the queue before the native call, "TryRead()" checks the token.
         Discarded, // Its list was reset or destroyed unexecuted, the GPU never ran it
         Ready,     // Root constants, read on the CPU
         Failed,    // The target draw can't be copied, see "error"
      };
      Status status = Status::Recorded;
      std::string error;
      const reshade::api::command_list* cmd_list = nullptr;
      reshade::api::command_queue* queue = nullptr;

      com_ptr<ID3D12Resource> buffer;   // READBACK, in COPY_DEST for its whole life
      com_ptr<ID3D12Resource> resolved; // Multisampled render targets are resolved into it first
      uint64_t token = 0;               // The GPU copies it into the buffer's last 8 bytes, proof that the list ran
      uint64_t token_offset = 0;

      // Textures: one mip of the layers the view covers, each subresource laid out by its footprint
      D3D12_RESOURCE_DESC desc = {}; // The source texture's (the data of multisampled ones is resolved)
      uint32_t mip = 0;
      uint32_t first_layer = 0;
      DXGI_FORMAT data_format = DXGI_FORMAT_UNKNOWN; // How to read the data: the view's format, or the depth stencil plane's
      std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
      uint32_t rows = 0; // Of each subresource (block rows for block compressed formats)

      // Constant buffers: "size" bytes at "buffer_offset" of the bound buffer, or the root constants
      std::string resource; // The frame capture's hash of the buffer
      uint64_t buffer_offset = 0;
      uint64_t size = 0;
      std::vector<uint32_t> root_constants;
      uint32_t register_index = 0;
      uint32_t register_space = 0;
   };

   // One per device
   struct State
   {
      std::shared_mutex mutex;        // Guards everything below
      std::optional<Target> target;   // Armed by an MCP job, taken by the target draw
      int32_t instances = 0;          // Draws matching "target" since the last present
      std::optional<Copy> copy;       // The MCP job's
      std::vector<Copy> retired;      // Copies a job gave up on, whose buffer the GPU may still write
      com_ptr<ID3D12Resource> tokens; // UPLOAD, mapped for its whole life
      uint64_t* mapped_tokens = nullptr;
      uint64_t next_token = 1;
   };

   // Checked without a lock by every draw: a "State::target" is armed (rechecked under its lock)
   std::atomic<bool> armed = false;
   // Checked without a lock by every command list execution, reset and destruction: copies in "Status::Recorded"
   std::atomic<uint32_t> recorded_copies = 0;

   constexpr uint32_t token_slots = 64; // Each copy takes its token from its own slot, the GPU reads it when the list runs

   // UPLOAD buffers start (and stay) in GENERIC_READ, READBACK ones in COPY_DEST
   com_ptr<ID3D12Resource> CreateBuffer(ID3D12Device* device, D3D12_HEAP_TYPE heap_type, uint64_t size)
   {
      const D3D12_HEAP_PROPERTIES heap = {.Type = heap_type};
      const D3D12_RESOURCE_DESC desc = {.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, .Width = size, .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1, .SampleDesc = {.Count = 1}, .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
      const D3D12_RESOURCE_STATES state = (heap_type == D3D12_HEAP_TYPE_UPLOAD) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
      com_ptr<ID3D12Resource> buffer;
      device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&buffer));
      return buffer;
   }

   // Expects "state->mutex" (exclusive)
   bool CreateTokens(State* state, ID3D12Device* device)
   {
      if (state->tokens)
         return true;
      state->tokens = CreateBuffer(device, D3D12_HEAP_TYPE_UPLOAD, token_slots * sizeof(uint64_t));
      const D3D12_RANGE nothing_read = {};
      if (state->tokens && SUCCEEDED(state->tokens->Map(0, &nothing_read, reinterpret_cast<void**>(&state->mapped_tokens))))
         return true;
      state->tokens.reset();
      return false;
   }

   // Expects "state->mutex" (exclusive) and "CreateTokens()"
   void RecordToken(State* state, ID3D12GraphicsCommandList* native_list, Copy* copy)
   {
      copy->token = state->next_token++;
      const uint32_t slot = uint32_t(copy->token % token_slots);
      state->mapped_tokens[slot] = copy->token;
      native_list->CopyBufferRegion(copy->buffer.get(), copy->token_offset, state->tokens.get(), slot * sizeof(uint64_t), sizeof(uint64_t));
   }

   // Records a copy of "subresources" of "resource" into "native_list", each in "draw_state" (the state the target draw needs, "COMMON" for the
   // back buffer at present). Multisampled render targets are resolved with "resolve_format" first. The resources are created before any
   // command is recorded: "reissue" re-issues the target draw only once nothing can fail anymore. Expects "state->mutex" (exclusive).
   template <typename Reissue>
   bool RecordTextureCopy(State* state, ID3D12Device* device, ID3D12GraphicsCommandList* native_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES draw_state, std::span<const uint32_t> subresources, DXGI_FORMAT resolve_format, Copy* copy, Reissue&& reissue)
   {
      const D3D12_RESOURCE_DESC source_desc = resource->GetDesc();
      const bool resolve = source_desc.SampleDesc.Count > 1;
      ID3D12Resource* copy_source = resource;
      std::vector<uint32_t> copy_subresources(subresources.begin(), subresources.end());
      if (resolve)
      {
         // Copies need matching sample counts. Multisampled textures have one mip.
         D3D12_RESOURCE_DESC resolved_desc = source_desc;
         resolved_desc.Alignment = 0;
         resolved_desc.DepthOrArraySize = UINT16(subresources.size());
         resolved_desc.MipLevels = 1;
         resolved_desc.Format = resolve_format;
         resolved_desc.SampleDesc = {.Count = 1};
         resolved_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
         resolved_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
         const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
         if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resolved_desc, D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr, IID_PPV_ARGS(&copy->resolved))))
            return false;
         copy_source = copy->resolved.get();
         for (uint32_t i = 0; i < copy_subresources.size(); i++)
            copy_subresources[i] = i;
      }

      copy->desc = source_desc;
      const D3D12_RESOURCE_DESC copied_desc = copy_source->GetDesc();
      copy->footprints.resize(copy_subresources.size());
      uint64_t offset = 0;
      for (size_t i = 0; i < copy_subresources.size(); i++)
      {
         UINT rows = 0;
         UINT64 row_size = 0;
         device->GetCopyableFootprints(&copied_desc, copy_subresources[i], 1, offset, &copy->footprints[i], &rows, &row_size, nullptr);
         copy->rows = rows;
         const D3D12_SUBRESOURCE_FOOTPRINT& footprint = copy->footprints[i].Footprint;
         offset = (copy->footprints[i].Offset + uint64_t(footprint.RowPitch) * rows * footprint.Depth + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~uint64_t(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
      }
      copy->token_offset = offset;
      copy->buffer = CreateBuffer(device, D3D12_HEAP_TYPE_READBACK, offset + sizeof(uint64_t));
      if (!copy->buffer || !CreateTokens(state, device))
         return false;

      reissue();

      // Only the copied subresources change state, the others of the resource can be in any
      std::vector<D3D12_RESOURCE_BARRIER> barriers;
      const auto transition = [&barriers](ID3D12Resource* barrier_resource, uint32_t subresource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
      {
         barriers.push_back({.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, .Transition = {.pResource = barrier_resource, .Subresource = subresource, .StateBefore = before, .StateAfter = after}});
      };
      const D3D12_RESOURCE_STATES source_state = resolve ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
      for (const uint32_t subresource : subresources)
         transition(resource, subresource, draw_state, source_state);
      native_list->ResourceBarrier(UINT(barriers.size()), barriers.data());
      barriers.clear();
      if (resolve)
      {
         for (uint32_t i = 0; i < subresources.size(); i++)
            native_list->ResolveSubresource(copy_source, i, resource, subresources[i], resolve_format);
         transition(copy_source, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
      }
      else
      {
         for (size_t i = 0; i < copy_subresources.size(); i++)
         {
            const D3D12_TEXTURE_COPY_LOCATION destination = {.pResource = copy->buffer.get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = copy->footprints[i]};
            const D3D12_TEXTURE_COPY_LOCATION source = {.pResource = copy_source, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = copy_subresources[i]};
            native_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr); // Depth stencil sources need whole subresources
         }
      }
      for (const uint32_t subresource : subresources)
         transition(resource, subresource, source_state, draw_state);
      native_list->ResourceBarrier(UINT(barriers.size()), barriers.data());
      if (resolve)
      {
         for (size_t i = 0; i < copy_subresources.size(); i++)
         {
            const D3D12_TEXTURE_COPY_LOCATION destination = {.pResource = copy->buffer.get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = copy->footprints[i]};
            const D3D12_TEXTURE_COPY_LOCATION source = {.pResource = copy_source, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = copy_subresources[i]};
            native_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
         }
      }
      RecordToken(state, native_list, copy);
      return true;
   }

   // Records a copy of "size" bytes at "offset" of a buffer in an UPLOAD heap (always GENERIC_READ, which includes COPY_SOURCE, so no barrier).
   // Expects "state->mutex" (exclusive).
   bool RecordBufferCopy(State* state, ID3D12Device* device, ID3D12GraphicsCommandList* native_list, ID3D12Resource* resource, uint64_t offset, uint64_t size, Copy* copy)
   {
      copy->buffer_offset = offset;
      copy->size = size;
      copy->token_offset = (size + sizeof(uint64_t) - 1) & ~uint64_t(sizeof(uint64_t) - 1);
      copy->buffer = CreateBuffer(device, D3D12_HEAP_TYPE_READBACK, copy->token_offset + sizeof(uint64_t));
      if (!copy->buffer || !CreateTokens(state, device))
         return false;
      native_list->CopyBufferRegion(copy->buffer.get(), 0, resource, offset, size);
      RecordToken(state, native_list, copy);
      return true;
   }

   // The copied bytes once the GPU ran the copy (wait for its queue first), nothing if it didn't yet: the list wasn't natively submitted
   // yet, or the game's predication skipped the copy. Textures are their subresources' rows of "RowPitch" bytes, ordered [layer][z][row].
   std::optional<std::vector<char>> TryRead(const Copy& copy)
   {
      void* data = nullptr;
      if (FAILED(copy.buffer->Map(0, nullptr, &data)))
         return std::nullopt;
      const char* bytes = static_cast<const char*>(data);
      uint64_t token = 0;
      std::memcpy(&token, bytes + copy.token_offset, sizeof(token));
      std::optional<std::vector<char>> result;
      if (token == copy.token)
      {
         result.emplace();
         if (copy.footprints.empty())
         {
            result->assign(bytes, bytes + copy.size);
         }
         for (const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint : copy.footprints)
         {
            const size_t size = size_t(footprint.Footprint.RowPitch) * copy.rows * footprint.Footprint.Depth;
            result->insert(result->end(), bytes + footprint.Offset, bytes + footprint.Offset + size);
         }
      }
      const D3D12_RANGE nothing_written = {};
      copy.buffer->Unmap(0, &nothing_written);
      return result;
   }

   template <typename Function>
   void ForEachCopy(State* state, Function&& function)
   {
      if (state->copy)
      {
         function(&*state->copy);
      }
      for (Copy& copy : state->retired)
      {
         function(&copy);
      }
   }

   // Called before "cmd_list" goes to "queue"
   void OnExecute(State* state, const reshade::api::command_list* cmd_list, reshade::api::command_queue* queue)
   {
      const std::unique_lock lock(state->mutex);
      ForEachCopy(state, [&](Copy* copy)
         {
            if (copy->status == Copy::Status::Recorded && copy->cmd_list == cmd_list)
            {
               copy->status = Copy::Status::Executed;
               copy->queue = queue;
               recorded_copies--;
            } });
   }

   // Called when "cmd_list" is reset or destroyed: the copies it didn't execute never run
   void OnReset(State* state, const reshade::api::command_list* cmd_list)
   {
      const std::unique_lock lock(state->mutex);
      ForEachCopy(state, [&](Copy* copy)
         {
            if (copy->status == Copy::Status::Recorded && copy->cmd_list == cmd_list)
            {
               copy->status = Copy::Status::Discarded;
               recorded_copies--;
            } });
      std::erase_if(state->retired, [](const Copy& copy)
         { return copy.status == Copy::Status::Discarded; });
   }

   // Moves the job's copy to "retired" if the GPU may still write its buffer. Expects "state->mutex" (exclusive).
   void Retire(State* state)
   {
      if (state->copy && (state->copy->status == Copy::Status::Recorded || state->copy->status == Copy::Status::Executed))
      {
         state->retired.push_back(std::move(*state->copy));
      }
      state->copy.reset();
   }

   // Frees the retired copies the GPU is done with, waiting for the queues of executed ones (rare: a job gave up on them)
   void CollectRetired(State* state)
   {
      std::vector<std::pair<uint64_t, reshade::api::command_queue*>> executed; // Token, queue
      {
         const std::shared_lock lock(state->mutex);
         for (const Copy& copy : state->retired)
         {
            if (copy.status == Copy::Status::Executed)
            {
               executed.emplace_back(copy.token, copy.queue);
            }
         }
      }
      if (executed.empty())
         return;
      std::vector<reshade::api::command_queue*> waited;
      for (const auto& [token, queue] : executed)
      {
         if (std::ranges::find(waited, queue) == waited.end())
         {
            queue->wait_idle();
            waited.push_back(queue);
         }
      }
      const std::unique_lock lock(state->mutex);
      std::erase_if(state->retired, [&](const Copy& copy)
         { return std::ranges::any_of(executed, [&](const auto& item)
              { return item.first == copy.token; }); });
   }
} // namespace D3D12Readback
