#pragma once

// Proxy back buffer for scRGB games, which present HDR10 PQ: the game, ReShade and Luma's display composition keep seeing
// an fp16 scRGB back buffer (a plain texture), while the real DXGI buffer is R10G10B10A2_UNORM, written once per frame by
// the swapchain encode pass (see "OnReShadePresent()") right before "Present()".
//
// MinHook on the native swapchain (not ReShade's wrapper, which calls it too) makes every caller of "GetBuffer()" get the
// proxy, and "GetDesc()"/"GetDesc1()" report fp16. After the encode pass, "real_buffer_visible" gives the real buffer and
// format back to the rest of the present call (overlays hooking "Present()" read the real buffer), until its "finish_present".
#if ENABLE_SWAPCHAIN_PROXY
#include <d3d11.h>
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
   struct Entry
   {
      // Created by the first "GetBuffer()" after the swapchain (re)initialized, then lives until "Unregister()"
      com_ptr<ID3D11Texture2D> proxy;
      com_ptr<ID3D11ShaderResourceView> proxy_srv;

      // Only used by the present thread, which ReShade serializes with the swapchain init/destroy events
      com_ptr<ID3D11Texture2D> real_buffer;
      com_ptr<ID3D11RenderTargetView> real_rtv;
      // The display state is stale once it stops being current
      com_ptr<IDXGIFactory1> factory;
      // Whether the window's display is in HDR (PQ output), otherwise it's SDR (sRGB output)
      bool encode_pq = false;
      // Without one, ReShade skips "reshade_present", so the encode pass runs at the end of Luma's "present" instead
      bool has_effect_runtime = false;
   };

   using GetBufferFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, REFIID, void**);
   using GetDescFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
   using GetDesc1Function = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, DXGI_SWAP_CHAIN_DESC1*);

   // "IDXGISwapChain" vtable slots (IUnknown 0-2, IDXGIObject 3-6, IDXGIDeviceSubObject 7, then these), "IDXGISwapChain1" continues it
   constexpr size_t get_buffer_vtable_index = 9;
   constexpr size_t get_desc_vtable_index = 12;
   constexpr size_t get_desc1_vtable_index = 18;

   inline std::shared_mutex mutex;
   // The entries are boxed so their address stays valid while the map rehashes
   inline std::unordered_map<IDXGISwapChain*, std::unique_ptr<Entry>> swapchains;

   inline GetBufferFunction original_get_buffer = nullptr;
   inline GetDescFunction original_get_desc = nullptr;
   inline GetDesc1Function original_get_desc1 = nullptr;
   inline std::vector<void*> hooked_targets;

   // Set on the present thread after the encode pass, cleared by "finish_present" (and the next "present", in case it's skipped)
   inline thread_local bool real_buffer_visible = false;

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
   }

   // Empty if the swapchain isn't proxied (or the proxy can't be created), the caller then returns the real buffer
   inline std::optional<HRESULT> QueryProxy(IDXGISwapChain* swapchain, REFIID riid, void** surface)
   {
      {
         const std::shared_lock lock(mutex);
         const auto it = swapchains.find(swapchain);
         if (it == swapchains.end())
            return std::nullopt;
         if (it->second->proxy)
            return it->second->proxy->QueryInterface(riid, surface);
      }

      const std::unique_lock lock(mutex);
      const auto it = swapchains.find(swapchain);
      if (it == swapchains.end())
         return std::nullopt;
      if (!it->second->proxy)
      {
         CreateProxy(swapchain, it->second.get());
      }
      if (!it->second->proxy)
         return std::nullopt;
      return it->second->proxy->QueryInterface(riid, surface);
   }

   inline HRESULT STDMETHODCALLTYPE GetBufferDetour(IDXGISwapChain* swapchain, UINT buffer, REFIID riid, void** surface)
   {
      if (buffer == 0 && !real_buffer_visible)
      {
         if (const std::optional<HRESULT> hr = QueryProxy(swapchain, riid, surface))
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

   // The real buffer, whether or not "real_buffer_visible" is set
   inline com_ptr<ID3D11Texture2D> GetRealBackBuffer(IDXGISwapChain* swapchain)
   {
      com_ptr<ID3D11Texture2D> real_buffer;
      original_get_buffer(swapchain, 0, IID_PPV_ARGS(&real_buffer));
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
         const bool hooked =
            hook(vtable[get_buffer_vtable_index], reinterpret_cast<void*>(&GetBufferDetour), reinterpret_cast<void**>(&original_get_buffer)) &&
            hook(vtable[get_desc_vtable_index], reinterpret_cast<void*>(&GetDescDetour), reinterpret_cast<void**>(&original_get_desc)) &&
            hook(vtable[get_desc1_vtable_index], reinterpret_cast<void*>(&GetDesc1Detour), reinterpret_cast<void**>(&original_get_desc1));
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
