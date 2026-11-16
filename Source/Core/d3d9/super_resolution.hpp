#pragma once

// SR bridge client of a native D3D9 (x86) add-on: runs "Luma-Upscaler.exe" (SRBridgeProtocol.h) as a child process on textures the
// D3D9 device shares by KMT handle, synchronized by two KMT fences on the D3D9 queue (kmt_sync.h). The upscaler runs mid frame: the
// caller copies its color into the shared texture, "RunFrame()" signals "in", writes the frame line and makes the D3D9 queue wait for
// "out", then the caller copies the output back.
//
// Threading: one thread (the game's), plus a watchdog per started process that only signals "out" once that process exits.
// ponytail: a hung (alive, stalled) upscaler is not killed, only an exited one releases the D3D9 GPU wait.

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../sr_bridge/SRBridgeProtocol.h"
#include "kmt_sync.h"

namespace D3D9SR
{
   struct Bridge
   {
      HANDLE process = nullptr, stdin_write = nullptr, watchdog = nullptr;
      KmtSync sync; // Own selection of the D3D9 queue, independent of the presenter's
      KmtFence in, out;
      uint64_t frame = SRBridgeProtocol::kReady;
      bool ready = false;
      DWORD launch_error = 0; // GetLastError() of a failed CreateProcess in "Start()"
      bool gave_up = false;   // Kept by "Stop()": one start per set of shared textures
   };

   // The start line's settings (SRBridgeProtocol.h)
   struct Settings
   {
      uint32_t render_width = 0, render_height = 0, output_width = 0, output_height = 0;
      bool hdr = false, inverted_depth = false, jittered_motion = false, auto_exposure = false, dynamic_resolution = false;
      float motion_scale_x = 0.f, motion_scale_y = 0.f;
      int dlss_preset = 0;
      int fsr_version = 0; // 0: the newest
   };

   // Selects the D3D9 queue of the device "kmt_generation" (KmtGeneration()) and creates the shared "in" / "out" fences on it.
   inline bool Open(Bridge* bridge, int kmt_generation)
   {
      return KmtSelect(&bridge->sync, kmt_generation) && KmtCreateSharedFence(&bridge->sync, &bridge->in) &&
             KmtCreateSharedFence(&bridge->sync, &bridge->out);
   }

   // After "Open()": launches the helper from "directory" (ending in a separator; its stdout and stderr are appended to the log there),
   // hands it the fences and sends the start line. "out" reaches kReady once the upscaler exists (the caller polls KmtCompleted()).
   // ponytail: bInheritHandles also hands the game's other inheritable handles to the helper; PROC_THREAD_ATTRIBUTE_HANDLE_LIST if that
   // ever matters. No reply mapping is passed.
   inline bool Start(Bridge* bridge, const std::wstring& directory, int upscaler, LUID luid, const Settings& settings,
      const std::array<HANDLE, SRBridgeProtocol::kCount>& textures)
   {
      SECURITY_ATTRIBUTES inherit = {.nLength = sizeof(inherit), .bInheritHandle = TRUE};
      HANDLE stdin_read = nullptr;
      const HANDLE log = CreateFileW((directory + SRBridgeProtocol::kLogName).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
         &inherit, OPEN_ALWAYS, 0, nullptr);
      bool launched = log != INVALID_HANDLE_VALUE && CreatePipe(&stdin_read, &bridge->stdin_write, &inherit, 0);
      PROCESS_INFORMATION process_info = {};
      if (launched)
      {
         SetHandleInformation(bridge->stdin_write, HANDLE_FLAG_INHERIT, 0);
         STARTUPINFOW startup_info = {.cb = sizeof(startup_info), .dwFlags = STARTF_USESTDHANDLES, .hStdInput = stdin_read, .hStdOutput = log, .hStdError = log};
         const std::wstring exe = directory + SRBridgeProtocol::kHelperName;
         const std::wstring exe_directory = exe.substr(0, exe.find_last_of(L'\\'));
         launched = CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, exe_directory.c_str(), &startup_info,
            &process_info);
         if (launched)
         {
            CloseHandle(process_info.hThread);
         }
         else
         {
            bridge->launch_error = GetLastError();
         }
      }
      if (stdin_read)
      {
         CloseHandle(stdin_read);
      }
      if (log != INVALID_HANDLE_VALUE)
      {
         CloseHandle(log);
      }
      bridge->process = launched ? process_info.hProcess : nullptr;

      HANDLE remote_in = nullptr, remote_out = nullptr;
      bool ok = bridge->process &&
                DuplicateHandle(GetCurrentProcess(), bridge->in.nt, bridge->process, &remote_in, 0, FALSE, DUPLICATE_SAME_ACCESS) &&
                DuplicateHandle(GetCurrentProcess(), bridge->out.nt, bridge->process, &remote_out, 0, FALSE, DUPLICATE_SAME_ACCESS);
      const auto number = [](HANDLE handle)
      { return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(handle)); };
      char line[256];
      snprintf(line, sizeof(line), "bridge %d %d %lu %ld %u %u %u %u %d %d %d %d %d %g %g %d %d %llu %llu %llu %llu %llu %llu %llu %llu %llu 0\n",
         SRBridgeProtocol::kVersion, upscaler, luid.LowPart, luid.HighPart, settings.render_width, settings.render_height,
         settings.output_width, settings.output_height, int(settings.hdr), int(settings.inverted_depth), int(settings.jittered_motion),
         int(settings.auto_exposure), int(settings.dynamic_resolution), settings.motion_scale_x, settings.motion_scale_y,
         settings.dlss_preset, settings.fsr_version, number(textures[SRBridgeProtocol::kColor]), number(textures[SRBridgeProtocol::kMotion]),
         number(textures[SRBridgeProtocol::kDepth]), number(textures[SRBridgeProtocol::kReactive]),
         number(textures[SRBridgeProtocol::kTransparency]), number(textures[SRBridgeProtocol::kExposure]),
         number(textures[SRBridgeProtocol::kOutput]), number(remote_in), number(remote_out));
      DWORD written = 0;
      ok = ok && WriteFile(bridge->stdin_write, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
      // The helper signals "out" UINT64_MAX on any exit after kReady, but not if it dies before: no D3D9 wait may outlive it
      bridge->watchdog = ok ? CreateThread(
                                 nullptr, 0,
                                 [](void* parameter) -> DWORD
                                 {
                                    auto* watched = static_cast<Bridge*>(parameter);
                                    WaitForSingleObject(watched->process, INFINITE);
                                    KmtCpuSignal(&watched->sync, watched->out, UINT64_MAX);
                                    return 0;
                                 },
                                 bridge, 0, nullptr)
                            : nullptr;
      return bridge->watchdog != nullptr;
   }

   // EOF ends the upscaler (it signals "out" UINT64_MAX on exit, so no D3D9 wait is left). The shared textures are the caller's.
   inline void Stop(Bridge* bridge)
   {
      if (bridge->stdin_write)
      {
         CloseHandle(bridge->stdin_write);
      }
      if (bridge->process)
      {
         if (WaitForSingleObject(bridge->process, 2000) != WAIT_OBJECT_0)
         {
            TerminateProcess(bridge->process, 2);
         }
         CloseHandle(bridge->process);
      }
      if (bridge->watchdog)
      {
         WaitForSingleObject(bridge->watchdog, 3000);
         CloseHandle(bridge->watchdog);
      }
      KmtDestroyFence(&bridge->in);
      KmtDestroyFence(&bridge->out);
      KmtRelease(&bridge->sync);
      *bridge = {.gave_up = bridge->gave_up};
   }

   // Once ready, after the D3D9 copy of the color into the shared texture was flushed: numbers "frame" (resetting the first one after
   // kReady too), signals "in", sends the frame line and queues the D3D9 wait for "out". False: the bridge is broken ("sync"'s first
   // error), "Stop()" it.
   inline bool RunFrame(Bridge* bridge, SRBridgeProtocol::Frame* frame)
   {
      frame->n = ++bridge->frame;
      frame->reset = (frame->n == SRBridgeProtocol::kReady + 1 || frame->reset) ? 1 : 0;
      char line[200];
      snprintf(line, sizeof(line), "frame %llu %g %g %d %u %u %g %g %g %g %g\n", static_cast<unsigned long long>(frame->n), frame->jitter_x,
         frame->jitter_y, frame->reset, frame->render_width, frame->render_height, frame->pre_exposure, frame->sharpness,
         frame->near_plane, frame->far_plane, frame->vert_fov);
      DWORD written = 0;
      return KmtSignalObject(&bridge->sync, bridge->in.object, frame->n) &&
             WriteFile(bridge->stdin_write, line, static_cast<DWORD>(strlen(line)), &written, nullptr) &&
             KmtWaitObject(&bridge->sync, bridge->out.object, frame->n);
   }
} // namespace D3D9SR
