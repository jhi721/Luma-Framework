#define GAME_YAKUZA_3 1

#define DISABLE_AUTO_DEBUGGER 1
// SMAA replaces the game's CMAA2 or FXAA (see "RunSMAA").
#define ENABLE_SMAA 1

#include "..\..\Core\core.hpp"

// The engine has no tone curve: materials write gamma-space `color * exposure` into an 8-bit scene RT whose UNORM clamp
// is the only highlight limit. The whole post chain (scene, CMAA2, CAS, resample, fade) runs on swapchain-sized
// b8g8r8a8/r8g8b8a8 targets, upgraded to fp16 here. The HDR tonemap lives in the "color correct" replacements
// (Shaders/Yakuza 3 Remastered/Includes/ColorCorrect.hlsl), the first full-screen pass reading the finished scene.

namespace
{
   // User settings, persisted in the [Luma] config section.
   bool g_smaa_enable = true;
   float g_rcas_sharpness = 0.f;
   // Plane deviation that counts as a full predication edge, as a fraction of view depth (TW2's validated value; DEV slider).
   float g_smaa_pred_tolerance = 0.02f;
#if DEVELOPMENT
   bool g_smaa_predication = true;
   bool g_smaa_pred_debug = false;   // Show the predication mask (red) instead of the frame
   bool g_smaa_pred_measure = false; // One-shot: read the mask back and log its distribution (UI button)
#endif

   // Readers of the 4K r32 scene depth at t0: ASSAO prepare, and a full-screen depth restore (also used by prepasses; the
   // last one before the AA, after ASSAO, reads the main depth). Captured for SMAA predication.
   const std::unordered_set<uint32_t> depth_reader_pixel_shaders = {0x972BE5B5, 0x87917E9E};
   // QLOC's CMAA2: edges, candidates, dispatch args (these three skipped under SMAA), then the apply, which writes edge
   // pixels into u0, a copy of the post-ccr canvas that the game copies back afterwards.
   const std::unordered_set<uint32_t> cmaa2_pre_apply_compute_shaders = {0x2E998140, 0x60E701EA, 0x79976116};
   constexpr uint32_t cmaa2_first_compute_shader = 0x2E998140;
   constexpr uint32_t cmaa2_apply_compute_shader = 0x82DA801B;
   // FXAA 3.11: one pass from the post-ccr canvas (t0) into its own swapchain-sized target.
   constexpr uint32_t fxaa_pixel_shader = 0xE7A1D308;
   // FidelityFX CAS (t0 -> u0, copied back by the game), after the AA and the world-anchored markers.
   constexpr uint32_t cas_compute_shader = 0x491BAFA3;

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

   bool GetTextureDesc(ID3D11Resource* resource, D3D11_TEXTURE2D_DESC* desc)
   {
      com_ptr<ID3D11Texture2D> texture;
      if (!resource || FAILED(resource->QueryInterface(&texture)))
         return false;
      texture->GetDesc(desc);
      return true;
   }
} // namespace

#if DEVELOPMENT
namespace
{
   // The "color correct" family (ps_ccr_* x32, ps_color_collection, fx_ccr_*_mask): the pass that carries the HDR tonemap.
   const std::unordered_set<uint32_t> ccr_hashes = {0x00189B34, 0x085CB5DC, 0x0BD7E699, 0x0C663121, 0x0D70385F, 0x105A4974, 0x16366D61, 0x1A8B8E14, 0x1C68CAB4,
      0x299FC101, 0x2B9CB5CF, 0x2F9F68B6, 0x303C672B, 0x3B334605, 0x3F7303BE, 0x601ABB87, 0x62801D10, 0x653BBD8E, 0x65E3DED9, 0x66523C57, 0x70E5BEF5, 0x9B4BFD8A,
      0xAEE6B7E1, 0xB156D307, 0xC011BB2A, 0xC3458723, 0xC3925A82, 0xCF6F5FDB, 0xD05BC597, 0xD2AD713B, 0xD63B6BAF, 0xD66AFCDA, 0xF15A0660, 0xFB2C3B56};
   const std::unordered_set<uint32_t> video_hashes = {0xFD02F404 /*ps_sofdec*/, 0xC9782177 /*ps_sofdec_qloc*/};
   // Passes whose HDR behavior depends on their target and blend state, which only runtime shows.
   const std::unordered_map<uint32_t, const char*> watched_hashes = {{0x716ADB18, "focus_blur_pass1 (DoF)"}, {0x04359FA6, "focus_blur_pass2 (DoF)"},
      {0x74E5C6AC, "focus_blur_pass2_mask (DoF)"}, {0xFD02F404, "ps_sofdec"}, {0xC9782177, "ps_sofdec_qloc"}, {0xE1631197, "ps_haze"}, {0x3A7B40E4, "ps_afterimage01"},
      {0xFD4620B9, "fx_refraction"}, {0x7814519F, "fx_track_blur"}, {0xFBA57AE9, "CAS scaled (cs)"}, {0x82DA801B, "CMAA2 apply (cs)"}, {0x4A57A803, "fx_camera_blur"},
      {0x0A4BB34E, "fx_rdiffusion"}, {0x24726E96, "ps_lerp"}, {0xCAEFD55C, "ps_grayscale"}, {0x495BB3CA, "fx_lens_flare"}};
   constexpr uint32_t log_interval_frames = 120;
   constexpr size_t max_exposure_samples = 16;

   void LogFormatted(reshade::log::level level, const char* format, auto... args)
   {
      char line[512];
      snprintf(line, sizeof(line), format, args...);
      reshade::log::message(level, line);
   }
} // namespace
#endif

struct Yakuza3DeviceData final : public GameDeviceData
{
   // SMAA inputs: a snapshot of the gamma canvas (the target may be the canvas itself, so SMAA cannot also sample it) and
   // its linear-light decode. SMAA writes its output into the snapshot (only edge detection reads it, before that); RCAS
   // then sharpens it into the linear texture, free by then. Recreated when the canvas size or format changes.
   com_ptr<ID3D11Texture2D> smaa_gamma_texture;
   com_ptr<ID3D11ShaderResourceView> smaa_gamma_srv;
   com_ptr<ID3D11RenderTargetView> smaa_gamma_rtv;
   com_ptr<ID3D11Texture2D> smaa_linear_texture;
   com_ptr<ID3D11UnorderedAccessView> smaa_linear_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_linear_srv;
   com_ptr<ID3D11RenderTargetView> smaa_linear_rtv;
   com_ptr<ID3D11UnorderedAccessView> smaa_predication_uav;
   com_ptr<ID3D11ShaderResourceView> smaa_predication_srv;

   // Per frame
   com_ptr<ID3D11ShaderResourceView> depth_srv; // Scene depth for predication, null if no reader ran
   bool cmaa2_replaced = false;                 // The CMAA2 chain of this frame is skipped, its apply runs SMAA
   bool smaa_ran = false;                       // RCAS replaced the game's CAS, which becomes a copy

#if DEVELOPMENT
   // Per frame
   bool drew_ccr = false;
   bool drew_video = false;
   uint32_t ccr_hash = 0;
   std::vector<std::array<float, 4>> exposure_samples; // Material cb2[14] (scale applied to every material output)
   // Across frames
   bool previous_frame_drew_ccr = true;
   uint32_t frames_without_ccr = 0;
   uint32_t last_logged_ccr_hash = 0;
   std::vector<float> last_logged_ccr_cb5;
   std::unordered_set<uint32_t> logged_watched_hashes;
   std::unordered_set<uint32_t> logged_custom_size_hashes;
   // Staging copy for the one-shot predication mask readback (see LogPredicationStats), allocated on first use.
   com_ptr<ID3D11Texture2D> pred_measure_staging;
   bool pred_measure_pending = false;
#endif
};

class GameYakuza3 final : public Game
{
   static Yakuza3DeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<Yakuza3DeviceData*>(device_data.game);
   }

   // SMAA (+ RCAS) runs on the post-ccr fp16 canvas at output resolution; anything else (render scale below 100%, SDR
   // targets) keeps the game's own AA. The caller holds s_mutex_shader_objects.
   static bool CanRunSMAA(const DeviceData& device_data, const D3D11_TEXTURE2D_DESC& color_desc)
   {
      return g_smaa_enable && color_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && color_desc.SampleDesc.Count == 1 && color_desc.ArraySize == 1 && color_desc.Width == uint32_t(device_data.output_resolution.x + 0.5f) && color_desc.Height == uint32_t(device_data.output_resolution.y + 0.5f) && HasShaders(device_data.native_vertex_shaders, "SMAA Edge Detection VS"_h, "SMAA Blending Weight Calculation VS"_h, "SMAA Neighborhood Blending VS"_h, "Copy VS"_h) && HasShaders(device_data.native_pixel_shaders, "SMAA Edge Detection PS"_h, "SMAA Blending Weight Calculation PS"_h, "SMAA Neighborhood Blending PS"_h, "Y3 Sharpen PS"_h) && HasShaders(device_data.native_compute_shaders, "Y3 SMAA Linearize CS"_h);
   }

#if DEVELOPMENT
   // One-shot readback of the predication mask (port of the BL GOTY calibration aid), so the tolerance is calibrated from
   // numbers rather than screenshots. On a well-tuned frame the mask is 0 nearly everywhere, so this reports COVERAGE at
   // the level SMAA compares against (0.5) plus the shape either side of it, over every texel. Copies on the frame the
   // button is pressed and maps on a later one (non-blocking). Immediate context only.
   static void LogPredicationStats(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, Yakuza3DeviceData& game_device_data, ID3D11Texture2D* mask)
   {
      if (!g_smaa_pred_measure && !game_device_data.pred_measure_pending)
         return;

      D3D11_TEXTURE2D_DESC mask_desc;
      mask->GetDesc(&mask_desc);
      D3D11_TEXTURE2D_DESC staging_desc = {};
      if (game_device_data.pred_measure_staging)
         game_device_data.pred_measure_staging->GetDesc(&staging_desc);
      if (staging_desc.Width != mask_desc.Width || staging_desc.Height != mask_desc.Height)
      {
         staging_desc = mask_desc;
         staging_desc.Usage = D3D11_USAGE_STAGING;
         staging_desc.BindFlags = 0;
         staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
         staging_desc.MiscFlags = 0;
         game_device_data.pred_measure_staging.reset();
         game_device_data.pred_measure_pending = false;
         if (FAILED(native_device->CreateTexture2D(&staging_desc, nullptr, &game_device_data.pred_measure_staging)))
            return;
      }

      if (!game_device_data.pred_measure_pending)
      {
         g_smaa_pred_measure = false;
         native_device_context->CopyResource(game_device_data.pred_measure_staging.get(), mask);
         game_device_data.pred_measure_pending = true;
         return;
      }

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (FAILED(native_device_context->Map(game_device_data.pred_measure_staging.get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)) || mapped.pData == nullptr)
         return; // Still in flight, retry next frame

      constexpr uint32_t bins = 256;
      uint64_t histogram[bins] = {};
      uint64_t total = 0;
      uint64_t non_finite = 0;
      for (UINT y = 0; y < mask_desc.Height; y++)
      {
         const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch);
         for (UINT x = 0; x < mask_desc.Width; x++)
         {
            const float v = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
            total++;
            // A range test, not a clamp: NaN fails both comparisons and would index outside the array once cast.
            if (!(v >= 0.f && v <= 1.f))
               non_finite++;
            else
               histogram[uint32_t(v * float(bins - 1))]++;
         }
      }
      native_device_context->Unmap(game_device_data.pred_measure_staging.get(), 0);
      game_device_data.pred_measure_pending = false;

      const auto fraction_above = [&](float level)
      {
         uint64_t hits = 0;
         for (uint32_t b = uint32_t(level * float(bins - 1)) + 1u; b < bins; b++)
            hits += histogram[b];
         return 100.0 * double(hits) / double(total);
      };
      const auto percentile = [&](double p)
      {
         const uint64_t target = uint64_t(p * double(total));
         uint64_t running = 0;
         for (uint32_t b = 0; b < bins; b++)
         {
            running += histogram[b];
            if (running >= target)
               return float(b) / float(bins - 1);
         }
         return 1.f;
      };
      LogFormatted(reshade::log::level::info, "[Y3-Pred] tol=%.4f | FIRES(>0.5)=%.3f%% | >0.1=%.3f%% >0.25=%.3f%% >0.75=%.3f%% >0.9=%.3f%% | flat(bin0)=%.2f%% | p50=%.3f p90=%.3f p99=%.3f p999=%.3f | nonfinite=%llu | %ux%u",
         g_smaa_pred_tolerance, fraction_above(0.5f), fraction_above(0.1f), fraction_above(0.25f), fraction_above(0.75f), fraction_above(0.9f), 100.0 * double(histogram[0]) / double(total),
         percentile(0.5), percentile(0.9), percentile(0.99), percentile(0.999), (unsigned long long)non_finite, mask_desc.Width, mask_desc.Height);
   }
#endif

   // SMAA from "color" into "target" (the same resource for CMAA2, separate for FXAA), then RCAS when sharpness > 0.
   // Returns false, leaving everything untouched, if SMAA can't run.
   static bool RunSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, bool& updated_cbuffers, ID3D11Resource* color, ID3D11Resource* target)
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      D3D11_TEXTURE2D_DESC color_desc, target_desc;
      if (!GetTextureDesc(color, &color_desc) || !GetTextureDesc(target, &target_desc) || target_desc.Width != color_desc.Width || target_desc.Height != color_desc.Height || target_desc.Format != color_desc.Format)
         return false;

      // Held through SMAA so a shader reload cannot release them mid-use; "DrawSMAA" looks its shaders up with "at".
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      if (!CanRunSMAA(device_data, color_desc))
         return false;

      D3D11_TEXTURE2D_DESC snapshot_desc = {};
      if (game_device_data.smaa_gamma_texture)
         game_device_data.smaa_gamma_texture->GetDesc(&snapshot_desc);
      if (snapshot_desc.Width != color_desc.Width || snapshot_desc.Height != color_desc.Height || snapshot_desc.Format != color_desc.Format)
      {
         game_device_data.smaa_gamma_texture.reset();
         game_device_data.smaa_gamma_srv.reset();
         game_device_data.smaa_gamma_rtv.reset();
         game_device_data.smaa_linear_texture.reset();
         game_device_data.smaa_linear_uav.reset();
         game_device_data.smaa_linear_srv.reset();
         game_device_data.smaa_linear_rtv.reset();
         game_device_data.smaa_predication_uav.reset();
         game_device_data.smaa_predication_srv.reset();
         D3D11_TEXTURE2D_DESC desc = color_desc;
         desc.MipLevels = 1;
         desc.Usage = D3D11_USAGE_DEFAULT;
         desc.CPUAccessFlags = 0;
         desc.MiscFlags = 0;
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_gamma_texture)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_gamma_texture.get(), nullptr, &game_device_data.smaa_gamma_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return false;
         }
         desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &game_device_data.smaa_linear_texture)) || FAILED(native_device->CreateUnorderedAccessView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_uav)) || FAILED(native_device->CreateShaderResourceView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_srv)) || FAILED(native_device->CreateRenderTargetView(game_device_data.smaa_linear_texture.get(), nullptr, &game_device_data.smaa_linear_rtv)))
         {
            game_device_data.smaa_gamma_texture.reset();
            return false;
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

      D3D11_TEXTURE2D_DESC depth_desc = {};
      com_ptr<ID3D11Resource> depth_resource;
      if (game_device_data.depth_srv)
         game_device_data.depth_srv->GetResource(&depth_resource);
      bool predicate = game_device_data.smaa_predication_uav && HasShaders(device_data.native_compute_shaders, "Y3 SMAA Predication CS"_h) && GetTextureDesc(depth_resource.get(), &depth_desc) && depth_desc.Width == color_desc.Width && depth_desc.Height == color_desc.Height;
#if DEVELOPMENT
      predicate = predicate && g_smaa_predication;
#endif

      native_device_context->CopyResource(game_device_data.smaa_gamma_texture.get(), color);
      {
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(native_device_context, device_data.uav_max_count);
         ID3D11UnorderedAccessView* const linear_uav = game_device_data.smaa_linear_uav.get();
         ID3D11ShaderResourceView* const gamma_srv = game_device_data.smaa_gamma_srv.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &linear_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &gamma_srv);
         native_device_context->CSSetShader(device_data.native_compute_shaders.at("Y3 SMAA Linearize CS"_h).get(), nullptr, 0);
         native_device_context->Dispatch((color_desc.Width + 7) / 8, (color_desc.Height + 7) / 8, 1);
         if (predicate)
         {
            // The game can still have the depth bound as a writable DSV here, and D3D11 then silently nulls the depth SRV
            // (the mask came out all zero): unbind the output merger for the dispatch.
            com_ptr<ID3D11RenderTargetView> om_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> om_dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &om_rtvs[0], &om_dsv);
            if (om_dsv)
               native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

            ID3D11UnorderedAccessView* const predication_uav = game_device_data.smaa_predication_uav.get();
            ID3D11ShaderResourceView* const depth_srv = game_device_data.depth_srv.get();
            native_device_context->CSSetUnorderedAccessViews(0, 1, &predication_uav, nullptr);
            native_device_context->CSSetShaderResources(0, 1, &depth_srv);
#if DEVELOPMENT
            // Key 2 (no real shader hash): the runtime still rejected the depth SRV (another conflicting binding).
            com_ptr<ID3D11ShaderResourceView> bound_depth_srv;
            native_device_context->CSGetShaderResources(0, 1, &bound_depth_srv);
            if (!bound_depth_srv && game_device_data.logged_watched_hashes.insert(2u).second)
               LogFormatted(reshade::log::level::warning, "[Y3] frame %u SMAA predication: depth SRV rejected by the runtime", cb_luma_global_settings.FrameIndex);
#endif
            SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::compute, LumaConstantBufferType::LumaData, 0, 0, g_smaa_pred_tolerance);
            native_device_context->CSSetShader(device_data.native_compute_shaders.at("Y3 SMAA Predication CS"_h).get(), nullptr, 0);
            native_device_context->Dispatch((color_desc.Width + 7) / 8, (color_desc.Height + 7) / 8, 1);

            if (om_dsv)
            {
               ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
               for (size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                  rtvs[i] = om_rtvs[i].get();
               native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, om_dsv.get());
            }
         }
         compute_state.Restore(native_device_context);
      }

#if DEVELOPMENT
      if (predicate)
      {
         com_ptr<ID3D11Resource> mask_resource;
         game_device_data.smaa_predication_srv->GetResource(&mask_resource);
         com_ptr<ID3D11Texture2D> mask;
         if (cmd_list_data.is_primary && SUCCEEDED(mask_resource->QueryInterface(&mask)))
            LogPredicationStats(native_device, native_device_context, game_device_data, mask.get());

         // Calibration view: the single-channel mask lands in red, replacing the frame (black on flat surfaces, red across
         // silhouettes; all red = tolerance too low, all black = too high).
         if (g_smaa_pred_debug && HasShaders(device_data.native_pixel_shaders, "Copy PS"_h))
         {
            DrawStateStack<DrawStateStackType::FullGraphics> debug_state;
            debug_state.Cache(native_device_context, device_data.uav_max_count);
            DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Copy PS"_h).get(), game_device_data.smaa_predication_srv.get(), game_device_data.smaa_gamma_rtv.get(), color_desc.Width, color_desc.Height, false);
            debug_state.Restore(native_device_context);
            native_device_context->CopyResource(target, game_device_data.smaa_gamma_texture.get());
            game_device_data.smaa_ran = true;
            return true;
         }
      }
#endif

      // The SMAA shaders read the canvas size from the Luma settings and the predication scale from the Luma data, in both stages.
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, predicate ? 2.f : 1.f);
      updated_cbuffers = true;
      DrawSMAA(native_device, native_device_context, device_data, game_device_data.smaa_gamma_rtv.get(), game_device_data.smaa_linear_srv.get(), game_device_data.smaa_gamma_srv.get(), predicate ? game_device_data.smaa_predication_srv.get() : nullptr);

      ID3D11Texture2D* result = game_device_data.smaa_gamma_texture.get();
      if (g_rcas_sharpness > 0.f)
      {
         DrawStateStack<DrawStateStackType::FullGraphics> sharpen_state;
         sharpen_state.Cache(native_device_context, device_data.uav_max_count);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, 0, g_rcas_sharpness);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, device_data.native_vertex_shaders.at("Copy VS"_h).get(), device_data.native_pixel_shaders.at("Y3 Sharpen PS"_h).get(), game_device_data.smaa_gamma_srv.get(), game_device_data.smaa_linear_rtv.get(), color_desc.Width, color_desc.Height, false);
         sharpen_state.Restore(native_device_context);
         result = game_device_data.smaa_linear_texture.get();
      }
      native_device_context->CopyResource(target, result);
      game_device_data.smaa_ran = true;
#if DEVELOPMENT
      // Keys 0/1 (no real shader hash) log the first SMAA run without and with predication.
      if (game_device_data.logged_watched_hashes.insert(predicate ? 1u : 0u).second)
         LogFormatted(reshade::log::level::info, "[Y3] frame %u SMAA ran (predication %d, %ux%u)", cb_luma_global_settings.FrameIndex, predicate, color_desc.Width, color_desc.Height);
#endif
      return true;
   }

public:
   void OnInit(bool async) override
   {
      std::vector<ShaderDefineData> game_shader_defines_data = {
         {"TONEMAP_TYPE", '1', true, false, "0 - Vanilla SDR\n1 - Luma HDR (Vanilla+)", 1},
      };
      shader_defines_data.append_range(game_shader_defines_data);
      assert(shader_defines_data.size() < MAX_SHADER_DEFINES);

      // Gamma-space post processing (the vanilla chain and HUD are 2.2 gamma, UNORM, no sRGB views), 1.0 = paper white.
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      // The HUD is drawn straight onto the post-AA scene: the grade pre-scales the scene by game/UI paper white.
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // No game shader binds b8-b10 (b12/b13 are bound by the engine on every draw).
      luma_settings_cbuffer_index = 10;
      luma_data_cbuffer_index = 9;

      default_luma_global_game_settings.VideoAutoHDREnable = cb_luma_global_settings.GameSettings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = cb_luma_global_settings.GameSettings.VideoAutoHDRBoost = 0.5f; // peak ~165 nits
      default_luma_global_game_settings.Dithering = cb_luma_global_settings.GameSettings.Dithering = 1.f;

      native_shaders_definitions.emplace(CompileTimeStringHash("Y3 SMAA Linearize CS"), ShaderDefinition{"Luma_Y3_SMAALinearize", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("Y3 SMAA Predication CS"), ShaderDefinition{"Luma_Y3_SMAAPredication", reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("Y3 Sharpen PS"), ShaderDefinition{"Luma_Y3_Sharpen", reshade::api::pipeline_subobject_type::pixel_shader, nullptr, "sharpen_ps"});
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Anti-Aliasing");

      if (ImGui::Checkbox("SMAA Enable", &g_smaa_enable))
         reshade::set_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Replaces the game's CMAA2/FXAA with SMAA and its CAS with RCAS (requires Anti-Aliasing enabled in the game's graphics settings).");

      ImGui::BeginDisabled(!g_smaa_enable);
      ImGui::SliderFloat("RCAS Sharpness", &g_rcas_sharpness, 0.f, 1.f);
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Sharpening applied on top of SMAA (0 = off).");
      DrawResetButton(g_rcas_sharpness, 0.f, "RCASSharpness");
#if DEVELOPMENT
      // Predication only relaxes the edge threshold back to base ULTRA on geometry, never below, so off is strictly worse:
      // a bisect switch and calibration tools for devs (session-only), shipped on at 0.02.
      ImGui::Checkbox("SMAA Predication", &g_smaa_predication);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Finds edges by geometry (scene depth) instead of by brightness alone.\nKeeps textures sharp while still antialiasing real silhouettes.");
      ImGui::SliderFloat("SMAA Predication Tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("How far a surface may deviate from its local plane before it counts as an edge,\nas a fraction of view depth. Lower = more edges. This is the calibration lever,\nnot the SMAA threshold. Logarithmic: the parameter is relative.");
      ImGui::Checkbox("SMAA Predication Debug View", &g_smaa_pred_debug);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Show the predication mask (red) instead of the frame.\nWant: black on flat surfaces, red across silhouettes.\nAll red = tolerance too low (predication is doing nothing).\nAll black = too high (silhouettes never regain sensitivity).");
      if (ImGui::Button("Measure Predication"))
         g_smaa_pred_measure = true;
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Read the mask back and log its distribution to ReShade.log.\nStand still, set a tolerance, press; repeat per value and compare the lines.\nFIRES(>0.5) is the share of the frame that regains base sensitivity.");
#endif
      ImGui::EndDisabled();

      ImGui::SeparatorText("Effects");

      bool video_auto_hdr = gs.VideoAutoHDREnable > 0.5f;
      if (ImGui::Checkbox("Video AutoHDR", &video_auto_hdr))
      {
         gs.VideoAutoHDREnable = video_auto_hdr ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Adds HDR highlights to pre-rendered videos (HDR only).");

      ImGui::BeginDisabled(!video_auto_hdr);
      if (ImGui::SliderFloat("Video HDR Boost", &gs.VideoAutoHDRBoost, 0.f, 1.f))
         device_data.cb_luma_global_settings_dirty = true;
      if (ImGui::IsItemDeactivatedAfterEdit())
         reshade::set_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
         ImGui::SetTooltip("Video highlight strength (0 = off).");
      if (DrawResetButton(gs.VideoAutoHDRBoost, default_luma_global_game_settings.VideoAutoHDRBoost, "VideoAutoHDRBoost"))
         device_data.cb_luma_global_settings_dirty = true;
      ImGui::EndDisabled();

      bool dithering = gs.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         gs.Dithering = dithering ? 1.f : 0.f;
         reshade::set_config_value(nullptr, NAME, "Dithering", gs.Dithering);
         device_data.cb_luma_global_settings_dirty = true;
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new Yakuza3DeviceData;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const bool is_compute = (stages & reshade::api::shader_stage::compute) != 0;
      const uint32_t hash = is_compute ? original_shader_hashes.compute_shaders[0] : original_shader_hashes.pixel_shaders[0];

      if (!is_compute && depth_reader_pixel_shaders.contains(hash))
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
         if (srv)
            srv->GetDesc(&srv_desc);
         if (srv && srv_desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS && srv_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
            game_device_data.depth_srv = srv;
      }
      else if (is_compute && cmaa2_pre_apply_compute_shaders.contains(hash))
      {
         // The first pass decides for the whole chain: once skipped, the apply must run SMAA (its inputs were never built).
         if (hash == cmaa2_first_compute_shader)
         {
            com_ptr<ID3D11ShaderResourceView> srv;
            native_device_context->CSGetShaderResources(0, 1, &srv);
            com_ptr<ID3D11Resource> color;
            if (srv)
               srv->GetResource(&color);
            D3D11_TEXTURE2D_DESC color_desc;
            const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
            game_device_data.cmaa2_replaced = GetTextureDesc(color.get(), &color_desc) && CanRunSMAA(device_data, color_desc);
         }
         if (game_device_data.cmaa2_replaced)
            return DrawOrDispatchOverrideType::Replaced;
      }
      else if (is_compute && hash == cmaa2_apply_compute_shader && game_device_data.cmaa2_replaced)
      {
         // u0 already holds a full copy of the post-ccr canvas, so if SMAA fails now the frame just goes un-antialiased.
         game_device_data.cmaa2_replaced = false;
         com_ptr<ID3D11UnorderedAccessView> uav;
         native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
         com_ptr<ID3D11Resource> canvas;
         if (uav)
            uav->GetResource(&canvas);
         if (canvas)
            RunSMAA(native_device, native_device_context, cmd_list_data, device_data, updated_cbuffers, canvas.get(), canvas.get());
         return DrawOrDispatchOverrideType::Replaced;
      }
      else if (!is_compute && hash == fxaa_pixel_shader)
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->PSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> color, target;
         if (srv)
            srv->GetResource(&color);
         if (rtv)
            rtv->GetResource(&target);
         if (color && target && RunSMAA(native_device, native_device_context, cmd_list_data, device_data, updated_cbuffers, color.get(), target.get()))
         {
#if DEVELOPMENT
            if (game_device_data.logged_watched_hashes.insert(hash).second)
               LogFormatted(reshade::log::level::info, "[Y3] frame %u SMAA replaced FXAA", cb_luma_global_settings.FrameIndex);
#endif
            return DrawOrDispatchOverrideType::Replaced;
         }
#if DEVELOPMENT
         // Key 3 (no real shader hash): the native FXAA ran instead.
         if (game_device_data.logged_watched_hashes.insert(3u).second)
            LogFormatted(reshade::log::level::warning, "[Y3] frame %u native FXAA ran (SMAA off or unavailable)", cb_luma_global_settings.FrameIndex);
#endif
      }
      // RCAS already sharpened the SMAA output: the game's CAS only forwards its input (with the markers drawn since).
      else if (is_compute && hash == cas_compute_shader && game_device_data.smaa_ran)
      {
         com_ptr<ID3D11ShaderResourceView> srv;
         native_device_context->CSGetShaderResources(0, 1, &srv);
         com_ptr<ID3D11UnorderedAccessView> uav;
         native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
         com_ptr<ID3D11Resource> source, target;
         if (srv)
            srv->GetResource(&source);
         if (uav)
            uav->GetResource(&target);
         D3D11_TEXTURE2D_DESC source_desc, target_desc;
         if (GetTextureDesc(source.get(), &source_desc) && GetTextureDesc(target.get(), &target_desc) && source_desc.Width == target_desc.Width && source_desc.Height == target_desc.Height && source_desc.Format == target_desc.Format)
         {
            native_device_context->CopyResource(target.get(), source.get());
#if DEVELOPMENT
            if (game_device_data.logged_watched_hashes.insert(hash).second)
               LogFormatted(reshade::log::level::info, "[Y3] frame %u game CAS replaced by a copy (RCAS ran with SMAA)", cb_luma_global_settings.FrameIndex);
#endif
            return DrawOrDispatchOverrideType::Replaced;
         }
      }

#if DEVELOPMENT
      // Dev logger for values the DevKit can't show continuously: frames that skip the ccr (untonemapped scene), the active
      // ccr perm and its grade constants, the material exposure scale, and the first target/blend state of watched passes.
      const bool log_frame = cb_luma_global_settings.FrameIndex % log_interval_frames == 0;
      // Readbacks map staging copies, which only works on the immediate context.
      const bool can_read = cmd_list_data.is_primary;

      if (!is_compute && ccr_hashes.contains(hash))
      {
         game_device_data.drew_ccr = true;
         game_device_data.ccr_hash = hash;
         if (log_frame && can_read)
         {
            com_ptr<ID3D11Buffer> cb;
            native_device_context->PSGetConstantBuffers(5, 1, &cb);
            std::vector<float> data;
            com_ptr<ID3D11Buffer> cb_copy;
            if (cb.get() && CopyBuffer(cb, native_device_context, data, cb_copy) && (data != game_device_data.last_logged_ccr_cb5 || hash != game_device_data.last_logged_ccr_hash))
            {
               game_device_data.last_logged_ccr_cb5 = data;
               game_device_data.last_logged_ccr_hash = hash;
               LogFormatted(reshade::log::level::info, "[Y3] frame %u ccr 0x%08X cb5 (%zu floats):", cb_luma_global_settings.FrameIndex, hash, data.size());
               for (size_t i = 0; i + 3 < data.size() && i < 64; i += 4)
                  LogFormatted(reshade::log::level::info, "[Y3]   c%zu = %.4f %.4f %.4f %.4f", i / 4, data[i], data[i + 1], data[i + 2], data[i + 3]);
            }
         }
      }
      else if (!is_compute && video_hashes.contains(hash))
      {
         game_device_data.drew_video = true;
      }
      // ASSAO prepare linearizes depth as cb0[1].x / (cb0[1].y - d): y ~1 = standard Z, ~0 = reverse Z (SMAA predication).
      else if (!is_compute && hash == 0x972BE5B5 && log_frame && can_read)
      {
         com_ptr<ID3D11Buffer> cb;
         native_device_context->PSGetConstantBuffers(0, 1, &cb);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         if (cb.get() && CopyBuffer(cb, native_device_context, data, cb_copy) && data.size() >= 8)
            LogFormatted(reshade::log::level::info, "[Y3] frame %u ASSAO depth unpack cb0[1]: %f %f %f %f", cb_luma_global_settings.FrameIndex, data[4], data[5], data[6], data[7]);
      }
      // Materials: before the ccr, with the per-material cb1/cb2/cb11 set bound (post passes don't bind cb1).
      else if (!is_compute && log_frame && can_read && !game_device_data.drew_ccr && game_device_data.exposure_samples.size() < max_exposure_samples)
      {
         com_ptr<ID3D11Buffer> cbs[3];
         native_device_context->PSGetConstantBuffers(1, 2, &cbs[0]);
         native_device_context->PSGetConstantBuffers(11, 1, &cbs[2]);
         std::vector<float> data;
         com_ptr<ID3D11Buffer> cb_copy;
         if (cbs[0].get() && cbs[1].get() && cbs[2].get() && CopyBuffer(cbs[1], native_device_context, data, cb_copy) && data.size() >= 16 * 4)
         {
            const std::array<float, 4> exposure = {data[56], data[57], data[58], data[59]};
            if (std::find(game_device_data.exposure_samples.begin(), game_device_data.exposure_samples.end(), exposure) == game_device_data.exposure_samples.end())
               game_device_data.exposure_samples.push_back(exposure);
         }
      }

      // Every pass writing the 512x512/512x256 targets upgraded for the DoF: each one now sees fp16 values above 1.
      if (!is_compute && log_frame && !game_device_data.logged_custom_size_hashes.contains(hash))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> target;
         if (rtv.get())
            rtv->GetResource(&target);
         D3D11_TEXTURE2D_DESC desc = {};
         if (com_ptr<ID3D11Texture2D> texture; target.get() && SUCCEEDED(target->QueryInterface(&texture)))
            texture->GetDesc(&desc);
         if (desc.Width == 512 && (desc.Height == 512 || desc.Height == 256))
         {
            game_device_data.logged_custom_size_hashes.insert(hash);
            LogFormatted(reshade::log::level::info, "[Y3] frame %u custom-size target %ux%u format %u written by PS 0x%08X", cb_luma_global_settings.FrameIndex, desc.Width, desc.Height, desc.Format, hash);
         }
      }

      if (const auto watched = watched_hashes.find(hash); watched != watched_hashes.end() && !game_device_data.logged_watched_hashes.contains(hash))
      {
         game_device_data.logged_watched_hashes.insert(hash);
         com_ptr<ID3D11Resource> target;
         if (is_compute)
         {
            com_ptr<ID3D11UnorderedAccessView> uav;
            native_device_context->CSGetUnorderedAccessViews(0, 1, &uav);
            if (uav.get())
               uav->GetResource(&target);
         }
         else
         {
            com_ptr<ID3D11RenderTargetView> rtv;
            native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
            if (rtv.get())
               rtv->GetResource(&target);
         }
         D3D11_TEXTURE2D_DESC desc = {};
         if (com_ptr<ID3D11Texture2D> texture; target.get() && SUCCEEDED(target->QueryInterface(&texture)))
            texture->GetDesc(&desc);
         // t0 is the color input of the DoF and video passes: an 8-bit source there clips before the pass even runs.
         D3D11_TEXTURE2D_DESC source_desc = {};
         D3D11_BLEND_DESC blend = {};
         if (!is_compute)
         {
            com_ptr<ID3D11ShaderResourceView> srv;
            native_device_context->PSGetShaderResources(0, 1, &srv);
            com_ptr<ID3D11Resource> source;
            if (srv.get())
               srv->GetResource(&source);
            if (com_ptr<ID3D11Texture2D> texture; source.get() && SUCCEEDED(source->QueryInterface(&texture)))
               texture->GetDesc(&source_desc);

            com_ptr<ID3D11BlendState> blend_state;
            FLOAT blend_factor[4];
            UINT sample_mask;
            native_device_context->OMGetBlendState(&blend_state, blend_factor, &sample_mask);
            if (blend_state.get())
               blend_state->GetDesc(&blend);
         }
         const D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend.RenderTarget[0];
         LogFormatted(reshade::log::level::info, "[Y3] frame %u first %s 0x%08X: target %ux%u format %u, t0 %ux%u format %u, blend %d (color %d/%d, alpha %d/%d), ccr drawn before: %d", cb_luma_global_settings.FrameIndex, watched->second, hash, desc.Width, desc.Height, desc.Format, source_desc.Width, source_desc.Height, source_desc.Format, rt_blend.BlendEnable, rt_blend.SrcBlend, rt_blend.DestBlend, rt_blend.SrcBlendAlpha, rt_blend.DestBlendAlpha, game_device_data.drew_ccr);
      }

#endif

      return DrawOrDispatchOverrideType::None;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      game_device_data.depth_srv.reset();
      game_device_data.cmaa2_replaced = false;
      game_device_data.smaa_ran = false;

#if DEVELOPMENT
      const uint32_t frame = cb_luma_global_settings.FrameIndex;

      // Frames without the ccr reach the display untonemapped (the fp16 scene can exceed paper white many times).
      if (!game_device_data.drew_ccr)
      {
         if (game_device_data.previous_frame_drew_ccr)
            LogFormatted(reshade::log::level::warning, "[Y3] frame %u: no ccr pass (video %d) - scene not tonemapped", frame, game_device_data.drew_video);
         game_device_data.frames_without_ccr++;
      }
      else if (!game_device_data.previous_frame_drew_ccr)
      {
         LogFormatted(reshade::log::level::info, "[Y3] frame %u: ccr back (0x%08X) after %u frames without", frame, game_device_data.ccr_hash, game_device_data.frames_without_ccr);
         game_device_data.frames_without_ccr = 0;
      }

      if (!game_device_data.exposure_samples.empty()) // Only sampled on log frames
      {
         std::string line = "[Y3] frame " + std::to_string(frame) + " material cb2[14]:";
         for (const auto& e : game_device_data.exposure_samples)
            line += std::format(" ({:.3f} {:.3f} {:.3f} {:.3f})", e[0], e[1], e[2], e[3]);
         reshade::log::message(reshade::log::level::info, line.c_str());
      }

      game_device_data.previous_frame_drew_ccr = game_device_data.drew_ccr;
      game_device_data.drew_ccr = false;
      game_device_data.drew_video = false;
      game_device_data.exposure_samples.clear();
#endif
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Yakuza 3 Remastered Luma mod - about and credits section", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza 3 Remastered Luma mod", "", 1);
      Globals::DEVELOPMENT_STATE = Globals::ModDevelopmentState::WorkInProgress;

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // b8g8r8a8_unorm backbuffer -> r16g16b16a16_float
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      // The scene RT and every swapchain-sized post target (CMAA2/CAS/resample/fade) are 8-bit UNORM. The lower bloom
      // levels (256x128 and smaller) stay 8-bit on purpose: the vanilla [0,1] bound is part of the look.
      texture_upgrade_formats = {
         reshade::api::format::b8g8r8a8_unorm,
         reshade::api::format::b8g8r8a8_typeless,
         reshade::api::format::r8g8b8a8_unorm,
         reshade::api::format::r8g8b8a8_typeless,
      };
      // The DoF runs before the ccr on a 512x512 scene copy (ps_texture) blurred into 512x256, which would clip highlights
      // inside the blurred area. Both sizes are upgraded too. The top bloom level shares 512x256, so the glow_pass0/1
      // replacements saturate to keep the vanilla bloom bound. (Hash-based mirrors don't help: they go through the same size filter.)
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::CustomSize;
      texture_format_upgrades_2d_custom_sizes = {{512, 512}, {512, 256}};

      game = new GameYakuza3();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
