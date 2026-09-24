#define GAME_SAINTS_ROW_THE_THIRD_REMASTERED 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// SMAA replaces the game's FXAA (see DrawSMAAInPlaceOfFXAA)
#define ENABLE_SMAA 1

#include "..\..\Core\core.hpp"
#include "..\..\External\reshade\deps\minhook\include\MinHook.h"

namespace
{
   // SRTTR.exe addresses, valid only for the analysed build (PE TimeDateStamp 0x60EE85F4; image base 0x140000000)
   uint8_t* GetGameAddress(uintptr_t rva)
   {
      // Null for any other build; the PE header is only read once
      static uint8_t* const base = []() -> uint8_t*
      {
         auto* image = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
         const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
         const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos_header->e_lfanew);
         return nt_headers->FileHeader.TimeDateStamp == 0x60EE85F4 ? image : nullptr;
      }();
      return base ? base + rva : nullptr;
   }

   // The camera's TAA jitter counter starts near -2^24, and the pattern index is taken with a signed modulo ("and reg, 0x8000000N" + sign fixup),
   // so the index is 0 or negative and only entry 0 of each pattern is ever used, every N frames (every other frame for the vanilla 2x pattern).
   // Clearing the sign bit of each mask makes the index "counter & N", so every offset of the pattern cycles. High bytes of the 2x and 4x masks
   // (the 8x pattern is replaced as a whole, see "SetHaltonJitterPattern()"):
   constexpr uintptr_t jitter_index_mask_high_byte_rvas[] = {0x89BA06, 0x89B992};
   bool game_patches_applied = false;

#if DEVELOPMENT
   bool IsJitterIndexFixed()
   {
      const uint8_t* byte = GetGameAddress(jitter_index_mask_high_byte_rvas[0]);
      return byte && *byte == 0x00;
   }
#endif

   // Always on, development builds can toggle it
   void SetJitterIndexFix(bool enable)
   {
      for (const uintptr_t rva : jitter_index_mask_high_byte_rvas)
      {
         const uint8_t* byte = GetGameAddress(rva);
         if (!byte || (*byte != 0x80 && *byte != 0x00))
            return;
      }
      const uint8_t mask_high_byte = enable ? 0x00 : 0x80;
      for (const uintptr_t rva : jitter_index_mask_high_byte_rvas)
         System::PatchMemory(GetGameAddress(rva), &mask_high_byte, 1);
   }

   // TAA jitter pattern (int, .data, static 1): 0 none, 1 D3D 2x MSAA, 2 4x, 3 8x (switch at 0x14089B886, indexed by the camera's +0x59C counter)
   constexpr uintptr_t jitter_mode_rva = 0x11ADBB0;
   constexpr int32_t vanilla_jitter_mode = 1;
   constexpr int32_t sr_jitter_mode = 3;

   int32_t* GetJitterMode()
   {
      return reinterpret_cast<int32_t*>(GetGameAddress(jitter_mode_rva));
   }

#if ENABLE_SR
   // The 8x pattern code (only reached in jitter mode 3, from 0x14089B8B5 up to the 4x one at 0x14089B987) is replaced with a Halton (2, 3) table lookup,
   // indexed by the camera counter (& 15, which is fine for its negative values), then jumps to the shared code that scales the offset by 0.125 / resolution.
   // Offsets are in 1/16 pixels like the game's patterns (x right, y up). SR reads the resulting projection jitter back from the camera.
   constexpr uintptr_t jitter_pattern_8x_rva = 0x89B8B5;
   constexpr size_t jitter_pattern_8x_size = 0xD2;
   std::array<uint8_t, jitter_pattern_8x_size> jitter_pattern_8x_original = {};
   bool jitter_pattern_halton = false;

   void SetHaltonJitterPattern(bool enable)
   {
      uint8_t* block = GetGameAddress(jitter_pattern_8x_rva);
      if (!block || enable == jitter_pattern_halton)
         return;
      if (enable)
      {
         constexpr uint8_t expected[] = {0x8B, 0x86, 0x9C, 0x05, 0x00, 0x00, 0x25, 0x07, 0x00, 0x00, 0x80}; // mov eax, [rsi+0x59C]; and eax, 0x80000007
         if (std::memcmp(block, expected, sizeof(expected)) != 0)
            return;
         std::memcpy(jitter_pattern_8x_original.data(), block, jitter_pattern_8x_size);
      }
      std::array<uint8_t, jitter_pattern_8x_size> bytes = jitter_pattern_8x_original;
      if (enable)
      {
         // clang-format off
         constexpr uint8_t code[] = {
            0x8B, 0x86, 0x9C, 0x05, 0x00, 0x00, // mov eax, [rsi+0x59C]
            0x83, 0xE0, 0x0F,                   // and eax, 15
            0x48, 0x8D, 0x15, 0x10, 0x00, 0x00, 0x00, // lea rdx, [rip+0x10] (the table, right after this code)
            0xF3, 0x0F, 0x10, 0x0C, 0xC2,       // movss xmm1, [rdx+rax*8] (x)
            0xF3, 0x0F, 0x10, 0x74, 0xC2, 0x04, // movss xmm6, [rdx+rax*8+4] (y)
            0xE9, 0x66, 0x01, 0x00, 0x00,       // jmp 0x14089BA3B
         };
         // clang-format on
         std::memcpy(bytes.data(), code, sizeof(code));
         for (unsigned int i = 0; i < 16; i++)
         {
            const unsigned int phase = i % SR::GetDefaultJitterPhases();
            const float offset[2] = {SR::HaltonSequence(phase, 2) * 16.f, SR::HaltonSequence(phase, 3) * -16.f};
            std::memcpy(bytes.data() + sizeof(code) + i * sizeof(offset), offset, sizeof(offset));
         }
      }
      if (System::PatchMemory(block, bytes.data(), jitter_pattern_8x_size))
         jitter_pattern_halton = enable;
   }
#endif

   // Camera projection build (camera in rcx), called many times per frame on the thread that also issues the frame's draws.
   // It writes the projection at camera+0xE0 (row major): row 1 y scale (cot(fov_y/2)) at +0xF4, row 2 jitter (NDC) at +0x100/+0x104.
   // Near and far are at +0x530/+0x534. The jitter pattern index is the camera's +0x59C counter.
   // Only jitters if TAA passes the gate (0x140908A90) and [0x1411DCF40] == -1.
   constexpr uintptr_t camera_build_rva = 0x89B740;
   using CameraBuildFunc = uint64_t (*)(uint8_t* camera, void* rdx, void* r8, void* r9);
   CameraBuildFunc camera_build_original = nullptr;

   // The values of the last camera built on this thread, copied right after the build so the camera can't be read after being freed
   struct CameraData
   {
      float jitter_x = 0.f; // NDC
      float jitter_y = 0.f; // NDC
      float projection_y_scale = 0.f;
      float near_plane = 0.f;
      float far_plane = 0.f;
      bool valid = false;
   };
   thread_local CameraData last_built_camera;

#if DEVELOPMENT
   // Camera jitter trace
   constexpr uintptr_t camera_counter_increment_rva = 0x89B470; // inc [rcx+0x59C]
   constexpr uintptr_t taa_gate_rva = 0x908A90;
   constexpr uintptr_t jitter_gate_rva = 0x11DCF40;
   std::atomic<int> camera_log_calls_left = 0;
   using CameraCounterIncrementFunc = void (*)(uint8_t* camera);
   CameraCounterIncrementFunc camera_counter_increment_original = nullptr;

   void CameraCounterIncrementDetour(uint8_t* camera)
   {
      if (camera_log_calls_left > 0)
      {
         char line[128];
         std::snprintf(line, sizeof(line), "[SRTTR JIT] increment tid=%lu camera=%p counter=%d", GetCurrentThreadId(), camera, *reinterpret_cast<const int32_t*>(camera + 0x59C));
         reshade::log::message(reshade::log::level::info, line);
      }
      camera_counter_increment_original(camera);
   }
#endif

   uint64_t CameraBuildDetour(uint8_t* camera, void* rdx, void* r8, void* r9)
   {
#if DEVELOPMENT
      const bool log = camera_log_calls_left > 0 && camera_log_calls_left.fetch_sub(1) > 0;
      const int32_t counter = log ? *reinterpret_cast<const int32_t*>(camera + 0x59C) : 0;
      const bool taa_gate = log && reinterpret_cast<bool (*)()>(GetGameAddress(taa_gate_rva))();
      const int32_t jitter_gate = log ? *reinterpret_cast<const int32_t*>(GetGameAddress(jitter_gate_rva)) : 0;
#endif
      const uint64_t result = camera_build_original(camera, rdx, r8, r9);
      last_built_camera.jitter_x = *reinterpret_cast<const float*>(camera + 0x100);
      last_built_camera.jitter_y = *reinterpret_cast<const float*>(camera + 0x104);
      last_built_camera.projection_y_scale = *reinterpret_cast<const float*>(camera + 0xF4);
      last_built_camera.near_plane = *reinterpret_cast<const float*>(camera + 0x530);
      last_built_camera.far_plane = *reinterpret_cast<const float*>(camera + 0x534);
      last_built_camera.valid = true;
#if DEVELOPMENT
      if (log)
      {
         const float* row_2 = reinterpret_cast<const float*>(camera + 0x100);
         char line[256];
         std::snprintf(line, sizeof(line), "[SRTTR JIT] build tid=%lu camera=%p counter=%d taa_gate=%d jitter_gate=%d row2=%.9g,%.9g,%.9g,%.9g", GetCurrentThreadId(), camera, counter, int(taa_gate), jitter_gate, row_2[0], row_2[1], row_2[2], row_2[3]);
         reshade::log::message(reshade::log::level::info, line);
      }
#endif
      return result;
   }

   void InstallCameraHooks()
   {
      uint8_t* camera_build = GetGameAddress(camera_build_rva);
      // mov rax, rsp; push rbx; push rbp; push rsi; push rdi
      constexpr uint8_t camera_build_prologue[] = {0x48, 0x8B, 0xC4, 0x53, 0x55, 0x56, 0x57};
      if (!camera_build || std::memcmp(camera_build, camera_build_prologue, sizeof(camera_build_prologue)) != 0 || MH_Initialize() != MH_OK)
         return;
      if (MH_CreateHook(camera_build, reinterpret_cast<void*>(&CameraBuildDetour), reinterpret_cast<void**>(&camera_build_original)) == MH_OK)
         MH_EnableHook(camera_build);
#if DEVELOPMENT
      uint8_t* camera_counter_increment = GetGameAddress(camera_counter_increment_rva);
      constexpr uint8_t camera_counter_increment_prologue[] = {0xFF, 0x81, 0x9C, 0x05, 0x00, 0x00}; // inc dword ptr [rcx+0x59C]
      if (std::memcmp(camera_counter_increment, camera_counter_increment_prologue, sizeof(camera_counter_increment_prologue)) == 0 && MH_CreateHook(camera_counter_increment, reinterpret_cast<void*>(&CameraCounterIncrementDetour), reinterpret_cast<void**>(&camera_counter_increment_original)) == MH_OK)
         MH_EnableHook(camera_counter_increment);
#endif
   }

   // taa CS perms (RGB, YCoCg, and their exposure normalized versions): t0 color, t1 depth, t3 G-buffer motion vectors, u0 color output, cb10 TAA_PARAMS
   constexpr uint32_t taa_hashes[] = {0xAB470526, 0x629C161F, 0x8E309763, 0x7D190953};

   // hdr_filter compose perms, the last draw of the frame (scene and GUI layer onto the swapchain). Every HDR_DISPLAY one is replaced by the SDR perm's math,
   // so the game's HDR setting does not change the output. The first ones compose the scene, the GUI only ones run in menus (the SDR GUI only one,
   // 0xA283B6FB, stays vanilla).
   constexpr uint32_t compose_hashes[] = {0xFCCD77CD, 0xADB2056B, 0xEB9D7036, 0x50DC2D70, 0x7083C926, 0xCE7FF710, 0xA11A22A3, 0x681958CA, 0x3D126636};
   constexpr uint32_t compose_gui_hashes[] = {0x79D0B6FF, 0x8DFF00F4, 0x3EA6C5A9, 0x1AD38FF6, 0xDEEDDD60, 0x281056F7, 0x818B5759, 0xC165ACD1};

   // hdr_filter tonemap CS perms (LUT and no LUT, with and without luminance output), run in every frame with a scene
   constexpr uint32_t tonemap_hashes[] = {0x941A9154, 0x835784B0, 0xAB466B4A, 0xFEDD50B7};

   constexpr uint32_t sr_inputs_shader_hash = CompileTimeStringHash("SR Inputs");

   // pbr_fxaa, the game's only FXAA pass (in-game Anti-Aliasing = FXAA), replaced by SMAA: t0 = the tonemap output (gamma), RTV = a separate
   // fp16 texture that compose reads
   constexpr uint32_t fxaa_pixel_shader_hash = 0xD928AE8D;
   // ambient, the pass that applies the SSAO: its t0 is the scene depth (R24), which is no longer bound at the FXAA draw, so it's kept for
   // SMAA's predication
   constexpr uint32_t ambient_hash = 0xFD45DCA7;
   constexpr uint32_t smaa_linearize_shader_hash = CompileTimeStringHash("SRTTR SMAA Linearize CS");
   constexpr uint32_t smaa_predication_shader_hash = CompileTimeStringHash("SRTTR SMAA Predication CS");
   bool g_smaa_enable = true;
#if DEVELOPMENT
   bool g_smaa_predication = true;
   int g_smaa_debug_view = 0; // 0 off, 1 edges, 2 predication
   bool g_smaa_dump = false;
#endif

   // A Luma shader is usable only once compiled; true when all the named ones are. The caller holds s_mutex_shader_objects.
   template <typename T, typename... Names>
   bool HasShaders(const T& shaders, Names... names)
   {
      const auto has = [&](uint32_t name)
      {
         const auto it = shaders.find(name);
         return it != shaders.end() && it->second;
      };
      return (has(names) && ...);
   }

#if ENABLE_SR
   // Jitter sign conventions and the motion vectors jitter flag. The game's MVs have no jitter: with a static camera, both the TAA
   // reprojection (cb10 matReprojection) and the object MVs are ~0 while the jitter moves the image by up to ~0.9 pixels between frames.
   bool sr_flip_jitter_x = false;
   bool sr_flip_jitter_y = false;
   bool sr_mvs_jittered = false;
#endif
} // namespace

#if DEVELOPMENT
namespace
{
   // DLAA research: logs the TAA inputs the DevKit cannot read (cbuffers, depth state) to ReShade.log
   std::atomic<int> dlaa_log_frames_left = 0;
   uint32_t dlaa_log_frame = 0;
   bool dlaa_log_gbuffer_done = false;

   // The game binds ranges of one large constant buffer (*SetConstantBuffers1), so only the bound range is copied.
   // Reads up to "registers" float4s, fewer if the bound range is smaller; returns how many were read (0 on failure).
   UINT ReadBoundConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, reshade::api::shader_stage stage, UINT slot, UINT registers, float* data)
   {
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return 0;
      com_ptr<ID3D11Buffer> cb;
      UINT first = 0, count = 0;
      if (stage == reshade::api::shader_stage::vertex)
         native_device_context1->VSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      else if (stage == reshade::api::shader_stage::pixel)
         native_device_context1->PSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      else
         native_device_context1->CSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      if (!cb)
         return 0;
      D3D11_BUFFER_DESC desc = {};
      cb->GetDesc(&desc);
      if (count == 0 && first == 0) // Bound with the non range API
         count = desc.ByteWidth / 16;
      registers = (std::min)(registers, count);
      const UINT offset = first * 16;
      const UINT bytes = registers * 16;
      if (registers == 0 || offset + bytes > desc.ByteWidth)
         return 0;

      D3D11_BUFFER_DESC staging_desc = {};
      staging_desc.ByteWidth = bytes;
      staging_desc.Usage = D3D11_USAGE_STAGING;
      staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      com_ptr<ID3D11Buffer> staging;
      if (FAILED(native_device->CreateBuffer(&staging_desc, nullptr, &staging)))
         return 0;
      const D3D11_BOX box = {offset, 0, 0, offset + bytes, 1, 1};
      native_device_context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, cb.get(), 0, &box);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)))
         return 0;
      std::memcpy(data, mapped.pData, bytes);
      native_device_context->Unmap(staging.get(), 0);
      return registers;
   }

   // Raw texture dump for offline analysis (formats the DevKit cannot read back): header {width, height, dxgi_format}, then tight rows of 1 or 4 bytes
   // per pixel. Written to %TEMP%\srttr_<file_prefix>_<name>_<frame>.bin.
   void DumpTexture(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11ShaderResourceView* srv, const char* file_prefix, const char* name, uint32_t frame)
   {
      if (!srv)
         return;
      com_ptr<ID3D11Resource> resource;
      srv->GetResource(&resource);
      com_ptr<ID3D11Texture2D> texture;
      if (FAILED(resource->QueryInterface(&texture)))
         return;
      D3D11_TEXTURE2D_DESC desc = {};
      texture->GetDesc(&desc);
      UINT pixel_bytes = 4;
      if (desc.Format == DXGI_FORMAT_R8_UNORM)
         pixel_bytes = 1;
      else if (desc.Format == DXGI_FORMAT_R16_FLOAT || desc.Format == DXGI_FORMAT_R8G8_UNORM)
         pixel_bytes = 2;
      else if (desc.Format != DXGI_FORMAT_R16G16_UNORM && desc.Format != DXGI_FORMAT_R24G8_TYPELESS)
         return;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;
      desc.MipLevels = 1;
      com_ptr<ID3D11Texture2D> staging;
      if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &staging)))
         return;
      native_device_context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, texture.get(), 0, nullptr);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)))
         return;
      const auto path = std::filesystem::temp_directory_path() / (std::string("srttr_") + file_prefix + "_" + name + "_" + std::to_string(frame) + ".bin");
      if (FILE* file = _wfopen(path.c_str(), L"wb"))
      {
         const uint32_t header[3] = {desc.Width, desc.Height, uint32_t(desc.Format)};
         fwrite(header, sizeof(header), 1, file);
         for (UINT y = 0; y < desc.Height; y++)
            fwrite(static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, desc.Width * pixel_bytes, 1, file);
         fclose(file);
         reshade::log::message(reshade::log::level::info, ("[SRTTR] dump " + path.string()).c_str());
      }
      native_device_context->Unmap(staging.get(), 0);
   }

   // "prefix" starts the log line, e.g. "[SRTTR DLAA] frame=3"
   void LogRegisters(const char* prefix, const char* tag, const float* data, UINT first_register, UINT last_register)
   {
      char line[256];
      for (UINT i = first_register; i <= last_register; i++)
      {
         std::snprintf(line, sizeof(line), "%s %s c%u=%.9g,%.9g,%.9g,%.9g", prefix, tag, i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
         reshade::log::message(reshade::log::level::info, line);
      }
   }

   void LogTAAInputs(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, uint32_t taa_hash)
   {
      const std::string prefix = "[SRTTR DLAA] frame=" + std::to_string(dlaa_log_frame);
      // TAA_PARAMS cb10: c0-c3 matReprojection, c4 screenSize (uint2) + texelSize, c5 sharpness/neighbour_threshold/impulse_reduce/key_value, c6 max_weight/min_weight/frame_id (int)
      float taa[7 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::compute, 10, 7, taa) == 7)
      {
         uint32_t screen_size[2];
         int32_t frame_id;
         std::memcpy(screen_size, &taa[16], sizeof(screen_size));
         std::memcpy(&frame_id, &taa[26], sizeof(frame_id));
         const int32_t* jitter_mode = GetJitterMode();
         char line[256];
         // The jitter SR gets (last camera built on this thread), to compare against the pattern entry of the GPU frame_id
         std::snprintf(line, sizeof(line), "[SRTTR DLAA] frame=%u TAA 0x%08X screenSize=%u,%u frame_id=%d jitter_mode=%d sr_camera_valid=%d sr_camera_jitter_px=%.4f,%.4f tid=%lu", dlaa_log_frame, taa_hash, screen_size[0], screen_size[1], frame_id, jitter_mode ? *jitter_mode : -1, int(last_built_camera.valid), last_built_camera.jitter_x * screen_size[0] * 0.5f, -last_built_camera.jitter_y * screen_size[1] * 0.5f, GetCurrentThreadId());
         reshade::log::message(reshade::log::level::info, line);
         LogRegisters(prefix.c_str(), "TAA cb10", taa, 0, 6);
      }
      // CB_COMMON as bound for the post chain, to compare against the G-buffer one
      float common[42 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::compute, 1, 42, common) == 42)
         LogRegisters(prefix.c_str(), "TAA cb1", common, 34, 41);
      // Depth (t1) and motion vectors (t3) of the first two logged frames, to check the object MVs against the camera reprojection
      if (dlaa_log_frame < 2)
      {
         com_ptr<ID3D11ShaderResourceView> srvs[3];
         native_device_context->CSGetShaderResources(1, 3, &srvs[0]);
         DumpTexture(native_device, native_device_context, srvs[0].get(), "dlaa", "depth", dlaa_log_frame);
         DumpTexture(native_device, native_device_context, srvs[2].get(), "dlaa", "mv", dlaa_log_frame);
      }

      dlaa_log_frame++;
      dlaa_log_frames_left--;
      dlaa_log_gbuffer_done = false;
   }

   void LogGBufferInputs(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes)
   {
      // The first G-buffer draw of the frame: 4 MRTs with the motion vectors in RT3 (r16g16_unorm)
      com_ptr<ID3D11RenderTargetView> rtvs[4];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(4, &rtvs[0], &dsv);
      if (!rtvs[3])
         return;
      D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
      rtvs[3]->GetDesc(&rtv_desc);
      if (rtv_desc.Format != DXGI_FORMAT_R16G16_UNORM)
         return;
      dlaa_log_gbuffer_done = true;
      const std::string prefix = "[SRTTR DLAA] frame=" + std::to_string(dlaa_log_frame);

      D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
      if (dsv)
         dsv->GetDesc(&dsv_desc);
      com_ptr<ID3D11DepthStencilState> depth_state;
      UINT stencil_ref = 0;
      native_device_context->OMGetDepthStencilState(&depth_state, &stencil_ref);
      D3D11_DEPTH_STENCIL_DESC depth_desc = {};
      if (depth_state)
         depth_state->GetDesc(&depth_desc);
      char line[256];
      std::snprintf(line, sizeof(line), "[SRTTR DLAA] frame=%u GBuffer VS 0x%08X PS 0x%08X dsv_format=%d depth_enable=%d depth_write=%d depth_func=%d",
         dlaa_log_frame, uint32_t(original_shader_hashes.vertex_shaders[0]), uint32_t(original_shader_hashes.pixel_shaders[0]), int(dsv_desc.Format), int(depth_desc.DepthEnable), int(depth_desc.DepthWriteMask), int(depth_desc.DepthFunc));
      reshade::log::message(reshade::log::level::info, line);

      // CB_COMMON cb1: c1 Target_dimensions, c9 Velocity_calculation_data, c34-c37 projTM, c38-c41 unknown (not the previous camera the MVs use)
      float common[42 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::vertex, 1, 42, common) == 42)
      {
         LogRegisters(prefix.c_str(), "GBuffer VS cb1", common, 1, 1);
         LogRegisters(prefix.c_str(), "GBuffer VS cb1", common, 9, 9);
         LogRegisters(prefix.c_str(), "GBuffer VS cb1", common, 34, 41);
      }
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::pixel, 1, 10, common) == 10)
         LogRegisters(prefix.c_str(), "GBuffer PS cb1", common, 9, 9);
      // CB_VERTEX cb2: c10-c13 curr_to_prev_clip (per object, static meshes only)
      float vertex[14 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::vertex, 2, 14, vertex) == 14)
         LogRegisters(prefix.c_str(), "GBuffer VS cb2", vertex, 10, 13);
   }

   // XeGTAO research: the ssao_miniengine chain (cbuffers and bindings of every pass) and the ambient pass that consumes its AO
   constexpr uint32_t ssao_hashes[] = {0x2F4B251B, 0xAC38984B, 0x1DEB634A, 0xDE09F597, 0x23EF0DBA, 0x7378361E};
   std::atomic<int> xegtao_log_frames_left = 0;
   uint32_t xegtao_log_frame = 0;

   void LogViewTexture(const std::string& prefix, const std::string& slot, ID3D11View* view)
   {
      if (!view)
         return;
      com_ptr<ID3D11Resource> resource;
      view->GetResource(&resource);
      com_ptr<ID3D11Texture2D> texture;
      if (FAILED(resource->QueryInterface(&texture)))
         return;
      D3D11_TEXTURE2D_DESC desc = {};
      texture->GetDesc(&desc);
      char line[256];
      std::snprintf(line, sizeof(line), "%s %s res=%p %ux%u array=%u mips=%u format=%d", prefix.c_str(), slot.c_str(), resource.get(), desc.Width, desc.Height, desc.ArraySize, desc.MipLevels, int(desc.Format));
      reshade::log::message(reshade::log::level::info, line);
   }

   void LogSSAOPass(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, uint32_t hash)
   {
      const std::string prefix = std::format("[SRTTR XeGTAO] frame={} SSAO 0x{:08X}", xegtao_log_frame, hash);
      float params[64 * 4];
      if (const UINT registers = ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::compute, 10, 64, params))
         LogRegisters(prefix.c_str(), "cb10", params, 0, registers - 1);
      com_ptr<ID3D11ShaderResourceView> srvs[4];
      native_device_context->CSGetShaderResources(0, 4, &srvs[0]);
      com_ptr<ID3D11UnorderedAccessView> uavs[5];
      native_device_context->CSGetUnorderedAccessViews(0, 5, &uavs[0]);
      for (UINT i = 0; i < 4; i++)
         LogViewTexture(prefix, "t" + std::to_string(i), srvs[i].get());
      for (UINT i = 0; i < 5; i++)
         LogViewTexture(prefix, "u" + std::to_string(i), uavs[i].get());
   }

   void LogAmbientInputs(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context)
   {
      const std::string prefix = std::format("[SRTTR XeGTAO] frame={} ambient", xegtao_log_frame);
      // AMBIENT_PARAMS cb10 (52 registers): c0-c3 Projection, c4-c7 ViewInverse, c8-c11 ShadowMatrix, c12 RenderOffset, c13 LightDir,
      // c14/c15 AmbientLevelHigh/Low, c16-c22 ZonePos, c23 AmountInv/Normals/Power/Intensity, c24 _unused0/IntensityDiffuse/MaxIterations/NumberOfInteriors
      float params[52 * 4];
      if (const UINT registers = ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::pixel, 10, 52, params))
         LogRegisters(prefix.c_str(), "cb10", params, 0, (std::min)(registers, 25u) - 1);
      char line[256];
      std::snprintf(line, sizeof(line), "%s camera valid=%d projection_y_scale=%.9g near=%.9g far=%.9g jitter=%.9g,%.9g", prefix.c_str(), int(last_built_camera.valid), last_built_camera.projection_y_scale, last_built_camera.near_plane, last_built_camera.far_plane, last_built_camera.jitter_x, last_built_camera.jitter_y);
      reshade::log::message(reshade::log::level::info, line);
      // t0 depth, t1 view space normals, t2 SSAO
      com_ptr<ID3D11ShaderResourceView> srvs[3];
      native_device_context->PSGetShaderResources(0, 3, &srvs[0]);
      for (UINT i = 0; i < 3; i++)
         LogViewTexture(prefix, "t" + std::to_string(i), srvs[i].get());
      if (xegtao_log_frames_left == 2) // The first frame of each button press; the frame number keeps the files apart
      {
         DumpTexture(native_device, native_device_context, srvs[0].get(), "xegtao", "depth", xegtao_log_frame);
         DumpTexture(native_device, native_device_context, srvs[1].get(), "xegtao", "normals", xegtao_log_frame);
         DumpTexture(native_device, native_device_context, srvs[2].get(), "xegtao", "ao", xegtao_log_frame);
      }
      xegtao_log_frame++;
      xegtao_log_frames_left--;
   }
} // namespace
#endif

struct SaintsRowTheThirdRemasteredGameDeviceData final : public GameDeviceData
{
   // SMAA scratch at FXAA's size: the linear-light copy of its input and the predication edge-ness
   com_ptr<ID3D11UnorderedAccessView> smaa_linear_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   UINT smaa_width = 0;
   UINT smaa_height = 0;
   // This frame's scene depth, from the ambient pass (reset every present)
   com_ptr<ID3D11ShaderResourceView> smaa_depth_srv;

#if ENABLE_SR
   // SR inputs converted from the game's TAA ones
   com_ptr<ID3D11Texture2D> sr_motion_vectors;
   com_ptr<ID3D11UnorderedAccessView> sr_motion_vectors_uav;
   com_ptr<ID3D11Texture2D> sr_depth;
   com_ptr<ID3D11UnorderedAccessView> sr_depth_uav;
   bool sr_jitter_mode_set = false;
#endif
};

class SaintsRowTheThirdRemastered final : public Game
{
   static SaintsRowTheThirdRemasteredGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<SaintsRowTheThirdRemasteredGameDeviceData*>(device_data.game);
   }

public:
   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (reference)\n1 - HDR: native grade + reconstructed luminance + DICE display map", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // The tonemap CS writes display encoded (gamma 2.2) values into a float texture, compose copies them to the swapchain through a UNORM view
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The tonemap scales the scene by game / UI paper white so the GUI layer blended over it in gamma space by compose lands at UI paper white
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // The game binds cbuffers 0-4 and 6-10
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      default_luma_global_game_settings.RCASSharpness = 0.f;
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.FilmGrainIntensity = 1.f;
      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.HideGameplayUI = 0.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(smaa_linearize_shader_hash, ShaderDefinition{"Luma_SRTTR_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(smaa_predication_shader_hash, ShaderDefinition{"Luma_SRTTR_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
#if ENABLE_SR
      native_shaders_definitions.emplace(sr_inputs_shader_hash, ShaderDefinition{"Luma_SRTTR_SRInputs", reshade::api::pipeline_subobject_type::compute_shader});
      // SR takes its jitter from the patched game code, which only exists in the analysed build
      sr_game_tooltip = GetGameAddress(0) ? "Requires \"Anti-Aliasing\" set to \"TAA\" in the game's display settings.\n" : "Unsupported game executable version: Super Resolution can't engage.\n";
#endif
   }

   // SMAA in place of the FXAA draw: it reads FXAA's input (t0) and writes FXAA's render target. Returns false, and FXAA draws, when an input,
   // a shader or the scratch is missing.
   bool DrawSMAAInPlaceOfFXAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool& updated_cbuffers)
   {
      com_ptr<ID3D11RenderTargetView> rtv;
      native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
      com_ptr<ID3D11ShaderResourceView> color_srv;
      native_device_context->PSGetShaderResources(0, 1, &color_srv);
      if (!rtv || !color_srv)
         return false;
      uint4 size;
      uint4 target_size;
      DXGI_FORMAT format;
      GetResourceInfo(color_srv.get(), size, format);
      GetResourceInfo(rtv.get(), target_size, format);
      if (size.x != target_size.x || size.y != target_size.y || size.x == 0 || size.y == 0)
         return false;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at"
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h) || !HasShaders(device_data.native_compute_shaders, smaa_linearize_shader_hash))
         return false;

      auto& game_device_data = GetGameDeviceData(device_data);
      if (!game_device_data.smaa_linear_srv || game_device_data.smaa_width != size.x || game_device_data.smaa_height != size.y)
      {
         game_device_data.smaa_linear_uav = nullptr;
         game_device_data.smaa_linear_srv = nullptr;
         game_device_data.smaa_predication_uav = nullptr;
         game_device_data.smaa_predication_srv = nullptr;
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = size.x;
         desc.Height = size.y;
         desc.MipLevels = 1;
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> linear_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &linear_texture)) || FAILED(native_device->CreateUnorderedAccessView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_uav)) || FAILED(native_device->CreateShaderResourceView(linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)))
         {
            game_device_data.smaa_linear_uav = nullptr;
            game_device_data.smaa_linear_srv = nullptr;
            return false;
         }
         // Without it SMAA simply runs unpredicated
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav = nullptr;
            game_device_data.smaa_predication_srv = nullptr;
         }
         game_device_data.smaa_width = size.x;
         game_device_data.smaa_height = size.y;
      }

      // Predication depth: the ambient pass' R24 scene depth at FXAA's size. Anything else falls back to plain ULTRA.
      bool predication_available = game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, smaa_predication_shader_hash);
#if DEVELOPMENT
      predication_available = predication_available && g_smaa_predication;
#endif
      com_ptr<ID3D11ShaderResourceView> depth_srv = predication_available ? game_device_data.smaa_depth_srv : nullptr;
      if (depth_srv)
      {
         D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc;
         depth_srv->GetDesc(&depth_srv_desc);
         uint4 depth_size;
         GetResourceInfo(depth_srv.get(), depth_size, format);
         if (depth_srv_desc.Format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS || depth_srv_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || depth_size.x != size.x || depth_size.y != size.y)
            depth_srv = nullptr;
      }

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         // Unbind the render targets: were the depth still bound as the DSV, D3D11 would silently null its SRV in the predication pass
         com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

         ID3D11UnorderedAccessView* const linear_uav = game_device_data.smaa_linear_uav.get();
         ID3D11ShaderResourceView* const gamma_srv = color_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &linear_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &gamma_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at(smaa_linearize_shader_hash).get(), nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         if (depth_srv)
         {
            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const raw_depth_srv = depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at(smaa_predication_shader_hash).get(), nullptr, 0);
            native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         }

         native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D11RenderTargetView* const*>(&rtvs[0]), dsv.get());
         compute_state.Restore(native_device_context);
      }

      // The SMAA shaders read the target size from the Luma settings and the predication scale from the Luma data, in both stages
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
      updated_cbuffers = true;
      DrawSMAA(native_device, native_device_context, device_data, rtv.get(), game_device_data.smaa_linear_srv.get(), color_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);

#if DEVELOPMENT
      ID3D11ShaderResourceView* const edges_srv = device_data.managed_resources.shader_resource_views["smaa_edge_detection"_h].get();
      if (g_smaa_dump)
      {
         g_smaa_dump = false;
         const uint32_t frame = cb_luma_global_settings.FrameIndex;
         DumpTexture(native_device, native_device_context, depth_srv.get(), "smaa", "depth", frame);
         DumpTexture(native_device, native_device_context, depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr, "smaa", "predication", frame);
         DumpTexture(native_device, native_device_context, edges_srv, "smaa", "edges", frame);
      }
      // Calibration aid: SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red) replace the frame
      ID3D11ShaderResourceView* const debug_srv = g_smaa_debug_view == 1 ? edges_srv : (g_smaa_debug_view == 2 && depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
      if (debug_srv && HasShaders(device_data.native_vertex_shaders, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
      {
         DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
         debug_state.Cache(native_device_context, device_data.uav_max_count);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), debug_srv, rtv.get(), size.x, size.y, false);
         debug_state.Restore(native_device_context);
      }
#endif
      return true;
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new SaintsRowTheThirdRemasteredGameDeviceData;
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData has no virtual destructor
      delete static_cast<SaintsRowTheThirdRemasteredGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      if ((stages & reshade::api::shader_stage::compute) != reshade::api::shader_stage::compute)
      {
#if DEVELOPMENT
         if (dlaa_log_frames_left > 0 && !dlaa_log_gbuffer_done)
            LogGBufferInputs(native_device, native_device_context, original_shader_hashes);
#endif
         if (original_shader_hashes.Contains(ambient_hash, reshade::api::shader_stage::pixel))
         {
#if DEVELOPMENT
            if (xegtao_log_frames_left > 0)
               LogAmbientInputs(native_device, native_device_context);
#endif
            com_ptr<ID3D11ShaderResourceView> depth_srv;
            native_device_context->PSGetShaderResources(0, 1, &depth_srv);
            GetGameDeviceData(device_data).smaa_depth_srv = depth_srv;
         }
         else if (g_smaa_enable && original_shader_hashes.Contains(fxaa_pixel_shader_hash, reshade::api::shader_stage::pixel) && DrawSMAAInPlaceOfFXAA(native_device, native_device_context, cmd_list_data, device_data, updated_cbuffers))
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      const auto is_compute_shader = [&](uint32_t hash)
      { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::compute); };
#if DEVELOPMENT
      if (xegtao_log_frames_left > 0)
      {
         if (const auto ssao_hash = std::find_if(std::begin(ssao_hashes), std::end(ssao_hashes), is_compute_shader); ssao_hash != std::end(ssao_hashes))
            LogSSAOPass(native_device, native_device_context, *ssao_hash);
      }
#endif
      // The tonemap runs in every frame with a scene, whatever the anti-aliasing setting
      if (std::any_of(std::begin(tonemap_hashes), std::end(tonemap_hashes), is_compute_shader))
      {
         device_data.has_drawn_main_post_processing = true;
         return DrawOrDispatchOverrideType::None;
      }
      const auto taa_hash = std::find_if(std::begin(taa_hashes), std::end(taa_hashes), is_compute_shader);
      if (taa_hash == std::end(taa_hashes))
         return DrawOrDispatchOverrideType::None;

#if DEVELOPMENT
      if (dlaa_log_frames_left > 0)
         LogTAAInputs(native_device, native_device_context, *taa_hash);
#endif

      device_data.taa_detected = true;

#if ENABLE_SR
      // The jitter comes from the camera built on this thread (the render thread), and SR needs the game's 8x jitter pattern to be active
      const CameraData camera = last_built_camera;
      const int32_t* jitter_mode = GetJitterMode();
      if (device_data.sr_type == SR::Type::None || device_data.sr_suppressed || native_device_context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE || !camera.valid || !jitter_mode || *jitter_mode != sr_jitter_mode)
      {
         device_data.force_reset_sr = true;
         return DrawOrDispatchOverrideType::None;
      }
      ID3D11ComputeShader* const sr_inputs_shader = device_data.native_compute_shaders[sr_inputs_shader_hash].get();
      ASSERT_ONCE(sr_inputs_shader);
      if (!sr_inputs_shader)
         return DrawOrDispatchOverrideType::None;

      auto& game_device_data = GetGameDeviceData(device_data);
      com_ptr<ID3D11ShaderResourceView> srvs[4]; // t0 color, t1 depth, t3 motion vectors
      native_device_context->CSGetShaderResources(0, ARRAYSIZE(srvs), &srvs[0]);
      com_ptr<ID3D11UnorderedAccessView> output_uav;
      native_device_context->CSGetUnorderedAccessViews(0, 1, &output_uav);
      if (!srvs[0] || !srvs[1] || !srvs[3] || !output_uav)
         return DrawOrDispatchOverrideType::None;
      com_ptr<ID3D11Resource> source_color;
      srvs[0]->GetResource(&source_color);
      com_ptr<ID3D11Resource> output_color_resource;
      output_uav->GetResource(&output_color_resource);
      com_ptr<ID3D11Texture2D> output_color;
      if (FAILED(output_color_resource->QueryInterface(&output_color)))
         return DrawOrDispatchOverrideType::None;
      D3D11_TEXTURE2D_DESC output_desc;
      output_color->GetDesc(&output_desc);

      auto* sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data || output_desc.Width < sr_instance_data->min_resolution || output_desc.Height < sr_instance_data->min_resolution)
         return DrawOrDispatchOverrideType::None;

      // (Re)create the converted inputs at the TAA resolution
      const bool sr_inputs_changed = !game_device_data.sr_motion_vectors || !AreResourcesEqual(game_device_data.sr_motion_vectors.get(), output_color.get(), false);
      if (sr_inputs_changed)
      {
         CleanExtraSRResources(device_data);
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = output_desc.Width;
         desc.Height = output_desc.Height;
         desc.MipLevels = 1;
         desc.ArraySize = 1;
         desc.SampleDesc.Count = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         desc.Format = DXGI_FORMAT_R32G32_FLOAT;
         HRESULT hr = native_device->CreateTexture2D(&desc, nullptr, &game_device_data.sr_motion_vectors);
         if (SUCCEEDED(hr))
            hr = native_device->CreateUnorderedAccessView(game_device_data.sr_motion_vectors.get(), nullptr, &game_device_data.sr_motion_vectors_uav);
         desc.Format = DXGI_FORMAT_R32_FLOAT;
         if (SUCCEEDED(hr))
            hr = native_device->CreateTexture2D(&desc, nullptr, &game_device_data.sr_depth);
         if (SUCCEEDED(hr))
            hr = native_device->CreateUnorderedAccessView(game_device_data.sr_depth.get(), nullptr, &game_device_data.sr_depth_uav);
         ASSERT_ONCE(SUCCEEDED(hr));
         if (FAILED(hr))
         {
            CleanExtraSRResources(device_data);
            return DrawOrDispatchOverrideType::None;
         }
      }

      SR::SettingsData settings_data;
      settings_data.output_width = output_desc.Width;
      settings_data.output_height = output_desc.Height;
      settings_data.render_width = output_desc.Width;
      settings_data.render_height = output_desc.Height;
      settings_data.hdr = true; // TAA runs on the scene linear HDR color, before tonemapping
      settings_data.mvs_jittered = sr_mvs_jittered;
      // FSR's auto exposure clips highlights
      settings_data.auto_exposure = device_data.sr_type != SR::Type::FSR;
      settings_data.render_preset = dlss_render_preset;
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      DrawStateStack<DrawStateStackType::FullGraphics> draw_state_stack;
      DrawStateStack<DrawStateStackType::Compute> compute_state_stack;
      draw_state_stack.Cache(native_device_context, device_data.uav_max_count);
      compute_state_stack.Cache(native_device_context, device_data.uav_max_count);

      // Convert the motion vectors and depth, the game's cb10, t1 and t3 are still bound
      ID3D11UnorderedAccessView* const sr_inputs_uavs[] = {game_device_data.sr_motion_vectors_uav.get(), game_device_data.sr_depth_uav.get()};
      native_device_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(sr_inputs_uavs), sr_inputs_uavs, nullptr);
      native_device_context->CSSetShader(sr_inputs_shader, nullptr, 0);
      native_device_context->Dispatch((output_desc.Width + 7) / 8, (output_desc.Height + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[ARRAYSIZE(sr_inputs_uavs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(null_uavs), null_uavs, nullptr);

      SR::SuperResolutionImpl::DrawData draw_data;
      draw_data.source_color = source_color.get();
      draw_data.output_color = output_color.get();
      draw_data.motion_vectors = game_device_data.sr_motion_vectors.get();
      draw_data.depth_buffer = game_device_data.sr_depth.get();
      draw_data.render_width = output_desc.Width;
      draw_data.render_height = output_desc.Height;
      // Projection jitter (NDC, y up) to the sample offset in pixels (y down)
      draw_data.jitter_x = camera.jitter_x * output_desc.Width * (sr_flip_jitter_x ? 0.5f : -0.5f);
      draw_data.jitter_y = camera.jitter_y * output_desc.Height * (sr_flip_jitter_y ? -0.5f : 0.5f);
      draw_data.vert_fov = camera.projection_y_scale > 0.f ? 2.f * std::atan(1.f / camera.projection_y_scale) : (60.f * float(M_PI) / 180.f); // FSR fails without it
      if (camera.near_plane > 0.f && camera.far_plane > camera.near_plane)
      {
         draw_data.near_plane = camera.near_plane;
         draw_data.far_plane = camera.far_plane;
      }
      draw_data.reset = device_data.force_reset_sr || sr_inputs_changed;
      device_data.force_reset_sr = false;

      const bool sr_succeeded = sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data);
      draw_state_stack.Restore(native_device_context);
      compute_state_stack.Restore(native_device_context);
      if (!sr_succeeded)
      {
         device_data.force_reset_sr = true;
         return DrawOrDispatchOverrideType::None;
      }
      device_data.has_drawn_sr = true;
      return DrawOrDispatchOverrideType::Replaced; // The game's TAA history isn't updated, it's only read again if SR is turned off
#else
      return DrawOrDispatchOverrideType::None;
#endif // ENABLE_SR
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // Patched once the game runs, rather than at load time
      if (!game_patches_applied)
      {
         game_patches_applied = true;
         SetJitterIndexFix(true);
#if ENABLE_SR
         SetHaltonJitterPattern(true); // Before SR switches to the 8x pattern
#endif
         InstallCameraHooks();
      }

      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.smaa_depth_srv = nullptr;
      device_data.has_drawn_main_post_processing = false;
#if ENABLE_SR
      // SR resolves more detail than the game's TAA, so sharpen texture sampling while it draws (-1 at native resolution).
      // The offset is added to the game's own sampler bias, which is unknown, so the game's TAA keeps it unchanged.
      if (enable_samplers_upgrade && !custom_texture_mip_lod_bias_offset)
      {
         const float mip_lod_bias_offset = device_data.has_drawn_sr ? SR::GetMipLODBias(device_data.render_resolution.y, device_data.output_resolution.y) : 0.f;
         if (mip_lod_bias_offset != device_data.texture_mip_lod_bias_offset)
         {
            std::unique_lock lock_samplers(s_mutex_samplers); // The offset is a key of the samplers map, read by other threads
            device_data.texture_mip_lod_bias_offset = mip_lod_bias_offset;
         }
      }
      device_data.has_drawn_sr = false;
      // SR needs the 8x jitter pattern, vanilla TAA uses the 2x one. Only switched on changes, so the development slider stays usable.
      const bool sr_active = device_data.sr_type != SR::Type::None && !device_data.sr_suppressed;
      if (int32_t* jitter_mode = GetJitterMode(); jitter_mode && sr_active != game_device_data.sr_jitter_mode_set)
      {
         game_device_data.sr_jitter_mode_set = sr_active;
         *jitter_mode = sr_active ? sr_jitter_mode : vanilla_jitter_mode;
      }
#endif
   }

#if ENABLE_SR
   void CleanExtraSRResources(DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.sr_motion_vectors = nullptr;
      game_device_data.sr_motion_vectors_uav = nullptr;
      game_device_data.sr_depth = nullptr;
      game_device_data.sr_depth_uav = nullptr;
   }
#endif

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      auto& settings = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", settings.RCASSharpness);
      reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Contrast", settings.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", settings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", settings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", settings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", settings.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "FilmGrainIntensity", settings.FilmGrainIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;
      const auto& defaults = default_luma_global_game_settings;
      const auto slider = [&](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
            device_data.cb_luma_global_settings_dirty = true;
         if (ImGui::IsItemDeactivatedAfterEdit())
            reshade::set_config_value(nullptr, NAME, key, *value);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
      };

      ImGui::SeparatorText("Anti-Aliasing");
      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's FXAA with SMAA (only active when in-game Anti-Aliasing is set to FXAA).");
      DrawResetButton(g_smaa_enable, true, "SMAAEnable");
      // Not the canon "on top of SMAA": it sharpens the scene after any anti-aliasing (SMAA, TAA, DLAA, FSR 3), and the game's own sharpen is disabled
      slider("RCAS Sharpness", "RCASSharpness", &settings.RCASSharpness, defaults.RCASSharpness, 1.f, "Sharpening applied on top of anti-aliasing (0 = off). Replaces the game's Sharpen setting.");

      ImGui::SeparatorText("Grade");
      slider("Exposure", "Exposure", &settings.Exposure, defaults.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      slider("Contrast", "Contrast", &settings.Contrast, defaults.Contrast, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      slider("Saturation", "Saturation", &settings.Saturation, defaults.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", "HighlightsDesaturation", &settings.HighlightsDesaturation, defaults.HighlightsDesaturation, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      slider("Color Grading Intensity", "ColorGradingIntensity", &settings.ColorGradingIntensity, defaults.ColorGradingIntensity, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Effects");
      slider("Vignette Intensity", "VignetteIntensity", &settings.VignetteIntensity, defaults.VignetteIntensity, 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      slider("Film Grain Intensity", "FilmGrainIntensity", &settings.FilmGrainIntensity, defaults.FilmGrainIntensity, 1.f, "Scales the game's film grain (1 = vanilla, 0 = off).");
      bool dithering = settings.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         settings.Dithering = dithering ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");
      if (DrawResetButton(settings.Dithering, defaults.Dithering, "Dithering"))
         device_data.cb_luma_global_settings_dirty = true;

      ImGui::SeparatorText("UI");
      // Session only, so a restart never comes back without a HUD
      bool hide_gameplay_ui = settings.HideGameplayUI > 0.5f;
      if (ImGui::Checkbox("Hide Gameplay UI", &hide_gameplay_ui))
      {
         settings.HideGameplayUI = hide_gameplay_ui ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("TAA Jitter");
      if (int32_t* jitter_mode = GetJitterMode())
      {
         ImGui::Combo("TAA Jitter Pattern", jitter_mode, "None\0"
                                                         "2x (vanilla)\0"
                                                         "4x\0"
                                                         "8x (SR, Halton if enabled)\0");
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Game TAA jitter, D3D MSAA sample patterns. SR requires 8x. Not saved.");
         bool jitter_index_fixed = IsJitterIndexFixed();
         if (ImGui::Checkbox("Fix TAA Jitter Index", &jitter_index_fixed))
            SetJitterIndexFix(jitter_index_fixed);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The game's jitter counter is negative, so only the first offset of the pattern is ever used. This makes every offset cycle. Always on outside of development builds. Not saved.");
      }
      else
      {
         ImGui::TextDisabled("Unsupported SRTTR.exe build: jitter controls unavailable");
      }
#if ENABLE_SR
      bool jitter_pattern_halton_enabled = jitter_pattern_halton;
      if (ImGui::Checkbox("Halton Jitter", &jitter_pattern_halton_enabled))
      {
         // Patched while the 8x pattern isn't in use, as the other camera thread might run it
         int32_t* jitter_mode = GetJitterMode();
         const int32_t previous_jitter_mode = jitter_mode ? *jitter_mode : 0;
         if (jitter_mode)
            *jitter_mode = vanilla_jitter_mode;
         SetHaltonJitterPattern(jitter_pattern_halton_enabled);
         if (jitter_mode)
            *jitter_mode = previous_jitter_mode;
         device_data.force_reset_sr = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's 8x (D3D MSAA) jitter pattern, used by SR, with a Halton (2, 3) sequence of %i phases. Not saved.", SR::GetDefaultJitterPhases());

      ImGui::SeparatorText("Super Resolution");
      ImGui::Checkbox("Flip Jitter X", &sr_flip_jitter_x);
      ImGui::SameLine();
      ImGui::Checkbox("Flip Jitter Y", &sr_flip_jitter_y);
      if (ImGui::Checkbox("MVs Jittered", &sr_mvs_jittered))
         device_data.force_reset_sr = true;
      ImGui::TextDisabled("Camera: jitter %.6f %.6f NDC, fov %.2f deg, near %.3f, far %.1f", last_built_camera.jitter_x, last_built_camera.jitter_y, last_built_camera.projection_y_scale > 0.f ? 2.f * std::atan(1.f / last_built_camera.projection_y_scale) * 180.f / float(M_PI) : 0.f, last_built_camera.near_plane, last_built_camera.far_plane);
#endif

      ImGui::SeparatorText("SMAA");
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (plane deviation of the scene depth) as well as by colour, so texture detail stays sharp while silhouettes are antialiased. Not saved.");
      ImGui::Combo("SMAA Predication Debug View", &g_smaa_debug_view, "Off\0Edges\0Predication\0");
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the frame with SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red).\nToggle SMAA Predication to compare: texture detail should lose edges, silhouettes keep them.");

      ImGui::SeparatorText("Diagnostics");
      if (ImGui::Button("Log DLAA Inputs"))
      {
         dlaa_log_frames_left = 16;
         camera_log_calls_left = 200;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Writes TAA cbuffers, camera matrices and depth state of the next 16 frames, and the next 200 camera projection builds, to ReShade.log.");
      if (ImGui::Button("Dump SMAA Inputs"))
         g_smaa_dump = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Dumps the next SMAA frame's depth, predication and edges to %%TEMP%%\\srttr_smaa_*_<frame>.bin (in-game Anti-Aliasing = FXAA).");
      if (ImGui::Button("Log XeGTAO Inputs"))
         xegtao_log_frames_left = 2;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Writes the cbuffers and bindings of every SSAO pass and of the ambient pass, and the camera, of the next 2 frames to ReShade.log. The first frame also dumps the ambient depth, normals and AO to %%TEMP%%\\srttr_xegtao_*.bin.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Saints Row: The Third Remastered\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, replaces the game's TAA with DLAA or FSR 3 native anti-aliasing and its FXAA with SMAA, plus 16x anisotropic filtering.\n"
         "Set Anti-Aliasing in the game's display settings to TAA for DLAA and FSR 3, or to FXAA for SMAA.\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      ImGui::PopStyleColor(3);

      ImGui::NewLine();
      static const std::string social_link = std::string("Join our \"HDR Den\" Discord ") + std::string(ICON_FK_SEARCH);
      if (ImGui::Button(social_link.c_str()))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do not share for other purposes.
         static const std::string discord_link = std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      static const std::string contributing_link = std::string("Contribute on Github ") + std::string(ICON_FK_FILE_CODE);
      if (ImGui::Button(contributing_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nDICE (HDR tonemapper)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR Native AA)"
                  "\nNVIDIA NGX (DLSS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Saints Row: The Third Remastered Luma mod", "", 1);

      // Scene, UI and video are composed into the swapchain by the last draw of the frame
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // The scene, TAA and tonemap outputs are r11g11b10_float at swapchain resolution: fp16 removes banding in HDR highlights.
      // The GUI layer (with Bink video) and the TAA history stay RGBA8; the GUI layer's UNORM clamp is what bounds the Bink YCbCr conversion.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {reshade::api::format::r11g11b10_float};
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution;

      // Anisotropic samplers get 16x AF and the SR mip bias offset (added to the game's bias)
      enable_samplers_upgrade = true;
      samplers_upgrade_mode = 4;

      for (const uint32_t hash : compose_hashes)
         redirected_shader_hashes["Compose"].insert(std::format("{:08X}", hash));
      for (const uint32_t hash : compose_gui_hashes)
         redirected_shader_hashes["ComposeGUI"].insert(std::format("{:08X}", hash));

      game = new SaintsRowTheThirdRemastered();
   }
   // The camera hook detour lives in this module: remove it before it unloads
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      MH_DisableHook(MH_ALL_HOOKS);
      MH_Uninitialize();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
