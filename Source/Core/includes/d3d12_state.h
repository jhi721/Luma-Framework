#pragma once

// D3D12 state Core tracks from ReShade events, given that D3D12 has no state queries ("docs/DX12-Support-Plan.md" decision D3):
// the root signatures, what each command list binds, and a shadow copy of the descriptor heaps to resolve bound tables to descriptors.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include <d3d12.h>
#include <include/reshade.hpp>

namespace D3D12State
{
   // A root signature parameter, deep copied as ReShade's arrays only live through "init_pipeline_layout"
   struct RootParameter
   {
      reshade::api::pipeline_layout_param_type type = reshade::api::pipeline_layout_param_type::push_descriptors;
      reshade::api::constant_range constants = {};        // Root constants
      std::vector<reshade::api::descriptor_range> ranges; // Descriptor table ranges, or the root descriptor
   };

   struct PipelineLayouts
   {
      std::shared_mutex mutex;
      std::unordered_map<uint64_t, std::vector<RootParameter>> parameters_by_layout;
   };

   void AddPipelineLayout(PipelineLayouts* layouts, reshade::api::pipeline_layout layout, uint32_t param_count, const reshade::api::pipeline_layout_param* params)
   {
      std::vector<RootParameter> parameters(param_count);
      for (uint32_t i = 0; i < param_count; i++)
      {
         const reshade::api::pipeline_layout_param& param = params[i];
         RootParameter& parameter = parameters[i];
         parameter.type = param.type;
         switch (param.type)
         {
         case reshade::api::pipeline_layout_param_type::push_constants:
            parameter.constants = param.push_constants;
            break;
         case reshade::api::pipeline_layout_param_type::push_descriptors:
            parameter.ranges.push_back(param.push_descriptors);
            break;
         case reshade::api::pipeline_layout_param_type::descriptor_table:
         case reshade::api::pipeline_layout_param_type::push_descriptors_with_ranges:
            parameter.ranges.assign(param.descriptor_table.ranges, param.descriptor_table.ranges + param.descriptor_table.count);
            break;
         case reshade::api::pipeline_layout_param_type::descriptor_table_with_flags:
         case reshade::api::pipeline_layout_param_type::push_descriptors_with_ranges_and_flags:
            // The derived range type has another stride (static samplers aren't kept, they take no descriptors)
            for (uint32_t j = 0; j < param.descriptor_table_with_flags.count; j++)
            {
               parameter.ranges.push_back(param.descriptor_table_with_flags.ranges[j]);
            }
            break;
         }
      }
      const std::unique_lock lock(layouts->mutex);
      layouts->parameters_by_layout[layout.handle] = std::move(parameters);
   }

   void RemovePipelineLayout(PipelineLayouts* layouts, reshade::api::pipeline_layout layout)
   {
      const std::unique_lock lock(layouts->mutex);
      layouts->parameters_by_layout.erase(layout.handle);
   }

   // What a command list bound to a root parameter
   struct RootBinding
   {
      reshade::api::descriptor_table table = {};                                                      // Descriptor table (a GPU descriptor handle)
      reshade::api::descriptor_type descriptor_type = reshade::api::descriptor_type::constant_buffer; // Root descriptor
      uint64_t descriptor = 0;                                                                        // Root descriptor: the buffer of a CBV, the GPU virtual address of an SRV/UAV (that's all ReShade passes for them)
      uint64_t descriptor_offset = 0;                                                                 // Root CBV offset in its buffer
      std::vector<uint32_t> constants;                                                                // Root constants
   };

   // Graphics and compute have their own root signature and root arguments
   struct RootState
   {
      reshade::api::pipeline_layout layout = {};
      std::vector<RootBinding> bindings; // By root parameter index
   };

   // What a view descriptor held when it was written or bound. D3D12 copies descriptors by value and reads CPU descriptors (render
   // targets) when they are bound, while ReShade's view handles are descriptor addresses, which games rewrite right after.
   struct ViewInfo
   {
      uint64_t resource = 0; // Null for a null descriptor
      reshade::api::format format = reshade::api::format::unknown;
      uint32_t first_level = 0;
   };

   ViewInfo GetViewInfo(reshade::api::device* device, reshade::api::resource_view view)
   {
      if (view.handle == 0)
         return {};
      const reshade::api::resource_view_desc desc = device->get_resource_view_desc(view);
      return {.resource = device->get_resource_from_view(view).handle, .format = desc.format, .first_level = (desc.type != reshade::api::resource_view_type::buffer) ? desc.texture.first_level : 0};
   }

   // A bound index or vertex buffer: ReShade resolves their GPU virtual address to the buffer
   struct BufferBinding
   {
      uint64_t buffer = 0;
      uint64_t offset = 0;
      uint32_t stride = 0; // The index size for index buffers
   };

   struct CommandListState
   {
      reshade::api::pipeline pipeline = {};
      RootState graphics;
      RootState compute;
      reshade::api::primitive_topology primitive_topology = reshade::api::primitive_topology::undefined;
      BufferBinding index_buffer;
      std::vector<BufferBinding> vertex_buffers; // By slot
      std::vector<reshade::api::resource_view> render_targets;
      std::vector<ViewInfo> render_target_views;
      reshade::api::resource_view depth_stencil = {};
      ViewInfo depth_stencil_view;
   };

   // Setting a different root signature invalidates all root arguments, setting the same one keeps them
   void SetRootSignature(RootState* root, reshade::api::pipeline_layout layout)
   {
      if (root->layout != layout)
      {
         root->layout = layout;
         root->bindings.clear();
      }
   }

   RootBinding* GetRootBinding(RootState* root, reshade::api::pipeline_layout layout, uint32_t param)
   {
      SetRootSignature(root, layout);
      if (root->bindings.size() <= param)
      {
         root->bindings.resize(param + 1);
      }
      return &root->bindings[param];
   }

   // ReShade passes "all_graphics" for the graphics root signature and arguments, "all_compute" (with ray tracing) for the compute ones
   template <typename Function>
   void ForEachRootState(CommandListState* state, reshade::api::shader_stage stages, Function&& function)
   {
      if ((stages & reshade::api::shader_stage::all_graphics) != 0)
      {
         function(&state->graphics);
      }
      if ((stages & reshade::api::shader_stage::all_compute) != 0)
      {
         function(&state->compute);
      }
   }

   // "Set*RootSignature()" arrives here with no tables
   void BindDescriptorTables(CommandListState* state, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t first, uint32_t count, const reshade::api::descriptor_table* tables)
   {
      ForEachRootState(state, stages, [&](RootState* root)
         {
            SetRootSignature(root, layout);
            for (uint32_t i = 0; i < count; i++)
            {
               GetRootBinding(root, layout, first + i)->table = tables[i];
            } });
   }

   void PushDescriptors(CommandListState* state, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t param, const reshade::api::descriptor_table_update& update)
   {
      if (update.count == 0)
         return;
      ForEachRootState(state, stages, [&](RootState* root)
         {
            RootBinding* binding = GetRootBinding(root, layout, param);
            binding->descriptor_type = update.type;
            if (update.type == reshade::api::descriptor_type::constant_buffer)
            {
               const auto& range = *static_cast<const reshade::api::buffer_range*>(update.descriptors);
               binding->descriptor = range.buffer.handle;
               binding->descriptor_offset = range.offset;
            }
            else
            {
               binding->descriptor = *static_cast<const uint64_t*>(update.descriptors);
               binding->descriptor_offset = 0;
            } });
   }

   void PushConstants(CommandListState* state, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t param, uint32_t first, uint32_t count, const void* values)
   {
      ForEachRootState(state, stages, [&](RootState* root)
         {
            RootBinding* binding = GetRootBinding(root, layout, param);
            binding->constants.resize((std::max)(binding->constants.size(), size_t(first) + count));
            std::memcpy(binding->constants.data() + first, values, count * sizeof(uint32_t)); });
   }

   // "IASetPrimitiveTopology()" arrives as a dynamic state
   void BindPipelineStates(CommandListState* state, uint32_t count, const reshade::api::dynamic_state* states, const uint32_t* values)
   {
      for (uint32_t i = 0; i < count; i++)
      {
         if (states[i] == reshade::api::dynamic_state::primitive_topology)
         {
            state->primitive_topology = static_cast<reshade::api::primitive_topology>(values[i]);
         }
      }
   }

   void BindVertexBuffers(CommandListState* state, uint32_t first, uint32_t count, const reshade::api::resource* buffers, const uint64_t* offsets, const uint32_t* strides)
   {
      if (state->vertex_buffers.size() < size_t(first) + count)
      {
         state->vertex_buffers.resize(size_t(first) + count);
      }
      for (uint32_t i = 0; i < count; i++)
      {
         state->vertex_buffers[first + i] = {.buffer = buffers[i].handle, .offset = offsets[i], .stride = strides[i]};
      }
   }

   void BindRenderTargets(CommandListState* state, reshade::api::device* device, uint32_t count, const reshade::api::resource_view* rtvs, reshade::api::resource_view dsv)
   {
      state->render_targets.assign(rtvs, rtvs + count);
      state->render_target_views.resize(count);
      for (uint32_t i = 0; i < count; i++)
      {
         state->render_target_views[i] = GetViewInfo(device, rtvs[i]);
      }
      state->depth_stencil = dsv;
      state->depth_stencil_view = GetViewInfo(device, dsv);
   }

   void BeginRenderPass(CommandListState* state, reshade::api::device* device, uint32_t count, const reshade::api::render_pass_render_target_desc* rts, const reshade::api::render_pass_depth_stencil_desc* ds)
   {
      state->render_targets.resize(count);
      state->render_target_views.resize(count);
      for (uint32_t i = 0; i < count; i++)
      {
         state->render_targets[i] = rts[i].view;
         state->render_target_views[i] = GetViewInfo(device, rts[i].view);
      }
      state->depth_stencil = ds ? ds->view : reshade::api::resource_view{};
      state->depth_stencil_view = GetViewInfo(device, state->depth_stencil);
   }

   // What a game wrote into a descriptor heap slot
   struct Descriptor
   {
      reshade::api::descriptor_type type = reshade::api::descriptor_type::sampler;
      uint64_t handle = 0; // The view or sampler, or the buffer of a constant buffer view
      uint64_t offset = 0; // Constant buffer view range in its buffer
      uint64_t size = 0;
      ViewInfo view; // Views only, resolved when written as the copies of a descriptor outlive what its address holds next
   };

   // By heap (CPU only and shader visible ones), indexed by descriptor
   struct DescriptorHeaps
   {
      std::shared_mutex mutex;
      std::unordered_map<uint64_t, std::vector<Descriptor>> descriptors_by_heap;
   };

   // "get_descriptor_heap_offset()" maps both CPU and GPU descriptor handles. Expects "heaps->mutex" to be locked.
   std::vector<Descriptor>* GetHeapDescriptors(DescriptorHeaps* heaps, reshade::api::device* device, reshade::api::descriptor_table table, uint32_t binding, uint32_t count, uint32_t* offset)
   {
      reshade::api::descriptor_heap heap = {};
      device->get_descriptor_heap_offset(table, binding, 0, &heap, offset);
      if (heap.handle == 0)
         return nullptr;
      std::vector<Descriptor>& descriptors = heaps->descriptors_by_heap[heap.handle];
      if (descriptors.size() < size_t(*offset) + count)
      {
         descriptors.resize(size_t(*offset) + count);
      }
      return &descriptors;
   }

   // What a table slot holds, nullptr if the game never wrote it. Expects "heaps.mutex" to be locked (shared is enough).
   const Descriptor* FindDescriptor(const DescriptorHeaps& heaps, reshade::api::device* device, reshade::api::descriptor_table table, uint32_t binding)
   {
      reshade::api::descriptor_heap heap = {};
      uint32_t offset = 0;
      device->get_descriptor_heap_offset(table, binding, 0, &heap, &offset);
      const auto it = heaps.descriptors_by_heap.find(heap.handle);
      if (it == heaps.descriptors_by_heap.end() || offset >= it->second.size())
         return nullptr;
      return &it->second[offset];
   }

   void UpdateDescriptors(DescriptorHeaps* heaps, reshade::api::device* device, uint32_t count, const reshade::api::descriptor_table_update* updates)
   {
      const std::unique_lock lock(heaps->mutex);
      for (uint32_t i = 0; i < count; i++)
      {
         const reshade::api::descriptor_table_update& update = updates[i];
         uint32_t offset = 0;
         std::vector<Descriptor>* descriptors = GetHeapDescriptors(heaps, device, update.table, update.binding + update.array_offset, update.count, &offset);
         if (!descriptors)
            continue;
         for (uint32_t j = 0; j < update.count; j++)
         {
            Descriptor& descriptor = (*descriptors)[offset + j];
            descriptor = {.type = update.type};
            switch (update.type)
            {
            case reshade::api::descriptor_type::constant_buffer:
            case reshade::api::descriptor_type::shader_storage_buffer:
            {
               const auto& range = static_cast<const reshade::api::buffer_range*>(update.descriptors)[j];
               descriptor.handle = range.buffer.handle;
               descriptor.offset = range.offset;
               descriptor.size = range.size;
               break;
            }
            case reshade::api::descriptor_type::sampler_with_resource_view:
               descriptor.handle = static_cast<const reshade::api::sampler_with_resource_view*>(update.descriptors)[j].view.handle;
               descriptor.view = GetViewInfo(device, {descriptor.handle});
               break;
            case reshade::api::descriptor_type::sampler:
               descriptor.handle = static_cast<const uint64_t*>(update.descriptors)[j];
               break;
            default: // Views are a single handle
               descriptor.handle = static_cast<const uint64_t*>(update.descriptors)[j];
               descriptor.view = GetViewInfo(device, {descriptor.handle});
               break;
            }
         }
      }
   }

   void CopyDescriptors(DescriptorHeaps* heaps, reshade::api::device* device, uint32_t count, const reshade::api::descriptor_table_copy* copies)
   {
      const std::unique_lock lock(heaps->mutex);
      for (uint32_t i = 0; i < count; i++)
      {
         const reshade::api::descriptor_table_copy& copy = copies[i];
         uint32_t source_offset = 0;
         uint32_t dest_offset = 0;
         std::vector<Descriptor>* source = GetHeapDescriptors(heaps, device, copy.source_table, copy.source_binding + copy.source_array_offset, copy.count, &source_offset);
         std::vector<Descriptor>* dest = GetHeapDescriptors(heaps, device, copy.dest_table, copy.dest_binding + copy.dest_array_offset, copy.count, &dest_offset);
         if (!source || !dest)
            continue;
         // Through a temporary: the ranges can overlap within a heap, and resizing the destination can move the source ("GetHeapDescriptors()")
         std::vector<Descriptor> copied(source->begin() + source_offset, source->begin() + source_offset + copy.count);
         std::copy(copied.begin(), copied.end(), dest->begin() + dest_offset);
      }
   }

   // Buffers by GPU virtual address: ReShade passes root SRVs and UAVs as one
   struct BufferAddress
   {
      uint64_t resource = 0;
      uint64_t size = 0;
   };

   struct BufferAddresses
   {
      std::shared_mutex mutex;
      std::map<uint64_t, BufferAddress> buffers_by_address;
   };

   void AddBuffer(BufferAddresses* addresses, reshade::api::resource buffer, uint64_t size)
   {
      const D3D12_GPU_VIRTUAL_ADDRESS address = reinterpret_cast<ID3D12Resource*>(buffer.handle)->GetGPUVirtualAddress();
      if (address == 0)
         return;
      const std::unique_lock lock(addresses->mutex);
      addresses->buffers_by_address[address] = {.resource = buffer.handle, .size = size};
   }

   // Textures have no address. Placed buffers aliasing the same memory share one: only the last created is kept.
   void RemoveBuffer(BufferAddresses* addresses, reshade::api::resource resource)
   {
      const D3D12_GPU_VIRTUAL_ADDRESS address = reinterpret_cast<ID3D12Resource*>(resource.handle)->GetGPUVirtualAddress();
      if (address == 0)
         return;
      const std::unique_lock lock(addresses->mutex);
      if (const auto it = addresses->buffers_by_address.find(address); it != addresses->buffers_by_address.end() && it->second.resource == resource.handle)
      {
         addresses->buffers_by_address.erase(it);
      }
   }

   // The buffer holding "address", none if unknown. Expects "addresses.mutex" to be locked (shared is enough).
   BufferAddress FindBuffer(const BufferAddresses& addresses, uint64_t address)
   {
      auto it = addresses.buffers_by_address.upper_bound(address);
      if (it == addresses.buffers_by_address.begin())
         return {};
      --it;
      return (address - it->first < it->second.size) ? it->second : BufferAddress{};
   }
} // namespace D3D12State
