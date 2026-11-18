#pragma once

// NVAPI for the native D3D9 module: Core's Reflex ("includes/reflex.h") and HDR10+ GAMING ("utils/display.hpp") on the presenter's
// D3D12 device and window. D3D9 has no Reflex, so the driver's sleep and markers go to the device that presents (presenter.hpp), whose
// frames are the game's: the presenter publishes it here, the game's thread runs Core's per-present Reflex on it around its D3D9
// present, as Core does around the game's DXGI present (its markers are the same approximation of the game's frame).
// NVAPI is linked statically on Win32 too (Luma.Features.props "UseLumaNVAPI", External/NVAPI/x86/nvapi.lib).

// As core.hpp: Reflex needs NVAPI and MinHook (its sleep hook), and reads "DEVELOPMENT" in expressions.
#ifndef DEVELOPMENT
#define DEVELOPMENT 0
#endif
#ifndef ENABLE_NVAPI
#define ENABLE_NVAPI 0
#endif
#ifndef ENABLE_MINHOOK
#define ENABLE_MINHOOK 0
#endif
#if !ENABLE_NVAPI || !ENABLE_MINHOOK
#undef ENABLE_REFLEX
#define ENABLE_REFLEX 0
#elif !defined(ENABLE_REFLEX)
#define ENABLE_REFLEX 1
#endif

#if ENABLE_NVAPI
#include <algorithm> // std::ranges::any_of (reflex.h)
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>

#include <windows.h>
#include <d3d12.h>

#include <source/com_ptr.hpp> // display.hpp
#include "../utils/display.hpp"
#include "../includes/reflex.h"

namespace D3D9NvApi
{
   // HDR10+ GAMING as Core ("SetDisplayOutputMode(..., false, false)": never toggles the display's HDR itself, falls back to HDR10
   // when the display has no HDR10+). Off only sets HDR10 back, Core has no off path.
   inline bool SetHdr10Plus(HWND window, bool enable)
   {
      return Display::NVAPI::Init() && Display::NVAPI::SetDisplayOutputMode(window, enable ? NV_DISPLAY_OUTPUT_MODE_HDR10PLUS_GAMING : NV_DISPLAY_OUTPUT_MODE_HDR10, false, false);
   }

#if ENABLE_REFLEX
   // The presenter's device, null while none runs. The presenter clears it (exclusive) before releasing the device; the game's
   // thread holds the shared lock through its NVAPI calls on it.
   struct ReflexTarget
   {
      std::shared_mutex mutex;
      ID3D12Device* device = nullptr;
      uint32_t generation = 0; // Per publish: a new device can reuse the old one's address
   };
   inline ReflexTarget reflex_target;

   inline void PublishReflexDevice(ID3D12Device* device)
   {
      const std::unique_lock lock(reflex_target.mutex);
      reflex_target.device = device;
      reflex_target.generation++;
   }

   // Game thread only. Each presenter (re)start makes a new device, whose Reflex state starts over.
   struct ReflexGameState
   {
      uint32_t generation = 0;
      std::optional<Reflex::DeviceData> data;
   };
   inline ReflexGameState reflex_game;

   // Reflex::OnDestroyDevice() only releases Luma's hooks and threads, not the (already released) old device
   inline void DestroyReflexData()
   {
      if (reflex_game.data)
      {
         Reflex::OnDestroyDevice(&*reflex_game.data);
         reflex_game.data.reset();
      }
   }

   // Under the shared lock: the published device's Reflex state, null when no presenter runs or "OnGamePresent()" hasn't made the
   // new one's yet
   inline Reflex::DeviceData* ReflexData()
   {
      return (reflex_game.data && reflex_game.generation == reflex_target.generation) ? &*reflex_game.data : nullptr;
   }

   // Game thread, right before the game's D3D9 present (before the frame line): SIMULATION_END, RENDERSUBMIT_END, PRESENT_START.
   // A newly published device starts its Reflex state over here.
   inline void OnGamePresent()
   {
      const std::shared_lock lock(reflex_target.mutex);
      if (reflex_game.generation != reflex_target.generation)
      {
         DestroyReflexData();
         reflex_game.generation = reflex_target.generation;
         if (reflex_target.device)
         {
            reflex_game.data.emplace();
         }
      }
      if (Reflex::DeviceData* const data = ReflexData())
      {
         Reflex::OnPresent(reflex_target.device, *data);
      }
   }

   // Game thread, after the game's D3D9 present returned: PRESENT_END, the sleep mode, the sleep, the next frame's start markers.
   // Called with every mode (Off too), as Core.
   inline void OnGameFinishPresent(HWND window, Reflex::Mode mode)
   {
      const std::shared_lock lock(reflex_target.mutex);
      if (Reflex::DeviceData* const data = ReflexData())
      {
         Reflex::OnFinishPresent(reflex_target.device, window, data, mode);
      }
   }

   // Game thread, at detach: Reflex's ping thread, message hook and sleep hook run the add-on's code
   inline void ShutdownReflex()
   {
      DestroyReflexData();
   }

   // DEV panel / MCP
   inline const Reflex::DeviceData* GetReflexData()
   {
      return reflex_game.data ? &*reflex_game.data : nullptr;
   }
#endif // ENABLE_REFLEX
} // namespace D3D9NvApi
#endif // ENABLE_NVAPI
