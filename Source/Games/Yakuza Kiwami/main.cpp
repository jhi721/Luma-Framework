#define GAME_YAKUZA_KIWAMI 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

// SMAA replaces the game's FXAA (see "OnDrawOrDispatch()")
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

#include "..\..\Core\core.hpp"

// RGG "Old Engine" (the Kiwami 1 / Yakuza 3-5 family), D3D12 only (DXIL SM 6.0). See NOTES.md.
// The materials tonemap inline (Hable, then a gamma 2.0 encoding) into an 8-bit scene: their replacements continue the curve
// unbounded in HDR, every scene-sized target is upgraded to fp16 (placed textures get fp16 clones), and the "color correct"
// replacements (Shaders/Yakuza Kiwami/Includes/ColorCorrect.hlsl) map the scene to the display before FXAA, the HUD and render_fb.

namespace
{
   // ps_down_sample_2x4 (the glow source) and ps_down_sample_4x4 (the auto exposure meter source) read the fp16 scene into a target
   // that stays 8-bit: their replacements read it as vanilla stored it (see "YKR_SampleVanillaScene"), on the draws flagged here
   const ShaderHashesList shader_hashes_vanilla_scene_sources = {.pixel_shaders = {0xEDB2FBDE, 0x53B63FF4}};

   // SMAA in place of FXAA. D3D12 release builds don't know the resources behind the SRVs, so the inputs are taken where they are
   // written: the scene depth is the depth target of the depth + normals prepass (ps_normal, ps_normal_pt, ps_normal_tree, _pt),
   // the canvas FXAA reads is the target of ps_resolve_alphaluma (the ccr output with luma in alpha, for FXAA).
   const ShaderHashesList shader_hashes_depth_prepass = {.pixel_shaders = {0x3042199D, 0x4FA302B9, 0x4F16A152, 0xD9458156}};
   const ShaderHashesList shader_hashes_fxaa_input = {.pixel_shaders = {0xBF71EF76}};
   // fx_ssao_test, fx_ssao: their t0 is the scene depth, complete there and in a state pixel shaders read (the game clears it before
   // the post chain), so the predication draws from it at that point
   const ShaderHashesList shader_hashes_ssao = {.pixel_shaders = {0x5F373FFF, 0x83F4F60B}};
   // fx_fxaa311 q-low, q-medium, q-high, q-ultra. The "linear" permutations read a linear canvas: they stay
   const ShaderHashesList shader_hashes_fxaa = {.pixel_shaders = {0x47272CCB, 0xF1B4271C, 0xE4123563, 0x03933167}};

   // The color correct (ccr) permutations: every ps_ccr_* (but ps_texture_a255) and the 3 fx_ccr_*_mask. Their t0 is the fp16 scene, rt0 of
   // the draw right before them on their list (the glow composite, or a late particle): DLAA runs on it there, before the grade.
   const ShaderHashesList shader_hashes_color_correct = {.pixel_shaders = {0x6D1C9FA9, 0x58F2C4F0, 0x9DAF7DB7, 0x9ECD97D6, 0x61966971, 0xB3FB5120, 0x17B5EBE2, 0x06317C91, 0xB65DCB82, 0x610F5616, 0x5ED223C8, 0x1CA987C6, 0xFC89801D, 0xEE029834, 0x40982C7F,
                                                            0xCB74D2A0, 0x4E638B19, 0x6A3DAC9E, 0xB82F56C5, 0x56F12D55, 0x5D77D604, 0xA78E78A7, 0xF0C9699F, 0xD7FA738E, 0x19119791, 0x6BCBF281, 0x20579838, 0xC29C4AB0, 0xCD437C1A, 0x205D75F8,
                                                            0x064069A9, 0xEEDDE2B4, 0xC198424E, 0x5EF619B4, 0xB6C95B59, 0x89305791, 0xBF672B8C, 0x5FAFE955, 0xB2A5A291, 0x58953ECA, 0x996843BB, 0x6AB907C7, 0xB0EFB0C5, 0x58514F0F}};

   bool g_smaa_enable = true;
   // Predication only relaxes the edge threshold back to ULTRA's on geometry, never below: off is strictly worse (MoH 2010, ME1)
   bool g_smaa_predication = true;

   // The engine's projection setter "FUN_140babb00(context, const float4x4* projection)" (see NOTES.md "Exe RE"): it copies the matrix (row
   // vectors, clip w = -view z) into the context's cb12 mirror, for every pass, on several threads. The main camera's passes pass one of
   // the global camera slots, the others (shadows, cubemaps, icons, UI) their own matrices. Steam build only (exe TimeDateStamp below).
   constexpr DWORD game_time_date_stamp = 0x696F498D;
   constexpr uintptr_t set_projection_rva = 0xBABB00;
   // vmovups xmm1, [rdx]; mov rcx, [rcx+0x12C98]; vmovups [rcx+0x3A0], xmm1; vmovups xmm5, [rdx+0x10] (unique)
   constexpr uint8_t set_projection_prologue[] = {0xC5, 0xF8, 0x10, 0x0A, 0x48, 0x8B, 0x89, 0x98, 0x2C, 0x01, 0x00, 0xC5, 0xF8, 0x11, 0x89, 0xA0, 0x03, 0x00, 0x00, 0xC5, 0xF8, 0x10, 0x6A, 0x10};
   // MinHook can't hook it: its disassembler stops at the VEX encoded instructions around the "mov rcx". So the 7 byte "mov rcx" at +4
   // becomes "jmp rel32" (+ 2 nops) to a relay near the exe that jumps to the detour, written with the 16 aligned bytes around it in one
   // atomic operation (a thread runs either the old or the new code). The original runs from a stub: the first 11 bytes (no RIP relative
   // operand), then a jump to +11.
   constexpr uintptr_t set_projection_patch_offset = 4;
   constexpr size_t set_projection_stub_bytes = 11;
   constexpr uint8_t jmp_absolute[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}; // jmp [rip+0], then the address
   // The 4 global camera slots' views, then their projections (a matrix each). Slot 1 is the planar reflection's, which gets no jitter.
   constexpr uintptr_t camera_views_rva = 0x15CBAF0;
   constexpr uintptr_t camera_projections_rva = 0x15CBBF0;
   constexpr uintptr_t camera_matrix_size = 16 * sizeof(float);

   using SetProjection = void (*)(void* context, const float* projection);
   SetProjection set_projection_original = nullptr; // Null without the hook (unknown build): no DLAA
   uintptr_t game_module = 0;
   bool projection_hook_attempted = false; // At the first present

   // The replay thread's constant mirror (see "ReadDrawAnchor()"), from "FUN_1400ee460" and the pre-draw commit "FUN_140ba5b70": the thread finds
   // its D3D12 context through the exe's TLS, "ctx = *(*(game_module + render_globals_rva) + thread_contexts_offset + thread_number * 8)", the
   // engine's fallback context "*(... + fallback_context_offset)" if that is null. The mirror "*(ctx + context_mirror_offset)" holds the
   // cbuffers the commit flushed right before the draw, cb10 (id 0x0a: bones "c_matTBL", the rigid "c_matW"; 3 float4 rows per bone) at
   // "mirror + mirror_data_offset + cb10_base_offset"
   constexpr uintptr_t render_globals_rva = 0x1D39AA0; // A pointer to the render globals
   constexpr uintptr_t thread_contexts_offset = 0x38A228;
   constexpr uintptr_t fallback_context_offset = 0xC0;
   constexpr uintptr_t context_mirror_offset = 0x12C98;
   constexpr uintptr_t mirror_data_offset = 0x420;
   constexpr uintptr_t cb10_base_offset = 0x250;
   constexpr uintptr_t tls_thread_number_offset = 0x80; // In the exe's TLS block, 0 until the engine numbers the thread
   constexpr uintptr_t tls_initialized_offset = 0xA8;   // The MSVC on-demand TLS init flag
   constexpr uint32_t tls_index_unknown = UINT32_MAX;
   uint32_t exe_tls_index = tls_index_unknown; // "_tls_index" of the exe, set with "game_module"

   // The vertex shaders of the depth prepass draws with a bone 0: the characters' (ps_normal_mask_n*), the skinned ps_normal and the hair
   const ShaderHashesList shader_hashes_character_prepass = {.vertex_shaders = {0x026FFF36, 0x6AE49DE2, 0xEEFE68EE, 0xDBB9339C, 0x6A6B42A3}};
   // The engine's root signature (FUN_140ba26f0): the VS' CBV b0-b13 + SRV t0-t23 table is root parameter 2, so b10 is its descriptor 10
   constexpr uint32_t vs_descriptor_table_root_parameter = 2;
   constexpr uint32_t bone_cbv_binding = 10;

   // Whether DLAA runs and the jitter of the main camera's projections (0 in a frame after one DLAA didn't draw), latched at present for the
   // next frame, and the main camera as its passes set it
   struct CameraState
   {
      std::shared_mutex mutex;
      bool dlaa_active = false;
      std::array<float, 2> jitter_pixels = {}; // As applied: +y down
      std::array<float, 2> jitter_ndc = {};
      // The last unjittered view and projection of the slot DLAA reprojects with ("dlaa_camera_slot"), and the jitter they got
      std::array<float, 16> view = {};
      std::array<float, 16> projection = {};
      std::array<float, 2> applied_jitter_pixels = {};
      std::array<float, 2> applied_jitter_ndc = {};
      bool captured = false;
   };
   CameraState camera_state;

   // The scene (rt0) of this thread's last draw and its state since, from the game's barriers (see "OnBarrier()"): what a ccr draw reads
   struct SceneWriter
   {
      uint64_t resource = 0;
      reshade::api::resource_view_desc desc = {};
      reshade::api::resource_usage state = reshade::api::resource_usage::render_target;
      uint64_t present_count = UINT64_MAX; // Of that draw: one from an older frame may be of a texture the game destroyed since
   };
   thread_local SceneWriter scene_writer;

   // DLAA writes the scene's color back, its alpha stays (some ccr permutations read it)
   constexpr D3D12_RENDER_TARGET_BLEND_DESC rgb_only_blend = {.SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE, .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = UINT8(D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE)};

   PUBLISHING_CONSTEXPR int g_dlaa_camera_slot = 0;
   // How far DLAA goes, to isolate a pass: -1 nothing (the jitter hook and the mip bias only), 0 the depth copy and the motion vectors, 1 the
   // decoded scene too, 2 DLSS too (its output not written back), 3 all
   PUBLISHING_CONSTEXPR int g_dlaa_stages = 3;
   PUBLISHING_CONSTEXPR bool g_dlaa_mip_bias = true;
#if DEVELOPMENT
   int g_dlaa_jitter_flip = 0; // Bit 0: x, bit 1: y, of the jitter given to DLSS
   int g_dlaa_motion_flip = 0; // Bit 0: x, bit 1: y, of the motion vectors
   // The projection setter's calls by the slot passed (4: any other matrix): being counted, and the last frame's
   std::array<uint32_t, 5> projection_sets = {};
   std::array<uint32_t, 5> projection_sets_frame = {};
   uint32_t dlaa_scene_state = 0; // The scene's "reshade::api::resource_usage" at the last DLAA draw
   // The draw anchor's reads (see "ReadDrawAnchor()") by outcome, being counted, and the last frame's: read, unavailable, compared with the
   // bound CBV's bone 0 ("checked"), and different from it. "anchor_mismatch_total" counts them all along
   enum AnchorCount : size_t
   {
      ANCHOR_READS,
      ANCHOR_UNAVAILABLE,
      ANCHOR_CHECKED,
      ANCHOR_MISMATCH,
      ANCHOR_COUNT,
   };
   std::array<uint32_t, ANCHOR_COUNT> anchor_counts = {};
   std::array<uint32_t, ANCHOR_COUNT> anchor_counts_frame = {};
   uint32_t anchor_mismatch_total = 0;
   std::atomic_flag anchor_mismatch_logged;
#endif

   void SetProjectionDetour(void* context, const float* projection)
   {
      const uintptr_t offset = reinterpret_cast<uintptr_t>(projection) - (game_module + camera_projections_rva);
      const uint32_t slot = (offset % camera_matrix_size == 0 && offset / camera_matrix_size < 4) ? uint32_t(offset / camera_matrix_size) : 4;
#if DEVELOPMENT
      std::atomic_ref(projection_sets[slot]).fetch_add(1, std::memory_order_relaxed);
#endif
      if (slot == 1 || slot == 4)
      {
         set_projection_original(context, projection);
         return;
      }
      std::array<float, 16> jittered;
      std::copy_n(projection, 16, jittered.data());
      bool dlaa_active = false;
      {
         const std::unique_lock lock(camera_state.mutex);
         dlaa_active = camera_state.dlaa_active;
         if (dlaa_active && slot == uint32_t(g_dlaa_camera_slot))
         {
            std::copy_n(reinterpret_cast<const float*>(game_module + camera_views_rva + slot * camera_matrix_size), 16, camera_state.view.data());
            camera_state.projection = jittered;
            camera_state.applied_jitter_pixels = camera_state.jitter_pixels;
            camera_state.applied_jitter_ndc = camera_state.jitter_ndc;
            camera_state.captured = true;
         }
         // NDC x = clip x / -view z, so _31 and _32 (view z's) move it by minus themselves
         jittered[8] -= camera_state.jitter_ndc[0];
         jittered[9] -= camera_state.jitter_ndc[1];
      }
      set_projection_original(context, dlaa_active ? jittered.data() : projection);
   }

   // Writes the 16 "bytes" over the 16 aligned bytes of code at "code" in one atomic operation, if they still are the 16 "expected"
   bool WriteCodeAtomically(uint8_t* code, const uint8_t* expected, const uint8_t* bytes)
   {
      DWORD old_protection = 0;
      if (!VirtualProtect(code, 16, PAGE_EXECUTE_READWRITE, &old_protection))
      {
         return false;
      }
      alignas(16) long long comparand[2];
      alignas(16) long long exchange[2];
      std::memcpy(comparand, expected, 16);
      std::memcpy(exchange, bytes, 16);
      const bool written = _InterlockedCompareExchange128(reinterpret_cast<volatile long long*>(code), exchange[1], exchange[0], comparand) != 0;
      VirtualProtect(code, 16, old_protection, &old_protection);
      FlushInstructionCache(GetCurrentProcess(), code, 16);
      return written;
   }

   void InstallProjectionHook()
   {
      auto* const module = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
      const auto* const nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module + reinterpret_cast<const IMAGE_DOS_HEADER*>(module)->e_lfanew);
      uint8_t* const set_projection = module + set_projection_rva;
      if (nt_headers->FileHeader.TimeDateStamp != game_time_date_stamp || std::memcmp(set_projection, set_projection_prologue, sizeof(set_projection_prologue)) != 0)
      {
         reshade::log::message(reshade::log::level::warning, "[DLAA] Unknown game build (its projection code isn't where expected), so DLAA is unavailable");
         return;
      }
      game_module = uintptr_t(module);
      // The TLS directory's index slot is a VA of the loaded image, the loader numbered it already
      if (const DWORD tls_directory_rva = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress; tls_directory_rva != 0)
      {
         const auto* const tls_directory = reinterpret_cast<const IMAGE_TLS_DIRECTORY64*>(module + tls_directory_rva);
         exe_tls_index = *reinterpret_cast<const DWORD*>(tls_directory->AddressOfIndex);
      }

      // The relay (at 0) and the stub (at 16), within a rel32 jump of the exe
      SYSTEM_INFO system_info = {};
      GetSystemInfo(&system_info);
      const uintptr_t granularity = system_info.dwAllocationGranularity;
      const uintptr_t origin = uintptr_t(set_projection) & ~(granularity - 1);
      uint8_t* code = nullptr;
      for (uintptr_t distance = granularity; !code && distance < 0x7FF00000; distance += granularity)
      {
         code = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(origin - distance), 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
         if (!code)
         {
            code = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(origin + distance), 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
         }
      }
      if (!code)
      {
         reshade::log::message(reshade::log::level::warning, "[DLAA] No memory near the game's code for the projection hook, so DLAA is unavailable");
         return;
      }
      const void* const detour = reinterpret_cast<const void*>(&SetProjectionDetour);
      const uint8_t* const resume = set_projection + set_projection_stub_bytes;
      std::memcpy(code, jmp_absolute, sizeof(jmp_absolute));
      std::memcpy(code + sizeof(jmp_absolute), &detour, sizeof(detour));
      uint8_t* const stub = code + 16;
      std::memcpy(stub, set_projection_prologue, set_projection_stub_bytes);
      std::memcpy(stub + set_projection_stub_bytes, jmp_absolute, sizeof(jmp_absolute));
      std::memcpy(stub + set_projection_stub_bytes + sizeof(jmp_absolute), &resume, sizeof(resume));
      DWORD old_protection = 0;
      VirtualProtect(code, 64, PAGE_EXECUTE_READ, &old_protection);
      FlushInstructionCache(GetCurrentProcess(), code, 64);
      // Before the hook runs
      set_projection_original = reinterpret_cast<SetProjection>(stub);

      // jmp rel32 to the relay, then nops to the end of the "mov rcx" (never run: no thread can be inside it)
      uint8_t patched[16];
      std::copy_n(set_projection_prologue, 16, patched);
      const int32_t relay_offset = int32_t(intptr_t(code) - intptr_t(set_projection + set_projection_patch_offset + 5));
      patched[set_projection_patch_offset] = 0xE9;
      std::memcpy(patched + set_projection_patch_offset + 1, &relay_offset, sizeof(relay_offset));
      patched[set_projection_patch_offset + 5] = 0x90;
      patched[set_projection_patch_offset + 6] = 0x90;
      if (!WriteCodeAtomically(set_projection, set_projection_prologue, patched))
      {
         set_projection_original = nullptr;
         reshade::log::message(reshade::log::level::warning, "[DLAA] Couldn't write the projection hook, so DLAA is unavailable");
      }
   }

   // The world position of bone 0 (the translations of its 3x4 row-major matrix: floats 3, 7, 11) of the draw being recorded on this thread, read
   // from the engine's constant mirror (see "render_globals_rva"). The engine flushes the mirror right before "DrawIndexedInstanced()" on the
   // thread that replays the draw, so from ReShade's draw event it holds this draw's cb10. False without that: unknown build, a thread the
   // engine hasn't numbered (its allocator is never called from here), or no context. Plain reads: no lock, no allocation.
   bool ReadDrawAnchor(std::array<float, 3>* anchor)
   {
      if (exe_tls_index == tls_index_unknown)
      {
         return false;
      }
      const auto* const tls_blocks = reinterpret_cast<const uint8_t* const*>(__readgsqword(0x58)); // "ThreadLocalStoragePointer"
      const uint8_t* const tls_block = (tls_blocks ? tls_blocks[exe_tls_index] : nullptr);
      if (!tls_block || tls_block[tls_initialized_offset] == 0)
      {
         return false;
      }
      const uint32_t thread_number = *reinterpret_cast<const uint32_t*>(tls_block + tls_thread_number_offset);
      const uint8_t* const render_globals = *reinterpret_cast<const uint8_t* const*>(game_module + render_globals_rva);
      if (thread_number == 0 || !render_globals)
      {
         return false;
      }
      const uint8_t* context = *reinterpret_cast<const uint8_t* const*>(render_globals + thread_contexts_offset + thread_number * sizeof(void*));
      if (!context)
      {
         context = *reinterpret_cast<const uint8_t* const*>(render_globals + fallback_context_offset);
      }
      const uint8_t* const mirror = (context ? *reinterpret_cast<const uint8_t* const*>(context + context_mirror_offset) : nullptr);
      if (!mirror)
      {
         return false;
      }
      const auto* const bone_0 = reinterpret_cast<const float*>(mirror + mirror_data_offset + cb10_base_offset);
      *anchor = {bone_0[3], bone_0[7], bone_0[11]};
      return true;
   }

#if DEVELOPMENT
   // DRED (auto breadcrumbs and page faults), enabled before the game's device is made (ReShade's "create_device" comes before it): when the
   // GPU hangs, "LogDeviceRemoval()" logs what the unfinished command lists were doing and the faulting address
   bool EnableDred(reshade::api::device_api api, uint32_t& api_version)
   {
      const HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
      const auto get_debug_interface = (d3d12 ? reinterpret_cast<PFN_D3D12_GET_DEBUG_INTERFACE>(GetProcAddress(d3d12, "D3D12GetDebugInterface")) : nullptr);
      com_ptr<ID3D12DeviceRemovedExtendedDataSettings> settings;
      if (api == reshade::api::device_api::d3d12 && get_debug_interface && SUCCEEDED(get_debug_interface(IID_PPV_ARGS(&settings))))
      {
         settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
         settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
         reshade::log::message(reshade::log::level::info, "[DLAA] DRED enabled");
      }
      return false;
   }

   std::atomic_flag device_removal_logged;
   void LogDeviceRemoval(ID3D12Device* device)
   {
      if (SUCCEEDED(device->GetDeviceRemovedReason()) || device_removal_logged.test_and_set())
      {
         return;
      }
      const auto narrow = [](const wchar_t* text)
      {
         std::string result;
         for (; text && *text; text++)
         {
            result += (*text < 128 ? char(*text) : '?');
         }
         return result;
      };
      const auto log = [](const std::string& line)
      {
         reshade::log::message(reshade::log::level::error, ("[DRED] " + line).c_str());
      };
      log(std::format("Device removed: 0x{:08X}", uint32_t(device->GetDeviceRemovedReason())));
      com_ptr<ID3D12DeviceRemovedExtendedData1> dred;
      if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
      {
         log("No DRED interface");
         return;
      }
      D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
      if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)))
      {
         for (const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
         {
            const UINT done = (node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0);
            if (done >= node->BreadcrumbCount)
            {
               continue;
            }
            // D3D12_AUTO_BREADCRUMB_OP values, the first one not done in brackets
            std::string ops;
            for (UINT i = (done > 12 ? done - 12 : 0); i < (std::min)(node->BreadcrumbCount, done + 6); i++)
            {
               ops += (i == done ? std::format("[{}] ", int(node->pCommandHistory[i])) : std::format("{} ", int(node->pCommandHistory[i])));
            }
            log(std::format("List \"{}\" {} on queue \"{}\" {}: {} of {} done, ops {}", narrow(node->pCommandListDebugNameW), static_cast<const void*>(node->pCommandList), narrow(node->pCommandQueueDebugNameW), static_cast<const void*>(node->pCommandQueue), done, node->BreadcrumbCount, ops));
            for (UINT i = 0; i < node->BreadcrumbContextsCount; i++)
            {
               log(std::format("   context at {}: {}", node->pBreadcrumbContexts[i].BreadcrumbIndex, narrow(node->pBreadcrumbContexts[i].pContextString)));
            }
         }
      }
      D3D12_DRED_PAGE_FAULT_OUTPUT1 page_fault = {};
      if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&page_fault)))
      {
         log(std::format("Page fault VA 0x{:X}", page_fault.PageFaultVA));
         for (const D3D12_DRED_ALLOCATION_NODE1* node = page_fault.pHeadExistingAllocationNode; node; node = node->pNext)
         {
            log(std::format("   existing allocation \"{}\" type {}", narrow(node->ObjectNameW), int(node->AllocationType)));
         }
         for (const D3D12_DRED_ALLOCATION_NODE1* node = page_fault.pHeadRecentFreedAllocationNode; node; node = node->pNext)
         {
            log(std::format("   recently freed \"{}\" type {}", narrow(node->ObjectNameW), int(node->AllocationType)));
         }
      }
   }
#endif

   void OnBarrier(reshade::api::command_list* cmd_list, uint32_t count, const reshade::api::resource* resources, const reshade::api::resource_usage* old_states, const reshade::api::resource_usage* new_states)
   {
      if (scene_writer.resource == 0)
      {
         return;
      }
      for (uint32_t i = 0; i < count; i++)
      {
         if (resources[i].handle == scene_writer.resource)
         {
            scene_writer.state = new_states[i];
         }
      }
   }
} // namespace

struct YakuzaKiwamiDeviceData final : public GameDeviceData
{
   // The SMAA predication pass ("Luma_YKR_SMAAPredication") and its R8 target, at the depth's size (made again with it). Its mutex also
   // guards the target, which is in a shader resource state between draws
   DeviceData::GamePassD3D12 predication_pass;
   reshade::api::resource predication = {};
   uint64_t predication_present = UINT64_MAX; // The present count of its last draw: it only counts in that frame
   // Recorded on other threads than the post chain (see "shader_hashes_depth_prepass")
   std::atomic<uint64_t> scene_depth = 0;
   std::atomic<reshade::api::format> scene_depth_format = reshade::api::format::unknown;
   std::atomic<uint64_t> fxaa_input = 0;
   std::atomic<reshade::api::format> fxaa_input_format = reshade::api::format::unknown;

   // DLAA (see "DrawDLAA()"). Each pass' mutex also guards its targets, kept in "state" between draws: the copy pass the depth copy (R32,
   // a shader resource; it also stands in for FXAA), the motion vector pass the motion vectors (RG16F, a shader resource), the color pass
   // the decoded scene (RGBA16F, a shader resource) and DLSS' output (RGBA16F, unordered access)
   std::atomic<bool> dlaa_active = false; // Latched at present
   DeviceData::GamePassD3D12 copy_pass;
   DeviceData::GamePassD3D12 motion_vector_pass;
   DeviceData::GamePassD3D12 color_pass;
   reshade::api::resource depth_copy = {};
   uint64_t depth_copy_present = UINT64_MAX; // The present count of its last copy: it only counts in that frame
   reshade::api::resource motion_vectors = {};
   reshade::api::resource decoded_scene = {};
   reshade::api::resource dlaa_output = {};
   bool dlaa_output_recreated = false;    // Until a DLSS draw into it succeeds
   std::atomic<uint32_t> depth_width = 0; // For the jitter in NDC
   std::atomic<uint32_t> depth_height = 0;
   std::atomic<uint64_t> dlaa_claimed_present = UINT64_MAX; // DLAA draws at the first ccr draw of a frame
   std::atomic<uint64_t> dlaa_drawn_present = UINT64_MAX;   // FXAA and SMAA step aside in that frame
   // The main camera at the last DLAA draw, column vectors (only "DrawDLAA()" touches it, once a frame)
   Math::Matrix44D previous_view_projection;
   uint64_t previous_view_projection_present = UINT64_MAX;

#if DEVELOPMENT
   // The engine's constant ring "<pxd:cb_pool>" (one persistently mapped upload buffer every cbuffer CBV points into), mapped on the first
   // CBV found in it. Its mutex guards the fields, placed first
   struct
   {
      std::shared_mutex mutex;
      uint64_t resource = 0;
      const uint8_t* mapped = nullptr;
      uint64_t size = 0;
   } constant_ring;

   // Bone 0's translations as the draw's bound VS CBV b10 holds them (the descriptors Core shadows -> the ring's CPU pointer): false if
   // any of that isn't known. Mapping an upload buffer again returns the pointer it was mapped at already.
   bool ReadBoundBoneAnchor(const D3D12State::CommandListState& state, DeviceData& device_data, std::array<float, 3>* anchor)
   {
      if (state.graphics.bindings.size() <= vs_descriptor_table_root_parameter)
      {
         return false;
      }
      const reshade::api::descriptor_table table = state.graphics.bindings[vs_descriptor_table_root_parameter].table;
      if (table.handle == 0)
      {
         return false;
      }
      uint64_t buffer = 0;
      uint64_t offset = 0;
      {
         const std::shared_lock lock(device_data.descriptor_heaps.mutex);
         const D3D12State::Descriptor* const descriptor = D3D12State::FindDescriptor(device_data.descriptor_heaps, device_data.reshade_device, table, bone_cbv_binding);
         if (!descriptor || descriptor->type != reshade::api::descriptor_type::constant_buffer)
         {
            return false;
         }
         buffer = descriptor->handle;
         offset = descriptor->offset;
      }
      if (buffer == 0)
      {
         return false;
      }
      constexpr uint64_t bone_0_size = 12 * sizeof(float);
      const uint8_t* mapped = nullptr;
      {
         const std::shared_lock lock(constant_ring.mutex);
         if (constant_ring.resource == buffer && offset + bone_0_size <= constant_ring.size)
         {
            mapped = constant_ring.mapped;
         }
      }
      if (!mapped)
      {
         auto* const resource = reinterpret_cast<ID3D12Resource*>(buffer);
         D3D12_HEAP_PROPERTIES heap = {};
         resource->GetHeapProperties(&heap, nullptr);
         const D3D12_RESOURCE_DESC desc = resource->GetDesc();
         void* map = nullptr;
         // No read range: the CPU reads it
         if (heap.Type != D3D12_HEAP_TYPE_UPLOAD || desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || offset + bone_0_size > desc.Width || FAILED(resource->Map(0, nullptr, &map)))
         {
            return false;
         }
         const std::unique_lock lock(constant_ring.mutex);
         constant_ring.resource = buffer;
         constant_ring.mapped = static_cast<const uint8_t*>(map);
         constant_ring.size = desc.Width;
         mapped = constant_ring.mapped;
      }
      const auto* const bone_0 = reinterpret_cast<const float*>(mapped + offset);
      *anchor = {bone_0[3], bone_0[7], bone_0[11]};
      return true;
   }
#endif

   // "*target" (a texture of "pass", in "state" between draws) at "width" x "height", made again (the old one retired) at another size. With
   // "pass->mutex" held. "*created": whether it was made now
   static bool EnsurePassTarget(DeviceData::GamePassD3D12* pass, reshade::api::resource* target, reshade::api::device* device, uint64_t present_count, uint32_t width, uint32_t height, reshade::api::format format, reshade::api::resource_usage usage, reshade::api::resource_usage state, const wchar_t* name, bool* created = nullptr)
   {
      if (target->handle != 0)
      {
         const reshade::api::resource_desc desc = device->get_resource_desc(*target);
         if (desc.texture.width == width && desc.texture.height == height)
         {
            return true;
         }
         RetireD3D12GamePassResource(pass, present_count, target);
      }
      const reshade::api::resource_desc desc(width, height, 1, 1, format, 1, reshade::api::memory_heap::gpu_only, usage);
      // Destroyed with the device otherwise
      const bool made = device->create_resource(desc, nullptr, state, target);
      if (made)
      {
         reinterpret_cast<ID3D12Resource*>(target->handle)->SetName(name);
      }
      if (created)
      {
         *created = made;
      }
      return made;
   }

   // The predication target at "width" x "height". With "predication_pass.mutex" held
   bool EnsurePredicationTarget(reshade::api::device* device, uint64_t present_count, uint32_t width, uint32_t height)
   {
      return EnsurePassTarget(&predication_pass, &predication, device, present_count, width, height, reshade::api::format::r8_unorm, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR SMAA Predication");
   }
};

class YakuzaKiwami final : public Game
{
public:
   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Gamma-space post processing (the vanilla chain and the HUD are 2.2 gamma, UNORM, no sRGB views), 1.0 = paper white.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The HUD is drawn straight onto the post-FXAA scene: the grade pre-scales the scene by game/UI paper white.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // The game binds b1-b3, b5-b10, b12 and b13. Luma's cbuffers live in their own register space on D3D12 (SM 6), so these
      // registers can't collide with the game's; b11 and b13 keep them out of the way of SM 5.0 tools too.
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 11;

      // Root parameters of the object motion vectors (design B, "Object motion vectors" in NOTES.md), appended after Luma's CBVs to every
      // root signature: the per-frame camera CBV, the per-draw constants, the current position cache (raw UAV) and the previous one (raw SRV).
      // Nothing binds them yet. Core reads this once at device init, which comes after OnInit.
      d3d12_game_root_parameters = {
         {.type = D3D12_ROOT_PARAMETER_TYPE_CBV, .shader_register = 9},
         {.type = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, .shader_register = 8, .constant_count = 4},
         {.type = D3D12_ROOT_PARAMETER_TYPE_UAV, .shader_register = 0},
         {.type = D3D12_ROOT_PARAMETER_TYPE_SRV, .shader_register = 0},
      };

      default_luma_global_game_settings.VideoAutoHDREnable = cb_luma_global_settings.GameSettings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = cb_luma_global_settings.GameSettings.VideoAutoHDRBoost = 0.5f; // peak ~165 nits
      default_luma_global_game_settings.Dithering = cb_luma_global_settings.GameSettings.Dithering = 1.f;
      default_luma_global_game_settings.Exposure = cb_luma_global_settings.GameSettings.Exposure = 1.f;

      native_shaders_definitions.emplace(CompileTimeStringHash("YKR SMAA Predication PS"), ShaderDefinition{"Luma_YKR_SMAAPredication", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Copy PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "copy_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Motion Vectors PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "motion_vectors_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Decode PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "decode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("YKR DLAA Encode PS"), ShaderDefinition{"Luma_YKR_DLAA", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "encode_ps"});

      sr_game_tooltip = "DLAA only (no upscaling), with SSAO on in the game's graphics settings. Motion comes from the camera alone for now, so moving characters can leave trails.\n";

      reshade::register_event<reshade::addon_event::barrier>(OnBarrier);
#if DEVELOPMENT
      reshade::register_event<reshade::addon_event::create_device>(EnableDred);
#endif

#if DEVELOPMENT
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_predication", &g_smaa_predication}, { "dlaa_mip_bias",
                               &g_dlaa_mip_bias }});
      Mcp::RegisterInts({{"dlaa_camera_slot", &g_dlaa_camera_slot, 0, 3}, {"dlaa_jitter_flip", &g_dlaa_jitter_flip, 0, 3}, {"dlaa_motion_flip", &g_dlaa_motion_flip, 0, 3}, {"dlaa_stages", &g_dlaa_stages, -1, 3}});
      Mcp::RegisterCounters({
         {"projection_sets.slot0", &projection_sets_frame[0]},
         {"projection_sets.slot1", &projection_sets_frame[1]},
         {"projection_sets.slot2", &projection_sets_frame[2]},
         {"projection_sets.slot3", &projection_sets_frame[3]},
         {"projection_sets.other", &projection_sets_frame[4]},
         {"dlaa.scene_state", &dlaa_scene_state},
         {"anchor.reads", &anchor_counts_frame[ANCHOR_READS]},
         {"anchor.unavailable", &anchor_counts_frame[ANCHOR_UNAVAILABLE]},
         {"anchor.checked", &anchor_counts_frame[ANCHOR_CHECKED]},
         {"anchor.mismatch", &anchor_counts_frame[ANCHOR_MISMATCH]},
         {"anchor.mismatch_total", &anchor_mismatch_total},
      });
      Mcp::RegisterTextures({
         {"smaa.predication", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.predication_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.predication.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.depth", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.copy_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.depth_copy.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.motion_vectors", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.motion_vector_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.motion_vectors.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.decoded", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.color_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.decoded_scene.handle), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
            }},
         {"dlaa.output", [](DeviceData& device_data) -> Mcp::DevTexture
            {
               auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
               const std::shared_lock lock(game_device_data.color_pass.mutex);
               return {reinterpret_cast<ID3D12Resource*>(game_device_data.dlaa_output.handle), D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
            }},
      });
#endif
   }

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new YakuzaKiwamiDeviceData;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(NativeDevice* native_device, NativeCommandList* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const bool dlaa_active = game_device_data.dlaa_active.load(std::memory_order_relaxed);

      if (dlaa_active && g_dlaa_stages >= 0 && original_shader_hashes.Contains(shader_hashes_color_correct))
      {
         DrawDLAA(cmd_list_data, device_data, &game_device_data);
      }
      // What the next draw on this list may read as the scene
      if (dlaa_active && !state.render_target_views.empty() && !state.render_target_descs.empty())
      {
         scene_writer = {.resource = state.render_target_views[0].resource, .desc = state.render_target_descs[0], .state = reshade::api::resource_usage::render_target, .present_count = device_data.present_count.load(std::memory_order_relaxed)};
      }

      // Nothing uses the anchor yet. It reads the mirror even in release builds, to prove it there
      if (original_shader_hashes.Contains(shader_hashes_character_prepass))
      {
         [[maybe_unused]] std::array<float, 3> anchor = {};
         [[maybe_unused]] const bool anchor_read = ReadDrawAnchor(&anchor);
#if DEVELOPMENT
         // Against the bone 0 of the CBV the draw reads, which the CPU copy of the engine's mirror must have left exactly
         if (!anchor_read)
         {
            std::atomic_ref(anchor_counts[ANCHOR_UNAVAILABLE]).fetch_add(1, std::memory_order_relaxed);
         }
         else
         {
            std::atomic_ref(anchor_counts[ANCHOR_READS]).fetch_add(1, std::memory_order_relaxed);
            std::array<float, 3> bound_anchor = {};
            if (game_device_data.ReadBoundBoneAnchor(state, device_data, &bound_anchor))
            {
               std::atomic_ref(anchor_counts[ANCHOR_CHECKED]).fetch_add(1, std::memory_order_relaxed);
               if (std::memcmp(anchor.data(), bound_anchor.data(), sizeof(anchor)) != 0)
               {
                  std::atomic_ref(anchor_counts[ANCHOR_MISMATCH]).fetch_add(1, std::memory_order_relaxed);
                  std::atomic_ref(anchor_mismatch_total).fetch_add(1, std::memory_order_relaxed);
                  if (!anchor_mismatch_logged.test_and_set())
                  {
                     reshade::log::message(reshade::log::level::warning, std::format("[Anchor] First mismatch, VS 0x{:08X}: mirror ({}, {}, {}), bound CBV ({}, {}, {})", original_shader_hashes.vertex_shaders[0], anchor[0], anchor[1], anchor[2], bound_anchor[0], bound_anchor[1], bound_anchor[2]).c_str());
                  }
               }
            }
         }
#endif
      }

      if (original_shader_hashes.Contains(shader_hashes_depth_prepass))
      {
         game_device_data.scene_depth_format.store(state.depth_stencil_view.format, std::memory_order_relaxed);
         game_device_data.scene_depth.store(state.depth_stencil_view.resource, std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::None;
      }

      // Barriers are invalid in a native render pass (the Luma passes refuse to draw there too)
      const bool predicate = g_smaa_enable && g_smaa_predication;
      if ((predicate || dlaa_active) && !state.in_render_pass && original_shader_hashes.Contains(shader_hashes_ssao))
      {
         const reshade::api::resource depth = {game_device_data.scene_depth.load(std::memory_order_relaxed)};
         if (depth.handle == 0)
         {
            return DrawOrDispatchOverrideType::None;
         }
         reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
         const reshade::api::resource_desc depth_desc = device->get_resource_desc(depth);
         const uint32_t width = depth_desc.texture.width;
         const uint32_t height = depth_desc.texture.height;
         game_device_data.depth_width.store(width, std::memory_order_relaxed);
         game_device_data.depth_height.store(height, std::memory_order_relaxed);
         const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
         const reshade::api::format depth_format = reshade::api::format_to_default_typed(game_device_data.scene_depth_format.load(std::memory_order_relaxed));
         if (predicate)
         {
            std::unique_lock lock(game_device_data.predication_pass.mutex);
            if (game_device_data.EnsurePredicationTarget(device, present_count, width, height))
            {
               const reshade::api::resource target = game_device_data.predication;
               // "DrawFullscreenPassD3D12()" takes it. A target retired meanwhile lives on until the lists recorded with it ran.
               lock.unlock();
               const reshade::api::resource_view_desc target_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r8_unorm, 0, 1, 0, 1);
               // TW2's validated tolerance (2% of view depth), standard Z
               const float params[] = {0.02f, 0.f};
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
               const bool drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.predication_pass, CompileTimeStringHash("YKR SMAA Predication PS"), "SMAA Predication", "Luma SMAA Predication", depth, depth_format, target, target_desc, params);
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
               if (drawn)
               {
                  lock.lock();
                  game_device_data.predication_present = present_count;
               }
            }
         }
         if (dlaa_active && g_dlaa_stages >= 0)
         {
            std::unique_lock lock(game_device_data.copy_pass.mutex);
            if (game_device_data.EnsurePassTarget(&game_device_data.copy_pass, &game_device_data.depth_copy, device, present_count, width, height, reshade::api::format::r32_float, reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Depth"))
            {
               const reshade::api::resource target = game_device_data.depth_copy;
               lock.unlock();
               const reshade::api::resource_view_desc target_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r32_float, 0, 1, 0, 1);
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
               const bool drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), "DLAA Depth Copy", "Luma DLAA Depth Copy", depth, depth_format, target, target_desc, {});
               cmd_list_data.cmd_list->barrier(target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
               if (drawn)
               {
                  lock.lock();
                  game_device_data.depth_copy_present = present_count;
               }
            }
         }
         return DrawOrDispatchOverrideType::None;
      }

      if (original_shader_hashes.Contains(shader_hashes_fxaa_input) && !state.render_target_views.empty())
      {
         game_device_data.fxaa_input_format.store(state.render_target_views[0].format, std::memory_order_relaxed);
         game_device_data.fxaa_input.store(state.render_target_views[0].resource, std::memory_order_relaxed);
         return DrawOrDispatchOverrideType::None;
      }

      if (!state.in_render_pass && original_shader_hashes.Contains(shader_hashes_fxaa))
      {
         const reshade::api::resource color = {game_device_data.fxaa_input.load(std::memory_order_relaxed)};
         if (color.handle == 0 || state.render_target_views.empty() || state.render_target_descs.empty() || state.render_target_views[0].resource == 0 || state.render_target_views[0].resource == color.handle)
         {
            return DrawOrDispatchOverrideType::None;
         }
         const reshade::api::resource target = {state.render_target_views[0].resource};
         const reshade::api::format color_format = game_device_data.fxaa_input_format.load(std::memory_order_relaxed);
         const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
         // DLAA antialiased the scene: the canvas goes through as it is
         if (game_device_data.dlaa_drawn_present.load(std::memory_order_relaxed) == present_count)
         {
            const bool copied = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data.copy_pass, CompileTimeStringHash("YKR DLAA Copy PS"), "FXAA Bypass", "Luma FXAA Bypass", color, color_format, target, state.render_target_descs[0], {});
            return copied ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;
         }
         if (!g_smaa_enable)
         {
            return DrawOrDispatchOverrideType::None;
         }
         reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
         const reshade::api::resource_desc target_desc = device->get_resource_desc(target);
         reshade::api::resource predication = {};
         bool predicated = false;
         {
            const std::unique_lock lock(game_device_data.predication_pass.mutex);
            // The edge detection reads its t1 unpredicated too (at strength 0): without the SSAO draw, a target of the canvas' size is made for it
            if (game_device_data.predication.handle != 0 || game_device_data.EnsurePredicationTarget(device, present_count, target_desc.texture.width, target_desc.texture.height))
            {
               predication = game_device_data.predication;
               const reshade::api::resource_desc predication_desc = device->get_resource_desc(predication);
               predicated = g_smaa_predication && game_device_data.predication_present == present_count && predication_desc.texture.width == target_desc.texture.width && predication_desc.texture.height == target_desc.texture.height;
            }
         }
         if (predication.handle == 0)
         {
            return DrawOrDispatchOverrideType::None;
         }
         // "Luma_SMAA_impl.hlsl": the predication's scale and strength, plain ULTRA without it
         const float smaa_constants[4] = {predicated ? 2.f : 1.f, predicated ? 0.5f : 0.f, 0.f, 0.f};
         if (DrawSMAA(cmd_list_data, device_data, color, color_format, predication, reshade::api::format::r8_unorm, target, state.render_target_descs[0], smaa_constants))
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      if (is_custom_pass && original_shader_hashes.Contains(shader_hashes_vanilla_scene_sources))
      {
         const std::vector<D3D12State::ViewInfo>& render_targets = cmd_list_data.d3d12_state.render_target_views;
         const reshade::api::format target_format = (render_targets.empty() ? reshade::api::format::unknown : render_targets[0].format);
         const bool target_stays_8_bit = target_format == reshade::api::format::b8g8r8a8_unorm || target_format == reshade::api::format::r8g8b8a8_unorm;
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, target_stays_8_bit ? 1 : 0);
         updated_cbuffers = true;
      }
      return DrawOrDispatchOverrideType::None;
   }

   // DLAA of the scene a ccr draw reads (see "shader_hashes_color_correct"), recorded before that draw, at most once a frame: camera motion
   // vectors from the depth copy, the scene decoded into DLSS' input, DLSS, then its output encoded back into the scene in place. Without a
   // piece (no depth copy this frame, no camera, another size) the scene stays as it is and FXAA or SMAA run; a failed DLSS turns it off
   // until it's picked again.
   static void DrawDLAA(CommandListData& cmd_list_data, DeviceData& device_data, YakuzaKiwamiDeviceData* game_device_data)
   {
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      const SceneWriter scene = scene_writer;
      if (state.in_render_pass || !sr_instance_data || scene.resource == 0 || scene.present_count != present_count || state.render_target_views.empty() || state.render_target_views[0].resource == scene.resource || game_device_data->dlaa_claimed_present.exchange(present_count, std::memory_order_relaxed) == present_count)
      {
         return;
      }
      reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
      reshade::api::device* const device = cmd_list->get_device();
      const reshade::api::resource scene_resource = {scene.resource};
      const reshade::api::resource_desc scene_desc = device->get_resource_desc(scene_resource);
      const uint32_t width = scene_desc.texture.width;
      const uint32_t height = scene_desc.texture.height;

      reshade::api::resource depth = {};
      {
         const std::shared_lock lock(game_device_data->copy_pass.mutex);
         if (game_device_data->depth_copy_present == present_count)
         {
            depth = game_device_data->depth_copy;
         }
      }
      if (depth.handle == 0)
      {
         return;
      }
      // A retired copy lives on until the lists recorded with it ran
      const reshade::api::resource_desc depth_desc = device->get_resource_desc(depth);
      if (depth_desc.texture.width != width || depth_desc.texture.height != height)
      {
         return;
      }

      std::array<float, 16> view;
      std::array<float, 16> projection;
      std::array<float, 2> jitter_pixels;
      std::array<float, 2> jitter_ndc;
      {
         const std::shared_lock lock(camera_state.mutex);
         if (!camera_state.captured)
         {
            return;
         }
         view = camera_state.view;
         projection = camera_state.projection;
         jitter_pixels = camera_state.applied_jitter_pixels;
         jitter_ndc = camera_state.applied_jitter_ndc;
      }

      constexpr reshade::api::resource_usage render_target_and_shader_resource = reshade::api::resource_usage::render_target | reshade::api::resource_usage::shader_resource;
      reshade::api::resource motion_vectors = {};
      {
         const std::unique_lock lock(game_device_data->motion_vector_pass.mutex);
         if (game_device_data->EnsurePassTarget(&game_device_data->motion_vector_pass, &game_device_data->motion_vectors, device, present_count, width, height, reshade::api::format::r16g16_float, render_target_and_shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Motion Vectors"))
         {
            motion_vectors = game_device_data->motion_vectors;
         }
      }
      reshade::api::resource decoded_scene = {};
      reshade::api::resource output = {};
      bool output_recreated = false;
      {
         const std::unique_lock lock(game_device_data->color_pass.mutex);
         bool output_created = false;
         if (game_device_data->EnsurePassTarget(&game_device_data->color_pass, &game_device_data->decoded_scene, device, present_count, width, height, reshade::api::format::r16g16b16a16_float, render_target_and_shader_resource, reshade::api::resource_usage::shader_resource, L"Luma YKR DLAA Decoded Scene") &&
             game_device_data->EnsurePassTarget(&game_device_data->color_pass, &game_device_data->dlaa_output, device, present_count, width, height, reshade::api::format::r16g16b16a16_float, reshade::api::resource_usage::unordered_access | reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access, L"Luma YKR DLAA Output", &output_created))
         {
            decoded_scene = game_device_data->decoded_scene;
            output = game_device_data->dlaa_output;
            game_device_data->dlaa_output_recreated = game_device_data->dlaa_output_recreated || output_created;
            output_recreated = game_device_data->dlaa_output_recreated;
         }
      }
      if (motion_vectors.handle == 0 || output.handle == 0)
      {
         device_data.sr_suppressed = true;
         reshade::log::message(reshade::log::level::error, "[DLAA] ReShade refused its textures, so it's off until it's picked again");
         return;
      }

      // Current clip space to the previous frame's, column vectors (the game's matrices multiply row vectors), in double
      Math::Matrix44D current_view;
      Math::Matrix44D current_projection;
      std::copy_n(view.data(), 16, current_view.GetData());
      std::copy_n(projection.data(), 16, current_projection.GetData());
      Math::Matrix44D current = current_view * current_projection;
      current.Transpose();
      const bool has_previous = game_device_data->previous_view_projection_present != UINT64_MAX && game_device_data->previous_view_projection_present + 1 == present_count;
      const Math::Matrix44D previous = (has_previous ? game_device_data->previous_view_projection : current);
      game_device_data->previous_view_projection = current;
      game_device_data->previous_view_projection_present = present_count;
      const Math::Matrix44D reprojection = previous * current.GetInverted();
      // After the target's size: padding to the matrix' 16 byte row, the reprojection, the jitter (see "Luma_YKR_DLAA.hlsl")
      std::array<float, 20> motion_constants = {};
      std::transform(reprojection.GetData(), reprojection.GetData() + 16, motion_constants.begin() + 2, [](double value)
         { return float(value); });
      motion_constants[18] = jitter_ndc[0];
      motion_constants[19] = jitter_ndc[1];
      const reshade::api::resource_view_desc motion_vectors_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16_float, 0, 1, 0, 1);
      cmd_list->barrier(motion_vectors, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      const bool motion_vectors_drawn = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->motion_vector_pass, CompileTimeStringHash("YKR DLAA Motion Vectors PS"), "DLAA Motion Vectors", "Luma DLAA Motion Vectors", depth, reshade::api::format::r32_float, motion_vectors, motion_vectors_desc, motion_constants);
      cmd_list->barrier(motion_vectors, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
      if (!motion_vectors_drawn || g_dlaa_stages < 1)
      {
         return;
      }
#if DEVELOPMENT
      dlaa_scene_state = uint32_t(scene.state);
#endif

      // The scene is in the state the game's barriers left it in since its last draw, a pixel shader resource normally (ccr reads it)
      const bool scene_readable = (scene.state & reshade::api::resource_usage::shader_resource_pixel) == reshade::api::resource_usage::shader_resource_pixel;
      const reshade::api::resource_usage scene_read_state = (scene_readable ? scene.state : reshade::api::resource_usage::shader_resource_pixel);
      if (!scene_readable)
      {
         cmd_list->barrier(scene_resource, scene.state, scene_read_state);
      }
      const reshade::api::resource_view_desc color_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format::r16g16b16a16_float, 0, 1, 0, 1);
      cmd_list->barrier(decoded_scene, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);
      const bool decoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("YKR DLAA Decode PS"), "DLAA Decode", "Luma DLAA Decode", scene_resource, scene.desc.format, decoded_scene, color_desc, {});
      cmd_list->barrier(decoded_scene, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);

      bool dlss_drawn = false;
      if (decoded && g_dlaa_stages >= 2)
      {
#if DEVELOPMENT
         const float jitter_sign_x = (g_dlaa_jitter_flip & 1) ? -1.f : 1.f;
         const float jitter_sign_y = (g_dlaa_jitter_flip & 2) ? -1.f : 1.f;
         const float motion_sign_x = (g_dlaa_motion_flip & 1) ? -1.f : 1.f;
         const float motion_sign_y = (g_dlaa_motion_flip & 2) ? -1.f : 1.f;
#else
         constexpr float jitter_sign_x = 1.f;
         constexpr float jitter_sign_y = 1.f;
         constexpr float motion_sign_x = 1.f;
         constexpr float motion_sign_y = 1.f;
#endif
         // The inputs stay in a shader resource state (pixel and non pixel), which NGX reads; the output is in unordered access
         auto* const native_list = reinterpret_cast<ID3D12GraphicsCommandList*>(cmd_list->get_native());
         // UV deltas, previous minus current. Exposure 1: the scene is already exposed and tonemapped
         const SR::SettingsData settings_data = {.output_width = width, .output_height = height, .render_width = width, .render_height = height, .hdr = true, .inverted_depth = false, .mvs_jittered = false, .mvs_x_scale = float(width) * motion_sign_x, .mvs_y_scale = float(height) * motion_sign_y, .auto_exposure = false, .render_preset = dlss_render_preset};
         const SR::SuperResolutionImpl::DrawData draw_data = {
            .reset = device_data.force_reset_sr,
            .output_recreated = output_recreated,
            .output_color = reinterpret_cast<ID3D12Resource*>(output.handle),
            .source_color = reinterpret_cast<ID3D12Resource*>(decoded_scene.handle),
            .motion_vectors = reinterpret_cast<ID3D12Resource*>(motion_vectors.handle),
            .depth_buffer = reinterpret_cast<ID3D12Resource*>(depth.handle),
            .render_width = width,
            .render_height = height,
            .jitter_x = jitter_pixels[0] * jitter_sign_x,
            .jitter_y = jitter_pixels[1] * jitter_sign_y,
         };
         dlss_drawn = sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_list, settings_data) && sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_list, draw_data);
         // NGX binds its own descriptor heaps, root signature and pipeline
         D3D12State::RestoreCompute(cmd_list, state, {});
         RestoreGameGraphicsD3D12(cmd_list_data);
         TraceLumaPass(&cmd_list_data, device_data, "Luma DLAA", {{.resource = decoded_scene.handle, .format = reshade::api::format::r16g16b16a16_float}, {.resource = motion_vectors.handle, .format = reshade::api::format::r16g16_float}, {.resource = depth.handle, .format = reshade::api::format::r32_float}}, {{.resource = output.handle, .format = reshade::api::format::r16g16b16a16_float}}, true);
         if (dlss_drawn)
         {
            const std::unique_lock lock(game_device_data->color_pass.mutex);
            game_device_data->dlaa_output_recreated = false;
         }
         else
         {
            device_data.sr_suppressed = true;
            reshade::log::message(reshade::log::level::error, "[DLAA] DLSS failed, so it's off until it's picked again");
         }
      }

      bool encoded = false;
      if (dlss_drawn && g_dlaa_stages >= 3)
      {
         cmd_list->barrier(output, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::shader_resource);
         cmd_list->barrier(scene_resource, scene_read_state, reshade::api::resource_usage::render_target);
         encoded = DrawFullscreenPassD3D12(cmd_list_data, device_data, &game_device_data->color_pass, CompileTimeStringHash("YKR DLAA Encode PS"), "DLAA Encode", "Luma DLAA Encode", output, reshade::api::format::r16g16b16a16_float, scene_resource, scene.desc, {}, rgb_only_blend);
         cmd_list->barrier(scene_resource, reshade::api::resource_usage::render_target, scene_read_state);
         cmd_list->barrier(output, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);
      }
      if (!scene_readable)
      {
         cmd_list->barrier(scene_resource, scene_read_state, scene.state);
      }
      if (encoded)
      {
         device_data.has_drawn_sr = true;
         device_data.taa_detected = true; // No TAA to replace, but Core's upscaler UI checks for it
         game_device_data->dlaa_drawn_present.store(present_count, std::memory_order_relaxed);
      }
   }

   void OnPresent(NativeDevice* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      if (!std::exchange(projection_hook_attempted, true))
      {
         InstallProjectionHook();
      }

      // DLAA and its jitter for the next frame: DLSS' history restarts after a frame it didn't draw (menus, loading, just picked)
      const bool dlaa_active = LatchSRFrame(device_data) && set_projection_original;
      game_device_data.dlaa_active.store(dlaa_active, std::memory_order_relaxed);
      const uint32_t width = game_device_data.depth_width.load(std::memory_order_relaxed);
      const uint32_t height = game_device_data.depth_height.load(std::memory_order_relaxed);
      // The jitter follows DLAA's draws: in a frame it can't draw (SSAO off, a menu without a scene), FXAA would show it
      const bool dlaa_drawn = game_device_data.dlaa_drawn_present.load(std::memory_order_relaxed) == device_data.present_count.load(std::memory_order_relaxed);
      {
         const std::unique_lock lock(camera_state.mutex);
         camera_state.dlaa_active = dlaa_active;
         camera_state.jitter_pixels = ((dlaa_active && dlaa_drawn && width != 0 && height != 0) ? GetSRJitter(device_data, device_data.GetSRInstanceData()) : std::array<float, 2>{});
         camera_state.jitter_ndc = {camera_state.jitter_pixels[0] * 2.f / float((std::max)(width, 1u)), camera_state.jitter_pixels[1] * -2.f / float((std::max)(height, 1u))};
         camera_state.captured = camera_state.captured && dlaa_active;
      }
      // -1 at native resolution, with DLAA only (the game has no TAA: a negative bias would shimmer)
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         device_data.texture_mip_lod_bias_offset = ((dlaa_active && g_dlaa_mip_bias) ? SR::GetMipLODBias(1u, 1u) : 0.f);
      }

#if DEVELOPMENT
      LogDeviceRemoval(native_device);
      for (size_t i = 0; i < projection_sets.size(); i++)
      {
         projection_sets_frame[i] = std::atomic_ref(projection_sets[i]).exchange(0, std::memory_order_relaxed);
      }
      for (size_t i = 0; i < anchor_counts.size(); i++)
      {
         anchor_counts_frame[i] = std::atomic_ref(anchor_counts[i]).exchange(0, std::memory_order_relaxed);
      }
#endif
   }

   // DLAA was turned off: its textures go once no list can read them anymore
   void CleanExtraSRResources(DeviceData& device_data) override
   {
      auto& game_device_data = static_cast<YakuzaKiwamiDeviceData&>(*device_data.game);
      const uint64_t present_count = device_data.present_count.load(std::memory_order_relaxed);
      {
         const std::unique_lock lock(game_device_data.copy_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.copy_pass, present_count, &game_device_data.depth_copy);
      }
      {
         const std::unique_lock lock(game_device_data.motion_vector_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.motion_vector_pass, present_count, &game_device_data.motion_vectors);
      }
      {
         const std::unique_lock lock(game_device_data.color_pass.mutex);
         RetireD3D12GamePassResource(&game_device_data.color_pass, present_count, &game_device_data.decoded_scene);
         RetireD3D12GamePassResource(&game_device_data.color_pass, present_count, &game_device_data.dlaa_output);
         game_device_data.dlaa_output_recreated = false;
      }
   }

   void LoadConfigs() override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "Exposure", gs.Exposure);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Anti-Aliasing");

      // DLAA takes FXAA's place itself
      ImGui::BeginDisabled(device_data.sr_type != SR::Type::None);
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Replaces the game's FXAA with SMAA (requires anti-aliasing enabled in the game's graphics settings).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      if (g_smaa_enable)
      {
         DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, "Finds edges by geometry (scene depth) instead of by brightness alone (needs SSAO on in the game's graphics settings).");
      }
#endif

      // The grade sliders act on the HDR tonemap only.
      if (cb_luma_global_settings.DisplayMode != DisplayModeType::SDR)
      {
         ImGui::SeparatorText("Grade");

         DrawSettingSlider("Exposure", "Exposure", &gs.Exposure, default_luma_global_game_settings.Exposure, 0.f, 2.f, "Overall image brightness (1 = vanilla).");

         ImGui::SeparatorText("Videos");

         DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, default_luma_global_game_settings.VideoAutoHDREnable, "Expands the highlights of the game's videos.");

         ImGui::BeginDisabled(gs.VideoAutoHDREnable < 0.5f);
         DrawSettingSlider("Video AutoHDR Strength", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, 0.f, 1.f, "How bright the videos' highlights get (0 = like SDR).");
         ImGui::EndDisabled();
      }

      ImGui::SeparatorText("Output");

      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, default_luma_global_game_settings.Dithering, "Adds a tiny amount of noise to the final image to hide banding.");
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Yakuza Kiwami Luma mod", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza Kiwami Luma mod", "https://github.com/Filoppi/Luma-Framework/", 1);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // B8G8R8A8 back buffer -> R16G16B16A16 FLOAT
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_unorm,
         reshade::api::format::r8g8b8a8_unorm_srgb,
         reshade::api::format::r8g8b8a8_typeless,
         reshade::api::format::r8g8b8x8_unorm,
         reshade::api::format::r8g8b8x8_unorm_srgb,
         reshade::api::format::b8g8r8a8_unorm,
         reshade::api::format::b8g8r8a8_unorm_srgb,
         reshade::api::format::b8g8r8a8_typeless,
         reshade::api::format::b8g8r8x8_unorm,
         reshade::api::format::b8g8r8x8_unorm_srgb,
         reshade::api::format::b8g8r8x8_typeless,

         reshade::api::format::r10g10b10a2_unorm,
         reshade::api::format::r10g10b10a2_typeless,

         reshade::api::format::r11g11b10_float,
      };
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // DLAA's mip bias (see "OnPresent()"), on the anisotropic samplers, which also get 16x
      enable_samplers_upgrade = true;

      game = new YakuzaKiwami();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH && set_projection_original)
   {
      // ponytail: the relay and the stub stay (a thread may still be in them), 64 bytes
      uint8_t* const set_projection = reinterpret_cast<uint8_t*>(game_module + set_projection_rva);
      uint8_t patched[16];
      std::copy_n(set_projection, 16, patched);
      WriteCodeAtomically(set_projection, patched, set_projection_prologue);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
