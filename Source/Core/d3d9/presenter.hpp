// In-process x86 D3D12 output device of the d9native add-on (its own thread,
// PresenterThread). Protocol: presenter_protocol.h lines over an anonymous pipe.
// Opens the game's D3D9Ex shared render targets by KMT handle, composes them
// (compose.hlsl) plus ReShade's D3D9 overlay into a D3D12 flip swapchain on the
// game's window and presents. The device comes from ID3D12DeviceFactory, so
// ReShade does not wrap it and skips its swapchain (no second effect runtime).
// Spike code: one thread, waits for its own GPU every frame.
//
// Include the game's cbuffer structs (LUMA_GAME_CB_STRUCTS) before this header,
// as for cbuffers.h: the b13 root constants are CB::LumaGlobalSettingsPadded.
#pragma once
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <fcntl.h>
#include <io.h>
#include <iterator>
#include <span>
#include <windows.h>
#include <wrl/client.h>

#include "../includes/cbuffers.h"
#include "presenter_protocol.h"

#pragma comment(lib, "dxguid.lib") // CLSID_D3D12DeviceFactory
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

namespace D3D9Presenter
{
   using Microsoft::WRL::ComPtr;

   // Same order as d9native.cpp kModes (OUTPUT_SCRGB, OUTPUT_HDR10, OUTPUT_SRGB).
   inline constexpr struct
   {
      DXGI_FORMAT format;
      DXGI_COLOR_SPACE_TYPE color_space;
      UINT pixel_bytes;
   } kModes[] = {
      {.format = DXGI_FORMAT_R16G16B16A16_FLOAT, .color_space = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, .pixel_bytes = 8},
      {.format = DXGI_FORMAT_R10G10B10A2_UNORM, .color_space = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, .pixel_bytes = 4},
      {.format = DXGI_FORMAT_R8G8B8A8_UNORM, .color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, .pixel_bytes = 4}};
   constexpr UINT BUFFERS = 2;
   constexpr UINT BLOCK = 256;                        // overlay block (d9native.cpp OVERLAY_BLOCK)
   constexpr UINT64 PROBE_OFFSET = BLOCK * BLOCK * 8; // probes after the fp16 block in the readback buffer
   constexpr UINT64 READBACK_SIZE = PROBE_OFFSET + 4 * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

   inline PresenterStats* stats = nullptr;
   inline FILE* g_log = nullptr; // owned by the caller of PresenterThread

   inline void PLog(const char* format, ...)
   {
      if (!g_log)
      {
         return;
      }
      va_list args;
      va_start(args, format);
      vfprintf(g_log, format, args);
      va_end(args);
      fflush(g_log);
   }

   struct PresenterFailure
   {
   }; // Fail() unwinds to PresenterThread

   [[noreturn]] inline void Fail(const char* format, ...)
   {
      char text[128];
      va_list args;
      va_start(args, format);
      vsnprintf(text, sizeof(text), format, args);
      va_end(args);
      PLog("d9present: ERROR %s\n", text);
      if (stats)
      {
         memcpy(stats->error, text, sizeof(text));
      }
      throw PresenterFailure{};
   }
   inline void Check(HRESULT hr, const char* what)
   {
      if (FAILED(hr))
      {
         Fail("%s 0x%08lX", what, hr);
      }
   }

   inline float HalfToFloat(uint16_t h)
   {
      const unsigned sign = h >> 15, exponent = (h >> 10) & 31, mantissa = h & 1023;
      const float value = exponent == 0
                             ? std::ldexp(static_cast<float>(mantissa), -24)
                             : std::ldexp(static_cast<float>(mantissa | 1024), static_cast<int>(exponent) - 25);
      return sign ? -value : value;
   }

   struct Presenter
   {
      HWND hwnd = nullptr;
      ComPtr<IDXGIFactory6> factory;
      ComPtr<IDXGIAdapter1> adapter;
      ComPtr<ID3D12Device> device;
      ComPtr<ID3D12CommandQueue> queue;
      ComPtr<ID3D12CommandAllocator> allocators[2]; // alternate frames
      UINT64 allocator_done[2] = {};                // own fence value after the frame using it
      ComPtr<ID3D12Fence> fence_in, fence_out;      // D3D9 kernel fences (fences line)
      int color_refresh = 0;
      ComPtr<ID3D12GraphicsCommandList> list;
      ComPtr<ID3D12Fence> fence;
      UINT64 fence_value = 0;
      HANDLE fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      ComPtr<ID3D12RootSignature> root;
      ComPtr<ID3D12PipelineState> pso[3];
      ComPtr<ID3D12DescriptorHeap> rtv_heap, srv_heap;
      ComPtr<ID3D12Resource> readback;
      ComPtr<ID3D12Resource> textures[4]; // scene, canvas, overlay, snapshot
      ComPtr<IDXGISwapChain3> swapchain;
      ComPtr<ID3D12Resource> buffers[BUFFERS];
      HANDLE latency_handle = nullptr;
      int mode = -1;
      UINT width = 0, height = 0, swapchain_flags = 0, present_flags = 0;
      UINT requested_width = 0, requested_height = 0; // last frame line's size
      bool latency = false;
      bool needs_resize = false; // a failed Present: same-size ResizeBuffers

      void Idle()
      {
         Check(queue->Signal(fence.Get(), ++fence_value), "Signal");
         Check(fence->SetEventOnCompletion(fence_value, fence_event), "SetEventOnCompletion");
         WaitForSingleObject(fence_event, INFINITE);
      }

      // compose_vs / compose_ps: compose.hlsl VSMain / PSMain bytecode.
      void Init(HWND window, LUID luid, std::span<const std::byte> compose_vs, std::span<const std::byte> compose_ps)
      {
         hwnd = window;
         Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
         Check(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)), "EnumAdapterByLuid");
         ComPtr<ID3D12DeviceFactory> device_factory;
         Check(D3D12GetInterface(CLSID_D3D12DeviceFactory, IID_PPV_ARGS(&device_factory)), "D3D12GetInterface(device factory)");
         Check(device_factory->SetFlags(D3D12_DEVICE_FACTORY_FLAG_DISALLOW_STORING_NEW_DEVICE_AS_SINGLETON), "device factory SetFlags");
         Check(device_factory->CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device factory CreateDevice");
         const D3D12_COMMAND_QUEUE_DESC queue_desc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
         Check(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
         for (auto& allocator : allocators)
         {
            Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "allocator");
         }
         Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0].Get(), nullptr, IID_PPV_ARGS(&list)), "CreateCommandList");
         list->Close();
         Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

         // b0: the Composition cbuffer as 8 root constants; t0 hand-off, t1
         // overlay, t2 snapshot; b13 LumaSettings (the dither's frame index).
         const D3D12_DESCRIPTOR_RANGE range = {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = 3};
         const D3D12_ROOT_PARAMETER params[] = {
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
               .Constants = {.Num32BitValues = 8},
               .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
               .DescriptorTable = {1, &range},
               .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
               .Constants = {.ShaderRegister = 13, .Num32BitValues = sizeof(CB::LumaGlobalSettingsPadded) / 4},
               .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL}};
         const D3D12_ROOT_SIGNATURE_DESC root_desc = {.NumParameters = UINT(std::size(params)), .pParameters = params};
         ComPtr<ID3DBlob> blob, error;
         Check(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "root signature");
         Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "CreateRootSignature");

         if (compose_vs.empty() || compose_ps.empty())
         {
            Fail("compose_vs.cso / compose_ps.cso missing next to the add-on");
         }
         for (int m = 0; m < 3; ++m)
         {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
               .pRootSignature = root.Get(),
               .VS = {compose_vs.data(), compose_vs.size()},
               .PS = {compose_ps.data(), compose_ps.size()},
               .SampleMask = UINT_MAX,
               .RasterizerState = {.FillMode = D3D12_FILL_MODE_SOLID, .CullMode = D3D12_CULL_MODE_NONE, .DepthClipEnable = TRUE},
               .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
               .NumRenderTargets = 1,
               .SampleDesc = {1, 0}};
            desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            desc.RTVFormats[0] = kModes[m].format;
            Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso[m])), "CreateGraphicsPipelineState");
         }
         const D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = BUFFERS};
         Check(device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap)), "RTV heap");
         const D3D12_DESCRIPTOR_HEAP_DESC srv_desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 6, // (scene | canvas) + overlay + snapshot
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
         Check(device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&srv_heap)), "SRV heap");
         const D3D12_HEAP_PROPERTIES readback_heap = {.Type = D3D12_HEAP_TYPE_READBACK};
         const D3D12_RESOURCE_DESC buffer_desc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Width = READBACK_SIZE,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .SampleDesc = {1, 0},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
         Check(device->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)), "readback buffer");
      }

      // KMT handles of the D3D9 shared render targets (0 = none). SRV slots: 0
      // scene, 1 overlay, 2 snapshot, 3 canvas, 4 overlay, 5 snapshot, so the table
      // at 0 or 3 is (source, overlay, snapshot).
      void SetTextures(const unsigned long long handles[4])
      {
         Idle();
         if (!handles[0] && !handles[1] && !handles[2] && !handles[3])
         {
            // Reset or loss: the next frame's size applies again, as a new swapchain
            // after the game's Reset would.
            requested_width = requested_height = 0;
         }
         const UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         const D3D12_CPU_DESCRIPTOR_HANDLE base = srv_heap->GetCPUDescriptorHandleForHeapStart();
         const UINT slots[4][2] = {{0, 0}, {3, 3}, {1, 4}, {2, 5}};
         for (int i = 0; i < 4; ++i)
         {
            textures[i].Reset();
            HRESULT hr = S_OK;
            if (handles[i])
            {
               hr = device->OpenSharedHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handles[i])), IID_PPV_ARGS(&textures[i]));
               if (FAILED(hr))
               {
                  Fail("OpenSharedHandle(texture %d, 0x%llX) 0x%08lX", i, handles[i], hr);
               }
            }
            // Null SRVs keep the table valid when a texture is absent.
            D3D12_SHADER_RESOURCE_VIEW_DESC view = {
               .Format = textures[i] ? textures[i]->GetDesc().Format : DXGI_FORMAT_R8G8B8A8_UNORM,
               .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
               .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
               .Texture2D = {.MipLevels = 1}};
            for (const UINT slot : slots[i])
            {
               device->CreateShaderResourceView(textures[i].Get(), &view, {base.ptr + slot * increment});
            }
            PLog("d9present: texture %d handle 0x%llX -> %s\n", i, handles[i], textures[i] ? "opened" : "none");
            if (i == 0)
            {
               const D3D12_RESOURCE_DESC desc = textures[0] ? textures[0]->GetDesc() : D3D12_RESOURCE_DESC{};
               stats->shared_desc_ok = textures[0] && desc.MipLevels == 1 && desc.SampleDesc.Count == 1 && !(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
            }
         }
      }

      void ReleaseBuffers()
      {
         for (auto& buffer : buffers)
         {
            buffer.Reset();
         }
      }

      void AcquireBuffers()
      {
         const UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
         for (UINT i = 0; i < BUFFERS; ++i)
         {
            Check(swapchain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])), "GetBuffer");
            device->CreateRenderTargetView(buffers[i].Get(), nullptr, {rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr + i * increment});
         }
         if (latency_handle)
         {
            CloseHandle(latency_handle);
            latency_handle = nullptr;
         }
         if (swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
         {
            stats->latency_hr = swapchain->SetMaximumFrameLatency(1);
            latency_handle = swapchain->GetFrameLatencyWaitableObject();
         }
         stats->latency_flag = latency_handle != nullptr;
      }

      void DestroySwapchain()
      {
         if (!swapchain)
         {
            return;
         }
         Idle();
         ReleaseBuffers();
         if (latency_handle)
         {
            CloseHandle(latency_handle);
            latency_handle = nullptr;
         }
         swapchain.Reset();
         ++stats->swapchain_releases;
         stats->ready = 0;
      }

      // Flip swapchain in `new_mode` on the game's window, after the game's D3D9
      // blt presents (no throwaway blt swapchain needed in-process:
      // kmt12_probe/present_probe "inproc").
      bool CreateSwapchain(int new_mode, UINT w, UINT h, bool want_latency)
      {
         DestroySwapchain();
         BOOL tearing = FALSE;
         factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
         swapchain_flags = (tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) | (want_latency ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0);
         present_flags = tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
         const DXGI_SWAP_CHAIN_DESC1 desc = {
            .Width = w,
            .Height = h,
            .Format = kModes[new_mode].format,
            .SampleDesc = {1, 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = BUFFERS,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = swapchain_flags};
         ComPtr<IDXGISwapChain1> swapchain1;
         stats->swapchain_hr = factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &desc, nullptr, nullptr, &swapchain1);
         if (FAILED(stats->swapchain_hr))
         {
            PLog("d9present: CreateSwapChainForHwnd 0x%08X\n", stats->swapchain_hr);
            return false;
         }
         factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN);
         swapchain1.As(&swapchain);
         // Test: a refused colour space (sRGB needs none). The add-on excludes the
         // mode and asks for the fallback.
         if (new_mode != 2 && stats->fault_cs_unsupported > 0)
         {
            InterlockedDecrement(&stats->fault_cs_unsupported);
            stats->color_space_hr = DXGI_ERROR_UNSUPPORTED;
         }
         else
         {
            stats->color_space_hr = swapchain->SetColorSpace1(kModes[new_mode].color_space);
         }
         stats->swapchain_flags = swapchain_flags;
         mode = stats->mode = new_mode;
         stats->format = kModes[new_mode].format;
         stats->color_space = kModes[new_mode].color_space;
         width = stats->width = w;
         height = stats->height = h;
         latency = want_latency;
         AcquireBuffers();
         ++stats->swapchain_creates;
         stats->ready = 1;
         PLog("d9present: swapchain %ux%u mode %d flags 0x%X color space hr 0x%08X\n", w, h, new_mode, swapchain_flags, stats->color_space_hr);
         return true;
      }

      void Resize(UINT w, UINT h)
      {
         Idle();
         ReleaseBuffers();
         if (stats->fault_resize > 0) // test: the size stays, later frames retry
         {
            InterlockedDecrement(&stats->fault_resize);
            stats->resize_hr = DXGI_ERROR_INVALID_CALL;
         }
         else
         {
            stats->resize_hr = swapchain->ResizeBuffers(0, w, h, kModes[mode].format, swapchain_flags);
         }
         if (SUCCEEDED(stats->resize_hr))
         {
            width = stats->width = w;
            height = stats->height = h;
            ++stats->resizes;
         }
         AcquireBuffers();
      }

      // NT handles of the D3D9 kernel fences (D3DKMTShareObjects), duplicated into
      // this process.
      void SetFences(unsigned long long in, unsigned long long out)
      {
         Check(device->OpenSharedHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(in)), IID_PPV_ARGS(&fence_in)), "OpenSharedHandle(fence in)");
         Check(device->OpenSharedHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(out)), IID_PPV_ARGS(&fence_out)), "OpenSharedHandle(fence out)");
         CloseHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(in)));
         CloseHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(out)));
         PLog("d9present: KMT fences opened\n");
      }

      struct Frame
      {
         unsigned long long n;
         int source, mode;
         float paper_white, decode_gamma;
         int sync_interval, overlay, test_flags, follow;
         UINT width, height;
         int latency_wait, block, probe_count, probe_xy[8];
         int gpu_wait, color_refresh;
      };

      void Present(const Frame& f)
      {
         // GPU wait: everything this frame submits reads the textures after the
         // D3D9 queue signalled "in" n (no CPU drain on the game side).
         if (f.gpu_wait && fence_in)
         {
            Check(queue->Wait(fence_in.Get(), f.n), "Wait(fence in)");
         }
         RECT client = {};
         GetClientRect(hwnd, &client);
         // Not following the window: the requested size applies when it changes; a
         // size reached by following the window stays until then.
         const bool new_request = f.width != requested_width || f.height != requested_height;
         requested_width = f.width;
         requested_height = f.height;
         UINT w = f.follow ? static_cast<UINT>(client.right) : f.width,
              h = f.follow ? static_cast<UINT>(client.bottom) : f.height;
         if (swapchain && !f.follow && !new_request)
         {
            w = width;
            h = height;
         }
         if (w == 0 || h == 0 || IsIconic(hwnd))
         {
            return; // minimized: nothing to present, the frame still completes
         }
         if (!swapchain || f.mode != mode || (f.latency_wait != 0) != latency)
         {
            if (!CreateSwapchain(f.mode, w, h, f.latency_wait != 0))
            {
               return;
            }
            needs_resize = false;
         }
         else if (w != width || h != height || needs_resize)
         {
            Resize(w, h);
            needs_resize = needs_resize && FAILED(stats->resize_hr);
         }
         if (f.color_refresh != color_refresh)
         {
            // The add-on saw the display change (IsCurrent() FALSE): re-apply.
            color_refresh = f.color_refresh;
            stats->color_space_hr = swapchain->SetColorSpace1(kModes[mode].color_space);
         }
         if (latency_handle)
         {
            ++stats->latency_waits;
            stats->latency_timeouts += WaitForSingleObjectEx(latency_handle, 100, TRUE) != WAIT_OBJECT_0;
         }
         ID3D12Resource* source = textures[f.source == 1 ? 1 : 0].Get();
         if (!source)
         {
            return;
         }
         // Letterbox the source into the output (d9native.cpp OnPresent).
         const D3D12_RESOURCE_DESC source_desc = source->GetDesc();
         const float src_w = static_cast<float>(source_desc.Width),
                     src_h = static_cast<float>(source_desc.Height);
         const float scale = std::fmin(width / src_w, height / src_h);
         const float vp_w = std::floor(src_w * scale + 0.5f),
                     vp_h = std::floor(src_h * scale + 0.5f);
         const float vp_x = std::floor((width - vp_w) * 0.5f),
                     vp_y = std::floor((height - vp_h) * 0.5f);
         stats->vp[0] = static_cast<uint32_t>(vp_x);
         stats->vp[1] = static_cast<uint32_t>(vp_y);
         stats->vp[2] = static_cast<uint32_t>(vp_w);
         stats->vp[3] = static_cast<uint32_t>(vp_h);
         const bool overlay = f.overlay && textures[2];
         const struct
         {
            float paper_white, vp_x, vp_y, decode_gamma, vp_w, vp_h;
            uint32_t output_mode, test_flags;
         } constants = {f.paper_white,
            vp_x,
            vp_y,
            f.decode_gamma,
            vp_w,
            vp_h,
            static_cast<uint32_t>(mode),
            static_cast<uint32_t>(f.test_flags & 3) |
               (overlay ? 4u : 0u) |
               // present clamp, and whether this frame has a snapshot
               (f.source == 1 ? f.test_flags & 8 : 0) |
               (textures[3] ? f.test_flags & 16 : 0) |
               (f.test_flags & 32)};
         CB::LumaGlobalSettingsPadded settings = {};
         settings.FrameIndex = static_cast<uint32_t>(f.n);

         const UINT index = swapchain->GetCurrentBackBufferIndex();
         ID3D12Resource* back_buffer = buffers[index].Get();
         const D3D12_CPU_DESCRIPTOR_HANDLE rtv = {
            rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr + index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
         D3D12_GPU_DESCRIPTOR_HANDLE table = srv_heap->GetGPUDescriptorHandleForHeapStart();
         table.ptr += (f.source == 1 ? 3 : 0) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         const bool block = f.block && mode == 0 && width >= BLOCK && height >= BLOCK;
         const bool readback_needed = block || f.probe_count > 0;

         const int slot = static_cast<int>(f.n & 1);
         if (fence->GetCompletedValue() < allocator_done[slot])
         {
            Check(fence->SetEventOnCompletion(allocator_done[slot], fence_event), "SetEventOnCompletion");
            WaitForSingleObject(fence_event, INFINITE);
         }
         Check(allocators[slot]->Reset(), "allocator Reset");
         Check(list->Reset(allocators[slot].Get(), pso[mode].Get()), "list Reset");
         D3D12_RESOURCE_BARRIER barrier = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Transition = {.pResource = back_buffer,
               .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
               .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
               .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET}};
         list->ResourceBarrier(1, &barrier);
         // The shared textures are SIMULTANEOUS_ACCESS (D3D9 KMT surfaces open with
         // it): promoted from COMMON on use.
         const D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
         const D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
         list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
         list->RSSetViewports(1, &viewport);
         list->RSSetScissorRects(1, &scissor);
         list->SetGraphicsRootSignature(root.Get());
         ID3D12DescriptorHeap* heaps[] = {srv_heap.Get()};
         list->SetDescriptorHeaps(1, heaps);
         list->SetGraphicsRoot32BitConstants(0, 8, &constants, 0);
         list->SetGraphicsRootDescriptorTable(1, table);
         list->SetGraphicsRoot32BitConstants(2, sizeof(settings) / 4, &settings, 0);
         list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         list->DrawInstanced(3, 1, 0, 0);
         barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
         barrier.Transition.StateAfter = readback_needed ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_PRESENT;
         list->ResourceBarrier(1, &barrier);
         const auto copy = [&](UINT x, UINT y, UINT w, UINT h, UINT64 offset, UINT pitch)
         {
            const D3D12_TEXTURE_COPY_LOCATION dst = {
               .pResource = readback.Get(),
               .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
               .PlacedFootprint = {offset, {kModes[mode].format, w, h, 1, pitch}}};
            const D3D12_TEXTURE_COPY_LOCATION src = {
               .pResource = back_buffer,
               .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
               .SubresourceIndex = 0};
            const D3D12_BOX box = {x, y, 0, x + w, y + h, 1};
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
         };
         bool probed[4] = {};
         for (int i = 0; i < f.probe_count && i < 4; ++i)
         {
            const int x = f.probe_xy[2 * i], y = f.probe_xy[2 * i + 1];
            if (x >= 0 && y >= 0 && x < static_cast<int>(width) && y < static_cast<int>(height))
            {
               probed[i] = true;
               copy(x, y, 1, 1, PROBE_OFFSET + i * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
            }
         }
         if (block)
         {
            copy(0, 0, BLOCK, BLOCK, 0, BLOCK * 8);
         }
         if (readback_needed)
         {
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            list->ResourceBarrier(1, &barrier);
         }
         Check(list->Close(), "list Close");
         ID3D12CommandList* lists[] = {list.Get()};
         queue->ExecuteCommandLists(1, lists);
         const UINT sync_interval = static_cast<UINT>(f.sync_interval);
         stats->last_present_hr = swapchain->Present(sync_interval, sync_interval == 0 ? present_flags : 0);
         if (stats->fault_present_invalid > 0)
         {
            InterlockedDecrement(&stats->fault_present_invalid);
            stats->last_present_hr = DXGI_ERROR_INVALID_CALL;
         }
         if (FAILED(stats->last_present_hr))
         {
            ++stats->present_failed;
            // Recovery (RenoDX device_proxy): same-size ResizeBuffers at the next
            // frame, retried until it succeeds.
            if (!stats->no_recovery && !needs_resize)
            {
               needs_resize = true;
               ++stats->recoveries;
            }
         }
         else
         {
            ++stats->present_ok;
         }
         Check(queue->Signal(fence.Get(), ++fence_value), "Signal");
         allocator_done[slot] = fence_value;
         // E0 frames: the textures are free once frames = n, so the GPU must be
         // done; readback frames: the stats of this frame before frames = n.
         if (f.gpu_wait && fence_out && !readback_needed)
         {
            return;
         }
         Idle();
         if (!readback_needed)
         {
            return;
         }
         const D3D12_RANGE range = {0, READBACK_SIZE};
         void* mapped = nullptr;
         Check(readback->Map(0, &range, &mapped), "readback Map");
         const auto* bytes = static_cast<const uint8_t*>(mapped);
         for (int i = 0; i < 4; ++i)
         {
            if (!probed[i])
            {
               continue;
            }
            const uint8_t* p = bytes + PROBE_OFFSET + i * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
            float* rgba = stats->probe_rgba[i];
            if (mode == 0)
            {
               for (int c = 0; c < 4; ++c)
               {
                  rgba[c] = HalfToFloat(reinterpret_cast<const uint16_t*>(p)[c]);
               }
            }
            else if (mode == 1) // R10G10B10A2: R bits 0-9, G 10-19, B 20-29
            {
               const uint32_t code = *reinterpret_cast<const uint32_t*>(p);
               rgba[0] = (code & 1023) / 1023.0f;
               rgba[1] = ((code >> 10) & 1023) / 1023.0f;
               rgba[2] = ((code >> 20) & 1023) / 1023.0f;
               rgba[3] = (code >> 30) / 3.0f;
            }
            else
            {
               for (int c = 0; c < 4; ++c)
               {
                  rgba[c] = p[c] / 255.0f;
               }
            }
         }
         if (block)
         {
            double sum = 0;
            const auto* half = reinterpret_cast<const uint16_t*>(bytes);
            for (UINT i = 0; i < BLOCK * BLOCK * 4; ++i)
            {
               sum += i % 4 == 3 ? 0.0f : HalfToFloat(half[i]);
            }
            stats->overlay_block_sum = sum;
            ++stats->overlay_block_frames;
         }
         const D3D12_RANGE no_write = {0, 0};
         readback->Unmap(0, &no_write);
      }
   };

   // Reads the presenter_protocol.h lines from `in` until EOF.
   // Exit code 0 = EOF (normal stop), 1 = Fail().
   inline DWORD RunPresenter(FILE* in, std::span<const std::byte> compose_vs, std::span<const std::byte> compose_ps)
   {
      char line[512];
      if (!fgets(line, sizeof(line), in))
      {
         return 0;
      }
      int version = 0;
      unsigned long long hwnd = 0, mapping = 0;
      LUID luid = {};
      if (sscanf_s(line, "start %d %llu %lu %ld %llu", &version, &hwnd, &luid.LowPart, &luid.HighPart, &mapping) != 5 || version != PRESENTER_VERSION)
      {
         Fail("bad start line: %s", line);
      }
      stats = static_cast<PresenterStats*>(MapViewOfFile(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(mapping)), FILE_MAP_WRITE, 0, 0, sizeof(PresenterStats)));
      if (!stats)
      {
         Fail("MapViewOfFile %lu", GetLastError());
      }
      // GetClientRect must return physical pixels for the swapchain size; this
      // thread only, the game's own awareness stays.
      SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
      Presenter presenter;
      char hang_text[16] = "";
      GetEnvironmentVariableA("D9P_HANG_AT", hang_text, sizeof(hang_text));
      const long long hang_at = atoll(hang_text); // 0 = off
      presenter.Init(reinterpret_cast<HWND>(static_cast<uintptr_t>(hwnd)), luid, compose_vs, compose_ps);
      PLog("d9present: ready, hwnd 0x%llX, LUID %08lX:%08lX\n", hwnd, luid.HighPart, luid.LowPart);
      while (fgets(line, sizeof(line), in))
      {
         if (unsigned long long handles[4] = {};
            sscanf_s(line, "textures %llu %llu %llu %llu", &handles[0], &handles[1], &handles[2], &handles[3]) == 4)
         {
            presenter.SetTextures(handles);
            continue;
         }
         if (unsigned long long in = 0, out = 0;
            sscanf_s(line, "fences %llu %llu", &in, &out) == 2)
         {
            presenter.SetFences(in, out);
            continue;
         }
         Presenter::Frame f = {};
         if (sscanf_s(line,
                "frame %llu %d %d %f %f %d %d %d %d %u %u %d %d %d %d %d %d "
                "%d %d %d %d %d %d %d",
                &f.n, &f.source, &f.mode, &f.paper_white, &f.decode_gamma,
                &f.sync_interval, &f.overlay, &f.test_flags, &f.follow,
                &f.width, &f.height, &f.latency_wait, &f.block, &f.probe_count,
                &f.probe_xy[0], &f.probe_xy[1], &f.probe_xy[2], &f.probe_xy[3],
                &f.probe_xy[4], &f.probe_xy[5], &f.probe_xy[6], &f.probe_xy[7],
                &f.gpu_wait, &f.color_refresh) != 24 ||
             f.mode < 0 || f.mode > 2)
         {
            Fail("bad line: %s", line);
         }
         if (static_cast<unsigned long long>(hang_at) == f.n)
         {
            Sleep(INFINITE); // test: a hung presenter (the add-on's watchdog gives up)
         }
         // test: a presenter that dies (a thread cannot be killed from outside).
         // ponytail: env read per frame, test-only cost ~1 us
         if (GetEnvironmentVariableA("D9P_FAIL_NOW", nullptr, 0))
         {
            Fail("D9P_FAIL_NOW at frame %llu", f.n);
         }
         presenter.Present(f);
         // "out" n: this frame's reads of the textures are done on the GPU; the D3D9
         // queue waits for it before writing them again.
         if (presenter.fence_out)
         {
            Check(presenter.queue->Signal(presenter.fence_out.Get(), f.n), "Signal(fence out)");
         }
         InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&stats->frames), static_cast<LONG64>(f.n));
      }
      presenter.DestroySwapchain();
      PLog("d9present: pipe closed, exiting\n");
      return 0;
   }

   // Thread body: `pipe` = read end of the add-on's line pipe (closed here).
   // `log` (may be null) receives the d9present lines; the caller owns it.
   inline DWORD PresenterThread(void* pipe, std::span<const std::byte> compose_vs, std::span<const std::byte> compose_ps, FILE* log)
   {
      g_log = log;
      const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(pipe), _O_RDONLY);
      FILE* in = fd >= 0 ? _fdopen(fd, "r") : nullptr;
      if (!in)
      {
         CloseHandle(pipe);
         return 1;
      }
      DWORD code = 1;
      try
      {
         code = RunPresenter(in, compose_vs, compose_ps);
      }
      catch (const PresenterFailure&)
      {
      }
      fclose(in); // also closes the pipe handle
      if (stats)
      {
         UnmapViewOfFile(stats);
         stats = nullptr;
      }
      return code;
   }
} // namespace D3D9Presenter
