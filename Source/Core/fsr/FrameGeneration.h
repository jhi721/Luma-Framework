#pragma once

// FSR 3 frame generation (frame interpolation) on D3D11: docs/Frame-Generation-Best-Practices.md (route A).
// No vendor ships it for D3D11: the DX11 FidelityFX fork runs the optical flow and interpolation passes, and the presentation is
// Luma's: a D3D12 composition swapchain over the game's window shows the interpolated frame, then the real one half a real frame
// later, from a thread of its own ("Presenter"), while the game's own presents are dropped ("GamePresentHook").
// Per frame: the game calls "Prepare()" with its upscaler inputs, Core snapshots the back buffer before the first UI draw into it
// ("CaptureHudless()") and runs it through the display composition, then "Present()" at ReShade's present (after its effects and
// overlay) interpolates and hands both frames over.
#if ENABLE_FRAME_GENERATION
#include "FidelityFX/host/backends/dx11/ffx_dx11.h"
#include "FidelityFX/host/ffx_fsr3.h"

#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dcomp.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <format>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

#include "../includes/com_ptr.h"
#include "../includes/super_resolution.h"
#include "../utils/system.h"
#ifdef _WIN64
#include "../../External/reshade/deps/minhook/src/hde/hde64.c"
#endif

namespace FrameGeneration
{
   // The game's presents are dropped while the presenter shows its frames: slots 8 (Present) and 22 (Present1) of the swapchain
   // vtable, which ReShade calls after its overlay. The vtable is shared by every DXGI swapchain in the process (the presenter's
   // too): filtered by "this".
   struct GamePresentHook
   {
      static inline std::atomic<IDXGISwapChain*> game_swapchain = nullptr; // Dropped while set
      static inline void** vtable = nullptr;
      using PresentFunc = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
      using Present1Func = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
      static inline PresentFunc present = nullptr;
      static inline Present1Func present1 = nullptr;

      static HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
      {
         if (swapchain == game_swapchain.load() && (flags & DXGI_PRESENT_TEST) == 0)
            return S_OK;
         return present(swapchain, sync_interval, flags);
      }
      static HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* swapchain, UINT sync_interval, UINT flags, const DXGI_PRESENT_PARAMETERS* parameters)
      {
         if (swapchain == game_swapchain.load() && (flags & DXGI_PRESENT_TEST) == 0)
            return S_OK;
         return present1(swapchain, sync_interval, flags, parameters);
      }

      static void Install(IDXGISwapChain* swapchain)
      {
         void** const swapchain_vtable = *reinterpret_cast<void***>(swapchain);
         if (vtable == swapchain_vtable)
            return;
         Uninstall();
         vtable = swapchain_vtable;
         present = reinterpret_cast<PresentFunc>(vtable[8]);
         present1 = reinterpret_cast<Present1Func>(vtable[22]);
         void* const hooks[] = {reinterpret_cast<void*>(&Present), reinterpret_cast<void*>(&Present1)};
         System::PatchMemory(&vtable[8], &hooks[0], sizeof(void*), System::PatchMemoryType::Data);
         System::PatchMemory(&vtable[22], &hooks[1], sizeof(void*), System::PatchMemoryType::Data);
      }
      // Also at addon unload: the vtable must not point into the unloaded addon
      static void Uninstall()
      {
         if (!vtable)
            return;
         game_swapchain = nullptr;
         if (vtable[8] == reinterpret_cast<void*>(&Present))
            System::PatchMemory(&vtable[8], &present, sizeof(void*), System::PatchMemoryType::Data);
         if (vtable[22] == reinterpret_cast<void*>(&Present1))
            System::PatchMemory(&vtable[22], &present1, sizeof(void*), System::PatchMemoryType::Data);
         vtable = nullptr;
      }
   };

   // The presenter's presents must not reach other addons' Present hooks: they hook every DXGI swapchain's code and treat the
   // presenter as the game, as the game's own presents are dropped before them. Display Commander moved its FPS limiter and
   // Reflex onto the presenter's thread (FGL-6), the Steam overlay's hook hung the game. So "Present" runs through code built
   // from dxgi.dll's file: the vtable entry (Display Commander detours its adjustor thunk), followed through jumps into the
   // method's body, whose first instructions (the Steam overlay patches the first 5 bytes) are copied ahead of a jump back past them.
   // nullptr if the code can't be relocated (relative operands), or another hook patched past the copied instructions.
   // ponytail: built once against the hooks of that time, a hook patching deeper later isn't seen; x64 only.
   using PresentFunc = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
   static PresentFunc BuildUnhookedPresent(IDXGISwapChain* swapchain)
   {
#ifdef _WIN64
      void** const vtable = *reinterpret_cast<void***>(swapchain);
      HMODULE module = nullptr;
      wchar_t path[MAX_PATH];
      if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(vtable), &module) ||
          !GetModuleFileNameW(module, path, MAX_PATH))
         return nullptr;
      const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
      if (file == INVALID_HANDLE_VALUE)
         return nullptr;
      // An image view of the file (sections at their RVAs, not relocated)
      const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0, 0, nullptr);
      CloseHandle(file);
      if (!mapping)
         return nullptr;
      const auto* const image = static_cast<const uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
      CloseHandle(mapping);
      if (!image)
         return nullptr;
      const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
      auto* const base = reinterpret_cast<uint8_t*>(module);

      uint8_t code[64];
      size_t code_size = 0;
      size_t rva = *reinterpret_cast<const uint64_t*>(image + (reinterpret_cast<uint8_t*>(&vtable[8]) - base)) - nt->OptionalHeader.ImageBase;
      bool relocatable = rva < nt->OptionalHeader.SizeOfImage; // Not if another module replaced the vtable
      // 16 bytes of the body cover a 5 byte relative or a 14 byte absolute jump
      for (size_t copied_from_body = 0; copied_from_body < 16 && relocatable;)
      {
         hde64s instruction;
         if (rva + 16 > nt->OptionalHeader.SizeOfImage)
         {
            relocatable = false;
            break;
         }
         const uint32_t length = hde64_disasm(image + rva, &instruction);
         if (instruction.opcode == 0xE9 || instruction.opcode == 0xEB)
         {
            rva += length + (instruction.opcode == 0xE9 ? int32_t(instruction.imm.imm32) : int8_t(instruction.imm.imm8));
            copied_from_body = 0;
            continue;
         }
         const bool rip_relative = (instruction.flags & F_MODRM) && instruction.modrm_mod == 0 && instruction.modrm_rm == 5;
         relocatable = !(instruction.flags & (F_ERROR | F_RELATIVE)) && !rip_relative && code_size + length + 14 <= sizeof(code);
         if (relocatable)
         {
            memcpy(code + code_size, image + rva, length);
            code_size += length;
            rva += length;
            copied_from_body += length;
         }
      }
      const bool untouched_past_copy = relocatable && memcmp(base + rva, image + rva, 8) == 0;
      UnmapViewOfFile(image);
      if (!untouched_past_copy)
         return nullptr;

      const uint8_t* const resume = base + rva;
      const uint8_t jump[6] = {0xFF, 0x25, 0, 0, 0, 0}; // jmp [rip], the address follows
      memcpy(code + code_size, jump, sizeof(jump));
      memcpy(code + code_size + sizeof(jump), &resume, sizeof(resume));
      code_size += sizeof(jump) + sizeof(resume);
      void* const executable = VirtualAlloc(nullptr, code_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      DWORD protection;
      if (!executable)
         return nullptr;
      memcpy(executable, code, code_size);
      VirtualProtect(executable, code_size, PAGE_EXECUTE_READ, &protection);
      FlushInstructionCache(GetCurrentProcess(), executable, code_size);
      return reinterpret_cast<PresentFunc>(executable); // Kept for the process' lifetime, every presenter swapchain shares it
#else
      return nullptr;
#endif
   }

   // Shows the real and interpolated frames from its own thread: a D3D12 composition swapchain in a DirectComposition visual over
   // the game's window (topmost, above the game's own swapchain), so input goes to the game's window untouched. Frames cross from
   // D3D11 in NT shared textures, two slots, ordered by shared fences ("ready" signalled by D3D11, "consumed" by D3D12). The
   // composition and the swapchain belong to the presenter's thread; the rest of the D3D12 objects are created in "Start()".
   class Presenter
   {
   public:
      static constexpr uint32_t slot_count = 2;

      ~Presenter()
      {
         Stop();
      }

      bool Running() const
      {
         return thread.joinable();
      }
      uint32_t Width() const
      {
         return width;
      }
      uint32_t Height() const
      {
         return height;
      }
      IDXGISwapChain* GameSwapchain() const
      {
         return presented_game_swapchain;
      }
      bool Shown() const
      {
         return shown;
      }
      // Game thread: hides the visual and gives the presents back to the game, or the reverse, keeping the swapchain. A new
      // swapchain re-runs other hooks' setup: turning frame generation off and on (Stop + Start) made Display Commander and the
      // Steam overlay hook Present over each other into an endless loop that hung the game (Borderlands GOTY, 2026-10-02)
      void SetShown(bool show)
      {
         if (show == shown)
            return;
         shown = show;
         if (!show)
            GamePresentHook::game_swapchain = nullptr;
         visual_shown = show;
         SetEvent(frame_event);
         if (show)
            GamePresentHook::game_swapchain = presented_game_swapchain;
      }
      ID3D11Texture2D* Real(uint32_t slot) const
      {
         return slots[slot].real.get();
      }
      ID3D11Texture2D* Interpolated(uint32_t slot) const
      {
         return slots[slot].interpolated.get();
      }
      // Since the last call: real frames handed over, frames the presenter showed (both kinds)
      std::pair<uint32_t, uint32_t> TakeCounts()
      {
         return {submitted_count.exchange(0), presented_count.exchange(0)};
      }

      // Game thread, with the back buffer's size: false (nothing left behind, logged) if a step fails
      bool Start(ID3D11Device* device, IDXGISwapChain* game_swapchain, HWND window, uint32_t back_buffer_width, uint32_t back_buffer_height)
      {
         width = back_buffer_width;
         height = back_buffer_height;
         const auto fail = [&](const char* step, HRESULT hr)
         {
            reshade::log::message(reshade::log::level::warning, std::format("[Frame Generation] The presenter failed: {} (0x{:08X})", step, uint32_t(hr)).c_str());
            Release();
            return false;
         };
         com_ptr<ID3D11Device5> device5;
         HRESULT hr = device->QueryInterface(&device5);
         if (FAILED(hr))
            return fail("no D3D11 fences", hr);
         com_ptr<IDXGIDevice> dxgi_device;
         com_ptr<IDXGIAdapter> adapter;
         if (SUCCEEDED(hr = device->QueryInterface(&dxgi_device)))
            hr = dxgi_device->GetAdapter(&adapter);
         if (FAILED(hr))
            return fail("no adapter", hr);

         // ReShade (the game's dxgi.dll) wraps D3D12 devices: the native one comes from its "init_device" event, so the queue and
         // the swapchain are native too and ReShade puts no second runtime (effects, overlay) on the swapchain
         captured_device = nullptr;
         reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
         hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&proxy_device));
         reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
         if (FAILED(hr))
            return fail("D3D12CreateDevice", hr);
         device12 = captured_device ? captured_device : proxy_device.get();

         const D3D12_COMMAND_QUEUE_DESC queue_desc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
         if (FAILED(hr = device12->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue))))
            return fail("CreateCommandQueue", hr);
         for (auto& list : lists)
         {
            if (FAILED(hr = device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&list.allocator))) ||
                FAILED(hr = device12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, list.allocator.get(), nullptr, IID_PPV_ARGS(&list.list))) || FAILED(hr = list.list->Close()))
               return fail("command lists", hr);
         }
         if (FAILED(hr = device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&work_fence))))
            return fail("CreateFence", hr);
         work_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
         frame_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

         // D3D11 objects shared into D3D12 (NT handles, closed once opened)
         const auto share = [&](ID3D11DeviceChild* object, const IID& iid, void** opened)
         {
            HANDLE handle = nullptr;
            com_ptr<IDXGIResource1> resource;
            com_ptr<ID3D11Fence> fence;
            if (SUCCEEDED(hr = object->QueryInterface(&resource)))
               hr = resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle);
            else if (SUCCEEDED(hr = object->QueryInterface(&fence)))
               hr = fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handle);
            if (SUCCEEDED(hr))
               hr = device12->OpenSharedHandle(handle, iid, opened);
            if (handle)
               CloseHandle(handle);
            return SUCCEEDED(hr);
         };
         const D3D11_RESOURCE_MISC_FLAG shared = D3D11_RESOURCE_MISC_FLAG(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
         const CD3D11_TEXTURE2D_DESC real_desc(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, shared);
         const CD3D11_TEXTURE2D_DESC interpolated_desc(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, 1, 0, shared);
         for (auto& slot : slots)
         {
            if (FAILED(hr = device->CreateTexture2D(&real_desc, nullptr, &slot.real)) || !share(slot.real.get(), IID_PPV_ARGS(&slot.real12)) ||
                FAILED(hr = device->CreateTexture2D(&interpolated_desc, nullptr, &slot.interpolated)) || !share(slot.interpolated.get(), IID_PPV_ARGS(&slot.interpolated12)))
               return fail("shared textures", hr);
         }
         if (FAILED(hr = device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&ready))) || !share(ready.get(), IID_PPV_ARGS(&ready12)) ||
             FAILED(hr = device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&consumed))) || !share(consumed.get(), IID_PPV_ARGS(&consumed12)))
            return fail("shared fences", hr);

         stop = false;
         visual_shown = true;
         visual_shown_applied = true;
         std::promise<HRESULT> started;
         std::future<HRESULT> started_result = started.get_future();
         thread = std::thread([this, window, &started]
            { Run(window, &started); });
         hr = started_result.wait_for(std::chrono::seconds(5)) == std::future_status::ready ? started_result.get() : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
         if (FAILED(hr))
         {
            reshade::log::message(reshade::log::level::warning, std::format("[Frame Generation] The presenter failed: the composition swapchain (0x{:08X})", uint32_t(hr)).c_str());
            Stop(); // The thread ends without its loop
            return false;
         }

         presented_game_swapchain = game_swapchain;
         shown = true;
         GamePresentHook::Install(game_swapchain);
         GamePresentHook::game_swapchain = game_swapchain;
         reshade::log::message(reshade::log::level::info, std::format("[Frame Generation] Presenter started, {}x{}, D3D12 device {}", width, height, captured_device ? "native" : "ReShade's").c_str());
         return true;
      }

      void Stop()
      {
         if (!Running())
            return;
         GamePresentHook::game_swapchain = nullptr; // The game presents again (under the visual until it's gone)
         {
            const std::lock_guard lock(mutex);
            stop = true;
         }
         slot_free.notify_all();
         SetEvent(frame_event);
         thread.join();
         Release();
         reshade::log::message(reshade::log::level::info, "[Frame Generation] Presenter stopped");
      }

      // Game thread: this frame's slot, once the presenter handed it back (normally at once); the GPU waits for its copies out of it
      uint32_t Acquire(ID3D11DeviceContext4* context)
      {
         const uint32_t slot = uint32_t(frame % slot_count);
         std::unique_lock lock(mutex);
         slot_free.wait(lock, [&]
            { return stop || !slots[slot].busy; });
         if (slots[slot].frame != 0)
            context->Wait(consumed.get(), slots[slot].frame);
         return slot;
      }

      // Game thread: the slot's real frame (and interpolated one, if "interpolated") written, the real one shown after "half_interval_ms"
      void Submit(ID3D11DeviceContext4* context, uint32_t slot, bool interpolated, float half_interval_ms)
      {
         const uint64_t n = ++frame;
         context->Signal(ready.get(), n);
         context->Flush();
         {
            const std::lock_guard lock(mutex);
            slots[slot].busy = true;
            slots[slot].frame = n;
            frames.push_back({.n = n, .slot = slot, .interpolated = interpolated, .arrival = std::chrono::steady_clock::now(), .half_interval_ms = half_interval_ms});
         }
         submitted_count++;
         SetEvent(frame_event);
      }

   private:
      struct Slot
      {
         com_ptr<ID3D11Texture2D> real, interpolated;
         com_ptr<ID3D12Resource> real12, interpolated12;
         uint64_t frame = 0; // The last frame written into it
         bool busy = false;  // Handed over, not yet copied out by the presenter
      };
      struct Frame
      {
         uint64_t n = 0;
         uint32_t slot = 0;
         bool interpolated = false;
         std::chrono::steady_clock::time_point arrival;
         float half_interval_ms = 0.f;
      };
      struct List
      {
         com_ptr<ID3D12CommandAllocator> allocator;
         com_ptr<ID3D12GraphicsCommandList> list;
         uint64_t done = 0; // "work_fence" value it's free at
      };

      static inline ID3D12Device* captured_device = nullptr;
      static inline PresentFunc unhooked_present = nullptr;
      static void OnInitDevice(reshade::api::device* device)
      {
         if (device->get_api() == reshade::api::device_api::d3d12)
            captured_device = reinterpret_cast<ID3D12Device*>(device->get_native());
      }

      void Run(HWND window, std::promise<HRESULT>* started)
      {
         // A child window can't replace the visual: a disabled one swallows the mouse, a layered (click-through) one takes no flip swapchain
         HRESULT hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&composition));
         if (SUCCEEDED(hr))
            hr = composition->CreateTargetForHwnd(window, TRUE, &target);
         if (SUCCEEDED(hr))
            hr = composition->CreateVisual(&visual);
         com_ptr<IDXGIFactory4> factory;
         if (SUCCEEDED(hr))
            hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
         if (SUCCEEDED(hr))
         {
            // The interpolated frame's alpha holds FSR's inpainting weight, not coverage
            const DXGI_SWAP_CHAIN_DESC1 desc = {
               .Width = width,
               .Height = height,
               .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
               .SampleDesc = {.Count = 1},
               .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
               .BufferCount = buffer_count,
               .Scaling = DXGI_SCALING_STRETCH,
               .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
               .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
            };
            com_ptr<IDXGISwapChain1> swapchain1;
            hr = factory->CreateSwapChainForComposition(queue.get(), &desc, nullptr, &swapchain1);
            if (SUCCEEDED(hr))
               hr = swapchain1->QueryInterface(&swapchain);
         }
         if (SUCCEEDED(hr))
         {
            swapchain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709); // scRGB, as Core sets the game's
            for (uint32_t i = 0; i < buffer_count && SUCCEEDED(hr); i++)
               hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
         }
         if (SUCCEEDED(hr) && !unhooked_present)
         {
            unhooked_present = BuildUnhookedPresent(swapchain.get());
            if (!unhooked_present)
               reshade::log::message(reshade::log::level::warning, "[Frame Generation] The presenter's presents go through other addons' Present hooks");
         }
         if (SUCCEEDED(hr))
            hr = visual->SetContent(swapchain.get());
         if (SUCCEEDED(hr))
            hr = target->SetRoot(visual.get());
         if (SUCCEEDED(hr))
            hr = composition->Commit();
         started->set_value(hr);
         if (SUCCEEDED(hr))
            Loop();

         // Nothing the game's GPU waits for may stay unsignalled: its waits for slots end, then the queue drains
         queue->Signal(consumed12.get(), UINT64(1) << 40);
         WaitForWork(Signal());
         if (target)
         {
            target->SetRoot(nullptr);
            composition->Commit();
         }
         for (auto& buffer : buffers)
            buffer.reset();
         swapchain.reset();
         visual.reset();
         target.reset();
         composition.reset();
      }

      void Loop()
      {
         HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
         for (;;)
         {
            WaitForSingleObject(frame_event, 100);
            if (const bool show = visual_shown; show != visual_shown_applied)
            {
               target->SetRoot(show ? visual.get() : nullptr);
               composition->Commit();
               visual_shown_applied = show;
            }
            for (;;)
            {
               Frame shown;
               {
                  const std::lock_guard lock(mutex);
                  if (stop)
                  {
                     if (timer)
                        CloseHandle(timer);
                     return;
                  }
                  if (frames.empty())
                     break;
                  shown = frames.front();
                  frames.pop_front();
               }
               queue->Wait(ready12.get(), shown.n);
               const Slot& slot = slots[shown.slot];
               // When the next frame is already here the presenter is behind (GPU bound, presents blocking): waiting would hold the
               // game on its slots, lengthen the interval and with it the wait (in Mass Effect it settled at twice the GPU's frame
               // time, 60 real fps where 107 ran without frame generation)
               bool behind;
               {
                  const std::lock_guard lock(mutex);
                  behind = !frames.empty();
               }
               const auto now = std::chrono::steady_clock::now();
               if (shown.interpolated && !behind)
               {
                  // An even cadence with a bounded latency buffer (_tools/fsr3_dx11/pacing_test.cpp strategy J, measured at the
                  // display, 2026-10-02): the interpolated frame half a real frame after the previous real one's scheduled time, never
                  // before the arrival and at most 0.375 real frames after it, the real one half a frame later. Scheduled times, not when
                  // Present returned (that drifts by the call's duration into the cap); pulled 2 % of a frame towards the arrival per
                  // frame so the buffer empties without jitter. 0.375 is the knee of the sweep: with +-20 % game frame time jitter at 80 fps
                  // display interval p95-p5 2.3 ms for +1.3 ms latency (6.2 ms and 10 % dropped presents presenting at the arrival).
                  const auto half = std::chrono::duration<double, std::milli>(shown.half_interval_ms);
                  const auto half_ticks = std::chrono::duration_cast<std::chrono::steady_clock::duration>(half);
                  const auto cadence = last_real_scheduled + half_ticks - half_ticks / 25;
                  const auto interpolated_time = std::clamp(cadence, shown.arrival, shown.arrival + half_ticks * 3 / 4);
                  WaitUntil(timer, interpolated_time);
                  Show(slot.interpolated12.get());
                  const auto real_time = interpolated_time + half_ticks;
                  WaitUntil(timer, real_time);
                  last_real_scheduled = real_time;
               }
               else
               {
                  if (shown.interpolated)
                     Show(slot.interpolated12.get());
                  last_real_scheduled = now;
               }
               Show(slot.real12.get());
               queue->Signal(consumed12.get(), shown.n);
               {
                  const std::lock_guard lock(mutex);
                  slots[shown.slot].busy = false;
               }
               slot_free.notify_all();
            }
         }
      }

      // A high resolution waitable timer to 1 ms before, then a spin: the timer alone overshot by up to a millisecond
      static void WaitUntil(HANDLE timer, std::chrono::steady_clock::time_point time)
      {
         const double remaining_ms = std::chrono::duration<double, std::milli>(time - std::chrono::steady_clock::now()).count();
         if (remaining_ms <= 0.0 || remaining_ms > 100.0)
            return;
         if (remaining_ms > 1.0 && timer)
         {
            LARGE_INTEGER due;
            due.QuadPart = -LONGLONG((remaining_ms - 1.0) * 10000.0); // Relative, in 100 ns
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
               WaitForSingleObject(timer, 100);
         }
         while (std::chrono::steady_clock::now() < time)
            YieldProcessor();
      }

      // Copies a frame into the swapchain's next buffer and presents it
      void Show(ID3D12Resource* source)
      {
         List& list = lists[list_index];
         list_index = (list_index + 1) % std::size(lists);
         WaitForWork(list.done);
         list.allocator->Reset();
         list.list->Reset(list.allocator.get(), nullptr);
         ID3D12Resource* const buffer = buffers[swapchain->GetCurrentBackBufferIndex()].get();
         D3D12_RESOURCE_BARRIER barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
         barrier.Transition = {buffer, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST};
         list.list->ResourceBarrier(1, &barrier);
         list.list->CopyResource(buffer, source); // The shared texture is promoted from COMMON to COPY_SOURCE, and decays back
         std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
         list.list->ResourceBarrier(1, &barrier);
         list.list->Close();
         ID3D12CommandList* const execute = list.list.get();
         queue->ExecuteCommandLists(1, &execute);
         list.done = Signal();
         if (unhooked_present)
         {
            unhooked_present(swapchain.get(), 0, 0);
         }
         else
         {
            swapchain->Present(0, 0);
         }
         presented_count++;
      }

      uint64_t Signal()
      {
         queue->Signal(work_fence.get(), ++work_value);
         return work_value;
      }
      void WaitForWork(uint64_t value)
      {
         if (work_fence->GetCompletedValue() < value && SUCCEEDED(work_fence->SetEventOnCompletion(value, work_event)))
            WaitForSingleObject(work_event, 1000);
      }

      void Release()
      {
         for (auto& slot : slots)
            slot = {};
         for (auto& list : lists)
            list = {};
         ready.reset();
         consumed.reset();
         ready12.reset();
         consumed12.reset();
         work_fence.reset();
         queue.reset();
         device12 = nullptr;
         proxy_device.reset();
         for (HANDLE* event : {&work_event, &frame_event})
         {
            if (*event)
               CloseHandle(*event);
            *event = nullptr;
         }
         frames.clear();
         last_real_scheduled = {};
         presented_game_swapchain = nullptr;
         shown = false;
         frame = 0;
         work_value = 0;
         list_index = 0;
      }

      static constexpr uint32_t buffer_count = 3;
      uint32_t width = 0, height = 0;
      com_ptr<ID3D12Device> proxy_device; // ReShade's (kept alive while the native one is in use)
      ID3D12Device* device12 = nullptr;   // The native one (owned by the proxy)
      com_ptr<ID3D12CommandQueue> queue;
      List lists[4];
      size_t list_index = 0;
      com_ptr<ID3D12Fence> work_fence;
      uint64_t work_value = 0;
      HANDLE work_event = nullptr;
      Slot slots[slot_count];
      com_ptr<ID3D11Fence> ready, consumed;
      com_ptr<ID3D12Fence> ready12, consumed12;
      com_ptr<IDCompositionDevice> composition;
      com_ptr<IDCompositionTarget> target;
      com_ptr<IDCompositionVisual> visual;
      com_ptr<IDXGISwapChain3> swapchain;
      com_ptr<ID3D12Resource> buffers[buffer_count];

      std::thread thread;
      std::mutex mutex; // "frames", the slots' "busy" and "frame", "stop"
      std::condition_variable slot_free;
      std::deque<Frame> frames;
      HANDLE frame_event = nullptr;
      bool stop = false;
      uint64_t frame = 0;                                        // Game thread
      std::chrono::steady_clock::time_point last_real_scheduled; // Presenter thread
      IDXGISwapChain* presented_game_swapchain = nullptr;        // Game thread
      bool shown = false;                                        // Game thread
      std::atomic<bool> visual_shown = true;                     // Requested by the game thread
      bool visual_shown_applied = true;                          // Presenter thread
      std::atomic<uint32_t> submitted_count = 0, presented_count = 0;
   };

   // All on the game's thread (immediate context)
   struct DeviceData
   {
      bool enabled = false; // The user's setting, mirrored by Core at every present
#if DEVELOPMENT
      bool debug_tear_lines = false;
      bool debug_view = false;
#endif

      // This frame: "Prepare()" ran (the upscaler's inputs are this frame's), its history restarts, the HUD-less copy was taken
      bool prepared = false;
      bool reset = false;
      bool hudless_captured = false;
      bool prepared_previous = false;
      // Increases by exactly 1 per prepared frame, the same in prepare and dispatch (FSR treats a skip as a reset)
      uint64_t frame_id = 0;

      // Interpolation-only context (the upscaler's is separate); heap allocated, it's too large for the stack. FSR keeps one
      // frame generation context per process.
      std::unique_ptr<FfxFsr3Context> context;
      std::unique_ptr<std::byte[]> scratch_shared, scratch_interpolation;
      SR::SettingsData context_settings;

      // The back buffer before the first UI draw (the encoding the scene has before Core's display composition), and it composed
      // like the back buffer (what FSR wants as HUD-less color)
      com_ptr<ID3D11Texture2D> hudless_source;
      com_ptr<ID3D11ShaderResourceView> hudless_source_srv;
      com_ptr<ID3D11Texture2D> hudless;
      com_ptr<ID3D11RenderTargetView> hudless_rtv;
      bool hudless_composed = false; // This frame "hudless" holds "hudless_source" composed

      Presenter presenter;
      std::chrono::steady_clock::time_point retry_time; // No new presenter before this, after one failed
      std::chrono::steady_clock::time_point last_present;
      float real_interval_ms = 0.f; // Smoothed

#if DEVELOPMENT
      uint32_t interpolated_frames = 0, real_frames = 0; // Since the last stats read
#endif

      ~DeviceData()
      {
         presenter.Stop();
         DestroyContext(this);
      }

      static void DestroyContext(DeviceData* data)
      {
         if (data->context)
         {
            const FfxFrameGenerationConfig config = {.frameGenerationEnabled = false};
            ffxFsr3ConfigureFrameGeneration(data->context.get(), &config);
            ffxFsr3ContextDestroy(data->context.get());
         }
         data->context.reset();
         data->scratch_shared.reset();
         data->scratch_interpolation.reset();
         data->prepared_previous = false;
      }
   };

   inline FfxResource GetResource(ID3D11Resource* resource, FfxResourceStates state = FFX_RESOURCE_STATE_COMPUTE_READ)
   {
      return {.resource = resource, .description = GetFfxResourceDescriptionDX11(resource), .state = state};
   }

   // Frees the interpolation resources and hides the presenter (see "Presenter::SetShown()"), the game presents again
   inline void Release(DeviceData* data)
   {
      // ponytail: the hidden presenter keeps its swapchain and slots (~450 MB at 4K) until the device goes
      data->presenter.SetShown(false);
      DeviceData::DestroyContext(data);
      data->hudless_source.reset();
      data->hudless_source_srv.reset();
      data->hudless.reset();
      data->hudless_rtv.reset();
      data->prepared = false;
      data->hudless_captured = false;
   }

   inline bool UpdateContext(DeviceData* data, ID3D11Device* device, const SR::SettingsData& settings)
   {
      if (data->context && settings.output_width == data->context_settings.output_width && settings.output_height == data->context_settings.output_height &&
          settings.render_width == data->context_settings.render_width && settings.render_height == data->context_settings.render_height &&
          settings.hdr == data->context_settings.hdr && settings.inverted_depth == data->context_settings.inverted_depth &&
          settings.mvs_jittered == data->context_settings.mvs_jittered && settings.dynamic_resolution == data->context_settings.dynamic_resolution)
         return true;
      DeviceData::DestroyContext(data);

      FfxFsr3ContextDescription description = {};
      const size_t shared_size = ffxGetScratchMemorySizeDX11(1);
      const size_t interpolation_size = ffxGetScratchMemorySizeDX11(2); // Optical flow and frame interpolation
      data->scratch_shared = std::make_unique<std::byte[]>(shared_size);
      data->scratch_interpolation = std::make_unique<std::byte[]>(interpolation_size);
      FfxErrorCode error = ffxGetInterfaceDX11(&description.backendInterfaceSharedResources, ffxGetDeviceDX11(device), data->scratch_shared.get(), shared_size, 1);
      if (error == FFX_OK)
         error = ffxGetInterfaceDX11(&description.backendInterfaceFrameInterpolation, ffxGetDeviceDX11(device), data->scratch_interpolation.get(), interpolation_size, 2);
      if (error != FFX_OK)
      {
         reshade::log::message(reshade::log::level::warning, std::format("[Frame Generation] ffxGetInterfaceDX11 failed ({})", int(error)).c_str());
         DeviceData::DestroyContext(data);
         return false;
      }
      description.displaySize = {uint32_t(settings.output_width), uint32_t(settings.output_height)};
      description.maxUpscaleSize = description.displaySize;
      description.maxRenderSize = settings.dynamic_resolution ? description.displaySize : FfxDimensions2D{uint32_t(settings.render_width), uint32_t(settings.render_height)};
      description.flags = FFX_FSR3_ENABLE_INTERPOLATION_ONLY;
      if (settings.hdr)
         description.flags |= FFX_FSR3_ENABLE_HIGH_DYNAMIC_RANGE;
      if (settings.inverted_depth)
         description.flags |= FFX_FSR3_ENABLE_DEPTH_INVERTED | FFX_FSR3_ENABLE_DEPTH_INFINITE;
      if (settings.mvs_jittered)
         description.flags |= FFX_FSR3_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
      if (settings.dynamic_resolution)
         description.flags |= FFX_FSR3_ENABLE_DYNAMIC_RESOLUTION;
      description.backBufferFormat = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT; // The presenter's (scRGB)
#if DEVELOPMENT
      description.flags |= FFX_FSR3_ENABLE_DEBUG_CHECKING;
      description.fpMessage = [](FfxMsgType type, const wchar_t* message)
      {
         char buffer[512] = {};
         if (std::wcstombs(buffer, message ? message : L"", sizeof(buffer) - 1) == static_cast<std::size_t>(-1))
            return;
         reshade::log::message(type == FFX_MESSAGE_TYPE_ERROR ? reshade::log::level::error : reshade::log::level::warning, std::format("[Frame Generation] FSR: {}", buffer).c_str());
      };
#endif

      data->context = std::make_unique<FfxFsr3Context>();
      error = ffxFsr3ContextCreate(data->context.get(), &description);
      if (error != FFX_OK)
      {
         reshade::log::message(reshade::log::level::warning, std::format("[Frame Generation] ffxFsr3ContextCreate failed ({})", int(error)).c_str());
         data->context.reset(); // Nothing to destroy
         DeviceData::DestroyContext(data);
         return false;
      }
      const FfxFrameGenerationConfig config = {.frameGenerationEnabled = true};
      ffxFsr3ConfigureFrameGeneration(data->context.get(), &config);
      data->context_settings = settings;
      return true;
   }

   // Game thread, right after the upscaler's "Draw()" with the same inputs. The caller saves and restores the pipeline state (FSR
   // doesn't). False if this frame won't be interpolated.
   inline bool Prepare(DeviceData* data, ID3D11DeviceContext* device_context, const SR::SettingsData& settings, const SR::SuperResolutionImpl::DrawData& draw_data)
   {
      if (!data->enabled || data->prepared)
         return false;
      // Bad camera data has hung FSR's interpolation on the GPU (FGCOM-22)
      if (!(draw_data.vert_fov > 0.f && draw_data.vert_fov < 3.1416f) || !(draw_data.near_plane > 0.f) || !(draw_data.far_plane > draw_data.near_plane) || !std::isfinite(draw_data.far_plane))
         return false;
      com_ptr<ID3D11Device> device;
      device_context->GetDevice(&device);
      if (!UpdateContext(data, device.get(), settings))
         return false;

      data->frame_id++;
      data->reset = draw_data.reset || !data->prepared_previous;
      const bool inverted = settings.inverted_depth;
      const FfxFsr3DispatchFrameGenerationPrepareDescription prepare = {
         .commandList = ffxGetCommandListDX11(device_context),
         .depth = GetResource(draw_data.depth_buffer),
         .motionVectors = GetResource(draw_data.motion_vectors),
         .jitterOffset = {draw_data.jitter_x, draw_data.jitter_y},
         .motionVectorScale = {settings.mvs_x_scale, settings.mvs_y_scale},
         .renderSize = {uint32_t(draw_data.render_width), uint32_t(draw_data.render_height)},
         .frameTimeDelta = data->real_interval_ms > 0.f ? data->real_interval_ms : 1000.f / 60.f,
         .cameraNear = inverted ? draw_data.far_plane : draw_data.near_plane,
         .cameraFar = inverted ? draw_data.near_plane : draw_data.far_plane,
         .viewSpaceToMetersFactor = 1.f,
         .cameraFovAngleVertical = draw_data.vert_fov,
         .frameID = data->frame_id,
      };
      const FfxErrorCode error = ffxFsr3ContextDispatchFrameGenerationPrepare(data->context.get(), &prepare);
      data->prepared = error == FFX_OK;
      return data->prepared;
   }

   // Game thread, at a draw into the back buffer that isn't Luma's: the first one after "Prepare()" is the UI's
   inline void CaptureHudless(DeviceData* data, ID3D11Device* device, ID3D11DeviceContext* device_context, ID3D11Texture2D* back_buffer)
   {
      D3D11_TEXTURE2D_DESC desc;
      back_buffer->GetDesc(&desc);
      D3D11_TEXTURE2D_DESC source_desc = {};
      if (data->hudless_source)
         data->hudless_source->GetDesc(&source_desc);
      if (source_desc.Width != desc.Width || source_desc.Height != desc.Height || source_desc.Format != desc.Format)
      {
         data->hudless_source.reset();
         data->hudless_source_srv.reset();
         data->hudless.reset();
         data->hudless_rtv.reset();
         D3D11_TEXTURE2D_DESC copy_desc = desc;
         copy_desc.MipLevels = 1;
         copy_desc.ArraySize = 1;
         copy_desc.Usage = D3D11_USAGE_DEFAULT;
         copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         copy_desc.CPUAccessFlags = 0;
         copy_desc.MiscFlags = 0;
         if (FAILED(device->CreateTexture2D(&copy_desc, nullptr, &data->hudless_source)) || FAILED(device->CreateShaderResourceView(data->hudless_source.get(), nullptr, &data->hudless_source_srv)))
         {
            data->hudless_source.reset();
            return;
         }
         copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         if (FAILED(device->CreateTexture2D(&copy_desc, nullptr, &data->hudless)) || FAILED(device->CreateRenderTargetView(data->hudless.get(), nullptr, &data->hudless_rtv)))
         {
            data->hudless.reset();
            data->hudless_rtv.reset();
         }
      }
      device_context->CopyResource(data->hudless_source.get(), back_buffer);
      data->hudless_captured = true;
   }

   // Game thread, at ReShade's present (after its effects and overlay, so the real frame is what the game's present would show). The
   // caller saves and restores the pipeline state. Starts and stops the presenter with the setting.
   inline void Present(DeviceData* data, ID3D11Device* device, ID3D11DeviceContext* device_context, IDXGISwapChain* swapchain, float max_luminance_nits)
   {
      const auto now = std::chrono::steady_clock::now();
      if (data->last_present != std::chrono::steady_clock::time_point{})
      {
         const float interval_ms = std::chrono::duration<float, std::milli>(now - data->last_present).count();
         if (interval_ms < 100.f)
            data->real_interval_ms = data->real_interval_ms > 0.f ? (data->real_interval_ms * 0.9f + interval_ms * 0.1f) : interval_ms;
      }
      data->last_present = now;

      const bool prepared = std::exchange(data->prepared, false);
      const bool hudless = std::exchange(data->hudless_captured, false);
      const bool hudless_composed = std::exchange(data->hudless_composed, false);
      const bool reset = std::exchange(data->reset, false);
      data->prepared_previous = prepared;

      if (!data->enabled)
      {
         if (data->presenter.Shown() || data->context)
            Release(data);
         return;
      }

      com_ptr<ID3D11Texture2D> back_buffer;
      DXGI_SWAP_CHAIN_DESC swapchain_desc;
      BOOL fullscreen = FALSE;
      if (FAILED(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) || FAILED(swapchain->GetDesc(&swapchain_desc)))
         return;
      swapchain->GetFullscreenState(&fullscreen, nullptr);
      D3D11_TEXTURE2D_DESC back_buffer_desc;
      back_buffer->GetDesc(&back_buffer_desc);
      // The presenter is a composition swapchain over the window (no exclusive fullscreen) and scRGB like Luma's upgraded swapchain
      if (fullscreen || back_buffer_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
      {
         data->presenter.SetShown(false);
         return;
      }

      // A new size or game swapchain needs a new presenter
      if (data->presenter.Running() && (data->presenter.Width() != back_buffer_desc.Width || data->presenter.Height() != back_buffer_desc.Height || data->presenter.GameSwapchain() != swapchain))
         data->presenter.Stop();
      data->presenter.SetShown(data->presenter.Running());
      if (!data->presenter.Running())
      {
         if (now < data->retry_time)
            return;
         if (!data->presenter.Start(device, swapchain, swapchain_desc.OutputWindow, back_buffer_desc.Width, back_buffer_desc.Height))
         {
            data->retry_time = now + std::chrono::seconds(2);
            return;
         }
      }

      com_ptr<ID3D11DeviceContext4> device_context4;
      if (FAILED(device_context->QueryInterface(&device_context4)))
         return;
      const uint32_t slot = data->presenter.Acquire(device_context4.get());
      device_context->CopyResource(data->presenter.Real(slot), back_buffer.get());

      // Only frames whose inputs are this frame's, at the size the context was made for
      bool interpolated = prepared && data->context && uint32_t(data->context_settings.output_width) == back_buffer_desc.Width && uint32_t(data->context_settings.output_height) == back_buffer_desc.Height;
      if (interpolated)
      {
         ID3D11Texture2D* const hudless_texture = hudless ? (hudless_composed ? data->hudless.get() : data->hudless_source.get()) : nullptr;
         FfxFrameGenerationConfig config = {.frameGenerationEnabled = true};
         if (hudless_texture)
            config.HUDLessColor = GetResource(hudless_texture);
#if DEVELOPMENT
         config.flags = (data->debug_tear_lines ? FFX_FSR3_FRAME_GENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0) | (data->debug_view ? FFX_FSR3_FRAME_GENERATION_FLAG_DRAW_DEBUG_VIEW : 0);
#endif
         ffxFsr3ConfigureFrameGeneration(data->context.get(), &config);

         FfxFrameGenerationDispatchDescription dispatch = {
            .commandList = ffxGetCommandListDX11(device_context),
            .presentColor = GetResource(data->presenter.Real(slot)),
            .numInterpolatedFrames = 1,
            .reset = reset,
            .backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SCRGB,
            .minMaxLuminance = {0.f, max_luminance_nits},
            .interpolationRect = {0, 0, int32_t(back_buffer_desc.Width), int32_t(back_buffer_desc.Height)},
            .frameID = data->frame_id,
         };
         dispatch.outputs[0] = GetResource(data->presenter.Interpolated(slot), FFX_RESOURCE_STATE_UNORDERED_ACCESS);
         // After a reset FSR copies the current frame out: dispatched to start its history, not shown
         interpolated = ffxFsr3DispatchFrameGeneration(&dispatch) == FFX_OK && !reset;
      }
#if DEVELOPMENT
      (interpolated ? data->interpolated_frames : data->real_frames)++;
#endif
      data->presenter.Submit(device_context4.get(), slot, interpolated, data->real_interval_ms * 0.5f);
   }
} // namespace FrameGeneration
#endif // ENABLE_FRAME_GENERATION
