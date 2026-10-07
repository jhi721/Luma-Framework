#pragma once

// Luma TAA ("Shaders/Global/Luma_TAA_CS.hlsl", design and measurements in "docs/Luma-TAA.md"): native resolution temporal
// anti-aliasing as an SR implementation. It runs its compute passes on the game's own device and context, so it needs no SDK and
// works on any GPU, x86 included (no SR bridge). Core registers its shaders and attaches the "DeviceData" after "Init()".
#if ENABLE_LUMA_TAA
namespace LumaTAA
{
   // "TAA_QUALITY" of the resolve: 0 Low, 1 Medium (adds the flickering analysis), 2 High (adds the 2x history), 3 Ultra (adds a depth
   // clip, with a previous depth reconstruction pass). Core's UI sets it (saved as "LumaTAAQuality"); "UpdateSettings()" applies it.
   inline std::atomic<int> quality = 2;
   constexpr const char* quality_names[] = {"Low", "Medium", "High", "Ultra"};
   constexpr uint32_t resolve_shader_hashes[] = {Math::CompileTimeStringHash("Luma TAA Low CS"), Math::CompileTimeStringHash("Luma TAA Medium CS"), Math::CompileTimeStringHash("Luma TAA High CS"), Math::CompileTimeStringHash("Luma TAA Ultra CS")};
   constexpr uint32_t reconstruct_depth_shader_hash = Math::CompileTimeStringHash("Luma TAA Reconstruct Depth CS");
   // From Medium quality the resolve runs the flickering analysis ("TAA_FLICKER"), with its state texture pair
   constexpr int flicker_quality = 1;
   constexpr int ultra_quality = 3;
   // Halton(2, 3) phases: the history converges over ~25 frames, so 16 well spread sample positions get averaged
   constexpr int jitter_phases = 16;
   // From High quality the history is kept at 2x2 the render resolution ("TAA_HISTORY_2X"): repeated reprojection then blurs far
   // less, and each history texel accumulates the samples that land in its quarter pixel
   constexpr int history_2x_quality = 2;

   // Mirrors "LumaTAAData" in "Luma_TAA_CS.hlsl"
   struct alignas(16) CBData
   {
      float render_resolution[2];
      float inv_render_resolution[2];
      float motion_vector_scale[2];
      float depth_near_far[2];
      uint32_t flags;
      float jitter[2]; // Offset of this frame's sample from the pixel center, in pixels (the opposite of "DrawData" jitter)
      float padding;
   };
   static_assert(sizeof(CBData) == 48);
   constexpr uint32_t flag_reset = 1u << 0;
   constexpr uint32_t flag_inverted_depth = 1u << 1;
   constexpr UINT reconstructed_depth_clear_value = 0x7F7FFFFF; // asuint(FLT_MAX): nothing reconstructed

   struct TAAInstanceData : SR::InstanceData
   {
      DeviceData* device_data = nullptr; // Its native shaders and linear sampler. Set by Core right after "Init()".

      // Created by "UpdateSettings()" at the output resolution. The history alternates between the two: rgb linear color, alpha the
      // accumulated weight.
      com_ptr<ID3D11Texture2D> history[2];
      com_ptr<ID3D11ShaderResourceView> history_srvs[2];
      com_ptr<ID3D11UnorderedAccessView> history_uavs[2];
      com_ptr<ID3D11Texture2D> reconstructed_depth; // Ultra only, R32_UINT
      com_ptr<ID3D11ShaderResourceView> reconstructed_depth_srv;
      com_ptr<ID3D11UnorderedAccessView> reconstructed_depth_uav;
      // Thin feature lock frames left (R8_UINT), alternating with the history
      com_ptr<ID3D11Texture2D> locks[2];
      com_ptr<ID3D11ShaderResourceView> lock_srvs[2];
      com_ptr<ID3D11UnorderedAccessView> lock_uavs[2];
      // Flickering analysis state from Medium (R11G11B10_FLOAT at the render resolution), alternating with the history; cleared to 0 = no state
      com_ptr<ID3D11Texture2D> flicker[2];
      com_ptr<ID3D11ShaderResourceView> flicker_srvs[2];
      com_ptr<ID3D11UnorderedAccessView> flicker_uavs[2];
      com_ptr<ID3D11Buffer> cbuffer;
      // The quality level the resources were created for, latched by "UpdateSettings()" so "Draw()" never runs a resolve on resources
      // of another level
      int level = 0;

      // Per frame state ("Draw()" takes const data). The input views are kept while the same resources come back.
      mutable uint32_t history_index = 0;
      mutable bool history_valid = false;
      mutable com_ptr<ID3D11ShaderResourceView> source_color_srv;
      mutable com_ptr<ID3D11ShaderResourceView> depth_srv;
      mutable com_ptr<ID3D11ShaderResourceView> motion_vectors_srv;
      mutable com_ptr<ID3D11ShaderResourceView> reactive_mask_srv; // "DrawData::bias_mask", optional
      mutable com_ptr<ID3D11UnorderedAccessView> output_uav;
   };

   class TAA final : public SR::SuperResolutionImpl
   {
   public:
      bool HasInit(const SR::InstanceData* data) const override
      {
         return data != nullptr;
      }

      bool IsSupported(const SR::InstanceData* data) const override
      {
         return data != nullptr && data->is_supported;
      }

      bool Init(SR::InstanceData*& data, ID3D11Device* device, IDXGIAdapter* adapter = nullptr) override
      {
         Deinit(data, device);
         auto* const taa_data = new TAAInstanceData();
         // cs_5_0 with typed UAV stores, a typed R32_UINT UAV atomic and groupshared memory
         taa_data->is_supported = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
         taa_data->supports_dynamic_resolution = false;
         taa_data->supports_upscaling = false;
         taa_data->supports_arbitrary_jitter_phases = false;
         taa_data->supports_scrgb_hdr = true; // The per channel clip and the linear blend take negative (scRGB) channels as they are
         taa_data->automatically_restores_pipeline_state = true;
         data = taa_data;
         return taa_data->is_supported;
      }

      void Deinit(SR::InstanceData*& data, ID3D11Device* optional_device = nullptr) override
      {
         delete static_cast<TAAInstanceData*>(data);
         data = nullptr;
      }

      void ReleaseResources(SR::InstanceData* data) override
      {
         auto* const taa_data = static_cast<TAAInstanceData*>(data);
         if (!taa_data)
            return;
         for (int i = 0; i < 2; i++)
         {
            taa_data->history[i].reset();
            taa_data->history_srvs[i].reset();
            taa_data->history_uavs[i].reset();
            taa_data->locks[i].reset();
            taa_data->lock_srvs[i].reset();
            taa_data->lock_uavs[i].reset();
            taa_data->flicker[i].reset();
            taa_data->flicker_srvs[i].reset();
            taa_data->flicker_uavs[i].reset();
         }
         taa_data->reconstructed_depth.reset();
         taa_data->reconstructed_depth_srv.reset();
         taa_data->reconstructed_depth_uav.reset();
         taa_data->cbuffer.reset();
         taa_data->source_color_srv.reset();
         taa_data->depth_srv.reset();
         taa_data->motion_vectors_srv.reset();
         taa_data->reactive_mask_srv.reset();
         taa_data->output_uav.reset();
         taa_data->history_valid = false;
         taa_data->settings_data = {};
      }

      bool UpdateSettings(SR::InstanceData* data, ID3D11DeviceContext* command_list, const SR::SettingsData& settings_data) override
      {
         auto* const taa_data = static_cast<TAAInstanceData*>(data);
         if (!taa_data || !taa_data->is_supported)
            return false;
         // Native resolution only
         if (settings_data.render_width != settings_data.output_width || settings_data.render_height != settings_data.output_height)
            return false;
         const int level = quality.load();
         if (settings_data == taa_data->settings_data && level == taa_data->level && taa_data->history[0] && taa_data->cbuffer)
            return true;

         // Each level has its own set of resources, and the lock and flickering analysis states of another level aren't valid here (Low
         // doesn't write the latter): start over
         ReleaseResources(taa_data);
         com_ptr<ID3D11Device> device;
         command_list->GetDevice(&device);
         const UINT history_scale = (level >= history_2x_quality ? 2 : 1);
         const CD3D11_TEXTURE2D_DESC history_desc(DXGI_FORMAT_R16G16B16A16_FLOAT, settings_data.output_width * history_scale, settings_data.output_height * history_scale, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_TEXTURE2D_DESC reconstructed_depth_desc(DXGI_FORMAT_R32_UINT, settings_data.output_width, settings_data.output_height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_TEXTURE2D_DESC flicker_desc(DXGI_FORMAT_R11G11B10_FLOAT, settings_data.output_width, settings_data.output_height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_TEXTURE2D_DESC lock_desc(DXGI_FORMAT_R8_UINT, settings_data.output_width, settings_data.output_height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_BUFFER_DESC cbuffer_desc(sizeof(CBData), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
         bool created = SUCCEEDED(device->CreateBuffer(&cbuffer_desc, nullptr, &taa_data->cbuffer));
         for (int i = 0; i < 2 && created; i++)
         {
            created = SUCCEEDED(device->CreateTexture2D(&history_desc, nullptr, &taa_data->history[i])) && SUCCEEDED(device->CreateShaderResourceView(taa_data->history[i].get(), nullptr, &taa_data->history_srvs[i])) && SUCCEEDED(device->CreateUnorderedAccessView(taa_data->history[i].get(), nullptr, &taa_data->history_uavs[i]));
            created = created && SUCCEEDED(device->CreateTexture2D(&lock_desc, nullptr, &taa_data->locks[i])) && SUCCEEDED(device->CreateShaderResourceView(taa_data->locks[i].get(), nullptr, &taa_data->lock_srvs[i])) && SUCCEEDED(device->CreateUnorderedAccessView(taa_data->locks[i].get(), nullptr, &taa_data->lock_uavs[i]));
            if (created)
            {
               const UINT no_lock[4] = {};
               command_list->ClearUnorderedAccessViewUint(taa_data->lock_uavs[i].get(), no_lock);
            }
            if (created && level >= flicker_quality)
            {
               created = SUCCEEDED(device->CreateTexture2D(&flicker_desc, nullptr, &taa_data->flicker[i])) && SUCCEEDED(device->CreateShaderResourceView(taa_data->flicker[i].get(), nullptr, &taa_data->flicker_srvs[i])) && SUCCEEDED(device->CreateUnorderedAccessView(taa_data->flicker[i].get(), nullptr, &taa_data->flicker_uavs[i]));
               if (created)
               {
                  const FLOAT no_state[4] = {};
                  command_list->ClearUnorderedAccessViewFloat(taa_data->flicker_uavs[i].get(), no_state);
               }
            }
         }
         if (created && level >= ultra_quality)
         {
            created = SUCCEEDED(device->CreateTexture2D(&reconstructed_depth_desc, nullptr, &taa_data->reconstructed_depth)) && SUCCEEDED(device->CreateShaderResourceView(taa_data->reconstructed_depth.get(), nullptr, &taa_data->reconstructed_depth_srv)) && SUCCEEDED(device->CreateUnorderedAccessView(taa_data->reconstructed_depth.get(), nullptr, &taa_data->reconstructed_depth_uav));
         }
         if (!created)
         {
            ReleaseResources(taa_data);
            return false;
         }
         taa_data->settings_data = settings_data;
         taa_data->level = level;
         return true;
      }

      // Until Core has compiled its shaders (asynchronously, after boot) and "UpdateSettings()" created the history
      bool IsReady(const SR::InstanceData* data) const override
      {
         const auto* const taa_data = static_cast<const TAAInstanceData*>(data);
         if (!taa_data || !taa_data->device_data || !taa_data->history[0])
            return false;
         const int level = taa_data->level;
         const auto& shaders = taa_data->device_data->native_compute_shaders;
         return FindShader(shaders, resolve_shader_hashes[level]) && (level < ultra_quality || FindShader(shaders, reconstruct_depth_shader_hash));
      }

      int GetJitterPhases(const SR::InstanceData* data) const override
      {
         return jitter_phases;
      }

      bool Draw(const SR::InstanceData* data, ID3D11DeviceContext* command_list, const DrawData& draw_data) override
      {
         const auto* const taa_data = static_cast<const TAAInstanceData*>(data);
         if (!taa_data || !taa_data->device_data || !taa_data->history[0] || !draw_data.output_color || !draw_data.source_color || !draw_data.motion_vectors || !draw_data.depth_buffer)
            return false;
         if (!IsReady(taa_data))
         {
            // The shaders are still compiling (boot or a live reload): pass the color through, as the SR bridge does while its helper
            // starts, rather than failing (games then fall back to their own anti-aliasing until the user picks the upscaler again)
            com_ptr<ID3D11Texture2D> source, output;
            if (FAILED(draw_data.source_color->QueryInterface(&source)) || FAILED(draw_data.output_color->QueryInterface(&output)))
               return false;
            D3D11_TEXTURE2D_DESC source_desc, output_desc;
            source->GetDesc(&source_desc);
            output->GetDesc(&output_desc);
            if (source_desc.Width != output_desc.Width || source_desc.Height != output_desc.Height || !AreFormatsCopyCompatible(source_desc.Format, output_desc.Format))
               return false;
            command_list->CopyResource(output.get(), source.get());
            taa_data->history_valid = false;
            return true;
         }
         const int level = taa_data->level;
         const auto& shaders = taa_data->device_data->native_compute_shaders;
         ID3D11ComputeShader* const resolve_shader = FindShader(shaders, resolve_shader_hashes[level]);
         ID3D11ComputeShader* const reconstruct_depth_shader = (level >= ultra_quality ? FindShader(shaders, reconstruct_depth_shader_hash) : nullptr);
         const SR::SettingsData& settings_data = taa_data->settings_data;

         com_ptr<ID3D11Device> device;
         command_list->GetDevice(&device);
         // Views are recreated only when the game hands in a different resource (a view keeps its resource alive, so a cached one can't
         // match a new resource at a recycled address). Typeless resources are viewed with their float/unorm format.
         const auto update_srv = [&](ID3D11Resource* resource, com_ptr<ID3D11ShaderResourceView>* srv)
         {
            if (*srv)
            {
               com_ptr<ID3D11Resource> viewed;
               (*srv)->GetResource(&viewed);
               if (viewed.get() == resource)
                  return true;
            }
            srv->reset();
            if (SUCCEEDED(device->CreateShaderResourceView(resource, nullptr, &*srv)))
               return true;
            com_ptr<ID3D11Texture2D> texture;
            if (FAILED(resource->QueryInterface(&texture)))
               return false;
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            // ReShade's format values are DXGI's; depth typeless formats map to their depth channel's SRV format (R24_UNORM_X8, R32_FLOAT_X8X24)
            const auto format = DXGI_FORMAT(reshade::api::format_to_default_typed(reshade::api::format(desc.Format)));
            if (format == desc.Format)
               return false;
            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, format);
            return SUCCEEDED(device->CreateShaderResourceView(resource, &srv_desc, &*srv));
         };
         bool views = update_srv(draw_data.source_color, std::addressof(taa_data->source_color_srv)) && update_srv(draw_data.depth_buffer, std::addressof(taa_data->depth_srv)) && update_srv(draw_data.motion_vectors, std::addressof(taa_data->motion_vectors_srv));
         // The reactive mask (AMD FSR 2 semantics) is optional: the shader reads 0 where none is bound
         if (!draw_data.bias_mask || !update_srv(draw_data.bias_mask, std::addressof(taa_data->reactive_mask_srv)))
         {
            taa_data->reactive_mask_srv.reset();
         }
         if (views)
         {
            com_ptr<ID3D11Resource> viewed;
            if (taa_data->output_uav)
            {
               taa_data->output_uav->GetResource(&viewed);
            }
            if (viewed.get() != draw_data.output_color)
            {
               taa_data->output_uav.reset();
               views = SUCCEEDED(device->CreateUnorderedAccessView(draw_data.output_color, nullptr, &taa_data->output_uav));
            }
         }
         if (!views)
            return false;

         const bool reset = draw_data.reset || !taa_data->history_valid;
         D3D11_MAPPED_SUBRESOURCE mapped;
         if (FAILED(command_list->Map(taa_data->cbuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return false;
         const float width = float(settings_data.output_width), height = float(settings_data.output_height);
         const CBData cb_data = {
            .render_resolution = {width, height},
            .inv_render_resolution = {1.f / width, 1.f / height},
            .motion_vector_scale = {settings_data.mvs_x_scale, settings_data.mvs_y_scale},
            .depth_near_far = {draw_data.near_plane, draw_data.far_plane},
            .flags = (reset ? flag_reset : 0u) | (settings_data.inverted_depth ? flag_inverted_depth : 0u),
            // "DrawData" jitter offsets the projection, so the pixel center samples the scene at minus that offset
            .jitter = {-draw_data.jitter_x, -draw_data.jitter_y},
         };
         std::memcpy(mapped.pData, &cb_data, sizeof(cb_data));
         command_list->Unmap(taa_data->cbuffer.get(), 0);

         // The games call this from their own passes: leave their compute state as it was
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(command_list, taa_data->device_data->uav_max_count);

         const UINT groups_x = (settings_data.output_width + 7) / 8, groups_y = (settings_data.output_height + 7) / 8;
         ID3D11Buffer* const cbuffer = taa_data->cbuffer.get();
         ID3D11SamplerState* const sampler = taa_data->device_data->sampler_state_linear.get();
         command_list->CSSetConstantBuffers(0, 1, &cbuffer);
         command_list->CSSetSamplers(0, 1, &sampler);
         ID3D11ShaderResourceView* const null_srvs[8] = {};
         ID3D11UnorderedAccessView* const null_uavs[5] = {};
         if (reconstruct_depth_shader)
         {
            const UINT clear_value[4] = {reconstructed_depth_clear_value, reconstructed_depth_clear_value, reconstructed_depth_clear_value, reconstructed_depth_clear_value};
            command_list->ClearUnorderedAccessViewUint(taa_data->reconstructed_depth_uav.get(), clear_value);
            ID3D11ShaderResourceView* const srvs[3] = {nullptr, taa_data->depth_srv.get(), taa_data->motion_vectors_srv.get()};
            ID3D11UnorderedAccessView* const reconstructed_depth_uav = taa_data->reconstructed_depth_uav.get();
            command_list->CSSetShaderResources(0, 3, srvs);
            command_list->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
            command_list->CSSetUnorderedAccessViews(2, 1, &reconstructed_depth_uav, nullptr);
            command_list->CSSetShader(reconstruct_depth_shader, nullptr, 0);
            command_list->Dispatch(groups_x, groups_y, 1);
            command_list->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
         }
         const uint32_t write_index = taa_data->history_index;
         ID3D11ShaderResourceView* const srvs[8] = {taa_data->source_color_srv.get(), taa_data->depth_srv.get(), taa_data->motion_vectors_srv.get(), taa_data->history_srvs[write_index ^ 1].get(), taa_data->reconstructed_depth_srv.get(), taa_data->lock_srvs[write_index ^ 1].get(), taa_data->flicker_srvs[write_index ^ 1].get(), taa_data->reactive_mask_srv.get()};
         ID3D11UnorderedAccessView* const uavs[5] = {taa_data->history_uavs[write_index].get(), taa_data->output_uav.get(), nullptr, taa_data->lock_uavs[write_index].get(), taa_data->flicker_uavs[write_index].get()};
         command_list->CSSetShaderResources(0, 8, srvs);
         command_list->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);
         command_list->CSSetShader(resolve_shader, nullptr, 0);
         command_list->Dispatch(groups_x, groups_y, 1);
         command_list->CSSetShaderResources(0, 8, null_srvs);
         command_list->CSSetUnorderedAccessViews(0, 5, null_uavs, nullptr);

         compute_state.Restore(command_list);
         taa_data->history_index = write_index ^ 1;
         taa_data->history_valid = true;
         return true;
      }
   };
} // namespace LumaTAA
#endif // ENABLE_LUMA_TAA
