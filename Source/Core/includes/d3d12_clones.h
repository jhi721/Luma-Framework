#pragma once

#if DX12 && ENABLE_MINHOOK

// The committed clones of the game's placed textures. A placed texture lives in a heap the game sized and shares with other resources, so it
// is never upgraded in place: when it matches the texture upgrade filters, it gets a clone of its own in the upgraded format, which the
// hooks that follow redirect the game's views, barriers, clears and copies to. This file owns the registry and the clones' creation and
// release; the original stays as the game made it.
//
// The registry is process wide, because the hooks that read it are on ReShade's proxy, which has no device. They run on every game thread,
// so "Find()" is a lock free check of "count" and then a shared lock.
//
// Without a list of writer shaders every clone is active from its creation: all the game's views, copies and clears use it. With one ("seed
// mode", see "core.hpp"), a clone starts inactive: the game's views are created on the original as usual, and the hooks remember them
// (handle, kind, description). When a listed shader renders into the texture, the clone is promoted at the next present: the remembered
// views are created again on the clone and from then on everything follows it. Only the textures the listed shaders write get upgraded, the
// ones that hold packed data (IDs, masks) stay in their 8 bit format.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <d3d12.h>
#include <include/reshade.hpp>
#include <source/com_ptr.hpp>

#include "d3d12_resource_creation.h"

namespace D3D12Clones
{
   // A view the game created on a texture whose clone is inactive, to be created again on the clone when it gets promoted. The alternative of
   // "desc" is the kind of the view (SRV, UAV or RTV).
   struct ViewRecord
   {
      std::variant<D3D12_SHADER_RESOURCE_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC, D3D12_RENDER_TARGET_VIEW_DESC> desc;
      bool has_desc = false; // False if the game passed no description (the view takes the format of the resource)
   };

   // An atomic that the clones (kept in a map) can still be moved with: it is only moved before any other thread has the clone
   struct MovableFlag
   {
      std::atomic<bool> value = false;
      MovableFlag() = default;
      MovableFlag(const MovableFlag& other) : value(other.value.load())
      {
      }
      MovableFlag& operator=(const MovableFlag& other)
      {
         value.store(other.value.load());
         return *this;
      }
   };

   struct Clone
   {
      com_ptr<ID3D12Resource> resource;
      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
      bool layouts = false;            // Created with enhanced barriers' layouts (the barriers of the original must be mirrored as such)
      bool active = true;              // Written under the exclusive lock only. Inactive: the game's views are on the original, and are recorded in "views"
      MovableFlag promotion_requested; // A listed shader renders into the texture: "PromotePending()" activates the clone at the next present
      // The views of the game on the original while the clone is inactive, by the (ReShade encoded, for CBV/SRV/UAV heaps) CPU descriptor handle they were written to
      std::unordered_map<SIZE_T, ViewRecord> views;
   };

   // A clone whose original was destroyed. The GPU can still run commands that use it, so it lives until the fence passed the present after it.
   struct RetiredClone
   {
      com_ptr<ID3D12Resource> resource;
      uint64_t fence_value = 0; // 0 until a present signaled the fence behind it
   };

   struct Registry
   {
      std::shared_mutex mutex;
      std::unordered_map<ID3D12Resource*, Clone> by_original;
      std::atomic<uint32_t> count = 0; // Of "by_original", for the lock free fast out of every hook
      std::atomic<uint32_t> retired_count = 0;
      std::unordered_map<SIZE_T, ID3D12Resource*> record_owner; // Handle -> original, of every record in the "views" of the clones
      std::atomic<uint32_t> record_count = 0;                   // Of "record_owner", for the lock free fast out of the hooks that update the records
      std::atomic<uint32_t> inactive_count = 0;                 // The clones that aren't active, for the lock free fast out of "RequestPromotion()"
      std::atomic<uint32_t> pending_promotions = 0;             // The clones with "promotion_requested"
      std::vector<RetiredClone> retired;
      com_ptr<ID3D12Fence> fence;
      uint64_t last_signaled = 0;
   };

   // Never destroyed: the DLL can be unloaded after d3d12.dll, whose objects the members would release
   inline Registry& registry = *new Registry;

   // The clone of "original", null if it has none. The clone lives at least as long as its original (the retirement is the original's
   // destruction), so the pointer is good for as long as the caller has the original.
   // "active_only" is for what follows the clone's content (views, copies, clears): the barriers and discards go to every clone, which keeps
   // the state of an inactive one the original's.
   inline ID3D12Resource* Find(const ID3D12Resource* original, bool active_only = false)
   {
      if (registry.count.load(std::memory_order_relaxed) == 0)
      {
         return nullptr;
      }
      const std::shared_lock lock(registry.mutex);
      const auto found = registry.by_original.find(const_cast<ID3D12Resource*>(original));
      return (found != registry.by_original.end() && (found->second.active || !active_only) ? found->second.resource.get() : nullptr);
   }

   inline ID3D12Resource* FindActive(const ID3D12Resource* original)
   {
      return Find(original, true);
   }

   inline void Add(ID3D12Resource* original, Clone clone)
   {
      const std::unique_lock lock(registry.mutex);
      if (!clone.active)
      {
         registry.inactive_count.fetch_add(1);
      }
      registry.by_original.insert_or_assign(original, std::move(clone));
      registry.count.store(uint32_t(registry.by_original.size()), std::memory_order_relaxed);
   }

   // Drops the record of the view at "handle". The caller holds the exclusive lock.
   inline void EraseRecordLocked(SIZE_T handle)
   {
      const auto owner = registry.record_owner.find(handle);
      if (owner == registry.record_owner.end())
      {
         return;
      }
      if (const auto clone = registry.by_original.find(owner->second); clone != registry.by_original.end())
      {
         clone->second.views.erase(handle);
      }
      registry.record_owner.erase(owner);
      registry.record_count.store(uint32_t(registry.record_owner.size()), std::memory_order_relaxed);
   }

   // The clone moves to the retired ones (null if "original" had none), which "OnPresent()" releases when the GPU is done with them
   inline ID3D12Resource* Retire(const ID3D12Resource* original)
   {
      if (!Find(original)) // Games destroy buffers and transient textures all the time, which mustn't take the exclusive lock
      {
         return nullptr;
      }
      const std::unique_lock lock(registry.mutex);
      const auto found = registry.by_original.find(const_cast<ID3D12Resource*>(original));
      if (found == registry.by_original.end())
      {
         return nullptr;
      }
      ID3D12Resource* const clone = found->second.resource.get();
      for (const auto& [handle, record] : found->second.views)
      {
         registry.record_owner.erase(handle);
      }
      registry.record_count.store(uint32_t(registry.record_owner.size()), std::memory_order_relaxed);
      if (!found->second.active)
      {
         registry.inactive_count.fetch_sub(1);
      }
      if (found->second.promotion_requested.value.load())
      {
         registry.pending_promotions.fetch_sub(1);
      }
      registry.retired.push_back({.resource = std::move(found->second.resource)});
      registry.by_original.erase(found);
      registry.count.store(uint32_t(registry.by_original.size()), std::memory_order_relaxed);
      registry.retired_count.store(uint32_t(registry.retired.size()), std::memory_order_relaxed);
      return clone;
   }

   // At each present, on the present queue: the clones retired since the last one get the value of a signal behind everything the game
   // submitted on that queue so far, and the ones whose value passed are released. No wait.
   // ponytail: the fence is on the present queue only; a game that last used a clone on another queue and destroys its original without
   // synchronizing that queue to the present one breaks D3D12's own destruction rules too.
   inline void OnPresent(ID3D12CommandQueue* queue)
   {
      if (registry.retired_count.load(std::memory_order_relaxed) == 0)
      {
         return;
      }
      const std::unique_lock lock(registry.mutex);
      if (!registry.fence)
      {
         com_ptr<ID3D12Device> device;
         if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))) || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&registry.fence))))
         {
            return; // The clones stay retired (a leak) rather than released under the GPU
         }
      }
      bool needs_signal = false;
      for (const RetiredClone& retired : registry.retired)
      {
         needs_signal |= (retired.fence_value == 0);
      }
      if (needs_signal)
      {
         queue->Signal(registry.fence.get(), ++registry.last_signaled);
         for (RetiredClone& retired : registry.retired)
         {
            if (retired.fence_value == 0)
            {
               retired.fence_value = registry.last_signaled;
            }
         }
      }
      const uint64_t completed = registry.fence->GetCompletedValue();
      std::erase_if(registry.retired, [completed](const RetiredClone& retired)
         { return retired.fence_value <= completed; });
      registry.retired_count.store(uint32_t(registry.retired.size()), std::memory_order_relaxed);
   }

   // Where the game's view of "resource" at "handle" is created: on the clone if it is active, else on the original, and the view is recorded
   // if the clone is inactive ("record" null for the views that aren't of textures: they only drop the record of the handle they overwrite).
   // A view written into a recorded handle replaces the recorded one, so the record goes either way.
   inline ID3D12Resource* RouteView(ID3D12Resource* resource, SIZE_T handle, const ViewRecord* record)
   {
      if ((registry.count.load(std::memory_order_relaxed) == 0 && registry.record_count.load(std::memory_order_relaxed) == 0) || (!record && registry.record_count.load(std::memory_order_relaxed) == 0))
      {
         return resource;
      }
      ID3D12Resource* target = resource;
      bool owns_record = false;
      bool inactive_clone = false;
      {
         const std::shared_lock lock(registry.mutex);
         const auto found = registry.by_original.find(resource);
         if (found != registry.by_original.end())
         {
            if (found->second.active)
            {
               target = found->second.resource.get();
            }
            else
            {
               inactive_clone = (record != nullptr);
            }
         }
         owns_record = registry.record_owner.contains(handle);
      }
      if (inactive_clone || owns_record)
      {
         const std::unique_lock lock(registry.mutex);
         EraseRecordLocked(handle);
         const auto found = registry.by_original.find(resource);
         if (inactive_clone && found != registry.by_original.end() && !found->second.active)
         {
            found->second.views[handle] = *record;
            registry.record_owner[handle] = resource;
         }
         registry.record_count.store(uint32_t(registry.record_owner.size()), std::memory_order_relaxed);
      }
      return target;
   }

   // The game copied descriptors ("sources[i]" to "destinations[i]", in the encoding of the views' handles): a record follows its descriptor,
   // and an overwritten one is dropped.
   // ponytail: one lookup per copied descriptor while any record exists (shared lock), exclusive only when one is involved; a per heap bitmap
   // of recorded slots if a game copies enough descriptors for it to show in a profile.
   inline void PropagateRecords(const std::vector<std::pair<SIZE_T, SIZE_T>>& destinations_and_sources)
   {
      {
         const std::shared_lock lock(registry.mutex);
         bool involved = false;
         for (const auto& [destination, source] : destinations_and_sources)
         {
            involved |= (registry.record_owner.contains(destination) || registry.record_owner.contains(source));
         }
         if (!involved)
         {
            return;
         }
      }
      const std::unique_lock lock(registry.mutex);
      for (const auto& [destination, source] : destinations_and_sources)
      {
         if (destination == source)
         {
            continue;
         }
         ID3D12Resource* original = nullptr;
         std::optional<ViewRecord> copied;
         if (const auto owner = registry.record_owner.find(source); owner != registry.record_owner.end())
         {
            original = owner->second;
            if (const auto clone = registry.by_original.find(original); clone != registry.by_original.end())
            {
               if (const auto record = clone->second.views.find(source); record != clone->second.views.end())
               {
                  copied = record->second;
               }
            }
         }
         EraseRecordLocked(destination);
         if (copied)
         {
            registry.by_original.find(original)->second.views[destination] = *copied;
            registry.record_owner[destination] = original;
         }
      }
      registry.record_count.store(uint32_t(registry.record_owner.size()), std::memory_order_relaxed);
   }

   // The records of the descriptors in a descriptor heap that was destroyed. ReShade reuses the index of a destroyed heap in its handles, and
   // the native handles of an RTV heap are addresses that another heap can get, so a record in a dead heap would end up in someone else's.
   inline void DropRecordsInRange(SIZE_T first_handle, SIZE_T byte_size)
   {
      if (registry.record_count.load(std::memory_order_relaxed) == 0)
      {
         return;
      }
      const std::unique_lock lock(registry.mutex);
      std::vector<SIZE_T> dead;
      for (const auto& [handle, original] : registry.record_owner)
      {
         if (handle >= first_handle && handle - first_handle < byte_size)
         {
            dead.push_back(handle);
         }
      }
      for (const SIZE_T handle : dead)
      {
         EraseRecordLocked(handle);
      }
   }

   // Set as private data of the native descriptor heaps (the CBV/SRV/UAV and RTV ones, where the records live): D3D12 releases it when the
   // heap is destroyed, which nothing else tells reliably. ReShade's proxy heap "Release()" can't say when the last reference went, and
   // its RTV heaps aren't proxied at all.
   // The heaps that have one, so "Remove()" can take the sentinels back before the module unloads: a heap can outlive the add-on, and
   // D3D12 would release the sentinel through code that is gone. (Pinning the module instead keeps its statics across the reload ReShade
   // does between device cycles, which breaks the next "Init()".) Recursive: "Remove()" clears the private data of the heaps while holding it,
   // which destroys the sentinels, which take it to forget their heap.
   struct TrackedHeaps
   {
      std::recursive_mutex mutex;
      std::unordered_set<ID3D12DescriptorHeap*> heaps;
   };
   inline TrackedHeaps& tracked_heaps = *new TrackedHeaps; // Never destroyed, like the registry

   struct HeapSentinel final : IUnknown
   {
      ID3D12DescriptorHeap* heap = nullptr;
      SIZE_T first_handle = 0; // Of the heap's CPU descriptors, as the game sees them
      SIZE_T byte_size = 0;
      LONG references = 1;

      HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
      {
         if (!object)
         {
            return E_POINTER;
         }
         if (riid != __uuidof(IUnknown))
         {
            *object = nullptr;
            return E_NOINTERFACE;
         }
         *object = this;
         AddRef();
         return S_OK;
      }
      ULONG STDMETHODCALLTYPE AddRef() override
      {
         return ULONG(InterlockedIncrement(&references));
      }
      ULONG STDMETHODCALLTYPE Release() override
      {
         const ULONG remaining = ULONG(InterlockedDecrement(&references));
         if (remaining == 0)
         {
            DropRecordsInRange(first_handle, byte_size);
            {
               const std::lock_guard lock(tracked_heaps.mutex);
               tracked_heaps.heaps.erase(heap); // Already gone if "Remove()" took it
            }
            delete this;
         }
         return remaining;
      }
   };
   constexpr GUID HEAP_SENTINEL_GUID = {0x5f4a7c1e, 0x2b63, 0x4d0a, {0x9e, 0x31, 0x6c, 0x8d, 0x07, 0xa5, 0x42, 0xb9}};

   // A listed shader renders into the texture of "original": the next present activates its clone
   inline void RequestPromotion(const ID3D12Resource* original)
   {
      if (registry.inactive_count.load(std::memory_order_relaxed) == 0)
      {
         return;
      }
      const std::shared_lock lock(registry.mutex);
      const auto found = registry.by_original.find(const_cast<ID3D12Resource*>(original));
      if (found != registry.by_original.end() && !found->second.active && !found->second.promotion_requested.value.exchange(true))
      {
         registry.pending_promotions.fetch_add(1);
      }
   }

   struct PromotedClone
   {
      DXGI_FORMAT original_format = DXGI_FORMAT_UNKNOWN;
      DXGI_FORMAT clone_format = DXGI_FORMAT_UNKNOWN;
      size_t view_count = 0;
   };

   struct Promotion
   {
      std::vector<PromotedClone> clones;
      double waited_ms = 0.0;
      bool timed_out = false;
   };

   // At present, on the present queue, never mid frame: shader visible descriptors can be read by lists the GPU still runs. Waits for the GPU
   // to be done with the queue (one hitch per batch, at load or resize), then creates the recorded views again on the clones of the
   // requested promotions, through ReShade's proxy (so it updates its view registry and upgrades the formats like it does for any view), and
   // sets the clones active.
   // ponytail: waits on the present queue only (work of async compute/copy queues isn't synchronized); command lists recorded for the next
   // frame already consumed their RTV descriptors, so the first promoted frame can show a frame of stale content; a game thread copying a
   // descriptor while it is created again could read a torn one (descriptors have no lock to take).
   inline Promotion PromotePending(ID3D12CommandQueue* queue)
   {
      Promotion promotion;
      if (registry.pending_promotions.load(std::memory_order_relaxed) == 0)
      {
         return promotion;
      }
      com_ptr<ID3D12Device> device;
      ID3D12Device* const proxy = (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&device))) ? D3D12ProxyHook::FindProxy(device.get()) : nullptr);
      if (!proxy)
      {
         return promotion;
      }
      ID3D12Fence* fence = nullptr;
      uint64_t wait_value = 0;
      {
         const std::unique_lock lock(registry.mutex);
         if (!registry.fence && FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&registry.fence))))
         {
            return promotion;
         }
         fence = registry.fence.get();
         wait_value = ++registry.last_signaled;
         queue->Signal(fence, wait_value);
      }
      const auto wait_start = std::chrono::steady_clock::now();
      // A fresh event each time: one that a timed out wait left pending would end the next wait early
      const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      const bool waited = (event && SUCCEEDED(fence->SetEventOnCompletion(wait_value, event)) && WaitForSingleObject(event, 2000) == WAIT_OBJECT_0);
      if (event)
      {
         CloseHandle(event);
      }
      promotion.waited_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wait_start).count();
      if (!waited)
      {
         promotion.timed_out = true; // The requests stay: the next present tries again
         return promotion;
      }

      struct Recreation
      {
         ID3D12Resource* clone = nullptr;
         std::unordered_map<SIZE_T, ViewRecord> views;
      };
      std::vector<Recreation> recreations;
      {
         const std::unique_lock lock(registry.mutex);
         for (auto& [original, clone] : registry.by_original)
         {
            if (clone.active || !clone.promotion_requested.value.load())
            {
               continue;
            }
            clone.active = true;
            registry.inactive_count.fetch_sub(1);
            clone.promotion_requested.value.store(false);
            for (const auto& [handle, record] : clone.views)
            {
               registry.record_owner.erase(handle);
            }
            promotion.clones.push_back({.original_format = original->GetDesc().Format, .clone_format = clone.format, .view_count = clone.views.size()});
            recreations.push_back({.clone = clone.resource.get(), .views = std::move(clone.views)});
            clone.views.clear();
         }
         registry.pending_promotions.store(0);
         registry.record_count.store(uint32_t(registry.record_owner.size()), std::memory_order_relaxed);
      }
      // No lock held: ReShade fires its events from inside these, and the detours of the views take the registry's
      for (const Recreation& recreation : recreations)
      {
         for (const auto& [handle, record] : recreation.views)
         {
            const D3D12_CPU_DESCRIPTOR_HANDLE descriptor = {handle};
            switch (record.desc.index())
            {
            case 0:
               proxy->CreateShaderResourceView(recreation.clone, (record.has_desc ? &std::get<0>(record.desc) : nullptr), descriptor);
               break;
            case 1:
               proxy->CreateUnorderedAccessView(recreation.clone, nullptr, (record.has_desc ? &std::get<1>(record.desc) : nullptr), descriptor);
               break;
            default:
               proxy->CreateRenderTargetView(recreation.clone, (record.has_desc ? &std::get<2>(record.desc) : nullptr), descriptor);
               break;
            }
         }
      }
      return promotion;
   }

   // The clear value of a clone in "format", with the colour the game's value has. The upgraded format of a clone can be typeless, and a
   // clear value needs a typed one: none then (the clear value is only an optimization hint).
   inline std::optional<D3D12_CLEAR_VALUE> ClearValueFor(DXGI_FORMAT format, const D3D12ResourceCreation::PlacedCreationArguments& arguments)
   {
      switch (format)
      {
      case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_B8G8R8A8_TYPELESS:
         return std::nullopt;
      default:
         break;
      }
      if (!arguments.has_clear_value)
      {
         return std::nullopt;
      }
      D3D12_CLEAR_VALUE clear_value = arguments.clear_value;
      clear_value.Format = format;
      return clear_value;
   }

   // Creates the clone of the placed texture "original" in "format" on the native device and registers it, with the arguments the game's
   // create had. A committed resource in the default heap, with the flags and the initial state (or layout) of the original. Null if
   // the device refuses it (the game keeps its texture as it is). An inactive clone ("active" false) waits for its promotion.
   inline ID3D12Resource* Create(ID3D12Device* device, ID3D12Resource* original, DXGI_FORMAT format, const D3D12ResourceCreation::PlacedCreationArguments& arguments, bool active)
   {
      const D3D12_HEAP_PROPERTIES default_heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
      const std::optional<D3D12_CLEAR_VALUE> clear_value = ClearValueFor(format, arguments);
      const D3D12_CLEAR_VALUE* const clear_value_pointer = (clear_value ? &*clear_value : nullptr);
      com_ptr<ID3D12Resource> resource;
      HRESULT result = E_FAIL;
      if (arguments.uses_layouts)
      {
         com_ptr<ID3D12Resource2> original2;
         com_ptr<ID3D12Device10> device10;
         if (SUCCEEDED(original->QueryInterface(IID_PPV_ARGS(&original2))) && SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&device10))))
         {
            D3D12_RESOURCE_DESC1 desc = original2->GetDesc1();
            desc.Format = format;
            desc.Alignment = 0; // The placement alignment the game chose (e.g. 4 KB "small" textures) doesn't carry over to a bigger format
            result = device10->CreateCommittedResource3(&default_heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_BARRIER_LAYOUT(arguments.initial_state), clear_value_pointer, nullptr, 0, nullptr, IID_PPV_ARGS(&resource));
         }
      }
      else
      {
         D3D12_RESOURCE_DESC desc = original->GetDesc();
         desc.Format = format;
         desc.Alignment = 0;
         result = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATES(arguments.initial_state), clear_value_pointer, IID_PPV_ARGS(&resource));
      }
      if (FAILED(result))
      {
         return nullptr;
      }
      ID3D12Resource* const clone = resource.get();
      Add(original, {.resource = std::move(resource), .format = format, .layouts = arguments.uses_layouts, .active = active});
      return clone;
   }

   // A function of ReShade's proxy hooked once per process, in the way of "D3D12ResourceCreation::Creation"
   template <typename Function, size_t vtable_slot>
   struct ProxyFunction
   {
      static inline D3D12ProxyHook::HookedFunction<Function> hooked;

      static bool Install(void** proxy_vtable, Function detour)
      {
         return hooked.target || D3D12ProxyHook::Attach(&hooked, proxy_vtable[vtable_slot], detour);
      }

      static bool Remove()
      {
         return D3D12ProxyHook::Detach(&hooked);
      }
   };

   // The slots of "ID3D12Device" (and of "ID3D12Device4" for the second command list create) and of "ID3D12GraphicsCommandList7", which ReShade's
   // proxies implement (the interfaces after them only append methods). Checked against d3d12.h's C vtables, counting each method once
   // (like "CREATE_COMMITTED_RESOURCE1_SLOT" 53, which follows "CreateCommandList1" and "CreateProtectedResourceSession").
   constexpr size_t CREATE_COMMAND_LIST_SLOT = 12;
   constexpr size_t CREATE_DESCRIPTOR_HEAP_SLOT = 14;
   constexpr size_t CREATE_CONSTANT_BUFFER_VIEW_SLOT = 17;
   constexpr size_t CREATE_SHADER_RESOURCE_VIEW_SLOT = 18;
   constexpr size_t CREATE_UNORDERED_ACCESS_VIEW_SLOT = 19;
   constexpr size_t CREATE_RENDER_TARGET_VIEW_SLOT = 20;
   constexpr size_t COPY_DESCRIPTORS_SLOT = 23;
   constexpr size_t COPY_DESCRIPTORS_SIMPLE_SLOT = 24;
   // "ID3D12Device15" (not in every SDK's d3d12.h, so its detours take an "ID3D12Device*") appends the "TryCreate" views, which bypass the ones above: counted like the others, each method once
   constexpr size_t TRY_CREATE_SHADER_RESOURCE_VIEW_SLOT = 85;
   constexpr size_t TRY_CREATE_UNORDERED_ACCESS_VIEW_SLOT = 86;
   constexpr size_t TRY_CREATE_CONSTANT_BUFFER_VIEW_SLOT = 87;
   constexpr size_t TRY_CREATE_RENDER_TARGET_VIEW_SLOT = 89;
   constexpr size_t CREATE_COMMAND_LIST1_SLOT = 51; // d3d12.h's C vtable lists 3 earlier methods twice (C and C++ return conventions)
   constexpr size_t RESOURCE_BARRIER_SLOT = 26;
   constexpr size_t CLEAR_UNORDERED_ACCESS_VIEW_UINT_SLOT = 49;
   constexpr size_t CLEAR_UNORDERED_ACCESS_VIEW_FLOAT_SLOT = 50;
   constexpr size_t DISCARD_RESOURCE_SLOT = 51;
   constexpr size_t BARRIER_SLOT = 80;

   // The views of a game's texture are created on its clone (once it is active, see "RouteView()"), so the descriptors (which ReShade copies and
   // hands to the shaders as native ones) and its view registry both name the clone. The description stays the game's: "OnCreateResourceView"
   // upgrades its format. The views the game creates on an inactive clone's original are recorded to be created again at the promotion.
   using CreateShaderResourceViewHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), CREATE_SHADER_RESOURCE_VIEW_SLOT>;
   using CreateUnorderedAccessViewHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, ID3D12Resource*, const D3D12_UNORDERED_ACCESS_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), CREATE_UNORDERED_ACCESS_VIEW_SLOT>;
   using CreateRenderTargetViewHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), CREATE_RENDER_TARGET_VIEW_SLOT>;
   using CreateConstantBufferViewHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), CREATE_CONSTANT_BUFFER_VIEW_SLOT>;
   using TryCreateShaderResourceViewHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), TRY_CREATE_SHADER_RESOURCE_VIEW_SLOT>;
   using TryCreateUnorderedAccessViewHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, ID3D12Resource*, const D3D12_UNORDERED_ACCESS_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), TRY_CREATE_UNORDERED_ACCESS_VIEW_SLOT>;
   using TryCreateRenderTargetViewHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), TRY_CREATE_RENDER_TARGET_VIEW_SLOT>;
   using TryCreateConstantBufferViewHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE), TRY_CREATE_CONSTANT_BUFFER_VIEW_SLOT>;

   // A description the game passed, copied into a record
   template <typename Description>
   ViewRecord MakeViewRecord(const Description* desc)
   {
      ViewRecord record;
      record.desc = (desc ? *desc : Description{});
      record.has_desc = (desc != nullptr);
      return record;
   }

   inline void STDMETHODCALLTYPE CreateShaderResourceViewDetour(ID3D12Device* device, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      CreateShaderResourceViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), desc, handle);
   }

   // The counter resource is a buffer, which has no clones
   inline void STDMETHODCALLTYPE CreateUnorderedAccessViewDetour(ID3D12Device* device, ID3D12Resource* resource, ID3D12Resource* counter_resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      CreateUnorderedAccessViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), counter_resource, desc, handle);
   }

   inline void STDMETHODCALLTYPE CreateRenderTargetViewDetour(ID3D12Device* device, ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      CreateRenderTargetViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), desc, handle);
   }

   // A constant buffer view overwrites the descriptor of a recorded view in the same heap
   inline void STDMETHODCALLTYPE CreateConstantBufferViewDetour(ID3D12Device* device, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      RouteView(nullptr, handle.ptr, nullptr);
      CreateConstantBufferViewHook::hooked.original(device, desc, handle);
   }

   inline HRESULT STDMETHODCALLTYPE TryCreateShaderResourceViewDetour(ID3D12Device* device, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      return TryCreateShaderResourceViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), desc, handle);
   }

   inline HRESULT STDMETHODCALLTYPE TryCreateUnorderedAccessViewDetour(ID3D12Device* device, ID3D12Resource* resource, ID3D12Resource* counter_resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      return TryCreateUnorderedAccessViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), counter_resource, desc, handle);
   }

   inline HRESULT STDMETHODCALLTYPE TryCreateRenderTargetViewDetour(ID3D12Device* device, ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      const ViewRecord record = MakeViewRecord(desc);
      return TryCreateRenderTargetViewHook::hooked.original(device, RouteView(resource, handle.ptr, &record), desc, handle);
   }

   inline HRESULT STDMETHODCALLTYPE TryCreateConstantBufferViewDetour(ID3D12Device* device, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
   {
      RouteView(nullptr, handle.ptr, nullptr);
      return TryCreateConstantBufferViewHook::hooked.original(device, desc, handle);
   }

   using CopyDescriptorsHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, D3D12_DESCRIPTOR_HEAP_TYPE), COPY_DESCRIPTORS_SLOT>;
   using CopyDescriptorsSimpleHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE), COPY_DESCRIPTORS_SIMPLE_SLOT>;

   // Only the heaps the views are written to (CBV/SRV/UAV and RTV) can hold records
   constexpr bool HoldsRecords(D3D12_DESCRIPTOR_HEAP_TYPE type)
   {
      return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   }

   // The range lists of a copy walked in step, as (destination, source) pairs: destination "i" of a range is its start + "i" * the type's
   // increment, in the game's encoding on both sides
   inline void STDMETHODCALLTYPE CopyDescriptorsDetour(ID3D12Device* device, UINT destination_range_count, const D3D12_CPU_DESCRIPTOR_HANDLE* destination_starts, const UINT* destination_sizes, UINT source_range_count, const D3D12_CPU_DESCRIPTOR_HANDLE* source_starts, const UINT* source_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type)
   {
      CopyDescriptorsHook::hooked.original(device, destination_range_count, destination_starts, destination_sizes, source_range_count, source_starts, source_sizes, type);
      if (registry.record_count.load(std::memory_order_relaxed) == 0 || !HoldsRecords(type))
      {
         return;
      }
      const SIZE_T increment = device->GetDescriptorHandleIncrementSize(type);
      thread_local std::vector<std::pair<SIZE_T, SIZE_T>> destinations_and_sources;
      destinations_and_sources.clear();
      const auto range_size = [](const UINT* sizes, UINT range)
      { return (sizes ? sizes[range] : 1u); }; // Null sizes: every range is one descriptor
      UINT destination_range = 0;
      UINT source_range = 0;
      UINT destination_offset = 0;
      UINT source_offset = 0;
      while (destination_range < destination_range_count && source_range < source_range_count)
      {
         if (destination_offset >= range_size(destination_sizes, destination_range))
         {
            destination_range++;
            destination_offset = 0;
         }
         else if (source_offset >= range_size(source_sizes, source_range))
         {
            source_range++;
            source_offset = 0;
         }
         else
         {
            destinations_and_sources.emplace_back(destination_starts[destination_range].ptr + destination_offset * increment, source_starts[source_range].ptr + source_offset * increment);
            destination_offset++;
            source_offset++;
         }
      }
      PropagateRecords(destinations_and_sources);
   }

   inline void STDMETHODCALLTYPE CopyDescriptorsSimpleDetour(ID3D12Device* device, UINT descriptor_count, D3D12_CPU_DESCRIPTOR_HANDLE destination_start, D3D12_CPU_DESCRIPTOR_HANDLE source_start, D3D12_DESCRIPTOR_HEAP_TYPE type)
   {
      CopyDescriptorsSimpleHook::hooked.original(device, descriptor_count, destination_start, source_start, type);
      if (registry.record_count.load(std::memory_order_relaxed) == 0 || !HoldsRecords(type))
      {
         return;
      }
      const SIZE_T increment = device->GetDescriptorHandleIncrementSize(type);
      thread_local std::vector<std::pair<SIZE_T, SIZE_T>> destinations_and_sources;
      destinations_and_sources.clear();
      for (UINT i = 0; i < descriptor_count; i++)
      {
         destinations_and_sources.emplace_back(destination_start.ptr + i * increment, source_start.ptr + i * increment);
      }
      PropagateRecords(destinations_and_sources);
   }

   // Every heap the records can live in gets a "HeapSentinel", so its destruction drops its records. Only hooked in seed mode.
   using CreateDescriptorHeapHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_DESCRIPTOR_HEAP_DESC*, REFIID, void**), CREATE_DESCRIPTOR_HEAP_SLOT>;

   inline HRESULT STDMETHODCALLTYPE CreateDescriptorHeapDetour(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_DESC* desc, REFIID riid, void** heap_out)
   {
      const HRESULT result = CreateDescriptorHeapHook::hooked.original(device, desc, riid, heap_out);
      if (SUCCEEDED(result) && desc && heap_out && *heap_out && riid == __uuidof(ID3D12DescriptorHeap) && HoldsRecords(desc->Type))
      {
         ID3D12DescriptorHeap* const heap = static_cast<ID3D12DescriptorHeap*>(*heap_out);
         HeapSentinel* const sentinel = new HeapSentinel;
         sentinel->first_handle = heap->GetCPUDescriptorHandleForHeapStart().ptr;
         sentinel->byte_size = SIZE_T(desc->NumDescriptors) * device->GetDescriptorHandleIncrementSize(desc->Type);
         sentinel->heap = heap;
         {
            const std::lock_guard lock(tracked_heaps.mutex);
            tracked_heaps.heaps.insert(heap);
         }
         heap->SetPrivateDataInterface(HEAP_SENTINEL_GUID, sentinel);
         sentinel->Release();
      }
      return result;
   }

   using ResourceBarrierHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*), RESOURCE_BARRIER_SLOT>;
   using BarrierHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList7*, UINT32, const D3D12_BARRIER_GROUP*), BARRIER_SLOT>;
   using ClearUnorderedAccessViewUintHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource*, const UINT[4], UINT, const D3D12_RECT*), CLEAR_UNORDERED_ACCESS_VIEW_UINT_SLOT>;
   using ClearUnorderedAccessViewFloatHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource*, const FLOAT[4], UINT, const D3D12_RECT*), CLEAR_UNORDERED_ACCESS_VIEW_FLOAT_SLOT>;
   using DiscardResourceHook = ProxyFunction<void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, const D3D12_DISCARD_REGION*), DISCARD_RESOURCE_SLOT>;

   // Each barrier of the game on a texture that has a clone is followed by the same one on the clone, so the clone goes through the states
   // of the original (it was created in the original's initial state). Aliasing barriers aren't mirrored: the clones are committed.
   inline void STDMETHODCALLTYPE ResourceBarrierDetour(ID3D12GraphicsCommandList* list, UINT barrier_count, const D3D12_RESOURCE_BARRIER* barriers)
   {
      if (registry.count.load(std::memory_order_relaxed) == 0)
      {
         ResourceBarrierHook::hooked.original(list, barrier_count, barriers);
         return;
      }
      thread_local std::vector<D3D12_RESOURCE_BARRIER> combined; // Reused by the thread: allocates only while it grows
      combined.clear();
      for (UINT i = 0; i < barrier_count; i++)
      {
         combined.push_back(barriers[i]);
         D3D12_RESOURCE_BARRIER mirrored = barriers[i];
         if (mirrored.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
         {
            mirrored.Transition.pResource = Find(mirrored.Transition.pResource);
            if (mirrored.Transition.pResource)
            {
               combined.push_back(mirrored);
            }
         }
         else if (mirrored.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
         {
            mirrored.UAV.pResource = Find(mirrored.UAV.pResource);
            if (mirrored.UAV.pResource)
            {
               combined.push_back(mirrored);
            }
         }
      }
      ResourceBarrierHook::hooked.original(list, UINT(combined.size()), combined.data());
   }

   inline void STDMETHODCALLTYPE BarrierDetour(ID3D12GraphicsCommandList7* list, UINT32 group_count, const D3D12_BARRIER_GROUP* groups)
   {
      if (registry.count.load(std::memory_order_relaxed) == 0)
      {
         BarrierHook::hooked.original(list, group_count, groups);
         return;
      }
      // The texture barriers of all groups go into one vector, and the groups point into it once it is complete (it moves while it grows)
      thread_local std::vector<D3D12_TEXTURE_BARRIER> textures;
      thread_local std::vector<size_t> texture_starts;
      thread_local std::vector<D3D12_BARRIER_GROUP> combined_groups;
      textures.clear();
      texture_starts.clear();
      combined_groups.assign(groups, groups + group_count);
      for (UINT32 i = 0; i < group_count; i++)
      {
         texture_starts.push_back(textures.size());
         if (groups[i].Type != D3D12_BARRIER_TYPE_TEXTURE)
         {
            continue;
         }
         for (UINT32 j = 0; j < groups[i].NumBarriers; j++)
         {
            textures.push_back(groups[i].pTextureBarriers[j]);
            D3D12_TEXTURE_BARRIER mirrored = groups[i].pTextureBarriers[j];
            mirrored.pResource = Find(mirrored.pResource);
            if (mirrored.pResource)
            {
               textures.push_back(mirrored);
            }
         }
         combined_groups[i].NumBarriers = UINT32(textures.size() - texture_starts[i]);
      }
      for (UINT32 i = 0; i < group_count; i++)
      {
         if (groups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
         {
            combined_groups[i].pTextureBarriers = textures.data() + texture_starts[i];
         }
      }
      BarrierHook::hooked.original(list, group_count, combined_groups.data());
   }

   // The descriptors the game cleared with already name the clone if it is active (see the views), the resource argument has to follow
   inline void STDMETHODCALLTYPE ClearUnorderedAccessViewUintDetour(ID3D12GraphicsCommandList* list, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource* resource, const UINT values[4], UINT rect_count, const D3D12_RECT* rects)
   {
      ID3D12Resource* const clone = FindActive(resource);
      ClearUnorderedAccessViewUintHook::hooked.original(list, gpu_handle, cpu_handle, (clone ? clone : resource), values, rect_count, rects);
   }

   inline void STDMETHODCALLTYPE ClearUnorderedAccessViewFloatDetour(ID3D12GraphicsCommandList* list, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource* resource, const FLOAT values[4], UINT rect_count, const D3D12_RECT* rects)
   {
      ID3D12Resource* const clone = FindActive(resource);
      ClearUnorderedAccessViewFloatHook::hooked.original(list, gpu_handle, cpu_handle, (clone ? clone : resource), values, rect_count, rects);
   }

   inline void STDMETHODCALLTYPE DiscardResourceDetour(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, const D3D12_DISCARD_REGION* region)
   {
      DiscardResourceHook::hooked.original(list, resource, region);
      if (ID3D12Resource* const clone = Find(resource))
      {
         DiscardResourceHook::hooked.original(list, clone, region);
      }
   }

   inline std::atomic<bool> command_list_hooks_installed = false;
   inline std::mutex command_list_hooks_mutex; // Also for "Remove()"

   // ReShade's command lists are objects of one final class, so the first one's vtable is every list's: the command list hooks go in when
   // the game creates its first list.
   inline void OnCommandListCreated(void** command_list)
   {
      if (command_list_hooks_installed.load(std::memory_order_acquire) || !command_list || !*command_list)
      {
         return;
      }
      const std::lock_guard lock(command_list_hooks_mutex);
      if (command_list_hooks_installed.load(std::memory_order_relaxed))
      {
         return;
      }
      void** const vtable = *reinterpret_cast<void***>(*command_list);
      bool installed_all = ResourceBarrierHook::Install(vtable, &ResourceBarrierDetour);
      installed_all &= BarrierHook::Install(vtable, &BarrierDetour);
      installed_all &= ClearUnorderedAccessViewUintHook::Install(vtable, &ClearUnorderedAccessViewUintDetour);
      installed_all &= ClearUnorderedAccessViewFloatHook::Install(vtable, &ClearUnorderedAccessViewFloatDetour);
      installed_all &= DiscardResourceHook::Install(vtable, &DiscardResourceDetour);
      if (!installed_all)
      {
         reshade::log::message(reshade::log::level::warning, "[Texture upgrades] The command list hooks of the clones aren't all in, so the clones of placed textures can end in a wrong state");
      }
      command_list_hooks_installed.store(true, std::memory_order_release);
   }

   using CreateCommandListHook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator*, ID3D12PipelineState*, REFIID, void**), CREATE_COMMAND_LIST_SLOT>;
   using CreateCommandList1Hook = ProxyFunction<HRESULT(STDMETHODCALLTYPE*)(ID3D12Device4*, UINT, D3D12_COMMAND_LIST_TYPE, D3D12_COMMAND_LIST_FLAGS, REFIID, void**), CREATE_COMMAND_LIST1_SLOT>;

   inline HRESULT STDMETHODCALLTYPE CreateCommandListDetour(ID3D12Device* device, UINT node_mask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* allocator, ID3D12PipelineState* initial_state, REFIID riid, void** command_list)
   {
      const HRESULT result = CreateCommandListHook::hooked.original(device, node_mask, type, allocator, initial_state, riid, command_list);
      if (SUCCEEDED(result))
      {
         OnCommandListCreated(command_list);
      }
      return result;
   }

   inline HRESULT STDMETHODCALLTYPE CreateCommandList1Detour(ID3D12Device4* device, UINT node_mask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags, REFIID riid, void** command_list)
   {
      const HRESULT result = CreateCommandList1Hook::hooked.original(device, node_mask, type, flags, riid, command_list);
      if (SUCCEEDED(result))
      {
         OnCommandListCreated(command_list);
      }
      return result;
   }

   inline std::mutex hooks_mutex; // For "Install()" and "Remove()"

   // Hooks the proxy's view creates, descriptor copies and command list creates (the command list functions follow with the first list), once
   // per process. "seed_mode" (clones start inactive, see the top of this file) also hooks the creation of descriptor heaps. False if there
   // is no ReShade proxy, or MinHook failed.
   inline bool Install(ID3D12Device* native_device, bool seed_mode)
   {
      void** const proxy_vtable = D3D12ProxyHook::FindProxyVtable(native_device);
      if (!proxy_vtable)
      {
         return false;
      }
      const std::lock_guard lock(hooks_mutex);
      bool installed_all = CreateShaderResourceViewHook::Install(proxy_vtable, &CreateShaderResourceViewDetour);
      installed_all &= CreateUnorderedAccessViewHook::Install(proxy_vtable, &CreateUnorderedAccessViewDetour);
      installed_all &= CreateRenderTargetViewHook::Install(proxy_vtable, &CreateRenderTargetViewDetour);
      installed_all &= CreateConstantBufferViewHook::Install(proxy_vtable, &CreateConstantBufferViewDetour);
      installed_all &= TryCreateShaderResourceViewHook::Install(proxy_vtable, &TryCreateShaderResourceViewDetour);
      installed_all &= TryCreateUnorderedAccessViewHook::Install(proxy_vtable, &TryCreateUnorderedAccessViewDetour);
      installed_all &= TryCreateRenderTargetViewHook::Install(proxy_vtable, &TryCreateRenderTargetViewDetour);
      installed_all &= TryCreateConstantBufferViewHook::Install(proxy_vtable, &TryCreateConstantBufferViewDetour);
      installed_all &= CopyDescriptorsHook::Install(proxy_vtable, &CopyDescriptorsDetour);
      installed_all &= CopyDescriptorsSimpleHook::Install(proxy_vtable, &CopyDescriptorsSimpleDetour);
      if (seed_mode)
      {
         installed_all &= CreateDescriptorHeapHook::Install(proxy_vtable, &CreateDescriptorHeapDetour);
      }
      installed_all &= CreateCommandListHook::Install(proxy_vtable, &CreateCommandListDetour);
      installed_all &= CreateCommandList1Hook::Install(proxy_vtable, &CreateCommandList1Detour);
      return installed_all;
   }

   // See "D3D12ProxyHook::Detach()"
   inline bool Remove()
   {
      const std::lock_guard lock(hooks_mutex);
      const std::lock_guard list_lock(command_list_hooks_mutex);
      bool removed_all = CreateShaderResourceViewHook::Remove();
      removed_all &= CreateUnorderedAccessViewHook::Remove();
      removed_all &= CreateRenderTargetViewHook::Remove();
      removed_all &= CreateConstantBufferViewHook::Remove();
      removed_all &= TryCreateShaderResourceViewHook::Remove();
      removed_all &= TryCreateUnorderedAccessViewHook::Remove();
      removed_all &= TryCreateRenderTargetViewHook::Remove();
      removed_all &= TryCreateConstantBufferViewHook::Remove();
      removed_all &= CopyDescriptorsHook::Remove();
      removed_all &= CopyDescriptorsSimpleHook::Remove();
      removed_all &= CreateDescriptorHeapHook::Remove();
      {
         // The sentinels go while this code is still loaded. ponytail: a heap being destroyed on another thread at this very moment isn't
         // guarded (the add-on unloads at device or process teardown, when the game is done with its heaps).
         const std::lock_guard heaps_lock(tracked_heaps.mutex);
         const std::vector<ID3D12DescriptorHeap*> heaps(tracked_heaps.heaps.begin(), tracked_heaps.heaps.end());
         tracked_heaps.heaps.clear();
         for (ID3D12DescriptorHeap* const heap : heaps)
         {
            heap->SetPrivateDataInterface(HEAP_SENTINEL_GUID, nullptr);
         }
      }
      removed_all &= CreateCommandListHook::Remove();
      removed_all &= CreateCommandList1Hook::Remove();
      removed_all &= ResourceBarrierHook::Remove();
      removed_all &= BarrierHook::Remove();
      removed_all &= ClearUnorderedAccessViewUintHook::Remove();
      removed_all &= ClearUnorderedAccessViewFloatHook::Remove();
      removed_all &= DiscardResourceHook::Remove();
      command_list_hooks_installed.store(false, std::memory_order_release);
      return removed_all;
   }
} // namespace D3D12Clones

#endif // DX12 && ENABLE_MINHOOK
