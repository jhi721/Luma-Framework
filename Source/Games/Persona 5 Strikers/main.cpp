#define GAME_PERSONA_5_STRIKERS 1

// Core only checks whether this is defined (any value skips the "attach the debugger" popup)
#define DISABLE_AUTO_DEBUGGER 1

// Movies play through a separate DX9 device (Media Foundation), same as Nioh
#define CHECK_GRAPHICS_API_COMPATIBILITY 1
// SMAA runs right after the composite, through "original_draw_dispatch_func"
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
#define ENABLE_SMAA 1
// The UI shaders get a saturate appended in place (see "PatchShaderBytecodeSync")
#define LUMA_PATCH_BYTECODE_SYNC 1

#include "..\..\Core\core.hpp"
#include "..\..\External\WDK\includes\d3d11TokenizedProgramFormat.hpp"

namespace
{
   // Katana engine PostEffect3 composite: exposure, lens effects, vignette, then the baked HDR 3D LUT (tonemap + grade).
   // Every captured scene (menus, dialogue, hub, field) runs it once, writing straight into the swapchain.
   // The main menu runs it a second time into an off-screen RGBA8 target.
   constexpr uint32_t composite_hash = 0x45A96F2D;
   // FXAA 3: the game copies the swapchain (after the UI) and FXAAs the copy back into it
   constexpr uint32_t fxaa_hash = 0xED2D9823;
   // PostEffect3 ApplyFxaa{,Repair,Console,Quality}PS: the engine's own FXAA, which also carries the radial blur. When one of them runs, the
   // composite goes to an intermediate target and this pass draws it into the swapchain. Never seen in a capture yet.
   const ShaderHashesList shader_hashes_apply_fxaa = {.pixel_shaders = {0x0B6569A5, 0xC8A7BA1C, 0x95F3321A, 0xED7941FD}};
   // The SSAO depth downsample, whose t0 is the full res D32_FLOAT_S8X24 depth (a copy the game makes right before post): the SMAA predication input
   constexpr uint32_t ssao_depth_downsample_hash = 0x6E15840A;
   // The native SSAO calculate (half res R8 visibility), replaced by XeGTAO. Its two depth aware blurs, which upsample to full
   // res, and the merge into the G-buffer AO (gbuf0.a = min(material AO, SSAO), read by the deferred lighting) stay vanilla.
   constexpr uint32_t ssao_hash = 0x63435B03;
   // The UI pixel shaders, drawing into the swapchain (vanilla BGRA8 UNORM), which clamped their output before blending. Their texture
   // times vertex color, blend mode and saturation control (grey + k * (color - grey), grey a 0.299/0.587/0.114 weighted RGB sum, k a
   // cb0 scalar) go outside 0-1, which the fp16 swapchain no longer clamps. The whole family ends in that saturation tail; found by
   // disassembling all the dumped pixel shaders.
   // Each has a single o0.xyzw write and a single final ret.
   const std::unordered_set<uint32_t> ui_pixel_shaders = {0x90C6B12E, 0xEE9FC290, 0x07378D54, 0x4A0CB253, 0x8BA60D22, 0xD1BEFD65, 0xF0863953, 0x76C3BC5E, 0xA4DFC750, 0xBEF2C79E};

   bool g_hide_ui = false; // Session only, so a restart never comes back without a HUD

   bool g_gtao_enable = true;
   constexpr UINT gtao_knobs_cb_slot = 9; // "register(b9)" in Luma_P5S_XeGTAO.hlsl; b11 is core DrawBloom's
   float g_gtao_final_value_power = 1.f;  // DEV/TEST calibration knobs, not persisted
   float g_gtao_radius_override = 0.f;    // > 0 overrides the native radius (centimetres)
#if DEVELOPMENT
   int g_gtao_debug_view = 0; // 0=off 1=depth gradient 2=normals 3=AO x8 4=edges
#endif

#if DEVELOPMENT
   bool g_smaa_predication = true;
   int g_smaa_debug_view = 0; // 0 off, 1 edges, 2 predication
   std::atomic<bool> g_smaa_dump = false;
   std::atomic<bool> g_xegtao_log = false;                          // Consumed at the next present, which opens the logged frame
   std::atomic<bool> g_ui_dump = false;                             // Consumed at the next present too
   constexpr D3D11_BOX ui_dump_box = {2200, 700, 0, 3400, 1600, 1}; // Around the menu cursor at 3840x2160

   // XeGTAO research: the native SSAO chain (snapshot order) and the passes after it that may consume its AO, all on a deferred context.
   // Their bindings are logged when drawn, their PS constants and outputs (and the SSAO inputs) are read back.
   struct SSAOChainPass
   {
      uint32_t hash;
      const char* name;
   };
   constexpr SSAOChainPass ssao_chain_passes[] = {
      {ssao_depth_downsample_hash, "SSAO Depth Downsample"},
      {ssao_hash, "SSAO"},
      {0xDEBA65FD, "SSAO Pass 3"},
      {0x4D8EC71C, "SSAO Pass 4"},
      {0xC72A1A10, "After SSAO"},
      {0x4376F855, "Lighting"},
      {0x691D080F, "Deferred Lighting"},
   };
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

} // namespace

// Everything that holds a device object, so it is released with its device.
struct Persona5StrikersGameDeviceData final : public GameDeviceData
{
   // SMAA scratch, recreated when the canvas size changes: a linear copy of the canvas (SMAA writes the canvas, so it cannot
   // also sample it), its gamma encode (the edge detection input; with RCAS also SMAA's output, read by the finalize pass) and
   // the predication edge-ness.
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_gamma_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;

   // The scene depth for predication, from the SSAO depth downsample. The texture is persistent and written by the game before post
   // every frame, so the view is kept (the SSAO and post passes record on different deferred contexts, so per frame resets would race).
   std::mutex smaa_depth_mutex;
   com_ptr<ID3D11ShaderResourceView> smaa_depth_srv;

   // Set when SMAA ran after this frame's composite, so the vanilla FXAA is skipped
   std::atomic<bool> smaa_drawn = false;

   // MIN and MAX blends (all channels, alpha only), to clamp the swapchain to 0-1 under a UI draw's own geometry (see "OnDrawOrDispatch")
   com_ptr<ID3D11BlendState> ui_min_blend_states[2];
   com_ptr<ID3D11BlendState> ui_max_blend_states[2];

   // XeGTAO scratch, recreated when the SSAO target size changes. The SSAO records on worker threads (deferred contexts), so
   // everything below is guarded by the mutex.
   std::mutex gtao_mutex;
   com_ptr<ID3D11Texture2D> gtao_depth_mips_texture; // R32F view space depth pyramid, 5 mips
   com_ptr<ID3D11UnorderedAccessView> gtao_depth_mip_uavs[5];
   com_ptr<ID3D11ShaderResourceView> gtao_depth_mips_srv;
   com_ptr<ID3D11UnorderedAccessView> gtao_working_uavs[2]; // R8G8_UNORM AO + edges ping-pong
   com_ptr<ID3D11ShaderResourceView> gtao_working_srvs[2];
   com_ptr<ID3D11Texture2D> gtao_final_texture; // R8_UNORM copy source for the game's target
   com_ptr<ID3D11UnorderedAccessView> gtao_final_uav;
   uint32_t gtao_width = 0;
   uint32_t gtao_height = 0;
   com_ptr<ID3D11Buffer> gtao_knobs_cb; // immutable, recreated when a knob changes
   float gtao_knobs[8] = {};

   void ReleaseGTAOScratch()
   {
      gtao_depth_mips_texture.reset();
      for (auto& uav : gtao_depth_mip_uavs)
         uav.reset();
      gtao_depth_mips_srv.reset();
      for (auto& uav : gtao_working_uavs)
         uav.reset();
      for (auto& srv : gtao_working_srvs)
         srv.reset();
      gtao_final_texture.reset();
      gtao_final_uav.reset();
      gtao_width = 0;
      gtao_height = 0;
   }

#if DEVELOPMENT
   // Readbacks (texture and constant buffer range copies) recorded into staging resources on the context that draws the pass, which is
   // deferred and so cannot map. Written out a few presents later, once their command lists ran: textures to
   // %TEMP%\p5s_<name>.bin (header {width, height, dxgi_format}, then tight rows), constants to the log.
   struct Readback
   {
      std::string name;
      com_ptr<ID3D11Texture2D> texture;
      com_ptr<ID3D11Buffer> buffer;
   };
   std::mutex readback_mutex;
   std::vector<Readback> readbacks;
   std::atomic<bool> readbacks_queued = false;
   int readback_presents_left = 0;           // Present thread only
   std::atomic<bool> xegtao_logging = false; // The frame whose SSAO chain is logged
   std::atomic<bool> ui_dumping = false;     // The frame whose UI draws are dumped
   std::atomic<uint32_t> ui_dump_index = 0;
#endif
};

class Persona5Strikers final : public Game
{
   static Persona5StrikersGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<Persona5StrikersGameDeviceData*>(device_data.game);
   }

   static bool IsBackBuffer(DeviceData* device_data, ID3D11Resource* resource)
   {
      const std::shared_lock lock(device_data->mutex);
      return device_data->back_buffers.contains(reinterpret_cast<uint64_t>(resource));
   }

#if DEVELOPMENT
   // Queues a copy of the view's texture (subresource 0, so layer 0 of arrays, optionally a box of it), if its format can be written out
   static void QueueTextureReadback(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Persona5StrikersGameDeviceData* game_device_data, ID3D11View* view, std::string name, D3D11_BOX box = {0, 0, 0, UINT_MAX, UINT_MAX, 1})
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
      if (reshade::api::format_row_pitch(reshade::api::format(desc.Format), 1) == 0 || desc.SampleDesc.Count != 1)
         return;
      box.right = (std::min)(box.right, desc.Width);
      box.bottom = (std::min)(box.bottom, desc.Height);
      if (box.left >= box.right || box.top >= box.bottom)
         return;
      desc.Width = box.right - box.left;
      desc.Height = box.bottom - box.top;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      com_ptr<ID3D11Texture2D> staging;
      if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &staging)))
         return;
      native_device_context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, texture.get(), 0, &box);
      const std::lock_guard lock(game_device_data->readback_mutex);
      game_device_data->readbacks.push_back({std::move(name), std::move(staging), nullptr});
      game_device_data->readbacks_queued = true;
   }

   // Queues a copy of the pixel shader constant buffer range bound at "slot" (the game binds ranges of large buffers), up to 64 registers
   static void QueuePSConstantsReadback(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Persona5StrikersGameDeviceData* game_device_data, UINT slot, std::string name)
   {
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return;
      com_ptr<ID3D11Buffer> cb;
      UINT first = 0, count = 0;
      native_device_context1->PSGetConstantBuffers1(slot, 1, &cb, &first, &count);
      if (!cb)
         return;
      D3D11_BUFFER_DESC desc = {};
      cb->GetDesc(&desc);
      // Bound with the non range API, the range reads as 0 or as 4096 registers from 0, so clamp it to the buffer
      const UINT buffer_registers = desc.ByteWidth / 16;
      if (first >= buffer_registers)
         return;
      if (count == 0 || first + count > buffer_registers)
         count = buffer_registers - first;
      const UINT offset = first * 16;
      const UINT bytes = (std::min)(count, 64u) * 16;
      D3D11_BUFFER_DESC staging_desc = {};
      staging_desc.ByteWidth = bytes;
      staging_desc.Usage = D3D11_USAGE_STAGING;
      staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      com_ptr<ID3D11Buffer> staging;
      if (FAILED(native_device->CreateBuffer(&staging_desc, nullptr, &staging)))
         return;
      const D3D11_BOX box = {offset, 0, 0, offset + bytes, 1, 1};
      native_device_context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, cb.get(), 0, &box);
      const std::lock_guard lock(game_device_data->readback_mutex);
      game_device_data->readbacks.push_back({std::move(name), nullptr, std::move(staging)});
      game_device_data->readbacks_queued = true;
   }

   static void FlushReadbacks(ID3D11Device* native_device, Persona5StrikersGameDeviceData* game_device_data)
   {
      std::vector<Persona5StrikersGameDeviceData::Readback> readbacks;
      {
         const std::lock_guard lock(game_device_data->readback_mutex);
         readbacks.swap(game_device_data->readbacks);
      }
      com_ptr<ID3D11DeviceContext> immediate_context;
      native_device->GetImmediateContext(&immediate_context);
      for (const auto& readback : readbacks)
      {
         ID3D11Resource* const resource = readback.texture ? static_cast<ID3D11Resource*>(readback.texture.get()) : readback.buffer.get();
         D3D11_MAPPED_SUBRESOURCE mapped = {};
         if (FAILED(immediate_context->Map(resource, 0, D3D11_MAP_READ, 0, &mapped)))
            continue;
         if (readback.texture)
         {
            D3D11_TEXTURE2D_DESC desc = {};
            readback.texture->GetDesc(&desc);
            const auto path = std::filesystem::temp_directory_path() / ("p5s_" + readback.name + ".bin");
            if (FILE* file = _wfopen(path.c_str(), L"wb"))
            {
               const uint32_t header[3] = {desc.Width, desc.Height, uint32_t(desc.Format)};
               fwrite(header, sizeof(header), 1, file);
               // Rows of pixels, or of 4x4 blocks for compressed formats
               const auto format = reshade::api::format(desc.Format);
               const uint32_t row_bytes = reshade::api::format_row_pitch(format, desc.Width);
               const uint32_t rows = reshade::api::format_slice_pitch(format, row_bytes, desc.Height) / row_bytes;
               for (UINT y = 0; y < rows; y++)
                  fwrite(static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, row_bytes, 1, file);
               fclose(file);
               reshade::log::message(reshade::log::level::info, ("[P5S] dump " + path.string()).c_str());
            }
         }
         else
         {
            D3D11_BUFFER_DESC desc = {};
            readback.buffer->GetDesc(&desc);
            const float* const data = static_cast<const float*>(mapped.pData);
            for (UINT i = 0; i < desc.ByteWidth / 16; i++)
               reshade::log::message(reshade::log::level::info, std::format("{} c{}={:.9g},{:.9g},{:.9g},{:.9g}", readback.name, i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]).c_str());
         }
         immediate_context->Unmap(resource, 0);
      }
   }

   // Logs one SSAO chain pass: viewport, SRVs, RTVs and DSV now, its PS constants and output (and the SSAO's inputs) through readbacks
   static DrawOrDispatchOverrideType LogSSAOChainPass(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Persona5StrikersGameDeviceData* game_device_data, const SSAOChainPass& pass, std::function<void()>* original_draw_dispatch_func)
   {
      const uint32_t frame = cb_luma_global_settings.FrameIndex;
      const std::string prefix = std::format("[P5S XeGTAO] frame={} {} 0x{:08X}", frame, pass.name, pass.hash);
      const auto log = [](const std::string& line)
      { reshade::log::message(reshade::log::level::info, line.c_str()); };
      const auto log_texture = [&](const char* slot, UINT index, ID3D11View* view, int view_format, int view_dimension)
      {
         com_ptr<ID3D11Resource> resource;
         view->GetResource(&resource);
         com_ptr<ID3D11Texture2D> texture;
         D3D11_TEXTURE2D_DESC desc = {};
         if (SUCCEEDED(resource->QueryInterface(&texture)))
            texture->GetDesc(&desc);
         log(std::format("{} {}{} res={} {}x{} array={} mips={} format={} view_format={} view_dimension={}", prefix, slot, index, static_cast<void*>(resource.get()), desc.Width, desc.Height, desc.ArraySize, desc.MipLevels, int(desc.Format), view_format, view_dimension));
      };

      D3D11_VIEWPORT viewport = {};
      UINT viewports = 1;
      native_device_context->RSGetViewports(&viewports, &viewport);
      log(std::format("{} viewport={},{} {}x{}", prefix, viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height));
      com_ptr<ID3D11ShaderResourceView> srvs[16];
      native_device_context->PSGetShaderResources(0, 16, &srvs[0]);
      for (UINT i = 0; i < 16; i++)
      {
         if (!srvs[i])
            continue;
         D3D11_SHADER_RESOURCE_VIEW_DESC desc;
         srvs[i]->GetDesc(&desc);
         log_texture("t", i, srvs[i].get(), int(desc.Format), int(desc.ViewDimension));
      }
      com_ptr<ID3D11RenderTargetView> rtvs[8];
      com_ptr<ID3D11DepthStencilView> dsv;
      native_device_context->OMGetRenderTargets(8, &rtvs[0], &dsv);
      for (UINT i = 0; i < 8; i++)
      {
         if (!rtvs[i])
            continue;
         D3D11_RENDER_TARGET_VIEW_DESC desc;
         rtvs[i]->GetDesc(&desc);
         log_texture("rt", i, rtvs[i].get(), int(desc.Format), int(desc.ViewDimension));
      }
      if (dsv)
      {
         D3D11_DEPTH_STENCIL_VIEW_DESC desc;
         dsv->GetDesc(&desc);
         log_texture("dsv", 0, dsv.get(), int(desc.Format), int(desc.ViewDimension));
      }

      for (UINT slot = 0; slot < 5; slot++)
         QueuePSConstantsReadback(native_device, native_device_context, game_device_data, slot, std::format("{} cb{}", prefix, slot));
      if (pass.hash == ssao_hash)
      {
         for (UINT i = 0; i < 4; i++)
            QueueTextureReadback(native_device, native_device_context, game_device_data, srvs[i].get(), std::format("xegtao_{:08X}_t{}_{}", pass.hash, i, frame));
      }
      // The output after the pass drew
      if (!original_draw_dispatch_func || !*original_draw_dispatch_func)
         return DrawOrDispatchOverrideType::None;
      (*original_draw_dispatch_func)();
      QueueTextureReadback(native_device, native_device_context, game_device_data, rtvs[0].get(), std::format("xegtao_{:08X}_rt0_{}", pass.hash, frame));
      return DrawOrDispatchOverrideType::Replaced;
   }
#endif

public:
   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto* game_device_data = new Persona5StrikersGameDeviceData;
      device_data.game = game_device_data;
      for (int i = 0; i < 2; i++)
      {
         const UINT8 write_mask = i == 0 ? D3D11_COLOR_WRITE_ENABLE_ALL : D3D11_COLOR_WRITE_ENABLE_ALPHA;
         D3D11_BLEND_DESC blend_desc = {};
         blend_desc.RenderTarget[0] = {TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, write_mask};
         native_device->CreateBlendState(&blend_desc, &game_device_data->ui_min_blend_states[i]);
         blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
         blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
         native_device->CreateBlendState(&blend_desc, &game_device_data->ui_max_blend_states[i]);
      }
   }

   // "mov_sat o0.xyzw, o0.xyzw" before the final ret: the clamp the vanilla UNORM swapchain applied to the UI (as in Yakuza 3 Remastered)
   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      constexpr uint32_t ret_token = ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_RET) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(1);
      if (type != reshade::api::pipeline_subobject_type::pixel_shader || !ui_pixel_shaders.contains(uint32_t(shader_hash)) || size % sizeof(uint32_t) != 0 || size < sizeof(uint32_t) || reinterpret_cast<const uint32_t*>(code)[size / sizeof(uint32_t) - 1] != ret_token)
         return nullptr;
      // Encoded by hand rather than with ShaderPatching::GetSatInstruction, whose source operand uses the mask selection mode;
      // fxc encodes sources as swizzles.
      constexpr uint32_t operand_o0 = ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(D3D10_SB_OPERAND_4_COMPONENT) | ENCODE_D3D10_SB_OPERAND_TYPE(D3D10_SB_OPERAND_TYPE_OUTPUT) | ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D) | ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(0, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      constexpr uint32_t patch[] = {
         ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(5) | ENCODE_D3D10_SB_INSTRUCTION_SATURATE(true),
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL, 0,
         operand_o0 | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE, 0};
      static_assert(patch[0] == 0x05002036 && patch[1] == 0x001020F2 && patch[3] == 0x00102E46);
      const size_t ret_offset = size - sizeof(uint32_t);
      auto new_code = std::make_unique<std::byte[]>(size + sizeof(patch));
      std::memcpy(new_code.get(), code, ret_offset);
      std::memcpy(new_code.get() + ret_offset, patch, sizeof(patch));
      std::memcpy(new_code.get() + ret_offset + sizeof(patch), code + ret_offset, sizeof(uint32_t));
      size += sizeof(patch);
      return new_code;
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
      // GameDeviceData lacks a virtual destructor; delete through the concrete type to release derived members.
      delete static_cast<Persona5StrikersGameDeviceData*>(device_data.game);
      device_data.game = nullptr;
   }

   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - SDR: Vanilla (reference)\n1 - HDR: LUT grade extended above mid gray + DICE display map", 1},
         {"XE_GTAO_QUALITY", '3', true, false, "XeGTAO quality (slice count)\n0 - Low\n1 - Medium\n2 - High\n3 - Very High\n4 - Ultra", 4},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('1'); // The composite, UI and FXAA write the swapchain through sRGB views, so in linear
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('0'); // sRGB (implicit, through the swapchain views)
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2'); // The UI blends straight onto the swapchain after the composite

      // The game binds b0-b4
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      default_luma_global_game_settings.Dithering = 1.f;
      default_luma_global_game_settings.SMAAEnable = 1.f;
      default_luma_global_game_settings.VignetteIntensity = 1.f;
      default_luma_global_game_settings.LensFlareIntensity = 1.f;
      default_luma_global_game_settings.Exposure = 1.f;
      default_luma_global_game_settings.ColorGradingIntensity = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.BloomIntensity = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Encode CS"), ShaderDefinition{"Luma_P5S_SMAAEncode", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Predication CS"), ShaderDefinition{"Luma_P5S_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S SMAA Finalize PS"), ShaderDefinition{"Luma_P5S_SMAAFinalize", reshade::api::pipeline_subobject_type::pixel_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Draw White PS"), ShaderDefinition{"Luma_DrawColor_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, {{"COLOR", "float4(1.0, 1.0, 1.0, 1.0)"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S Draw Black PS"), ShaderDefinition{"Luma_DrawColor_PS", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, nullptr, {{"COLOR", "float4(0.0, 0.0, 0.0, 0.0)"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S UI Peak Clamp PS"), ShaderDefinition{"Luma_P5S_UIPeakClamp", reshade::api::pipeline_subobject_type::pixel_shader});
      // XeGTAO passes (Luma_P5S_XeGTAO.hlsl); the two denoisers differ only by XE_GTAO_FINAL_APPLY.
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Prefilter Depths CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "prefilter_depths16x16_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Main Pass CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "main_pass_cs"});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 1 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "0"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("P5S XeGTAO Denoise Pass 2 CS"), ShaderDefinition{"Luma_P5S_XeGTAO", reshade::api::pipeline_subobject_type::compute_shader, nullptr, "denoise_pass_cs", {{"XE_GTAO_FINAL_APPLY", "1"}}});
   }

   // Draws the composite, then SMAA on the canvas it wrote (the swapchain), before the UI: copy, gamma encode, predication,
   // SMAA into the gamma copy, then the finalize pass (RCAS, decode, dither) back into the canvas; without RCAS, SMAA straight into the canvas.
   // Anything missing (shaders still compiling, an unexpected target) leaves the composite alone, and the vanilla FXAA runs.
   static DrawOrDispatchOverrideType DrawCompositeWithSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool* updated_cbuffers, const std::function<void()>& original_draw_dispatch_func)
   {
      com_ptr<ID3D11RenderTargetView> canvas_rtv;
      native_device_context->OMGetRenderTargets(1, &canvas_rtv, nullptr);
      com_ptr<ID3D11Resource> canvas_resource;
      if (canvas_rtv)
         canvas_rtv->GetResource(&canvas_resource);
      com_ptr<ID3D11Texture2D> canvas_texture;
      // Not the main menu's second composite, into an off-screen target
      if (!canvas_resource || FAILED(canvas_resource->QueryInterface(&canvas_texture)) || !IsBackBuffer(&device_data, canvas_resource.get()))
         return DrawOrDispatchOverrideType::None;
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.smaa_drawn = false;
      D3D11_TEXTURE2D_DESC canvas_desc;
      canvas_texture->GetDesc(&canvas_desc);
      // The upgraded (linear fp16) swapchain, which the SMAA copies are in
      if (canvas_desc.SampleDesc.Count != 1 || canvas_desc.ArraySize != 1 || (canvas_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && canvas_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS))
         return DrawOrDispatchOverrideType::None;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at".
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h, "Copy VS"_h) || !HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h, "P5S SMAA Finalize PS"_h) || !HasShaders(device_data.native_compute_shaders, "P5S SMAA Encode CS"_h))
         return DrawOrDispatchOverrideType::None;

      D3D11_TEXTURE2D_DESC scratch_desc = {};
      if (game_device_data.smaa_linear_texture)
         game_device_data.smaa_linear_texture->GetDesc(&scratch_desc);
      if (scratch_desc.Width != canvas_desc.Width || scratch_desc.Height != canvas_desc.Height)
      {
         game_device_data.smaa_linear_texture.reset();
         game_device_data.smaa_linear_srv.reset();
         game_device_data.smaa_gamma_srv.reset();
         game_device_data.smaa_gamma_rtv.reset();
         game_device_data.smaa_gamma_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         game_device_data.smaa_predication_uav.reset();
         D3D11_TEXTURE2D_DESC desc = canvas_desc;
         desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
         desc.MipLevels = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_linear_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv));
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> gamma_texture;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gamma_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) && SUCCEEDED(native_device->CreateRenderTargetView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)) && SUCCEEDED(native_device->CreateUnorderedAccessView(gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_uav));
         if (!ok)
         {
            // Retried next frame (the size check above sees no texture)
            game_device_data.smaa_linear_texture.reset();
            return DrawOrDispatchOverrideType::None;
         }
         // Without it SMAA simply runs unpredicated.
         desc.Format = DXGI_FORMAT_R16_FLOAT;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         com_ptr<ID3D11Texture2D> predication_texture;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &predication_texture)) || FAILED(native_device->CreateUnorderedAccessView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_uav)) || FAILED(native_device->CreateShaderResourceView(predication_texture.get(), nullptr, &game_device_data.smaa_predication_srv)))
         {
            game_device_data.smaa_predication_uav.reset();
            game_device_data.smaa_predication_srv.reset();
         }
      }

      // Predication depth, if captured and canvas sized. Anything else falls back to plain ULTRA.
      com_ptr<ID3D11ShaderResourceView> depth_srv;
      bool predication_available = game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "P5S SMAA Predication CS"_h);
#if DEVELOPMENT
      predication_available = predication_available && g_smaa_predication;
#endif
      if (predication_available)
      {
         const std::lock_guard lock(game_device_data.smaa_depth_mutex);
         depth_srv = game_device_data.smaa_depth_srv;
      }
      if (depth_srv)
      {
         uint4 depth_size;
         DXGI_FORMAT unused_format;
         GetResourceInfo(depth_srv.get(), depth_size, unused_format);
         if (depth_size.x != canvas_desc.Width || depth_size.y != canvas_desc.Height)
            depth_srv.reset();
      }

#if DEVELOPMENT
      // Every intermediate of this frame, from the composite's output on
      const bool dump = g_smaa_dump.exchange(false);
      const uint32_t frame = cb_luma_global_settings.FrameIndex;
      const auto queue_dump = [&](ID3D11View* view, const char* name)
      {
         if (dump)
            QueueTextureReadback(native_device, native_device_context, &game_device_data, view, std::format("smaa_{}_{}", name, frame));
      };
#endif

      // The replaced composite reads the Luma cbuffers, which Core only binds after this callback returns. They stay bound for
      // SMAA and the finalize pass: the settings for the canvas size, the data for the predication scale (CustomData3, never 0
      // here, which also tells the composite to leave the dither to the end of the SMAA chain).
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, depth_srv ? 2.f : 1.f);
      *updated_cbuffers = true;
      original_draw_dispatch_func();
#if DEVELOPMENT
      queue_dump(canvas_rtv.get(), "canvas");
#endif
      native_device_context->CopyResource(game_device_data.smaa_linear_texture.get(), canvas_resource.get());
#if DEVELOPMENT
      queue_dump(game_device_data.smaa_linear_srv.get(), "linear");
#endif

      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const gamma_uav = game_device_data.smaa_gamma_uav.get();
         ID3D11ShaderResourceView* const linear_srv = game_device_data.smaa_linear_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &gamma_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &linear_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at("P5S SMAA Encode CS"_h).get(), nullptr, 0);
         native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         if (depth_srv)
         {
            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const raw_depth_srv = depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &raw_depth_srv);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("P5S SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((canvas_desc.Width + 7) / 8, (canvas_desc.Height + 7) / 8, 1);
         }
         compute_state.Restore(native_device_context);
      }
#if DEVELOPMENT
      queue_dump(game_device_data.smaa_gamma_srv.get(), "gamma");
#endif

      // Without RCAS, SMAA writes (and dithers) the canvas directly: it only samples the linear copy, and the finalize pass would just decode
      const bool sharpen = cb_luma_global_settings.GameSettings.RCASSharpness > 0.f;
      DrawSMAA(native_device, native_device_context, device_data, sharpen ? game_device_data.smaa_gamma_rtv.get() : canvas_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
#if DEVELOPMENT
      queue_dump(sharpen ? static_cast<ID3D11View*>(game_device_data.smaa_gamma_srv.get()) : canvas_rtv.get(), "output");
#endif

      DrawStateStack<DrawStateStackType::FullGraphics> finalize_state;
      finalize_state.Cache(native_device_context, device_data.uav_max_count);
      if (sharpen)
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("P5S SMAA Finalize PS"_h).get(), game_device_data.smaa_gamma_srv.get(), canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
#if DEVELOPMENT
      // Calibration aid: SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red) replace the frame
      ID3D11ShaderResourceView* const edges_srv = device_data.managed_resources.shader_resource_views["smaa_edge_detection"_h].get();
      ID3D11ShaderResourceView* const debug_srv = g_smaa_debug_view == 1 ? edges_srv : (g_smaa_debug_view == 2 && depth_srv ? game_device_data.smaa_predication_srv.get() : nullptr);
      if (debug_srv && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), debug_srv, canvas_rtv.get(), canvas_desc.Width, canvas_desc.Height, false);
      queue_dump(depth_srv.get(), "depth");
      if (depth_srv)
         queue_dump(game_device_data.smaa_predication_srv.get(), "predication");
      queue_dump(edges_srv, "edges");
      queue_dump(canvas_rtv.get(), "final");
#endif
      finalize_state.Restore(native_device_context);

      game_device_data.smaa_drawn = true;
      return DrawOrDispatchOverrideType::Replaced;
   }

   // XeGTAO in place of the SSAO calculate draw: prefilter, main pass and two denoisers on the draw's own inputs (t0 half res
   // depth, t1 full res normals, its $Globals at b0 for this frame's projection and radius), then a copy into its render target.
   // Returns false, and the native draw runs, when an input, a shader or the scratch is missing.
   static bool RunXeGTAO(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      // Held through the dispatches so a shader reload cannot release them mid-use.
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      const auto& shaders = device_data.native_compute_shaders;
      if (!HasShaders(shaders, "P5S XeGTAO Prefilter Depths CS"_h, "P5S XeGTAO Main Pass CS"_h, "P5S XeGTAO Denoise Pass 1 CS"_h, "P5S XeGTAO Denoise Pass 2 CS"_h))
         return false;
      com_ptr<ID3D11DeviceContext1> native_device_context1;
      if (FAILED(native_device_context->QueryInterface(&native_device_context1)))
         return false;

      com_ptr<ID3D11ShaderResourceView> depth_srv;
      com_ptr<ID3D11ShaderResourceView> normals_srv;
      com_ptr<ID3D11Buffer> globals_cb;
      UINT globals_first = 0, globals_count = 0;
      com_ptr<ID3D11RenderTargetView> target_rtv;
      com_ptr<ID3D11DepthStencilView> target_dsv;
      native_device_context->PSGetShaderResources(0, 1, &depth_srv);
      native_device_context->PSGetShaderResources(1, 1, &normals_srv);
      native_device_context1->PSGetConstantBuffers1(0, 1, &globals_cb, &globals_first, &globals_count);
      native_device_context->OMGetRenderTargets(1, &target_rtv, &target_dsv);
      if (!depth_srv || !normals_srv || !globals_cb || !target_rtv)
         return false;
      com_ptr<ID3D11Resource> target;
      target_rtv->GetResource(&target);
      uint4 target_size, depth_size, normals_size;
      DXGI_FORMAT target_format, unused_format;
      GetResourceInfo(target.get(), target_size, target_format);
      GetResourceInfo(depth_srv.get(), depth_size, unused_format);
      GetResourceInfo(normals_srv.get(), normals_size, unused_format);
      const uint32_t width = target_size.x;
      const uint32_t height = target_size.y;
      // The final denoiser's R8_UNORM output is copied into the target, so its format must match
      if (width == 0 || height == 0 || target_format != DXGI_FORMAT_R8_UNORM || depth_size.x != width || depth_size.y != height)
         return false;
      // Full res normals per target pixel (2, the SSAO is half res)
      const uint32_t normal_input_scale = (normals_size.x + width / 2) / width;
      if (normal_input_scale == 0 || (normals_size.y + height / 2) / height != normal_input_scale)
         return false;

      auto& game_device_data = GetGameDeviceData(device_data);
      const std::lock_guard lock(game_device_data.gtao_mutex);
      if (game_device_data.gtao_width != width || game_device_data.gtao_height != height)
      {
         game_device_data.ReleaseGTAOScratch();
         D3D11_TEXTURE2D_DESC desc = {};
         desc.Width = width;
         desc.Height = height;
         desc.MipLevels = 5; // XE_GTAO_DEPTH_MIP_LEVELS
         desc.ArraySize = 1;
         desc.Format = DXGI_FORMAT_R32_FLOAT;
         desc.SampleDesc.Count = 1;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
         bool ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_depth_mips_texture)) && SUCCEEDED(native_device->CreateShaderResourceView(game_device_data.gtao_depth_mips_texture.get(), nullptr, &game_device_data.gtao_depth_mips_srv));
         D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
         uav_desc.Format = desc.Format;
         uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
         for (UINT mip = 0; ok && mip < 5; mip++)
         {
            uav_desc.Texture2D.MipSlice = mip;
            ok = SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_depth_mips_texture.get(), &uav_desc, &game_device_data.gtao_depth_mip_uavs[mip]));
         }
         desc.MipLevels = 1;
         desc.Format = DXGI_FORMAT_R8G8_UNORM;
         for (int i = 0; ok && i < 2; i++)
         {
            com_ptr<ID3D11Texture2D> working_texture;
            ok = SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &working_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(working_texture.get(), nullptr, &game_device_data.gtao_working_uavs[i])) && SUCCEEDED(native_device->CreateShaderResourceView(working_texture.get(), nullptr, &game_device_data.gtao_working_srvs[i]));
         }
         desc.Format = DXGI_FORMAT_R8_UNORM;
         ok = ok && SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.gtao_final_texture)) && SUCCEEDED(native_device->CreateUnorderedAccessView(game_device_data.gtao_final_texture.get(), nullptr, &game_device_data.gtao_final_uav));
         if (!ok)
            game_device_data.ReleaseGTAOScratch();
         game_device_data.gtao_width = width;
         game_device_data.gtao_height = height;
      }
      if (!game_device_data.gtao_final_uav)
         return false;

#if DEVELOPMENT
      const float debug_view = float(g_gtao_debug_view);
#else
      const float debug_view = 0.f;
#endif
      const float knobs[8] = {g_gtao_final_value_power, float(normal_input_scale), g_gtao_radius_override, debug_view, 1.f / float(width), 1.f / float(height), 0.f, 0.f};
      if (!game_device_data.gtao_knobs_cb || std::memcmp(game_device_data.gtao_knobs, knobs, sizeof(knobs)) != 0)
      {
         game_device_data.gtao_knobs_cb.reset();
         D3D11_BUFFER_DESC cb_desc = {};
         cb_desc.ByteWidth = sizeof(knobs);
         cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
         cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         const D3D11_SUBRESOURCE_DATA cb_data = {knobs};
         if (FAILED(native_device->CreateBuffer(&cb_desc, &cb_data, &game_device_data.gtao_knobs_cb)))
            return false;
         std::memcpy(game_device_data.gtao_knobs, knobs, sizeof(knobs));
      }

      DrawStateStack<DrawStateStackType::Compute> compute_state;
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The game may bind $Globals as a range of a bigger buffer; a zero count means a plain binding
      ID3D11Buffer* const cbs[] = {globals_cb.get(), game_device_data.gtao_knobs_cb.get()};
      if (globals_count != 0)
         native_device_context1->CSSetConstantBuffers1(0, 1, &cbs[0], &globals_first, &globals_count);
      else
         native_device_context->CSSetConstantBuffers(0, 1, &cbs[0]);
      native_device_context->CSSetConstantBuffers(gtao_knobs_cb_slot, 1, &cbs[1]);
      ID3D11SamplerState* const point_sampler = device_data.sampler_state_point.get();
      native_device_context->CSSetSamplers(0, 1, &point_sampler);

      // Each pass binds its destination UAV before its source SRVs: D3D11 otherwise nulls an SRV that still aliases the
      // previous pass's bound UAV.
      ID3D11ShaderResourceView* const null_srvs[2] = {};
      ID3D11UnorderedAccessView* const null_uavs[5] = {};
      const auto pass = [&](uint32_t shader_name_hash, UINT uav_count, ID3D11UnorderedAccessView* const* uavs, ID3D11ShaderResourceView* const(&srvs)[2], UINT groups_x, UINT groups_y)
      {
         native_device_context->CSSetShaderResources(0, 2, null_srvs);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, uavs, nullptr);
         native_device_context->CSSetShaderResources(0, 2, srvs);
         native_device_context->CSSetShader(shaders.at(shader_name_hash).get(), nullptr, 0);
         native_device_context->Dispatch(groups_x, groups_y, 1);
         native_device_context->CSSetUnorderedAccessViews(0, uav_count, null_uavs, nullptr);
      };
      ID3D11UnorderedAccessView* const mip_uavs[5] = {game_device_data.gtao_depth_mip_uavs[0].get(), game_device_data.gtao_depth_mip_uavs[1].get(), game_device_data.gtao_depth_mip_uavs[2].get(), game_device_data.gtao_depth_mip_uavs[3].get(), game_device_data.gtao_depth_mip_uavs[4].get()};
      ID3D11UnorderedAccessView* const working_uavs[2] = {game_device_data.gtao_working_uavs[0].get(), game_device_data.gtao_working_uavs[1].get()};
      ID3D11UnorderedAccessView* const final_uav = game_device_data.gtao_final_uav.get();
      pass("P5S XeGTAO Prefilter Depths CS"_h, 5, mip_uavs, {depth_srv.get(), nullptr}, (width + 15) / 16, (height + 15) / 16);
      pass("P5S XeGTAO Main Pass CS"_h, 1, &working_uavs[0], {game_device_data.gtao_depth_mips_srv.get(), normals_srv.get()}, (width + 7) / 8, (height + 7) / 8);
      pass("P5S XeGTAO Denoise Pass 1 CS"_h, 1, &working_uavs[1], {game_device_data.gtao_working_srvs[0].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      pass("P5S XeGTAO Denoise Pass 2 CS"_h, 1, &final_uav, {game_device_data.gtao_working_srvs[1].get(), nullptr}, (width + 15) / 16, (height + 7) / 8);
      compute_state.Restore(native_device_context);

      // The target is still bound as the draw's render target, and a copy into an OM bound resource is a hazard the runtime
      // does not resolve: unbind around the copy, then give the game its binding back.
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      native_device_context->CopyResource(target.get(), game_device_data.gtao_final_texture.get());
      ID3D11RenderTargetView* const rtv = target_rtv.get();
      native_device_context->OMSetRenderTargets(1, &rtv, target_dsv.get());
      return true;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);

      if (original_shader_hashes.Contains(ssao_depth_downsample_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11ShaderResourceView> depth_srv;
         native_device_context->PSGetShaderResources(0, 1, &depth_srv);
         D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc;
         if (depth_srv)
            depth_srv->GetDesc(&depth_srv_desc);
         if (depth_srv && depth_srv_desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS && depth_srv_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
         {
            const std::lock_guard lock(game_device_data.smaa_depth_mutex);
            game_device_data.smaa_depth_srv = depth_srv;
         }
      }

#if DEVELOPMENT
      if (game_device_data.xegtao_logging)
      {
         const auto pass = std::ranges::find_if(ssao_chain_passes, [&](const SSAOChainPass& chain_pass)
            { return original_shader_hashes.Contains(chain_pass.hash, reshade::api::shader_stage::pixel); });
         if (pass != std::end(ssao_chain_passes) && pass->hash == ssao_hash && g_gtao_enable && original_draw_dispatch_func && *original_draw_dispatch_func)
         {
            // Logs what the frame actually draws: XeGTAO's output, or the native one if it could not run
            std::function<void()> draw = [&]
            {
               if (!RunXeGTAO(native_device, native_device_context, device_data))
                  (*original_draw_dispatch_func)();
            };
            return LogSSAOChainPass(native_device, native_device_context, &game_device_data, *pass, &draw);
         }
         if (pass != std::end(ssao_chain_passes))
            return LogSSAOChainPass(native_device, native_device_context, &game_device_data, *pass, original_draw_dispatch_func);
      }
#endif

      if (g_gtao_enable && original_shader_hashes.Contains(ssao_hash, reshade::api::shader_stage::pixel))
         return RunXeGTAO(native_device, native_device_context, device_data) ? DrawOrDispatchOverrideType::Replaced : DrawOrDispatchOverrideType::None;

      if (original_shader_hashes.Contains(composite_hash, reshade::api::shader_stage::pixel))
      {
         device_data.has_drawn_main_post_processing = true;
         if (cb_luma_global_settings.GameSettings.SMAAEnable > 0.5f && original_draw_dispatch_func && *original_draw_dispatch_func)
            return DrawCompositeWithSMAA(native_device, native_device_context, cmd_list_data, device_data, &updated_cbuffers, *original_draw_dispatch_func);
         return DrawOrDispatchOverrideType::None;
      }

      // Everything the game draws onto the swapchain after the composite is UI (HUD, menus, dialogue boxes, fades), except the FXAA passes.
      // Checked after the composite, so a flag left over from the previous frame can never stop the scene from drawing.
      if (device_data.has_drawn_main_post_processing && (stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && !original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && !original_shader_hashes.Contains(shader_hashes_apply_fxaa))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> rtv_resource;
         if (rtv)
            rtv->GetResource(&rtv_resource);
         if (rtv_resource && IsBackBuffer(&device_data, rtv_resource.get()))
         {
            if (g_hide_ui)
               return DrawOrDispatchOverrideType::Skip;

            // UI draws that depend on the swapchain's magnitude through their blend saw it clamped to 0-1 by the vanilla UNORM target, while
            // the additive UI before them (e.g. the menu cursor's RGB cards, alpha 1 + 1 + 1) now accumulates above 1 on the fp16 one: the
            // cursor's reverse subtracted option text vanished, and destination alpha masks (HUD, main menu) read alphas up to 2. Clamp first,
            // under the draw's own geometry and stencil: the same draw with a white pixel shader and a MIN blend. Colors only before a color
            // subtract; destination alpha factors only need the (never displayed) alpha, and destination color factors are left alone, so the
            // HDR scene under the UI keeps its range. A subtract's own result went below 0 too (the dialogue bubbles, -1.5), so it's floored
            // after, likewise with a black pixel shader and a MAX blend. Additive UI over a bright scene went beyond the display's peak
            // (vanilla clipped it at 1), so it's clamped after to the peak, likewise.
            bool drawn = false;
            if (original_draw_dispatch_func && *original_draw_dispatch_func)
            {
               com_ptr<ID3D11BlendState> blend_state;
               FLOAT blend_factor[4];
               UINT sample_mask;
               native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
               D3D11_BLEND_DESC blend_desc = {};
               if (blend_state)
                  blend_state->GetDesc(&blend_desc);
               const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend_desc.RenderTarget[0];
               const auto subtracts = [](D3D11_BLEND_OP op)
               { return op == D3D11_BLEND_OP_SUBTRACT || op == D3D11_BLEND_OP_REV_SUBTRACT; };
               const auto reads_destination_alpha = [](D3D11_BLEND blend)
               { return blend == D3D11_BLEND_DEST_ALPHA || blend == D3D11_BLEND_INV_DEST_ALPHA || blend == D3D11_BLEND_SRC_ALPHA_SAT; };
               const bool subtracts_colors = rt_blend.BlendEnable && subtracts(rt_blend.BlendOp);
               const bool subtracts_alpha = rt_blend.BlendEnable && subtracts(rt_blend.BlendOpAlpha);
               const bool clamp_alpha = subtracts_alpha || (rt_blend.BlendEnable && (reads_destination_alpha(rt_blend.SrcBlend) || reads_destination_alpha(rt_blend.DestBlend) || reads_destination_alpha(rt_blend.SrcBlendAlpha) || reads_destination_alpha(rt_blend.DestBlendAlpha)));
               const bool adds_colors = rt_blend.BlendEnable && rt_blend.BlendOp == D3D11_BLEND_OP_ADD && rt_blend.DestBlend == D3D11_BLEND_ONE;
               if (subtracts_colors || clamp_alpha || adds_colors)
               {
                  com_ptr<ID3D11PixelShader> white_pixel_shader;
                  com_ptr<ID3D11PixelShader> black_pixel_shader;
                  com_ptr<ID3D11PixelShader> peak_pixel_shader;
                  {
                     const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
                     if (HasShaders(device_data.native_pixel_shaders, "P5S Draw White PS"_h, "P5S Draw Black PS"_h, "P5S UI Peak Clamp PS"_h))
                     {
                        white_pixel_shader = device_data.native_pixel_shaders.at("P5S Draw White PS"_h);
                        black_pixel_shader = device_data.native_pixel_shaders.at("P5S Draw Black PS"_h);
                        peak_pixel_shader = device_data.native_pixel_shaders.at("P5S UI Peak Clamp PS"_h);
                     }
                  }
                  // Index 0 all channels, 1 alpha only
                  const int channels = subtracts_colors ? 0 : 1;
                  if (white_pixel_shader && black_pixel_shader && peak_pixel_shader && game_device_data.ui_min_blend_states[0] && game_device_data.ui_min_blend_states[1] && game_device_data.ui_max_blend_states[channels])
                  {
                     com_ptr<ID3D11PixelShader> pixel_shader;
                     native_device_context->PSGetShader(&pixel_shader, nullptr, nullptr);
                     const auto draw = [&](ID3D11PixelShader* draw_pixel_shader, ID3D11BlendState* draw_blend_state)
                     {
                        native_device_context->PSSetShader(draw_pixel_shader, nullptr, 0);
                        native_device_context->OMSetBlendState(draw_blend_state, blend_factor, sample_mask);
                        (*original_draw_dispatch_func)();
                     };
                     if (subtracts_colors || clamp_alpha)
                        draw(white_pixel_shader.get(), game_device_data.ui_min_blend_states[channels].get());
                     draw(pixel_shader.get(), blend_state.get());
                     if (subtracts_colors || subtracts_alpha)
                        draw(black_pixel_shader.get(), game_device_data.ui_max_blend_states[channels].get());
                     if (adds_colors)
                     {
                        SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
                        updated_cbuffers = true;
                        draw(peak_pixel_shader.get(), game_device_data.ui_min_blend_states[0].get());
                     }
                     native_device_context->PSSetShader(pixel_shader.get(), nullptr, 0);
                     native_device_context->OMSetBlendState(blend_state.get(), blend_factor, sample_mask);
                     drawn = true;
                  }
               }
            }

#if DEVELOPMENT
            // "Dump UI Steps": the swapchain around the menu cursor after every UI draw (up to 128, each copy is ~9 MB until it's written out)
            if (game_device_data.ui_dumping && game_device_data.ui_dump_index < 128 && original_draw_dispatch_func && *original_draw_dispatch_func)
            {
               if (!drawn)
                  (*original_draw_dispatch_func)();
               drawn = true;
               QueueTextureReadback(native_device, native_device_context, &game_device_data, rtv.get(), std::format("ui_{:03}_{:08X}", game_device_data.ui_dump_index++, original_shader_hashes.pixel_shaders[0]), ui_dump_box);
            }
#endif
            if (drawn)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }

      // SMAA already antialiased the scene, before the UI
      if (original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel) && game_device_data.smaa_drawn.exchange(false))
         return DrawOrDispatchOverrideType::Skip;

      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      // Set by the composite; Core copies it into "has_drawn_main_post_processing_previous" before this, but never clears it
      device_data.has_drawn_main_post_processing = false;
#if DEVELOPMENT
      auto& game_device_data = GetGameDeviceData(device_data);
      // "Log XeGTAO Inputs" logs the frame between this present and the next
      game_device_data.xegtao_logging = g_xegtao_log.exchange(false);
      game_device_data.ui_dumping = g_ui_dump.exchange(false);
      game_device_data.ui_dump_index = 0;
      // Readbacks are written three presents after the first one queued: deferred command lists recorded around a present can
      // execute a frame later, and the map waits for the GPU anyway
      if (game_device_data.readbacks_queued.exchange(false) && game_device_data.readback_presents_left == 0)
         game_device_data.readback_presents_left = 3;
      if (game_device_data.readback_presents_left > 0 && --game_device_data.readback_presents_left == 0)
         FlushReadbacks(native_device, &game_device_data);
#endif
   }

   void LoadConfigs() override
   {
      auto& settings = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", settings.SMAAEnable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", settings.RCASSharpness);
      reshade::get_config_value(nullptr, NAME, "VignetteIntensity", settings.VignetteIntensity);
      reshade::get_config_value(nullptr, NAME, "LensFlareIntensity", settings.LensFlareIntensity);
      reshade::get_config_value(nullptr, NAME, "Exposure", settings.Exposure);
      reshade::get_config_value(nullptr, NAME, "ColorGradingIntensity", settings.ColorGradingIntensity);
      reshade::get_config_value(nullptr, NAME, "Saturation", settings.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", settings.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "BloomIntensity", settings.BloomIntensity);
      reshade::get_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;

      // Persisted GameSettings checkbox (0/1) with its tooltip and reset button
      const auto settings_toggle = [&](const char* label, const char* key, float* value, float default_value, const char* tooltip)
      {
         bool enabled = *value > 0.5f;
         if (ImGui::Checkbox(label, &enabled))
         {
            *value = enabled ? 1.f : 0.f;
            reshade::set_config_value(nullptr, NAME, key, *value);
            device_data.cb_luma_global_settings_dirty = true;
         }
         if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
         return *value > 0.5f;
      };
      // Persisted GameSettings slider, saved once the edit ends
      const auto settings_slider = [&](const char* label, const char* key, float* value, float default_value, float max_value, const char* tooltip)
      {
         if (ImGui::SliderFloat(label, value, 0.f, max_value))
            device_data.cb_luma_global_settings_dirty = true;
         if (ImGui::IsItemDeactivatedAfterEdit())
            reshade::set_config_value(nullptr, NAME, key, *value);
         if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
         if (DrawResetButton(*value, default_value, key))
            device_data.cb_luma_global_settings_dirty = true;
      };

      ImGui::SeparatorText("Anti-Aliasing");
      ImGui::BeginDisabled(!settings_toggle("SMAA Enable", "SMAAEnable", &settings.SMAAEnable, default_luma_global_game_settings.SMAAEnable, "Replaces the game's FXAA with SMAA (works with the game's anti-aliasing setting on or off)."));
      settings_slider("RCAS Sharpness", "RCASSharpness", &settings.RCASSharpness, default_luma_global_game_settings.RCASSharpness, 1.f, "Sharpening applied on top of SMAA (0 = off).");
      ImGui::EndDisabled();

      ImGui::SeparatorText("Grade");
      settings_slider("Exposure", "Exposure", &settings.Exposure, default_luma_global_game_settings.Exposure, 2.f, "Overall image brightness (1 = vanilla).");
      settings_slider("Saturation", "Saturation", &settings.Saturation, default_luma_global_game_settings.Saturation, 2.f, "Color saturation, HDR only (1 = vanilla).");
      settings_slider("Highlights Desaturation", "HighlightsDesaturation", &settings.HighlightsDesaturation, default_luma_global_game_settings.HighlightsDesaturation, 1.f, "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");
      settings_slider("Color Grading Intensity", "ColorGradingIntensity", &settings.ColorGradingIntensity, default_luma_global_game_settings.ColorGradingIntensity, 1.f, "Strength of the game's own color grading (1 = vanilla, 0 = neutral).");

      ImGui::SeparatorText("Bloom");
      settings_slider("Bloom Intensity", "BloomIntensity", &settings.BloomIntensity, default_luma_global_game_settings.BloomIntensity, 2.f, "Bloom strength (1 = vanilla, 0 = none).");

      ImGui::SeparatorText("Ambient Occlusion");
      if (ImGui::Checkbox("XeGTAO Enable", &g_gtao_enable))
         reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's SSAO with XeGTAO (cleaner, more accurate ambient occlusion; requires Ambient Occlusion enabled in the game's graphic settings).");
#if DEVELOPMENT || TEST
      ImGui::BeginDisabled(!g_gtao_enable);
      ImGui::SliderFloat("GTAO Final Value Power", &g_gtao_final_value_power, 0.3f, 4.5f, "%.2f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Primary darkness dial (higher = darker AO). Not saved.");
      ImGui::SliderFloat("GTAO Radius Override", &g_gtao_radius_override, 0.f, 200.f, "%.1f");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("0 = the game's SSAO radius (40 cm, capped at 108 half res pixels up close); > 0 overrides it, in centimetres, uncapped. Not saved.");
#if DEVELOPMENT // the shader's debug blocks exist in DEVELOPMENT only
      ImGui::Combo("GTAO Debug View", &g_gtao_debug_view, "Off\0Depth gradient\0Normals\0AO x8\0Edges\0");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Draws diagnostics through the game's SSAO blurs into the G-buffer AO (darkens the ambient lighting only).\nDepth gradient flat or blocky = wrong input; Normals: camera-facing surfaces bright, black everywhere = NORMAL_Z_SIGN inverted;\nAO x8 = spot broad over-occlusion.");
#endif
      ImGui::EndDisabled();
#endif

      ImGui::SeparatorText("Effects");
      settings_slider("Vignette Intensity", "VignetteIntensity", &settings.VignetteIntensity, default_luma_global_game_settings.VignetteIntensity, 1.f, "Scales the game's vignette darkening (1 = vanilla, 0 = none).");
      settings_slider("Lens Flare Intensity", "LensFlareIntensity", &settings.LensFlareIntensity, default_luma_global_game_settings.LensFlareIntensity, 2.f, "Lens-flare / glare strength (1 = vanilla, 0 = off).");
      settings_toggle("Dithering", "Dithering", &settings.Dithering, default_luma_global_game_settings.Dithering, "Reduces gradient banding.");

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

#if DEVELOPMENT
   void DrawImGuiDevSettings(DeviceData& device_data) override
   {
      ImGui::SeparatorText("SMAA");
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (plane deviation of the scene depth) as well as by color, so texture detail stays sharp while silhouettes are antialiased. Not saved.");
      ImGui::Combo("SMAA Predication Debug View", &g_smaa_debug_view, "Off\0Edges\0Predication\0");
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the frame with SMAA's edges (red = horizontal, green = vertical) or the predication edge-ness (red).\nToggle SMAA Predication to compare: texture detail should lose edges, silhouettes keep them.");
      if (ImGui::Button("Dump SMAA Inputs"))
         g_smaa_dump = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Dumps the next SMAA frame's depth, predication and edges to %%TEMP%%\\p5s_smaa_*_<frame>.bin.");

      ImGui::SeparatorText("UI");
      if (ImGui::Button("Dump UI Steps"))
         g_ui_dump = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Dumps the swapchain around the menu cursor (x %u-%u, y %u-%u) after every UI draw of the next frame (up to 128),\nto %%TEMP%%\\p5s_ui_<index>_<pixel shader>.bin.", ui_dump_box.left, ui_dump_box.right, ui_dump_box.top, ui_dump_box.bottom);

      ImGui::SeparatorText("XeGTAO");
      if (ImGui::Button("Log XeGTAO Inputs"))
         g_xegtao_log = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Logs the next frame's SSAO chain to ReShade.log (bindings, viewport, PS cb0-cb4), and dumps the SSAO's inputs and\nevery pass' output to %%TEMP%%\\p5s_xegtao_<hash>_<slot>_<frame>.bin. Needs in-game Ambient Occlusion on.");
   }
#endif

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Persona 5 Strikers\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR and replaces the game's FXAA with SMAA and its SSAO with XeGTAO.\n"
         "Enable Ambient Occlusion in the game's graphic settings for XeGTAO to apply; SMAA works either way.\n"
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
                  "\nDICE (HDR tonemapper)"
                  "\nSMAA (Iryoku)"
                  "\nXeGTAO (Intel)"
                  "\nAMD FidelityFX (RCAS)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Persona 5 Strikers Luma mod", "", 1);

      // The composite, UI and FXAA all write the swapchain
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // FXAA reads a BGRA8 copy of the swapchain (after UI), which has to hold HDR too.
      // The HDR scene (deferred lighting, the refraction grab copy) and the bloom and flare mips (swapchain aspect ratio) are R11G11B10_FLOAT, upgraded for
      // precision as in Nioh: the bloom chain requantizes through 11 passes, and R11G11B10's 5 bit blue mantissa tints the halos.
      // Arrays are never upgraded, so the R11G11B10 G-buffer array stays.
      // This also upgrades every other swapchain aspect ratio BGRA8 target (e.g. the G-buffer albedo); upgrading only the FXAA copy would save VRAM.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {reshade::api::format::b8g8r8a8_typeless, reshade::api::format::r11g11b10_float};
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

#if DEVELOPMENT
      forced_shader_names.emplace(composite_hash, "Composite");
      forced_shader_names.emplace(0x90C6B12E, "UI");
      forced_shader_names.emplace(fxaa_hash, "FXAA");
      for (const uint32_t hash : shader_hashes_apply_fxaa.pixel_shaders)
         forced_shader_names.emplace(hash, "Apply FXAA");
      forced_shader_names.emplace(0x619045C8, "Bloom Combine");
      forced_shader_names.emplace(0x88C4EC12, "Bloom Combine");
      forced_shader_names.emplace(0xB5F3F656, "Bloom Combine");
      forced_shader_names.emplace(0x4CE014A2, "Bloom Combine");
      forced_shader_names.emplace(0xB6289AC0, "Bloom Prefilter");
      forced_shader_names.emplace(ssao_depth_downsample_hash, "SSAO Depth Downsample");
      forced_shader_names.emplace(ssao_hash, "SSAO");
      forced_shader_names.emplace(0x691D080F, "Deferred Lighting");
#endif

      game = new Persona5Strikers();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
