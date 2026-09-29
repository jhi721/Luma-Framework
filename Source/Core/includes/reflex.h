#pragma once

// NVIDIA Reflex injected at present, for games without their own: "NvAPI_D3D_Sleep()" right after the game's present returns
// (where most engines start simulating the next frame) keeps the render queue empty when GPU bound.
// The markers are only an approximation of the game's frame (simulation and render submission span the whole CPU frame),
// so they aren't used for driver optimizations ("bUseMarkersToOptimize").
#if ENABLE_REFLEX
#include "nvapi.h"

#include <psapi.h>

namespace Reflex
{
   enum class Mode : int
   {
      Off,
      On,
      Boost, // Also keeps GPU clocks at their maximum
   };

   enum class State
   {
      Inactive,
      Running,
      Unsupported,      // Not an NVIDIA GPU or driver
      Game,             // The game (or another tool) already runs Reflex
      DisplayCommander, // Display Commander handles Reflex (it can inject it too)
   };

   // The driver reports another caller's sleep ("bUseGameSleep") only a few presents after it started (3 in a test), so Luma
   // watches this many before taking over. A game that turns its own Reflex on later isn't detected.
   constexpr uint32_t detection_presents = 60;

   struct DeviceData
   {
      State state = State::Inactive;
      Mode applied_mode = Mode::Off;
      uint64_t frame_id = 0;
      uint32_t presents = 0; // Watched for another Reflex owner, up to "detection_presents"
   };

   bool IsDisplayCommanderLoaded()
   {
      HMODULE modules[1024];
      DWORD size = 0;
      if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &size))
         return false;
      for (DWORD i = 0; i < (std::min)(size, DWORD(sizeof(modules))) / sizeof(HMODULE); i++)
      {
         wchar_t name[MAX_PATH];
         if (GetModuleBaseNameW(GetCurrentProcess(), modules[i], name, MAX_PATH) && wcsstr(_wcslwr(name), L"display_commander"))
            return true;
      }
      return false;
   }

   void SetMarker(IUnknown* device, uint64_t frame_id, NV_LATENCY_MARKER_TYPE type)
   {
      NV_LATENCY_MARKER_PARAMS params = {NV_LATENCY_MARKER_PARAMS_VER};
      params.frameID = frame_id;
      params.markerType = type;
      NvAPI_D3D_SetLatencyMarker(device, &params);
   }

   // Before the game's present
   void OnPresent(IUnknown* device, const DeviceData& data)
   {
      if (data.state != State::Running)
         return;
      SetMarker(device, data.frame_id, SIMULATION_END);
      SetMarker(device, data.frame_id, RENDERSUBMIT_END);
      SetMarker(device, data.frame_id, PRESENT_START);
   }

   // After the game's present returned: the frame ends, and the next one starts after the sleep
   void OnFinishPresent(IUnknown* device, DeviceData& data, Mode mode)
   {
      if (data.state == State::Unsupported || data.state == State::Game || data.state == State::DisplayCommander)
         return;
      if (data.presents < detection_presents)
      {
         NV_GET_SLEEP_STATUS_PARAMS status = {NV_GET_SLEEP_STATUS_PARAMS_VER};
         if (NvAPI_D3D_GetSleepStatus(device, &status) != NVAPI_OK) // Also when NVAPI didn't initialize
            data.state = State::Unsupported;
         else if (data.presents == 0 && IsDisplayCommanderLoaded())
            data.state = State::DisplayCommander;
         else if (status.bLowLatencyMode || status.bUseGameSleep)
            data.state = State::Game;
         data.presents++;
         return;
      }

      if (data.applied_mode != mode)
      {
         NV_SET_SLEEP_MODE_PARAMS params = {NV_SET_SLEEP_MODE_PARAMS_VER};
         params.bLowLatencyMode = mode != Mode::Off;
         params.bLowLatencyBoost = mode == Mode::Boost;
         if (NvAPI_D3D_SetSleepMode(device, &params) != NVAPI_OK)
         {
            data.state = State::Unsupported;
            return;
         }
         data.applied_mode = mode;
      }
      if (mode == Mode::Off)
      {
         data.state = State::Inactive;
         return;
      }

      if (data.state == State::Running)
         SetMarker(device, data.frame_id, PRESENT_END);
      data.state = State::Running;
      NvAPI_D3D_Sleep(device);
      data.frame_id++;
      SetMarker(device, data.frame_id, SIMULATION_START);
      SetMarker(device, data.frame_id, RENDERSUBMIT_START);
   }
} // namespace Reflex
#endif // ENABLE_REFLEX
