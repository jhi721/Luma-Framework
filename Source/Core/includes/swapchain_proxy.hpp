#pragma once

// Proxy back buffer for scRGB games, which present HDR10 PQ: the game, ReShade and Luma's display composition keep seeing
// an fp16 scRGB back buffer (a plain texture), while the real DXGI buffer is R10G10B10A2_UNORM, written once per frame by
// the swapchain encode pass right before "Present()".
//
// MinHook on the native swapchain (not ReShade's wrapper, which calls it too) makes every caller of "GetBuffer()" get the
// proxy, and "GetDesc()"/"GetDesc1()" report fp16. After the encode pass, "real_buffer_visible" gives the real buffer and
// format back to the rest of the present call (overlays hooking "Present()" read the real buffer), until its "finish_present".
//
// D3D11 encodes at "reshade_present" (see "OnReShadePresent()"). D3D12 has one proxy per back buffer and encodes in a detour of
// the native "Present()"/"Present1()": ReShade's "reshade_present" fires there before it flushes the effects and the GUI it
// recorded, which ReShade's wrapper does before calling the native present.
#if ENABLE_SWAPCHAIN_PROXY
#if DX11
#include <d3d11.h>
#else
#include <d3d12.h>
#include <include/reshade.hpp>
#endif
#include <dxgi1_6.h>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include <source/com_ptr.hpp>

#include "minhook.h"

#pragma comment(lib, "dxgi.lib") // For "CreateDXGIFactory1()" (the display state tracking in Core)

namespace SwapchainProxy
{
#if DX11
   using Texture = ID3D11Texture2D;
#else
   using Texture = ID3D12Resource;
#endif

   struct Entry
   {
#if DX11
      // Created by the first "GetBuffer()" after the swapchain (re)initialized, then lives until "Unregister()"
      com_ptr<ID3D11Texture2D> proxy;
      com_ptr<ID3D11ShaderResourceView> proxy_srv;

      // Only used by the present thread, which ReShade serializes with the swapchain init/destroy events
      com_ptr<ID3D11Texture2D> real_buffer;
      com_ptr<ID3D11RenderTargetView> real_rtv;
#else
      // One per back buffer (committed, in the PRESENT state), created by the first "GetBuffer()" after the swapchain (re)initialized,
      // then lives until "Unregister()". The real buffers aren't kept: "ResizeBuffers()" fails while anything references them.
      std::vector<com_ptr<ID3D12Resource>> proxies;
      bool proxies_failed = false; // Logged once, not tried again until the swapchain (re)initializes
      // The queue of the swapchain's last "present" event, which the encode pass runs on (only used by the present thread)
      reshade::api::command_queue* queue = nullptr;
#endif
      // The display state is stale once it stops being current
      com_ptr<IDXGIFactory1> factory;
      // Whether the window's display is in HDR (PQ output), otherwise it's SDR (sRGB output)
      bool encode_pq = false;
#if DX11
      // Without one, ReShade skips "reshade_present", so the encode pass runs at the end of Luma's "present" instead
      bool has_effect_runtime = false;
#endif
   };

   using GetBufferFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, REFIID, void**);
   using GetDescFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
   using GetDesc1Function = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, DXGI_SWAP_CHAIN_DESC1*);
   using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
   using Present1Function = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);

   // "IDXGISwapChain" vtable slots (IUnknown 0-2, IDXGIObject 3-6, IDXGIDeviceSubObject 7, then these), "IDXGISwapChain1" continues it
   constexpr size_t present_vtable_index = 8;
   constexpr size_t get_buffer_vtable_index = 9;
   constexpr size_t get_desc_vtable_index = 12;
   constexpr size_t get_desc1_vtable_index = 18;
   constexpr size_t present1_vtable_index = 22;

   inline std::shared_mutex mutex;
   // The entries are boxed so their address stays valid while the map rehashes
   inline std::unordered_map<IDXGISwapChain*, std::unique_ptr<Entry>> swapchains;

   inline GetBufferFunction original_get_buffer = nullptr;
   inline GetDescFunction original_get_desc = nullptr;
   inline GetDesc1Function original_get_desc1 = nullptr;
   inline PresentFunction original_present = nullptr;
   inline Present1Function original_present1 = nullptr;
   inline std::vector<void*> hooked_targets;

   // Set on the present thread after the encode pass, cleared by "finish_present" (and the next "present", in case it's skipped)
   inline thread_local bool real_buffer_visible = false;

#if DX12
   // Core's encode pass, called by the present detours for a proxied swapchain (the proxy of the current back buffer into the real one)
   inline void (*encode)(IDXGISwapChain* swapchain) = nullptr;
#endif

   // Valid until "Unregister()" of that swapchain
   inline Entry* Find(IDXGISwapChain* swapchain)
   {
      const std::shared_lock lock(mutex);
      const auto it = swapchains.find(swapchain);
      return it != swapchains.end() ? it->second.get() : nullptr;
   }

   // The caller holds the unique lock
   inline void CreateProxy(IDXGISwapChain* swapchain, Entry* entry)
   {
      DXGI_SWAP_CHAIN_DESC swapchain_desc;
#if DX11
      com_ptr<ID3D11Device> device;
      if (FAILED(original_get_desc(swapchain, &swapchain_desc)) || FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))))
         return;

      const D3D11_TEXTURE2D_DESC proxy_desc = {
         .Width = swapchain_desc.BufferDesc.Width,
         .Height = swapchain_desc.BufferDesc.Height,
         .MipLevels = 1,
         .ArraySize = 1,
         .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
         .SampleDesc = {.Count = 1},
         .Usage = D3D11_USAGE_DEFAULT,
         .BindFlags = UINT(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | ((swapchain_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS) != 0 ? D3D11_BIND_UNORDERED_ACCESS : 0)),
      };
      if (FAILED(device->CreateTexture2D(&proxy_desc, nullptr, &entry->proxy)) || FAILED(device->CreateShaderResourceView(entry->proxy.get(), nullptr, &entry->proxy_srv)))
      {
         entry->proxy = nullptr;
         entry->proxy_srv = nullptr;
      }
#else
      // The proxies are made on the native device, so ReShade's proxy and Luma's creation hooks never see them (as for the real back buffers,
      // which fire no "init_resource" either). A D3D12 swapchain is made on a queue, which its "GetDevice()" may hand out instead of the device.
      if (entry->proxies_failed)
         return;
      const auto fail = [entry](const char* reason)
      {
         entry->proxies_failed = true;
         reshade::log::message(reshade::log::level::error, (std::string("[Swapchain proxy] ") + reason + ", so the game draws into the 10 bit buffer and HDR looks wrong").c_str());
      };
      com_ptr<ID3D12Device> device;
      if (FAILED(original_get_desc(swapchain, &swapchain_desc)))
         return fail("The swapchain refused its desc");
      if (com_ptr<ID3D12CommandQueue> queue; FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))) && (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&queue))) || FAILED(queue->GetDevice(IID_PPV_ARGS(&device)))))
         return fail("The swapchain gave out neither its device nor its queue");

      const D3D12_HEAP_PROPERTIES default_heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
      const D3D12_RESOURCE_DESC proxy_desc = {
         .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
         .Width = swapchain_desc.BufferDesc.Width,
         .Height = swapchain_desc.BufferDesc.Height,
         .DepthOrArraySize = 1,
         .MipLevels = 1,
         .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
         .SampleDesc = {.Count = 1},
         .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | ((swapchain_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS) != 0 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE),
      };
      entry->proxies.resize(swapchain_desc.BufferCount);
      for (com_ptr<ID3D12Resource>& proxy : entry->proxies)
      {
         if (FAILED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &proxy_desc, D3D12_RESOURCE_STATE_PRESENT, nullptr, IID_PPV_ARGS(&proxy))))
         {
            entry->proxies.clear();
            return fail("The device refused the proxy back buffers");
         }
         proxy->SetName(L"Luma swapchain proxy");
      }
#endif
   }

   // Empty if the swapchain isn't proxied (or the proxy can't be created), the caller then returns the real buffer
   inline std::optional<HRESULT> QueryProxy(IDXGISwapChain* swapchain, UINT buffer, REFIID riid, void** surface)
   {
      const auto query = [&](Entry* entry) -> std::optional<HRESULT>
      {
#if DX11
         if (entry->proxy)
            return entry->proxy->QueryInterface(riid, surface);
#else
         if (buffer < entry->proxies.size())
            return entry->proxies[buffer]->QueryInterface(riid, surface);
#endif
         return std::nullopt;
      };
      {
         const std::shared_lock lock(mutex);
         const auto it = swapchains.find(swapchain);
         if (it == swapchains.end())
            return std::nullopt;
         if (const std::optional<HRESULT> hr = query(it->second.get()))
            return hr;
      }

      const std::unique_lock lock(mutex);
      const auto it = swapchains.find(swapchain);
      if (it == swapchains.end())
         return std::nullopt;
      if (const std::optional<HRESULT> hr = query(it->second.get()))
         return hr;
      CreateProxy(swapchain, it->second.get());
      return query(it->second.get());
   }

   inline HRESULT STDMETHODCALLTYPE GetBufferDetour(IDXGISwapChain* swapchain, UINT buffer, REFIID riid, void** surface)
   {
#if DX11
      constexpr UINT proxied_buffers = 1; // D3D11 swapchains only give out buffer 0
#else
      constexpr UINT proxied_buffers = DXGI_MAX_SWAP_CHAIN_BUFFERS;
#endif
      if (buffer < proxied_buffers && !real_buffer_visible)
      {
         if (const std::optional<HRESULT> hr = QueryProxy(swapchain, buffer, riid, surface))
            return *hr;
      }
      return original_get_buffer(swapchain, buffer, riid, surface);
   }

   inline HRESULT STDMETHODCALLTYPE GetDescDetour(IDXGISwapChain* swapchain, DXGI_SWAP_CHAIN_DESC* desc)
   {
      const HRESULT hr = original_get_desc(swapchain, desc);
      if (SUCCEEDED(hr) && desc != nullptr && !real_buffer_visible && Find(swapchain))
      {
         desc->BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
      return hr;
   }

   inline HRESULT STDMETHODCALLTYPE GetDesc1Detour(IDXGISwapChain1* swapchain, DXGI_SWAP_CHAIN_DESC1* desc)
   {
      const HRESULT hr = original_get_desc1(swapchain, desc);
      if (SUCCEEDED(hr) && desc != nullptr && !real_buffer_visible && Find(swapchain))
      {
         desc->Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
      return hr;
   }

#if DX12
   // A test present ("DXGI_PRESENT_TEST") shows nothing, so it isn't encoded
   inline void EncodeBeforePresent(IDXGISwapChain* swapchain, UINT flags)
   {
      if ((flags & DXGI_PRESENT_TEST) == 0 && encode && Find(swapchain))
      {
         encode(swapchain);
         real_buffer_visible = true;
      }
   }

   inline HRESULT STDMETHODCALLTYPE PresentDetour(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
   {
      EncodeBeforePresent(swapchain, flags);
      const HRESULT hr = original_present(swapchain, sync_interval, flags);
      real_buffer_visible = false;
      return hr;
   }

   inline HRESULT STDMETHODCALLTYPE Present1Detour(IDXGISwapChain1* swapchain, UINT sync_interval, UINT flags, const DXGI_PRESENT_PARAMETERS* parameters)
   {
      EncodeBeforePresent(swapchain, flags);
      const HRESULT hr = original_present1(swapchain, sync_interval, flags, parameters);
      real_buffer_visible = false;
      return hr;
   }
#endif

   // The real buffer, whether or not "real_buffer_visible" is set
   inline com_ptr<Texture> GetRealBackBuffer(IDXGISwapChain* swapchain, UINT buffer = 0)
   {
      com_ptr<Texture> real_buffer;
      original_get_buffer(swapchain, buffer, IID_PPV_ARGS(&real_buffer));
      return real_buffer;
   }

   // Called when the addon unloads (explicitly, not at process exit)
   inline void RemoveHooks()
   {
      for (void* const target : hooked_targets)
      {
         MH_RemoveHook(target);
      }
      hooked_targets.clear();
   }

   // The hooks patch the DXGI implementation shared by every swapchain, so they install once, from the first swapchain's vtable.
   // That's before ReShade's first "get_back_buffer()" (its effect runtime inits right after the "init_swapchain" event) and the game's.
   inline bool InstallHooks(IDXGISwapChain* swapchain)
   {
      static const bool installed = [swapchain]()
      {
         com_ptr<IDXGISwapChain1> swapchain1;
         if (!InitializeMinHook() || FAILED(swapchain->QueryInterface(&swapchain1)))
            return false;

         void** const vtable = *reinterpret_cast<void***>(swapchain);
         const auto hook = [](void* target, void* detour, void** original)
         {
            if (MH_CreateHook(target, detour, original) != MH_OK)
               return false;
            if (MH_EnableHook(target) != MH_OK)
            {
               MH_RemoveHook(target);
               return false;
            }
            hooked_targets.push_back(target);
            return true;
         };
         bool hooked =
            hook(vtable[get_buffer_vtable_index], reinterpret_cast<void*>(&GetBufferDetour), reinterpret_cast<void**>(&original_get_buffer)) &&
            hook(vtable[get_desc_vtable_index], reinterpret_cast<void*>(&GetDescDetour), reinterpret_cast<void**>(&original_get_desc)) &&
            hook(vtable[get_desc1_vtable_index], reinterpret_cast<void*>(&GetDesc1Detour), reinterpret_cast<void**>(&original_get_desc1));
#if DX12
         hooked = hooked &&
                  hook(vtable[present_vtable_index], reinterpret_cast<void*>(&PresentDetour), reinterpret_cast<void**>(&original_present)) &&
                  hook(vtable[present1_vtable_index], reinterpret_cast<void*>(&Present1Detour), reinterpret_cast<void**>(&original_present1));
#endif
         if (!hooked)
         {
            RemoveHooks();
         }
         return hooked;
      }();
      return installed;
   }

   // False if the hooks couldn't be installed, the swapchain then isn't proxied.
   // Call before anything gets the swapchain's back buffer, and "Unregister()" before it's resized or destroyed.
   inline bool Register(IDXGISwapChain* swapchain)
   {
      if (!InstallHooks(swapchain))
         return false;

      const std::unique_lock lock(mutex);
      swapchains.insert_or_assign(swapchain, std::make_unique<Entry>());
      return true;
   }

   // Releases the proxy and the views of the real buffer, which must not outlive "ResizeBuffers()"
   inline void Unregister(IDXGISwapChain* swapchain)
   {
      std::unique_ptr<Entry> entry;
      {
         const std::unique_lock lock(mutex);
         const auto it = swapchains.find(swapchain);
         if (it == swapchains.end())
            return;
         entry = std::move(it->second);
         swapchains.erase(it);
      }
   }
} // namespace SwapchainProxy
#endif // ENABLE_SWAPCHAIN_PROXY
