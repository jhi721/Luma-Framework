#pragma once

// Luma TAA ("Shaders/Global/Luma_TAA_CS.hlsl", design and measurements in "docs/Luma-TAA.md"): temporal anti-aliasing, at native
// resolution or upscaling (TAAU, render smaller than output), as an SR implementation. It runs its compute passes on the game's own device and context, so it needs no SDK and
// works on any GPU, x86 included (no SR bridge). Core registers its shaders and attaches the "DeviceData" after "Init()".
// On D3D12 it records on the game's command list with its own root signature and descriptor heap, natively as the other upscalers do: the
// caller binds the game's state back ("NeedsStateRestoration()"). Core includes this after "CreateD3D12NativePipeline()", which makes its pipeline.
#if ENABLE_LUMA_TAA
namespace LumaTAA
{
   // "TAA_QUALITY" of the resolve: 0 Low, 1 Medium (adds a depth clip against the previous frame's depth), 2 High (adds the 2x
   // history). Core's UI sets it (saved as "LumaTAAQuality"); "UpdateSettings()" applies it.
   inline std::atomic<int> quality = 2;
   constexpr const char* quality_names[] = {"Low", "Medium", "High"};
   constexpr uint32_t resolve_shader_hashes[] = {Math::CompileTimeStringHash("Luma TAA Low CS"), Math::CompileTimeStringHash("Luma TAA Medium CS"), Math::CompileTimeStringHash("Luma TAA High CS")};
   // "TAA_UPSCALE" (one thread per output pixel; the history and the per pixel states at the output resolution, so High equals Medium)
   constexpr uint32_t upscale_shader_hashes[] = {Math::CompileTimeStringHash("Luma TAA Low Upscale CS"), Math::CompileTimeStringHash("Luma TAA Medium Upscale CS"), Math::CompileTimeStringHash("Luma TAA High Upscale CS")};
   // From Medium quality the lock texture also keeps each pixel's farthest 3x3 depth for the next frame's depth clip
   // ("TAA_PREVIOUS_DEPTH_CLIP")
   constexpr int medium_quality = 1;
   // Halton(2, 3) phases, as DLAA and FSR at native resolution. The history window (~25 frames, ~6 samples per 2x history texel)
   // doesn't average a whole cycle of 16, which left static aliasing pulsing at the cycle's period: 8 flickered ~25% less in
   // ME3LE and the lab (16 had 15-30% lower static error). Upscaling takes more (FSR's 8 x scale^2), so every output pixel gets
   // a sample within a cycle
   constexpr int jitter_phases = 8;
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
      float output_resolution[2]; // "TAA_UPSCALE"
      float inv_output_resolution[2];
   };
   static_assert(sizeof(CBData) == 64);
   constexpr uint32_t flag_reset = 1u << 0;
   constexpr uint32_t flag_inverted_depth = 1u << 1;
   constexpr uint32_t flag_reactive_mask = 1u << 2;

   inline bool IsUpscaling(const SR::SettingsData& settings_data)
   {
      return settings_data.render_width != settings_data.output_width || settings_data.render_height != settings_data.output_height;
   }

   inline uint32_t GetResolveShaderHash(const SR::SettingsData& settings_data, int level)
   {
      return (IsUpscaling(settings_data) ? upscale_shader_hashes : resolve_shader_hashes)[level];
   }

   inline CBData GetCBData(const SR::SettingsData& settings_data, const SR::SuperResolutionImpl::DrawData& draw_data, bool reset, bool reactive_mask)
   {
      const float render_width = float(settings_data.render_width), render_height = float(settings_data.render_height);
      const float output_width = float(settings_data.output_width), output_height = float(settings_data.output_height);
      return {
         .render_resolution = {render_width, render_height},
         .inv_render_resolution = {1.f / render_width, 1.f / render_height},
         .motion_vector_scale = {settings_data.mvs_x_scale, settings_data.mvs_y_scale},
         .depth_near_far = {draw_data.near_plane, draw_data.far_plane},
         .flags = (reset ? flag_reset : 0u) | (settings_data.inverted_depth ? flag_inverted_depth : 0u) | (reactive_mask ? flag_reactive_mask : 0u),
         // "DrawData" jitter offsets the projection, so the pixel center samples the scene at minus that offset
         .jitter = {-draw_data.jitter_x, -draw_data.jitter_y},
         .output_resolution = {output_width, output_height},
         .inv_output_resolution = {1.f / output_width, 1.f / output_height},
      };
   }

#if DX12
   // The table of "Luma_TAA_CS.hlsl": t0-t7, then u0-u3
   constexpr UINT srv_count = 8;
   constexpr UINT uav_count = 4;
   // A draw takes the next table of the ring: a game list recorded this many draws ago (one a frame) can't be reading it anymore
   constexpr UINT table_count = 32;
   constexpr D3D12_RESOURCE_STATES input_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   // The histories and locks between draws
   constexpr D3D12_RESOURCE_STATES rest_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

   // A format a view of a game's texture can have: typeless and depth formats have none
   inline DXGI_FORMAT GetViewFormat(DXGI_FORMAT format)
   {
      switch (format)
      {
      case DXGI_FORMAT_D32_FLOAT:
         return DXGI_FORMAT_R32_FLOAT;
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
         return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
         return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      case DXGI_FORMAT_D16_UNORM:
         return DXGI_FORMAT_R16_UNORM;
      default:
         // ReShade's format values are DXGI's; depth typeless formats map to their depth channel's SRV format
         return DXGI_FORMAT(reshade::api::format_to_default_typed(reshade::api::format(format)));
      }
   }
#endif

   struct TAAInstanceData : SR::InstanceData
   {
      // The quality level the resources were created for, latched by "UpdateSettings()" so "Draw()" never runs a resolve on resources
      // of another level
      int level = 0;
      // Per frame state ("Draw()" takes const data)
      mutable uint32_t history_index = 0;
      mutable bool history_valid = false;

#if DX12
      com_ptr<ID3D12Device> device; // The native one, so neither ReShade nor Core's hooks see what it makes
      com_ptr<ID3D12RootSignature> root_signature;
      com_ptr<ID3D12DescriptorHeap> descriptor_heap; // "table_count" tables of "srv_count" + "uav_count" descriptors
      UINT descriptor_increment = 0;
      mutable UINT next_table = 0;
      // As on D3D11, in "rest_state" between draws
      com_ptr<ID3D12Resource> history[2];
      com_ptr<ID3D12Resource> locks[2];
      // Made by "UpdateSettings()" once the natives are compiled, and again for another level or new natives
      com_ptr<ID3D12PipelineState> pipeline;
      uint32_t pipeline_shader_hash = 0;
      uint32_t pipeline_generation = 0;
      SR::DeferredReleases<com_ptr<IUnknown>> retired; // What the game's lists can still be using
#else
      // Created by "UpdateSettings()" at the output resolution. The history alternates between the two: rgb linear color, alpha the
      // accumulated weight.
      com_ptr<ID3D11Texture2D> history[2];
      com_ptr<ID3D11ShaderResourceView> history_srvs[2];
      com_ptr<ID3D11UnorderedAccessView> history_uavs[2];
      // Thin feature lock frames left (R8_UINT; R32_UINT from Medium, whose high 16 bits hold the depth clip's farthest depth),
      // alternating with the history
      com_ptr<ID3D11Texture2D> locks[2];
      com_ptr<ID3D11ShaderResourceView> lock_srvs[2];
      com_ptr<ID3D11UnorderedAccessView> lock_uavs[2];
      com_ptr<ID3D11Buffer> cbuffer;

      // The input views are kept while the same resources come back
      mutable com_ptr<ID3D11ShaderResourceView> source_color_srv;
      mutable com_ptr<ID3D11ShaderResourceView> depth_srv;
      mutable com_ptr<ID3D11ShaderResourceView> motion_vectors_srv;
      mutable com_ptr<ID3D11ShaderResourceView> reactive_mask_srv; // "DrawData::bias_mask", optional
      mutable com_ptr<ID3D11UnorderedAccessView> output_uav;
#endif
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

      int GetJitterPhases(const SR::InstanceData* data) const override
      {
         const auto* const taa_data = static_cast<const TAAInstanceData*>(data);
         if (!taa_data || !IsUpscaling(taa_data->settings_data))
            return jitter_phases;
         return (std::max)(jitter_phases, SR::GetFsrJitterPhases(taa_data->settings_data.render_width, taa_data->settings_data.output_width));
      }

#if DX12
      bool Init(SR::InstanceData*& data, ID3D12Device* device, IDXGIAdapter* adapter = nullptr) override
      {
         Deinit(data, device);
         auto* const taa_data = new TAAInstanceData();
         data = taa_data;
         taa_data->supports_dynamic_resolution = false;
         taa_data->supports_upscaling = true;
         taa_data->supports_arbitrary_jitter_phases = false;
         taa_data->supports_scrgb_hdr = true; // The per channel clip and the linear blend take negative (scRGB) channels as they are
         taa_data->device = device;
         taa_data->descriptor_increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

         const D3D12_DESCRIPTOR_RANGE1 ranges[] = {
            {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = srv_count, .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
            {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV, .NumDescriptors = uav_count, .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
         };
         const D3D12_ROOT_PARAMETER1 parameters[] = {
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, .Constants = {.Num32BitValues = sizeof(CBData) / sizeof(uint32_t) }},
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, .DescriptorTable = {.NumDescriptorRanges = UINT(std::size(ranges)),
                .pDescriptorRanges = ranges }},
         };
         const D3D12_STATIC_SAMPLER_DESC linear_clamp_sampler = {.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR, .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP, .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP, .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP, .MaxAnisotropy = 1, .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER, .MaxLOD = D3D12_FLOAT32_MAX};
         taa_data->root_signature = D3D12Passes::CreateRootSignature(device, {.Version = D3D_ROOT_SIGNATURE_VERSION_1_1, .Desc_1_1 = {.NumParameters = UINT(std::size(parameters)),
                                                                                 .pParameters = parameters,
                                                                                 .NumStaticSamplers = 1,
                                                                                 .pStaticSamplers = &linear_clamp_sampler }});
         const D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = table_count * (srv_count + uav_count), .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
         taa_data->is_supported = taa_data->root_signature && SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&taa_data->descriptor_heap)));
         return taa_data->is_supported;
      }

      void Deinit(SR::InstanceData*& data, ID3D12Device* optional_device = nullptr) override
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
            Retire(taa_data, std::addressof(taa_data->history[i]));
            Retire(taa_data, std::addressof(taa_data->locks[i]));
         }
         Retire(taa_data, std::addressof(taa_data->pipeline));
         taa_data->history_valid = false;
         taa_data->settings_data = {};
      }

      void ReleaseRetired(SR::InstanceData* data) override
      {
         static_cast<TAAInstanceData*>(data)->retired.Release([](auto*){}); // Dropping the entry releases it
      }

      bool UpdateSettings(SR::InstanceData* data, ID3D12GraphicsCommandList* command_list, const SR::SettingsData& settings_data) override
      {
         auto* const taa_data = static_cast<TAAInstanceData*>(data);
         if (!taa_data || !taa_data->is_supported || !taa_data->device_data)
            return false;
         if (settings_data.render_width > settings_data.output_width || settings_data.render_height > settings_data.output_height)
            return false;
         const int level = quality.load();
         if (!(settings_data == taa_data->settings_data && level == taa_data->level && taa_data->history[0]))
         {
            // Each level has its own set of resources, and the lock state of another level isn't valid here: start over
            ReleaseResources(taa_data);
            const UINT history_scale = (level >= history_2x_quality && !IsUpscaling(settings_data) ? 2 : 1);
            const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
            const D3D12_RESOURCE_DESC history_desc = {.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, .Width = settings_data.output_width * history_scale, .Height = settings_data.output_height * history_scale, .DepthOrArraySize = 1, .MipLevels = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {.Count = 1}, .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
            const D3D12_RESOURCE_DESC lock_desc = {.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, .Width = settings_data.output_width, .Height = settings_data.output_height, .DepthOrArraySize = 1, .MipLevels = 1, .Format = (level >= medium_quality ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R8_UINT), .SampleDesc = {.Count = 1}, .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
            // Committed resources start zeroed, which is what the D3D11 path clears them to
            for (int i = 0; i < 2; i++)
            {
               if (FAILED(taa_data->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &history_desc, rest_state, nullptr, IID_PPV_ARGS(&taa_data->history[i]))) ||
                   FAILED(taa_data->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &lock_desc, rest_state, nullptr, IID_PPV_ARGS(&taa_data->locks[i]))))
               {
                  ReleaseResources(taa_data);
                  return false;
               }
            }
            taa_data->settings_data = settings_data;
            taa_data->level = level;
         }

         const uint32_t shader_hash = GetResolveShaderHash(settings_data, level);
         const uint32_t generation = taa_data->device_data->native_shaders_generation.load(std::memory_order_acquire);
         if (!taa_data->pipeline || taa_data->pipeline_shader_hash != shader_hash || taa_data->pipeline_generation != generation)
         {
            Retire(taa_data, std::addressof(taa_data->pipeline));
            bool natives_found = false;
            taa_data->pipeline = CreateD3D12NativePipeline(taa_data->device.get(), taa_data->root_signature.get(), 0, shader_hash, DXGI_FORMAT_UNKNOWN, &natives_found);
            taa_data->pipeline_shader_hash = shader_hash;
            taa_data->pipeline_generation = generation;
            if (natives_found && !taa_data->pipeline)
            {
               reshade::log::message(reshade::log::level::error, "[Luma TAA] The device refused its pipeline (the debug layer says why)");
               return false;
            }
         }
         return true;
      }

      // Until Core has compiled its shaders (asynchronously, after boot) and "UpdateSettings()" made the pipeline and the history
      bool IsReady(const SR::InstanceData* data) const override
      {
         const auto* const taa_data = static_cast<const TAAInstanceData*>(data);
         return taa_data && taa_data->pipeline && taa_data->history[0];
      }

      bool NeedsStateRestoration() const override
      {
         return true;
      }

      bool Draw(const SR::InstanceData* data, ID3D12GraphicsCommandList* command_list, const DrawData& draw_data) override
      {
         const auto* const taa_data = static_cast<const TAAInstanceData*>(data);
         if (!taa_data || !taa_data->history[0] || !draw_data.output_color || !draw_data.source_color || !draw_data.motion_vectors || !draw_data.depth_buffer)
            return false;
         if (!IsReady(taa_data))
         {
            // The shaders are still compiling (boot or a live reload): pass the color through, as the SR bridge does while its helper
            // starts, rather than failing (games then fall back to their own anti-aliasing until the user picks the upscaler again)
            const D3D12_RESOURCE_DESC source_desc = draw_data.source_color->GetDesc();
            const D3D12_RESOURCE_DESC output_desc = draw_data.output_color->GetDesc();
            if (source_desc.Width != output_desc.Width || source_desc.Height != output_desc.Height || source_desc.MipLevels != output_desc.MipLevels || !AreFormatsCopyCompatible(source_desc.Format, output_desc.Format))
               return false;
            D3D12Passes::Transition(command_list, draw_data.source_color, input_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12Passes::Transition(command_list, draw_data.output_color, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
            command_list->CopyResource(draw_data.output_color, draw_data.source_color);
            D3D12Passes::Transition(command_list, draw_data.output_color, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            D3D12Passes::Transition(command_list, draw_data.source_color, D3D12_RESOURCE_STATE_COPY_SOURCE, input_state);
            taa_data->history_valid = false;
            return true;
         }

         const UINT table = taa_data->next_table;
         taa_data->next_table = (table + 1) % table_count;
         const SIZE_T table_offset = SIZE_T(table) * (srv_count + uav_count) * taa_data->descriptor_increment;
         D3D12_CPU_DESCRIPTOR_HANDLE descriptor = {taa_data->descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr + table_offset};
         ID3D12Device* const device = taa_data->device.get();
         const uint32_t write_index = taa_data->history_index;
         // The null ones are the slots the shader doesn't declare, and the optional reactive mask (AMD FSR 2 semantics)
         ID3D12Resource* const srvs[srv_count] = {draw_data.source_color, draw_data.depth_buffer, draw_data.motion_vectors, taa_data->history[write_index ^ 1].get(), nullptr, taa_data->locks[write_index ^ 1].get(), nullptr, draw_data.bias_mask};
         for (ID3D12Resource* const resource : srvs)
         {
            const DXGI_FORMAT format = (resource ? GetViewFormat(resource->GetDesc().Format) : DXGI_FORMAT_R8_UNORM);
            const D3D12_SHADER_RESOURCE_VIEW_DESC view = {.Format = format, .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D, .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, .Texture2D = {.MipLevels = 1}};
            device->CreateShaderResourceView(resource, &view, descriptor);
            descriptor.ptr += taa_data->descriptor_increment;
         }
         ID3D12Resource* const uavs[uav_count] = {taa_data->history[write_index].get(), draw_data.output_color, nullptr, taa_data->locks[write_index].get()};
         for (ID3D12Resource* const resource : uavs)
         {
            // Through typeless, as unordered access has no sRGB views
            const DXGI_FORMAT format = (resource ? GetViewFormat(DXGI_FORMAT(reshade::api::format_to_typeless(reshade::api::format(resource->GetDesc().Format)))) : DXGI_FORMAT_R8_UNORM);
            const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {.Format = format, .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
            device->CreateUnorderedAccessView(resource, nullptr, &view, descriptor);
            descriptor.ptr += taa_data->descriptor_increment;
         }

         ID3D12Resource* const history = taa_data->history[write_index].get();
         ID3D12Resource* const lock = taa_data->locks[write_index].get();
         D3D12Passes::Transition(command_list, history, rest_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         D3D12Passes::Transition(command_list, lock, rest_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         const bool reset = draw_data.reset || !taa_data->history_valid;
         const CBData cb_data = GetCBData(taa_data->settings_data, draw_data, reset, draw_data.bias_mask != nullptr);
         ID3D12DescriptorHeap* const heaps[] = {taa_data->descriptor_heap.get()};
         command_list->SetDescriptorHeaps(1, heaps);
         command_list->SetComputeRootSignature(taa_data->root_signature.get());
         command_list->SetComputeRoot32BitConstants(0, sizeof(cb_data) / sizeof(uint32_t), &cb_data, 0);
         command_list->SetComputeRootDescriptorTable(1, {taa_data->descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr + table_offset});
         command_list->SetPipelineState(taa_data->pipeline.get());
         command_list->Dispatch((taa_data->settings_data.output_width + 7) / 8, (taa_data->settings_data.output_height + 7) / 8, 1);
         D3D12Passes::Transition(command_list, history, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, rest_state);
         D3D12Passes::Transition(command_list, lock, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, rest_state);

         taa_data->history_index = write_index ^ 1;
         taa_data->history_valid = true;
         return true;
      }

   private:
      template <typename T>
      static void Retire(TAAInstanceData* taa_data, com_ptr<T>* object)
      {
         if (*object)
         {
            taa_data->retired.Push(com_ptr<IUnknown>(object->get()));
            object->reset();
         }
      }
#else
      bool Init(SR::InstanceData*& data, ID3D11Device* device, IDXGIAdapter* adapter = nullptr) override
      {
         Deinit(data, device);
         auto* const taa_data = new TAAInstanceData();
         // cs_5_0 with typed UAV stores and groupshared memory
         taa_data->is_supported = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
         taa_data->supports_dynamic_resolution = false;
         taa_data->supports_upscaling = true;
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
         }
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
         if (settings_data.render_width > settings_data.output_width || settings_data.render_height > settings_data.output_height)
            return false;
         const int level = quality.load();
         if (settings_data == taa_data->settings_data && level == taa_data->level && taa_data->history[0] && taa_data->cbuffer)
            return true;

         // Each level has its own set of resources, and the lock state of another level isn't valid here: start over
         ReleaseResources(taa_data);
         com_ptr<ID3D11Device> device;
         command_list->GetDevice(&device);
         // The history and the locks are at the output resolution (equal to the render one without upscaling), the native history at
         // 2x2 from High
         const UINT history_scale = (level >= history_2x_quality && !IsUpscaling(settings_data) ? 2 : 1);
         const CD3D11_TEXTURE2D_DESC history_desc(DXGI_FORMAT_R16G16B16A16_FLOAT, settings_data.output_width * history_scale, settings_data.output_height * history_scale, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         const CD3D11_TEXTURE2D_DESC lock_desc((level >= medium_quality ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R8_UINT), settings_data.output_width, settings_data.output_height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
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
               const FLOAT no_history[4] = {};
               command_list->ClearUnorderedAccessViewFloat(taa_data->history_uavs[i].get(), no_history);
            }
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
         return FindShader(taa_data->device_data->native_compute_shaders, GetResolveShaderHash(taa_data->settings_data, taa_data->level));
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
         ID3D11ComputeShader* const resolve_shader = FindShader(taa_data->device_data->native_compute_shaders, GetResolveShaderHash(taa_data->settings_data, taa_data->level));
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
         // The reactive mask (AMD FSR 2 semantics) is optional: "flag_reactive_mask" tells the shader one is bound
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
         const CBData cb_data = GetCBData(settings_data, draw_data, reset, bool(taa_data->reactive_mask_srv));
         std::memcpy(mapped.pData, &cb_data, sizeof(cb_data));
         command_list->Unmap(taa_data->cbuffer.get(), 0);

         // The games call this from their own passes: leave their compute state as it was
         DrawStateStack<DrawStateStackType::Compute> compute_state;
         compute_state.Cache(command_list, taa_data->device_data->uav_max_count);
         // D3D11 binds a resource still bound as an output as a null SRV: a game's scene left as a render target would read as black
         DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
         graphics_state.Cache(command_list, taa_data->device_data->uav_max_count);
         command_list->OMSetRenderTargets(0, nullptr, nullptr);

         // The resolve runs per output pixel when upscaling, per render pixel otherwise
         const UINT resolve_groups_x = (settings_data.output_width + 7) / 8, resolve_groups_y = (settings_data.output_height + 7) / 8;
         ID3D11Buffer* const cbuffer = taa_data->cbuffer.get();
         ID3D11SamplerState* const sampler = taa_data->device_data->sampler_state_linear.get();
         command_list->CSSetConstantBuffers(0, 1, &cbuffer);
         command_list->CSSetSamplers(0, 1, &sampler);
         const uint32_t write_index = taa_data->history_index;
         ID3D11ShaderResourceView* const srvs[8] = {taa_data->source_color_srv.get(), taa_data->depth_srv.get(), taa_data->motion_vectors_srv.get(), taa_data->history_srvs[write_index ^ 1].get(), nullptr, taa_data->lock_srvs[write_index ^ 1].get(), nullptr, taa_data->reactive_mask_srv.get()};
         ID3D11UnorderedAccessView* const uavs[4] = {taa_data->history_uavs[write_index].get(), taa_data->output_uav.get(), nullptr, taa_data->lock_uavs[write_index].get()};
         command_list->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
         command_list->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
         command_list->CSSetShader(resolve_shader, nullptr, 0);
         command_list->Dispatch(resolve_groups_x, resolve_groups_y, 1);
         ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
         ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
         command_list->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
         command_list->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);

         compute_state.Restore(command_list);
         graphics_state.Restore(command_list);
         taa_data->history_index = write_index ^ 1;
         taa_data->history_valid = true;
         return true;
      }
#endif // DX12
   };
} // namespace LumaTAA
#endif // ENABLE_LUMA_TAA
