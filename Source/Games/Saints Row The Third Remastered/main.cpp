#define GAME_SAINTS_ROW_THE_THIRD_REMASTERED 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// Development builds otherwise swallow focus loss, so the game keeps the cursor and can't be minimized
#define DISABLE_FOCUS_LOSS_SUPPRESSION 1

#include "..\..\Core\core.hpp"
#include "..\..\External\reshade\deps\minhook\include\MinHook.h"

namespace
{
   // SRTTR.exe addresses, valid only for the analysed build (PE TimeDateStamp 0x60EE85F4; image base 0x140000000)
   uint8_t* GetGameAddress(uintptr_t rva)
   {
      auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
      const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
      const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos_header->e_lfanew);
      return nt_headers->FileHeader.TimeDateStamp == 0x60EE85F4 ? base + rva : nullptr;
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
   // so the game's HDR setting does not change the output. The first ones compose the scene, the GUI only ones run in menus (the SDR one is 09 and 13).
   constexpr uint32_t compose_hashes[] = {0xFCCD77CD, 0xADB2056B, 0xEB9D7036, 0x50DC2D70, 0x7083C926, 0xCE7FF710, 0xA11A22A3, 0x681958CA, 0x3D126636};
   constexpr uint32_t compose_gui_hashes[] = {0xA283B6FB, 0x79D0B6FF, 0x8DFF00F4, 0x3EA6C5A9, 0x1AD38FF6, 0xDEEDDD60, 0x281056F7, 0x818B5759, 0xC165ACD1};
   // rl_prim_2d_bink_s_01, the Bink video, drawn into the RGBA8 GUI layer: it's redirected to an FP16 video layer (for its AutoHDR), which compose reads at t8
   constexpr uint32_t video_hash = 0xE85564EB;
   constexpr UINT video_layer_compose_slot = 8;

   // hdr_filter tonemap CS perms (LUT and no LUT, with and without luminance output), run in every frame with a scene
   constexpr uint32_t tonemap_hashes[] = {0x941A9154, 0x835784B0, 0xAB466B4A, 0xFEDD50B7};

   constexpr uint32_t sr_inputs_shader_hash = CompileTimeStringHash("SR Inputs");

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

   // The game binds ranges of one large constant buffer (*SetConstantBuffers1), so only the bound range is copied
   bool ReadBoundConstants(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, reshade::api::shader_stage stage, UINT slot, UINT registers, float* data)
   {
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return false;
      com_ptr<ID3D11Buffer> cb;
      UINT first = 0, count = 0;
      if (stage == reshade::api::shader_stage::vertex)
         native_device_context1->VSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      else if (stage == reshade::api::shader_stage::pixel)
         native_device_context1->PSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      else
         native_device_context1->CSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      if (!cb)
         return false;
      D3D11_BUFFER_DESC desc = {};
      cb->GetDesc(&desc);
      const UINT offset = first * 16;
      const UINT bytes = registers * 16;
      if (count < registers || offset + bytes > desc.ByteWidth)
         return false;

      D3D11_BUFFER_DESC staging_desc = {};
      staging_desc.ByteWidth = bytes;
      staging_desc.Usage = D3D11_USAGE_STAGING;
      staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      com_ptr<ID3D11Buffer> staging;
      if (FAILED(native_device->CreateBuffer(&staging_desc, nullptr, &staging)))
         return false;
      const D3D11_BOX box = {offset, 0, 0, offset + bytes, 1, 1};
      native_device_context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, cb.get(), 0, &box);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)))
         return false;
      std::memcpy(data, mapped.pData, bytes);
      native_device_context->Unmap(staging.get(), 0);
      return true;
   }

   // Raw texture dump for offline analysis (formats the DevKit cannot read back): header {width, height, dxgi_format}, then tight rows of 4 bytes per pixel
   void DumpTexture(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, ID3D11ShaderResourceView* srv, const char* name)
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
      if (desc.Format != DXGI_FORMAT_R16G16_UNORM && desc.Format != DXGI_FORMAT_R24G8_TYPELESS)
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
      const auto path = std::filesystem::temp_directory_path() / (std::string("srttr_dlaa_") + name + "_" + std::to_string(dlaa_log_frame) + ".bin");
      if (FILE* file = _wfopen(path.c_str(), L"wb"))
      {
         const uint32_t header[3] = {desc.Width, desc.Height, uint32_t(desc.Format)};
         fwrite(header, sizeof(header), 1, file);
         for (UINT y = 0; y < desc.Height; y++)
            fwrite(static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, desc.Width * 4, 1, file);
         fclose(file);
         reshade::log::message(reshade::log::level::info, ("[SRTTR DLAA] frame=" + std::to_string(dlaa_log_frame) + " dump " + path.string()).c_str());
      }
      native_device_context->Unmap(staging.get(), 0);
   }

   void LogRegisters(const char* tag, const float* data, UINT first_register, UINT last_register)
   {
      char line[256];
      for (UINT i = first_register; i <= last_register; i++)
      {
         std::snprintf(line, sizeof(line), "[SRTTR DLAA] frame=%u %s c%u=%.9g,%.9g,%.9g,%.9g", dlaa_log_frame, tag, i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
         reshade::log::message(reshade::log::level::info, line);
      }
   }

   void LogTAAInputs(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, uint32_t taa_hash)
   {
      // TAA_PARAMS cb10: c0-c3 matReprojection, c4 screenSize (uint2) + texelSize, c5 sharpness/neighbour_threshold/impulse_reduce/key_value, c6 max_weight/min_weight/frame_id (int)
      float taa[7 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::compute, 10, 7, taa))
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
         LogRegisters("TAA cb10", taa, 0, 6);
      }
      // CB_COMMON as bound for the post chain, to compare against the G-buffer one
      float common[42 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::compute, 1, 42, common))
         LogRegisters("TAA cb1", common, 34, 41);
      // Depth (t1) and motion vectors (t3) of the first two logged frames, to check the object MVs against the camera reprojection
      if (dlaa_log_frame < 2)
      {
         com_ptr<ID3D11ShaderResourceView> srvs[3];
         native_device_context->CSGetShaderResources(1, 3, &srvs[0]);
         DumpTexture(native_device, native_device_context, srvs[0].get(), "depth");
         DumpTexture(native_device, native_device_context, srvs[2].get(), "mv");
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
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::vertex, 1, 42, common))
      {
         LogRegisters("GBuffer VS cb1", common, 1, 1);
         LogRegisters("GBuffer VS cb1", common, 9, 9);
         LogRegisters("GBuffer VS cb1", common, 34, 41);
      }
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::pixel, 1, 10, common))
         LogRegisters("GBuffer PS cb1", common, 9, 9);
      // CB_VERTEX cb2: c10-c13 curr_to_prev_clip (per object, static meshes only)
      float vertex[14 * 4];
      if (ReadBoundConstants(native_device, native_device_context, reshade::api::shader_stage::vertex, 2, 14, vertex))
         LogRegisters("GBuffer VS cb2", vertex, 10, 13);
   }
} // namespace
#endif

struct SaintsRowTheThirdRemasteredGameDeviceData final : public GameDeviceData
{
   // The Bink video layer, cleared by the first video draw of each frame
   com_ptr<ID3D11Texture2D> video_layer;
   com_ptr<ID3D11RenderTargetView> video_layer_rtv;
   com_ptr<ID3D11ShaderResourceView> video_layer_srv;
   bool video_drawn = false;

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
      // The tonemap CS writes display encoded (gamma 2.2) values into a float texture, compose copies them to the swapchain through a UNORM view
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The tonemap scales the scene by game / UI paper white so the GUI layer blended over it in gamma space by compose lands at UI paper white
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // The game binds cbuffers 0-4 and 6-10
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f; // Peak ~165 nits
      default_luma_global_game_settings.HideGameplayUI = 0.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

#if ENABLE_SR
      native_shaders_definitions.emplace(sr_inputs_shader_hash, ShaderDefinition{"Luma_SRTTR_SRInputs", reshade::api::pipeline_subobject_type::compute_shader});
      // SR takes its jitter from the patched game code, which only exists in the analysed build
      sr_game_tooltip = GetGameAddress(0) ? "Requires \"Anti-Aliasing\" set to \"TAA\" in the game's display settings.\n" : "Unsupported game executable version: Super Resolution can't engage.\n";
#endif
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
         if (!original_draw_dispatch_func || !*original_draw_dispatch_func)
            return DrawOrDispatchOverrideType::None;
         auto& game_device_data = GetGameDeviceData(device_data);

         if (original_shader_hashes.Contains(video_hash, reshade::api::shader_stage::pixel))
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            if (!rtvs[0])
               return DrawOrDispatchOverrideType::None;
            com_ptr<ID3D11Resource> gui_layer;
            rtvs[0]->GetResource(&gui_layer);
            if (!game_device_data.video_layer || !AreResourcesEqual(game_device_data.video_layer.get(), gui_layer.get(), false))
            {
               game_device_data.video_layer_rtv = nullptr;
               game_device_data.video_layer_srv = nullptr;
               game_device_data.video_layer = CloneTexture<ID3D11Texture2D>(native_device, gui_layer.get(), DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, false, false);
               HRESULT hr = game_device_data.video_layer ? native_device->CreateRenderTargetView(game_device_data.video_layer.get(), nullptr, &game_device_data.video_layer_rtv) : E_FAIL;
               if (SUCCEEDED(hr))
                  hr = native_device->CreateShaderResourceView(game_device_data.video_layer.get(), nullptr, &game_device_data.video_layer_srv);
               ASSERT_ONCE(SUCCEEDED(hr));
               if (FAILED(hr))
               {
                  game_device_data.video_layer = nullptr; // The views are reset before the next creation
                  return DrawOrDispatchOverrideType::None;
               }
            }

            if (!game_device_data.video_drawn)
            {
               constexpr float transparent[4] = {};
               native_device_context->ClearRenderTargetView(game_device_data.video_layer_rtv.get(), transparent);
               game_device_data.video_drawn = true;
            }
            // Same blend state and viewport as the GUI layer, which gets the UI drawn on top later
            ID3D11RenderTargetView* const video_layer_rtv = game_device_data.video_layer_rtv.get();
            native_device_context->OMSetRenderTargets(1, &video_layer_rtv, dsv.get());
            (*original_draw_dispatch_func)();
            ID3D11RenderTargetView* const* original_rtvs = reinterpret_cast<ID3D11RenderTargetView* const*>(&rtvs[0]);
            native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, original_rtvs, dsv.get());
            return DrawOrDispatchOverrideType::Replaced;
         }

         const auto is_compose = [&](uint32_t hash)
         { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::pixel); };
         if (std::any_of(std::begin(compose_hashes), std::end(compose_hashes), is_compose) || std::any_of(std::begin(compose_gui_hashes), std::end(compose_gui_hashes), is_compose))
         {
            ID3D11ShaderResourceView* const video_layer_srv = game_device_data.video_drawn ? game_device_data.video_layer_srv.get() : nullptr;
            native_device_context->PSSetShaderResources(video_layer_compose_slot, 1, &video_layer_srv);
            (*original_draw_dispatch_func)();
            ID3D11ShaderResourceView* const null_srv = nullptr;
            native_device_context->PSSetShaderResources(video_layer_compose_slot, 1, &null_srv);
            return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      const auto is_compute_shader = [&](uint32_t hash)
      { return original_shader_hashes.Contains(hash, reshade::api::shader_stage::compute); };
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
      game_device_data.video_drawn = false;
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
      auto& settings = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
      reshade::get_config_value(nullptr, NAME, "Saturation", settings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", settings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", settings.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", settings.VideoAutoHDRBoost);
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

      ImGui::SeparatorText("Grade");
      slider("Exposure", "Exposure", &settings.Exposure, defaults.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      slider("Saturation", "Saturation", &settings.Saturation, defaults.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      slider("Highlights Desaturation", "HighlightsDesaturation", &settings.HighlightsDesaturation, defaults.HighlightsDesaturation, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");

      // Returns whether the toggle is on
      const auto toggle = [&](const char* label, const char* key, float* value, float default_value, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = enabled ? 1.f : 0.f;
            device_data.cb_luma_global_settings_dirty = true;
            reshade::set_config_value(nullptr, NAME, key, *value);
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
         return *value > 0.5f;
      };

      ImGui::SeparatorText("Effects");
      ImGui::BeginDisabled(!toggle("Video AutoHDR", "VideoAutoHDREnable", &settings.VideoAutoHDREnable, defaults.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only)."));
      slider("Video HDR Boost", "VideoAutoHDRBoost", &settings.VideoAutoHDRBoost, defaults.VideoAutoHDRBoost, 1.f, "Video highlight strength (0 = off).");
      ImGui::EndDisabled();
      toggle("Dithering", "Dithering", &settings.Dithering, defaults.Dithering, "Reduces gradient banding.");

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
      if (ImGui::Button("Log DLAA Inputs"))
      {
         dlaa_log_frames_left = 16;
         camera_log_calls_left = 200;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Writes TAA cbuffers, camera matrices and depth state of the next 16 frames, and the next 200 camera projection builds, to ReShade.log.");
      if (int32_t* jitter_mode = GetJitterMode())
      {
         ImGui::SliderInt("TAA Jitter Pattern", jitter_mode, 0, 3);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Game TAA jitter: 0 none, 1 2x (vanilla), 2 4x, 3 8x (D3D MSAA patterns, 3 is replaced by Halton and required by SR). Not saved.");
         bool jitter_index_fixed = IsJitterIndexFixed();
         if (ImGui::Checkbox("Fix TAA Jitter Index", &jitter_index_fixed))
            SetJitterIndexFix(jitter_index_fixed);
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The game's jitter counter is negative, so only the first offset of the pattern is ever used. This makes every offset cycle. Always on outside of development builds. Not saved.");
      }
      else
      {
         ImGui::TextUnformatted("TAA Jitter Pattern: unsupported SRTTR.exe build");
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
      ImGui::Checkbox("SR Flip Jitter X", &sr_flip_jitter_x);
      ImGui::Checkbox("SR Flip Jitter Y", &sr_flip_jitter_y);
      if (ImGui::Checkbox("SR MVs Jittered", &sr_mvs_jittered))
         device_data.force_reset_sr = true;
      ImGui::Text("SR camera: jitter %.6f %.6f NDC, fov %.2f deg, near %.3f, far %.1f", last_built_camera.jitter_x, last_built_camera.jitter_y, last_built_camera.projection_y_scale > 0.f ? 2.f * std::atan(1.f / last_built_camera.projection_y_scale) * 180.f / float(M_PI) : 0.f, last_built_camera.near_plane, last_built_camera.far_plane);
#endif
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::Text("Saints Row: The Third Remastered Luma mod - about and credits section", "");
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
