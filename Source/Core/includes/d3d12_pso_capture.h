#pragma once

#if DX12 && ENABLE_MINHOOK

// The exact pipeline streams of the game's D3D12 pipelines, so shader replacements are clones identical to the originals but for their
// shaders, instead of ReShade's rebuild from its pipeline subobjects (which drops what it doesn't model, like the strip cut value and
// view instancing). ReShade sends every game pipeline to the native "ID3D12Device2::CreatePipelineState" (the legacy
// "Create{Graphics,Compute}PipelineState" are repacked into a stream without the cached blob), where a detour copies the stream of
// each successful create into a thread local. ReShade fires "init_pipeline" on the same thread right after the native call, which
// takes the copy. The creates nobody takes (ReShade's own, the ones other add-ons rebuild, other IIDs, 9on12/11on12 devices) are
// replaced by the next one, and "Take()" never matches them to another pipeline.

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d3d12.h>
#include <source/com_ptr.hpp>

#include "minhook.h"

namespace D3D12PsoCapture
{
   // A subobject as a stream holds it, the type then its data, aligned to the pointer size like d3dx12's
   // "CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT" and ReShade's "D3D12_PIPELINE_STATE_STREAM" (the layouts games build streams with)
#pragma warning(push)
#pragma warning(disable : 4324) // Padded by the alignment
   template <typename T>
   struct alignas(void*) Subobject
   {
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;
      T data;
   };
#pragma warning(pop)

   // The Windows SDK's headers (D3D12_SDK_VERSION 616) end at type 28, this is from newer ones ("D3D12_SERIALIZED_ROOT_SIGNATURE_DESC")
   constexpr D3D12_PIPELINE_STATE_SUBOBJECT_TYPE SERIALIZED_ROOT_SIGNATURE = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE(29);
   struct SerializedRootSignatureDesc
   {
      const void* pSerializedBlob;
      SIZE_T SerializedBlobSizeInBytes;
   };

   constexpr size_t SUBOBJECT_TYPE_COUNT = 30; // Types 0 to 29

   // The size of each subobject type in a stream, 0 for the unknown ones (type 23 and the types after 29): a stream holding one
   // can't be walked, so it isn't captured
   constexpr std::array<size_t, SUBOBJECT_TYPE_COUNT> SUBOBJECT_SIZES = []
   {
      std::array<size_t, SUBOBJECT_TYPE_COUNT> sizes = {};
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE] = sizeof(Subobject<ID3D12RootSignature*>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT] = sizeof(Subobject<D3D12_STREAM_OUTPUT_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND] = sizeof(Subobject<D3D12_BLEND_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK] = sizeof(Subobject<UINT>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER] = sizeof(Subobject<D3D12_RASTERIZER_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL] = sizeof(Subobject<D3D12_DEPTH_STENCIL_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT] = sizeof(Subobject<D3D12_INPUT_LAYOUT_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE] = sizeof(Subobject<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY] = sizeof(Subobject<D3D12_PRIMITIVE_TOPOLOGY_TYPE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS] = sizeof(Subobject<D3D12_RT_FORMAT_ARRAY>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT] = sizeof(Subobject<DXGI_FORMAT>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC] = sizeof(Subobject<DXGI_SAMPLE_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK] = sizeof(Subobject<UINT>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO] = sizeof(Subobject<D3D12_CACHED_PIPELINE_STATE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS] = sizeof(Subobject<D3D12_PIPELINE_STATE_FLAGS>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1] = sizeof(Subobject<D3D12_DEPTH_STENCIL_DESC1>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING] = sizeof(Subobject<D3D12_VIEW_INSTANCING_DESC>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS] = sizeof(Subobject<D3D12_SHADER_BYTECODE>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2] = sizeof(Subobject<D3D12_DEPTH_STENCIL_DESC2>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1] = sizeof(Subobject<D3D12_RASTERIZER_DESC1>);
      sizes[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2] = sizeof(Subobject<D3D12_RASTERIZER_DESC2>);
      sizes[SERIALIZED_ROOT_SIGNATURE] = sizeof(Subobject<SerializedRootSignatureDesc>);
      return sizes;
   }();

   template <typename T>
   T& SubobjectData(std::byte* subobject)
   {
      return reinterpret_cast<Subobject<T>*>(subobject)->data;
   }

   // A pipeline's stream as the game created it, minus the cached blob, with what the pointers in it lead to. Not to be copied
   // (the pointers in "bytes" lead to the members), hence the pointer it is handed around with.
   struct Stream
   {
      const ID3D12PipelineState* pipeline = nullptr; // The pipeline the game got from the create, only compared (no reference)
      // The stream. What its pointers lead to is owned by the members below, but for the shaders' bytecode (the game's memory, for the
      // owner to point at a copy of its own) and the root signature (not referenced)
      std::vector<std::byte> bytes;
      uint32_t shader_count = 0; // The stages with bytecode in "bytes"

      std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
      std::vector<D3D12_SO_DECLARATION_ENTRY> stream_output_entries;
      std::vector<UINT> stream_output_strides;
      std::vector<D3D12_VIEW_INSTANCE_LOCATION> view_instance_locations;
      std::vector<std::byte> serialized_root_signature;
      std::deque<std::string> semantic_names; // Of "input_elements" and "stream_output_entries", in a deque as it never moves them

      // The data of the subobject of a type, nullptr if the stream has none. "stream_copy" is a copy of "bytes" (a clone's) to look in
      template <typename T>
      T* Find(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, std::byte* stream_copy = nullptr)
      {
         assert(sizeof(Subobject<T>) == SUBOBJECT_SIZES[type]);
         std::byte* const stream = (stream_copy ? stream_copy : bytes.data());
         for (size_t offset = 0; offset < bytes.size();)
         {
            const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobject_type = *reinterpret_cast<const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE*>(stream + offset);
            if (subobject_type == type)
            {
               return &SubobjectData<T>(stream + offset);
            }
            offset += SUBOBJECT_SIZES[subobject_type];
         }
         return nullptr;
      }
   };

   // Null if the stream can't be copied: it holds a subobject we don't know the layout of, a repeated one or a truncated one
   inline std::unique_ptr<Stream> Copy(const D3D12_PIPELINE_STATE_STREAM_DESC& desc)
   {
      auto stream = std::make_unique<Stream>();
      stream->bytes.reserve(desc.SizeInBytes);
      const auto own_name = [&stream](const char* name) -> const char*
      {
         return name ? stream->semantic_names.emplace_back(name).c_str() : nullptr;
      };

      const auto* source = static_cast<const std::byte*>(desc.pPipelineStateSubobjectStream);
      uint32_t seen_types = 0;
      for (size_t offset = 0; offset < desc.SizeInBytes;)
      {
         const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = *reinterpret_cast<const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE*>(source + offset);
         const size_t size = (static_cast<size_t>(type) < SUBOBJECT_SIZES.size() ? SUBOBJECT_SIZES[type] : 0);
         if (size == 0 || size > desc.SizeInBytes - offset || (seen_types & (1u << type)) != 0)
         {
            return nullptr;
         }
         seen_types |= 1u << type;
         const std::byte* const subobject = source + offset;
         offset += size;
         if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO)
         {
            continue;
         }

         stream->bytes.insert(stream->bytes.end(), subobject, subobject + size);
         std::byte* const copy = stream->bytes.data() + stream->bytes.size() - size;
         switch (type)
         {
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
         {
            stream->shader_count += (SubobjectData<D3D12_SHADER_BYTECODE>(copy).pShaderBytecode != nullptr);
            break;
         }
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
         {
            D3D12_INPUT_LAYOUT_DESC& layout = SubobjectData<D3D12_INPUT_LAYOUT_DESC>(copy);
            stream->input_elements.assign(layout.pInputElementDescs, layout.pInputElementDescs + layout.NumElements);
            for (D3D12_INPUT_ELEMENT_DESC& element : stream->input_elements)
            {
               element.SemanticName = own_name(element.SemanticName);
            }
            layout.pInputElementDescs = stream->input_elements.data();
            break;
         }
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
         {
            D3D12_STREAM_OUTPUT_DESC& output = SubobjectData<D3D12_STREAM_OUTPUT_DESC>(copy);
            stream->stream_output_entries.assign(output.pSODeclaration, output.pSODeclaration + output.NumEntries);
            for (D3D12_SO_DECLARATION_ENTRY& entry : stream->stream_output_entries)
            {
               entry.SemanticName = own_name(entry.SemanticName);
            }
            stream->stream_output_strides.assign(output.pBufferStrides, output.pBufferStrides + output.NumStrides);
            output.pSODeclaration = stream->stream_output_entries.data();
            output.pBufferStrides = stream->stream_output_strides.data();
            break;
         }
         case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
         {
            D3D12_VIEW_INSTANCING_DESC& instancing = SubobjectData<D3D12_VIEW_INSTANCING_DESC>(copy);
            stream->view_instance_locations.assign(instancing.pViewInstanceLocations, instancing.pViewInstanceLocations + instancing.ViewInstanceCount);
            instancing.pViewInstanceLocations = stream->view_instance_locations.data();
            break;
         }
         case SERIALIZED_ROOT_SIGNATURE:
         {
            SerializedRootSignatureDesc& signature = SubobjectData<SerializedRootSignatureDesc>(copy);
            const auto* blob = static_cast<const std::byte*>(signature.pSerializedBlob);
            stream->serialized_root_signature.assign(blob, blob + signature.SerializedBlobSizeInBytes);
            signature.pSerializedBlob = stream->serialized_root_signature.data();
            break;
         }
         }
      }
      return stream;
   }

   using CreatePipelineStateFunction = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device2*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);
   // "ID3D12Device2::CreatePipelineState" follows IUnknown (3 methods), ID3D12Object (4), ID3D12Device (37) and ID3D12Device1 (3)
   constexpr size_t CREATE_PIPELINE_STATE_SLOT = 47;

   struct Hook
   {
      void* target = nullptr;
      CreatePipelineStateFunction original = nullptr;
      // The target's start right after our hook went in: different later means another module hooked over it
      std::array<uint8_t, 8> hooked_prologue = {};
   };
   // The functions the devices' vtables lead "CreatePipelineState" to, each hooked once per process. Another device class has its own
   // (a debug layer's, another D3D12Core's), and so does a module that patches the runtime's vtable after we hooked the function in it.
   // Such a patch calls the function it replaced, so its creates are captured twice, the outer capture (the stream ReShade passed) last.
   constexpr size_t MAX_HOOKS = 4;
   struct Hooks
   {
      std::shared_mutex mutex;
      std::array<Hook, MAX_HOOKS> slots;
   };
   inline Hooks hooks;
   // The last pipeline this thread created through a hook, if it could be copied
   inline thread_local std::unique_ptr<Stream> pending;
   // Set while this thread creates a pipeline of ours, which isn't to be captured
   inline thread_local bool creating_own = false;

   // One per hook, as each calls its own trampoline
   template <size_t Index>
   HRESULT STDMETHODCALLTYPE Detour(ID3D12Device2* device, const D3D12_PIPELINE_STATE_STREAM_DESC* desc, REFIID riid, void** pipeline)
   {
      const HRESULT result = hooks.slots[Index].original(device, desc, riid, pipeline);
      if (SUCCEEDED(result) && desc && pipeline && *pipeline && !creating_own)
      {
         pending = Copy(*desc);
         if (pending)
         {
            pending->pipeline = static_cast<const ID3D12PipelineState*>(*pipeline);
         }
      }
      return result;
   }
   constexpr auto DETOURS = []<size_t... Indices>(std::index_sequence<Indices...>)
   {
      return std::array<CreatePipelineStateFunction, MAX_HOOKS>{&Detour<Indices>...};
   }(std::make_index_sequence<MAX_HOOKS>());

   // True if the device's pipelines are captured: its function is hooked, by now or for an earlier device. False if MinHook failed or
   // the hooks are all taken.
   inline bool Install(ID3D12Device* native_device)
   {
      com_ptr<ID3D12Device2> device2;
      if (FAILED(native_device->QueryInterface(&device2)))
      {
         return false;
      }
      void* const target = (*reinterpret_cast<void***>(device2.get()))[CREATE_PIPELINE_STATE_SLOT];
      const std::unique_lock lock(hooks.mutex);
      if (std::ranges::find(hooks.slots, target, &Hook::target) != hooks.slots.end())
      {
         return true;
      }
      const auto hook = std::ranges::find(hooks.slots, nullptr, &Hook::target);
      if (hook == hooks.slots.end() || !InitializeMinHook() || MH_CreateHook(target, reinterpret_cast<void*>(DETOURS[hook - hooks.slots.begin()]), reinterpret_cast<void**>(&hook->original)) != MH_OK)
      {
         return false;
      }
      if (MH_EnableHook(target) != MH_OK)
      {
         MH_RemoveHook(target);
         return false;
      }
      std::memcpy(hook->hooked_prologue.data(), target, hook->hooked_prologue.size());
      hook->target = target;
      return true;
   }

   // Only Luma's hooks (see "includes/minhook.h"). False if another module hooked over one: Luma's then stays, as the other hook's
   // trampoline jumps to its detour, and the addon is pinned like "Reflex::SleepHook::Remove()" does (too late if "DllMain" is unloading it)
   inline bool Remove()
   {
      const std::unique_lock lock(hooks.mutex);
      bool removed_all = true;
      for (Hook& hook : hooks.slots)
      {
         if (!hook.target)
         {
            continue;
         }
         if (std::memcmp(hook.target, hook.hooked_prologue.data(), hook.hooked_prologue.size()) != 0)
         {
            removed_all = false;
            continue;
         }
         MH_RemoveHook(hook.target);
         hook.target = nullptr;
      }
      if (!removed_all)
      {
         HMODULE module = nullptr;
         GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(DETOURS[0]), &module);
      }
      return removed_all;
   }

   // The capture of the pipeline this thread created last, if it's "pipeline" (a capture of another one is left for the next create
   // to replace)
   inline std::unique_ptr<Stream> Take(const ID3D12PipelineState* pipeline)
   {
      if (!pending || pending->pipeline != pipeline)
      {
         return nullptr;
      }
      return std::move(pending);
   }

   // Creates a pipeline of ours from a stream (a clone's, from a copy of "Stream::bytes"), which isn't captured. It calls through the
   // device, not a trampoline: a trampoline is one hooked function's, which can be another device class's or the one a vtable patch calls.
   inline HRESULT Create(ID3D12Device2* device, std::span<std::byte> stream, ID3D12PipelineState** pipeline)
   {
      const D3D12_PIPELINE_STATE_STREAM_DESC desc = {.SizeInBytes = stream.size(), .pPipelineStateSubobjectStream = stream.data()};
      const bool was_creating_own = std::exchange(creating_own, true);
      const HRESULT result = device->CreatePipelineState(&desc, IID_PPV_ARGS(pipeline));
      creating_own = was_creating_own;
      return result;
   }
} // namespace D3D12PsoCapture

#endif // DX12 && ENABLE_MINHOOK
