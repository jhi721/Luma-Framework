#pragma once

#if DX12 && ENABLE_MINHOOK

// How the game created each resource, for the code that can't treat them alike: a committed resource owns its memory, so it can be created
// bigger (a format upgrade), a placed one lives in a heap the game sized and shares with other resources (a bigger one would overflow it or
// alias them) and a reserved one has no memory until the game maps tiles to it. ReShade's "create_resource" and "init_resource" get the same
// description for the three and only flag the reserved ones ("sparse_binding"). So the game's committed and placed creates are hooked on
// ReShade's device proxy (one function per method for every proxy), where each detour sets a thread local for the length of the call:
// ReShade fires its events inside the call, on the same thread, and "KindOf()" reads the thread local from there.
// The creates that take castable formats (the ones of "ID3D12Device10") are noted the same way, as ReShade passes them on unchanged when an
// add-on changes the format of the resource: a resource can only be cast to formats of its own family, so an upgraded one would make the
// game's call fail.
// It also holds what the root signature hook ("d3d12_root_signatures.h") shares with it: finding ReShade's device proxy, and hooking and
// unhooking one function of it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <Windows.h>
#include <d3d12.h>
#include <include/reshade_api_resource.hpp>

#include "minhook.h"

namespace D3D12ProxyHook
{
   // ReShade keeps its device proxy in the native device's private data, under the proxy's class UUID ("D3D12Device" in "d3d12_device.hpp")
   constexpr GUID RESHADE_DEVICE_PROXY = {0x2523AFF4, 0x978B, 0x4939, {0xBA, 0x16, 0x8E, 0xE8, 0x76, 0xA4, 0xCB, 0x2A}};

   // The vtable of the device's ReShade proxy (the proxies are objects of one class, so they share it), null if the device has none
   inline void** FindProxyVtable(ID3D12Device* native_device)
   {
      void* proxy = nullptr;
      UINT proxy_size = sizeof(proxy);
      if (FAILED(native_device->GetPrivateData(RESHADE_DEVICE_PROXY, &proxy_size, &proxy)) || proxy_size != sizeof(proxy) || !proxy)
      {
         return nullptr;
      }
      return *reinterpret_cast<void***>(proxy);
   }

   // A function of the proxy hooked with MinHook. The owner locks around "Attach()" and "Detach()", the detour reads the rest without it as it only runs between them.
   template <typename Function>
   struct HookedFunction
   {
      void* target = nullptr;      // The function the hook went on, null if there is none
      Function original = nullptr; // The trampoline: the target as it was before the hook
      // The target's start right after our hook went in: different later means another module hooked over it
      std::array<uint8_t, 8> hooked_prologue = {};
   };

   // False if MinHook failed
   template <typename Function>
   bool Attach(HookedFunction<Function>* hooked, void* target, Function detour)
   {
      if (!InitializeMinHook() || MH_CreateHook(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(&hooked->original)) != MH_OK)
      {
         return false;
      }
      if (MH_EnableHook(target) != MH_OK)
      {
         MH_RemoveHook(target);
         return false;
      }
      std::memcpy(hooked->hooked_prologue.data(), target, hooked->hooked_prologue.size());
      hooked->target = target;
      return true;
   }

   // Only Luma's hook (see "includes/minhook.h"). False if another module hooked over it: Luma's then stays, as the other hook's
   // trampoline jumps to our detour, and the addon is pinned like "Reflex::SleepHook::Remove()" does (too late if "DllMain" is unloading it)
   template <typename Function>
   bool Detach(HookedFunction<Function>* hooked)
   {
      if (!hooked->target)
      {
         return true;
      }
      if (std::memcmp(hooked->target, hooked->hooked_prologue.data(), hooked->hooked_prologue.size()) != 0)
      {
         HMODULE module = nullptr;
         GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&FindProxyVtable), &module);
         return false;
      }
      MH_RemoveHook(hooked->target);
      hooked->target = nullptr;
      return true;
   }
} // namespace D3D12ProxyHook

namespace D3D12ResourceCreation
{
   enum class Kind : uint8_t
   {
      Unknown,   // Not created by a hooked call: a shared resource the game opened, or the hooks aren't in
      Committed, // Owns its memory
      Placed,    // In a heap of the game's
      Reserved,  // Tiled, without memory until the game maps tiles to it
   };
   constexpr std::array<const char*, 4> KIND_NAMES = {"unknown", "committed", "placed", "reserved"};

   // The slots of the creates in the vtable of "ID3D12Device10", which ReShade's proxy has implemented since 5.2 (the interfaces after it
   // only append methods). Checked against d3d12.h's C vtables by an offline test.
   constexpr size_t CREATE_COMMITTED_RESOURCE_SLOT = 27;
   constexpr size_t CREATE_PLACED_RESOURCE_SLOT = 29;
   constexpr size_t CREATE_COMMITTED_RESOURCE1_SLOT = 53;
   constexpr size_t CREATE_COMMITTED_RESOURCE2_SLOT = 69;
   constexpr size_t CREATE_PLACED_RESOURCE1_SLOT = 70;
   constexpr size_t CREATE_COMMITTED_RESOURCE3_SLOT = 76;
   constexpr size_t CREATE_PLACED_RESOURCE2_SLOT = 77;

   // The kind of the resource the game call this thread is in creates, unknown outside of one
   inline thread_local Kind current_kind = Kind::Unknown;
   // The number of castable formats that call passes, 0 for the ones that can't (or outside of a call)
   inline thread_local UINT current_castable_format_count = 0;

   // For the creates that have no castable formats
   constexpr size_t NO_CASTABLE_FORMATS = (std::numeric_limits<size_t>::max)();

   // A function of the proxy that creates resources of a "kind", hooked once per process. "Method" is its declaration in d3d12.h, so the
   // detour has the interface's own signature. "castable_count_argument" is the index of its "NumCastableFormats" argument, if it has one.
   template <typename Method, size_t vtable_slot, Kind kind, size_t castable_count_argument = NO_CASTABLE_FORMATS>
   struct Creation;

   template <typename Interface, typename... Arguments, size_t vtable_slot, Kind kind, size_t castable_count_argument>
   struct Creation<HRESULT (STDMETHODCALLTYPE Interface::*)(Arguments...), vtable_slot, kind, castable_count_argument>
   {
      using Function = HRESULT(STDMETHODCALLTYPE*)(Interface*, Arguments...);
      static inline D3D12ProxyHook::HookedFunction<Function> hooked;

      static HRESULT STDMETHODCALLTYPE Detour(Interface* device, Arguments... arguments)
      {
         UINT castable_format_count = 0;
         if constexpr (castable_count_argument != NO_CASTABLE_FORMATS)
         {
            static_assert(std::is_same_v<std::tuple_element_t<castable_count_argument, std::tuple<Arguments...>>, UINT32>, "The argument isn't a count");
            castable_format_count = std::get<castable_count_argument>(std::tie(arguments...));
         }
         const Kind outer_kind = std::exchange(current_kind, kind);
         const UINT outer_castable_format_count = std::exchange(current_castable_format_count, castable_format_count);
         const HRESULT result = hooked.original(device, arguments...);
         current_castable_format_count = outer_castable_format_count;
         current_kind = outer_kind;
         return result;
      }

      // True if the function is hooked, by now or for an earlier device. A later device's proxy can lead elsewhere when another module
      // patched the proxies' vtable after the first install, to a function that calls ReShade's: the hook on that covers it.
      static bool Install(void** proxy_vtable)
      {
         return hooked.target || D3D12ProxyHook::Attach(&hooked, proxy_vtable[vtable_slot], &Detour);
      }

      static bool Remove()
      {
         return D3D12ProxyHook::Detach(&hooked);
      }
   };

   using CommittedResource = Creation<decltype(&ID3D12Device::CreateCommittedResource), CREATE_COMMITTED_RESOURCE_SLOT, Kind::Committed>;
   using CommittedResource1 = Creation<decltype(&ID3D12Device4::CreateCommittedResource1), CREATE_COMMITTED_RESOURCE1_SLOT, Kind::Committed>;
   using CommittedResource2 = Creation<decltype(&ID3D12Device8::CreateCommittedResource2), CREATE_COMMITTED_RESOURCE2_SLOT, Kind::Committed>;
   using CommittedResource3 = Creation<decltype(&ID3D12Device10::CreateCommittedResource3), CREATE_COMMITTED_RESOURCE3_SLOT, Kind::Committed, 6>;
   using PlacedResource = Creation<decltype(&ID3D12Device::CreatePlacedResource), CREATE_PLACED_RESOURCE_SLOT, Kind::Placed>;
   using PlacedResource1 = Creation<decltype(&ID3D12Device8::CreatePlacedResource1), CREATE_PLACED_RESOURCE1_SLOT, Kind::Placed>;
   using PlacedResource2 = Creation<decltype(&ID3D12Device10::CreatePlacedResource2), CREATE_PLACED_RESOURCE2_SLOT, Kind::Placed, 5>;

   struct Hook
   {
      bool (*install)(void** proxy_vtable);
      bool (*remove)();
   };
   template <typename Create>
   constexpr Hook HOOK_OF = {.install = &Create::Install, .remove = &Create::Remove};
   // Every function of the proxy that the game creates committed and placed resources with
   constexpr std::array HOOKS = {HOOK_OF<CommittedResource>, HOOK_OF<CommittedResource1>, HOOK_OF<CommittedResource2>, HOOK_OF<CommittedResource3>, HOOK_OF<PlacedResource>, HOOK_OF<PlacedResource1>, HOOK_OF<PlacedResource2>};

   // For "Install()" and "Remove()"
   inline std::shared_mutex hooks_mutex;

   // True if the game's creates are hooked, by now or for an earlier device. False if there is no ReShade device proxy, or MinHook failed
   // (the creates that did get hooked stay and tag as usual).
   inline bool Install(ID3D12Device* native_device)
   {
      void** const proxy_vtable = D3D12ProxyHook::FindProxyVtable(native_device);
      if (!proxy_vtable)
      {
         return false;
      }
      const std::unique_lock lock(hooks_mutex);
      bool installed_all = true;
      for (const Hook& hook : HOOKS)
      {
         installed_all &= hook.install(proxy_vtable); // Each is tried even if another failed
      }
      return installed_all;
   }

   // See "D3D12ProxyHook::Detach()"
   inline bool Remove()
   {
      const std::unique_lock lock(hooks_mutex);
      bool removed_all = true;
      for (const Hook& hook : HOOKS)
      {
         removed_all &= hook.remove();
      }
      return removed_all;
   }

   // The kind of the resource a "create_resource" or "init_resource" event is for. ReShade fires them inside the game's call, and flags the
   // reserved resources itself (the heap of a reserved one is unknown).
   inline Kind KindOf(const reshade::api::resource_desc& desc)
   {
      return ((desc.flags & reshade::api::resource_flags::sparse_binding) != 0 ? Kind::Reserved : current_kind);
   }

   // The kind of each live texture of a device, by its "ID3D12Resource" pointer (a "reshade::api::resource" handle), for the dev report
   struct TextureKinds
   {
      std::shared_mutex mutex;
      std::unordered_map<uint64_t, Kind> by_resource;
   };
} // namespace D3D12ResourceCreation

#endif // DX12 && ENABLE_MINHOOK
