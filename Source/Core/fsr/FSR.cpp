#include "FSR.h"

#if ENABLE_FIDELITY_SK

#include "../includes/debug.h"

#include <include/reshade.hpp>

#include <cstring>
#include <cwchar>
#include <cassert>
#include <format>
#include <unordered_set>
#include <wrl/client.h>

#if DEVELOPMENT
namespace FidelityFX
{
   // For FidelityFX's message callbacks. The debug checker reports at every dispatch, so each message is logged once.
   static void LogMessage(bool error, const wchar_t* message)
   {
      char buffer[512] = {};
      if (std::wcstombs(buffer, message ? message : L"", sizeof(buffer) - 1) == static_cast<std::size_t>(-1))
      {
         return;
      }
      static std::unordered_set<std::string> logged_messages;
      if (!logged_messages.emplace(buffer).second)
         return;

      reshade::log::message(error ? reshade::log::level::error : reshade::log::level::warning, (std::string("FSR: ") + buffer).c_str());
      ASSERT_MSGF(!error, "FSR Error: %s\n", buffer);
   }
} // namespace FidelityFX
#endif

#if DX12

#include "ffx_api/ffx_api.h"
#include "ffx_api/dx12/ffx_api_dx12.h"
#include "ffx_api/ffx_upscale.h"

#include <vector>
#include <d3d12.h>

namespace FidelityFX
{
   struct FSRInstanceData : public SR::InstanceData
   {
      // "amd_fidelityfx_loader_dx12.dll", which loads the upscaler dll ("version_name" points into it)
      HMODULE module = nullptr;
      PfnFfxCreateContext create_context = nullptr;
      PfnFfxDestroyContext destroy_context = nullptr;
      PfnFfxQuery query = nullptr;
      PfnFfxDispatch dispatch = nullptr;
      Microsoft::WRL::ComPtr<ID3D12Device> device;

      // The upscaler versions the runtime supports on this device (FSR 4 isn't listed on GPUs that can't run it)
      std::vector<uint64_t> version_ids;

      ffxContext context = nullptr;
      int context_version_override = -1;
      int phase_count = SR::GetDefaultJitterPhases();
      SR::DeferredReleases<ffxContext> retired_contexts;

      void RetireContext()
      {
         retired_contexts.Retire(&context);
         version_name = nullptr;
      }
   };

   bool FidelityFX::FSR::Init(SR::InstanceData*& data, SR::Device* device, IDXGIAdapter* adapter)
   {
      if (data)
      {
         Deinit(data); // This will also null the pointer
      }

      // From the game's folder, where the package puts it next to the upscaler dll (not from the current directory).
      // If the game ships its own, that one is already loaded and is used instead.
      HMODULE module = LoadLibraryExW(L"amd_fidelityfx_loader_dx12.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
      if (!module)
      {
         reshade::log::message(reshade::log::level::warning, "FSR: amd_fidelityfx_loader_dx12.dll not found next to the game's executable");
         return false;
      }

      auto& custom_data = reinterpret_cast<FSRInstanceData*&>(data);
      custom_data = new FSRInstanceData();
      custom_data->module = module;
      custom_data->device = device;
      custom_data->min_resolution = 32;
      custom_data->create_context = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(module, "ffxCreateContext"));
      custom_data->destroy_context = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(module, "ffxDestroyContext"));
      custom_data->query = reinterpret_cast<PfnFfxQuery>(GetProcAddress(module, "ffxQuery"));
      custom_data->dispatch = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(module, "ffxDispatch"));
      if (!custom_data->create_context || !custom_data->destroy_context || !custom_data->query || !custom_data->dispatch)
      {
         return false;
      }

      // Count, then fill
      uint64_t version_count = 0;
      ffxQueryDescGetVersions versions_query = {};
      versions_query.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
      versions_query.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
      versions_query.device = device;
      versions_query.outputCount = &version_count;
      if (custom_data->query(nullptr, &versions_query.header) != FFX_API_RETURN_OK || version_count == 0)
      {
         reshade::log::message(reshade::log::level::warning, "FSR: the FidelityFX runtime has no upscaler for this device");
         return false;
      }
      custom_data->version_ids.resize(version_count);
      std::vector<const char*> version_names(version_count);
      versions_query.versionIds = custom_data->version_ids.data();
      versions_query.versionNames = version_names.data();
      if (custom_data->query(nullptr, &versions_query.header) != FFX_API_RETURN_OK)
      {
         return false;
      }

      std::string versions_text = "FSR: upscaler versions (\"core.fsr_version_index\": name):";
      for (size_t i = 0; i < version_names.size(); i++)
      {
         versions_text += std::format(" {}: {};", i, version_names[i] ? version_names[i] : "?");
      }
      reshade::log::message(reshade::log::level::info, versions_text.c_str());

      custom_data->is_supported = true;
      return true;
   }

   void FidelityFX::FSR::Deinit(SR::InstanceData*& data, SR::Device* optional_device)
   {
      auto& custom_data = reinterpret_cast<FSRInstanceData*&>(data);
      if (!custom_data)
      {
         return;
      }

      // The device is going away, nothing of it runs anymore
      custom_data->RetireContext();
      custom_data->retired_contexts.ReleaseAll([&](ffxContext* retired)
         { custom_data->destroy_context(retired, nullptr); });
      HMODULE module = custom_data->module;
      delete custom_data;
      custom_data = nullptr;
      FreeLibrary(module);
   }

   // The context (FSR's own textures). The next "UpdateSettings()" creates it again.
   void FidelityFX::FSR::ReleaseResources(SR::InstanceData* data)
   {
      if (data)
      {
         static_cast<FSRInstanceData*>(data)->RetireContext();
      }
   }

   void FidelityFX::FSR::ReleaseRetired(SR::InstanceData* data)
   {
      auto* const custom_data = static_cast<FSRInstanceData*>(data);
      custom_data->retired_contexts.Release([&](ffxContext* retired)
         { custom_data->destroy_context(retired, nullptr); });
   }

   bool FidelityFX::FSR::UpdateSettings(SR::InstanceData* data, SR::CommandList* command_list, const SR::SettingsData& settings_data)
   {
      auto* custom_data = static_cast<FSRInstanceData*>(data);
      if (!command_list || !custom_data || !custom_data->is_supported)
         return false;

      const int selected_version_override = ((version_index_override >= 0 && version_index_override < int(custom_data->version_ids.size())) ? version_index_override : -1);
      if (settings_data == custom_data->settings_data && custom_data->context && selected_version_override == custom_data->context_version_override)
      {
         return true;
      }

      ffxCreateBackendDX12Desc backend_desc = {};
      backend_desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
      backend_desc.device = custom_data->device.Get();

      ffxCreateContextDescUpscaleVersion version_desc = {};
      version_desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;
      version_desc.header.pNext = &backend_desc.header;
      version_desc.version = FFX_UPSCALER_VERSION;

      ffxOverrideVersion override_desc = {};
      override_desc.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
      override_desc.header.pNext = &version_desc.header;
      if (selected_version_override >= 0)
      {
         override_desc.versionId = custom_data->version_ids[selected_version_override];
      }

      ffxCreateContextDescUpscale context_desc = {};
      context_desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
      context_desc.header.pNext = (selected_version_override >= 0 ? &override_desc.header : &version_desc.header);
      context_desc.maxUpscaleSize.width = settings_data.output_width;
      context_desc.maxUpscaleSize.height = settings_data.output_height;
      if (settings_data.dynamic_resolution)
      {
         context_desc.maxRenderSize.width = settings_data.output_width;
         context_desc.maxRenderSize.height = settings_data.output_height;
         context_desc.flags |= FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION;
      }
      else
      {
         context_desc.maxRenderSize.width = settings_data.render_width;
         context_desc.maxRenderSize.height = settings_data.render_height;
      }
      if (settings_data.auto_exposure)
      {
         context_desc.flags |= FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
      }
      if (settings_data.hdr)
      {
         context_desc.flags |= FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
      }
      if (settings_data.inverted_depth)
      {
         context_desc.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_DEPTH_INFINITE;
      }
      if (settings_data.mvs_jittered)
      {
         context_desc.flags |= FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
      }
#if DEVELOPMENT
      // The checker works with the release dlls too
      context_desc.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING | FFX_UPSCALE_ENABLE_DEBUG_VISUALIZATION;
      context_desc.fpMessage = [](uint32_t type, const wchar_t* message)
      {
         // Without an exposure texture and auto exposure FSR uses an exposure of 1, as DLSS and XeSS do with the same inputs: not a misconfiguration
         if (std::wcsstr(message, L"exposure resource is null") != nullptr)
            return;
         LogMessage(type == FFX_API_MESSAGE_TYPE_ERROR, message);
      };
#endif

      custom_data->RetireContext();

      if (custom_data->create_context(&custom_data->context, &context_desc.header, nullptr) != FFX_API_RETURN_OK)
      {
         // No retry policy here: without a context "Draw()" fails, and the games then suppress SR until an upscaler is picked again
         custom_data->context = nullptr;
         ASSERT_ONCE_MSG(false, "Couldn't create the FSR context");
         return false;
      }

      custom_data->settings_data = settings_data;
      custom_data->context_version_override = selected_version_override;

      ffxQueryGetProviderVersion provider_query = {};
      provider_query.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
      if (custom_data->query(&custom_data->context, &provider_query.header) == FFX_API_RETURN_OK)
      {
         custom_data->version_name = provider_query.versionName;
      }

      custom_data->phase_count = SR::GetFsrJitterPhases(settings_data.render_width, settings_data.output_width); // Kept if the query fails
      ffxQueryDescUpscaleGetJitterPhaseCount phases_query = {};
      phases_query.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT;
      phases_query.renderWidth = settings_data.render_width;
      phases_query.displayWidth = settings_data.output_width;
      phases_query.pOutPhaseCount = &custom_data->phase_count;
      custom_data->query(&custom_data->context, &phases_query.header);

      return true;
   }

   int FidelityFX::FSR::GetJitterPhases(const SR::InstanceData* data) const
   {
      return static_cast<const FSRInstanceData*>(data)->phase_count;
   }

   bool FidelityFX::FSR::Draw(const SR::InstanceData* data, SR::CommandList* command_list, const DrawData& draw_data)
   {
      const auto* custom_data = static_cast<const FSRInstanceData*>(data);
      // No context: "UpdateSettings()" failed or wasn't called since "ReleaseResources()"
      if (!custom_data->context)
      {
         return false;
      }

      // The states the caller left them in (see "DrawData"): FidelityFX transitions them for its passes and back to these after
      constexpr uint32_t input_state = FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ;
      ffxDispatchDescUpscale dispatch_upscale = {};
      dispatch_upscale.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
      dispatch_upscale.commandList = command_list;
      dispatch_upscale.color = ffxApiGetResourceDX12(draw_data.source_color, input_state);
      dispatch_upscale.depth = ffxApiGetResourceDX12(draw_data.depth_buffer, input_state);
      dispatch_upscale.motionVectors = ffxApiGetResourceDX12(draw_data.motion_vectors, input_state);
      dispatch_upscale.exposure = ffxApiGetResourceDX12(draw_data.exposure, input_state);
      dispatch_upscale.reactive = ffxApiGetResourceDX12(draw_data.bias_mask, input_state);
      dispatch_upscale.transparencyAndComposition = ffxApiGetResourceDX12(draw_data.transparency_alpha, input_state);
      dispatch_upscale.output = ffxApiGetResourceDX12(draw_data.output_color, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);

      dispatch_upscale.upscaleSize.width = custom_data->settings_data.output_width;
      dispatch_upscale.upscaleSize.height = custom_data->settings_data.output_height;
      // 0: the settings' render size (like DLSS)
      dispatch_upscale.renderSize.width = (draw_data.render_width ? draw_data.render_width : custom_data->settings_data.render_width);
      dispatch_upscale.renderSize.height = (draw_data.render_height ? draw_data.render_height : custom_data->settings_data.render_height);
      dispatch_upscale.jitterOffset.x = draw_data.jitter_x;
      dispatch_upscale.jitterOffset.y = draw_data.jitter_y;
      dispatch_upscale.motionVectorScale.x = custom_data->settings_data.mvs_x_scale;
      dispatch_upscale.motionVectorScale.y = custom_data->settings_data.mvs_y_scale;
      dispatch_upscale.preExposure = (draw_data.pre_exposure == 0.f ? 1.f : draw_data.pre_exposure);

      dispatch_upscale.cameraFovAngleVertical = draw_data.vert_fov;
      if (custom_data->settings_data.inverted_depth)
      {
         dispatch_upscale.cameraFar = draw_data.near_plane;
         dispatch_upscale.cameraNear = draw_data.far_plane;
      }
      else
      {
         dispatch_upscale.cameraFar = draw_data.far_plane;
         dispatch_upscale.cameraNear = draw_data.near_plane;
      }
      dispatch_upscale.viewSpaceToMetersFactor = 1.f;

      dispatch_upscale.reset = draw_data.reset;
      dispatch_upscale.enableSharpening = draw_data.user_sharpness >= 0.f;
      dispatch_upscale.sharpness = (dispatch_upscale.enableSharpening ? draw_data.user_sharpness : 0.f);
      dispatch_upscale.frameTimeDelta = (draw_data.time_delta > 0.f ? draw_data.time_delta : (1.f / 60.f)) * 1000.f; // Milliseconds
#if ENABLE_FSR_DEBUG_VIEW
      if (SR::debug_view)
      {
         dispatch_upscale.flags |= FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW;
      }
#endif

      // The context is the dll's handle, the "const" is only ours
      ffxContext context = custom_data->context;
      const ffxReturnCode_t result = custom_data->dispatch(&context, &dispatch_upscale.header);
      ASSERT_ONCE_MSG(result == FFX_API_RETURN_OK, "FSR dispatch failed");
      return result == FFX_API_RETURN_OK;
   }
} // namespace FidelityFX

#else

#include "FidelityFX/host/backends/dx11/ffx_dx11.h"
#include "FidelityFX/host/ffx_fsr3.h"
#include "FidelityFX/host/ffx_fsr3upscaler.h"

#include <d3d11.h>

namespace FidelityFX
{
   constexpr int default_phase_count = 16;

   struct FSRInstanceData : public SR::InstanceData
   {
      FfxFsr3Context context = {};
      bool has_context = false;
      void* scratch_buffer = nullptr;
      int phase_count = default_phase_count;
   };

   // TODO: delete
   double MillisecondsNow()
   {
      static LARGE_INTEGER s_frequency;
      static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
      double milliseconds = 0;
      if (s_use_qpc)
      {
         LARGE_INTEGER now;
         QueryPerformanceCounter(&now);
         milliseconds = double(1000.0 * now.QuadPart) / s_frequency.QuadPart;
      }
      else
      {
         milliseconds = double(GetTickCount64());
      }
      return milliseconds;
   }
   double GetDeltaTime()
   {
      static double last_frame_time = MillisecondsNow();
      double currentTime = MillisecondsNow();
      double deltaTime = (currentTime - last_frame_time);
      last_frame_time = currentTime;
      return deltaTime;
   }

   // Calling this function in the library ("ffxGetResourceDX1") gives a linking error for some reason
   FfxResource ffxGetResourceDX11_local(ID3D11Resource* dx11Resource,
      FfxResourceDescription ffxResDescription,
      wchar_t const* ffxResName,
      FfxResourceStates state = FFX_RESOURCE_STATE_COMPUTE_READ)
   {
      FfxResource resource = {};
      resource.resource = reinterpret_cast<void*>(const_cast<ID3D11Resource*>(dx11Resource));
      resource.state = state;
      resource.description = ffxResDescription;

#if DEVELOPMENT
      if (ffxResName)
      {
         wcscpy_s(resource.name, ffxResName);
      }
#endif

      return resource;
   }

   bool FidelityFX::FSR::Init(SR::InstanceData*& data, ID3D11Device* device, IDXGIAdapter* adapter)
   {
      if (data)
      {
         Deinit(data); // This will also null the pointer
      }

      auto& custom_data = reinterpret_cast<FSRInstanceData*&>(data);

      if (device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0)
      {
         ASSERT_ONCE(false); // FSR isn't support on DX11.0, disable it for this game if possible, there's no point in bundling it

         return false;
      }
      else
      {
         custom_data = new FSRInstanceData();
         custom_data->is_supported = true;
         custom_data->automatically_restores_pipeline_state = false; // TODO: does it actually not do it? What states does it change? Does DLSS properly do it?
         custom_data->min_resolution = 32;                           // Not 100% accurate but should be a decent start
         static const std::string version = std::format("{}.{}.{}", FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
         custom_data->version_name = version.c_str();
      }

      // NOTE: the init for FSR is the same as the context creation. There's no separate global dll init like there is for DLSS,
      // and given our implementation is flexible and doesn't know the settings until the first drawn frame,
      // we delay the init there. There's no reason why FSR wouldn't run unless the user deleted the dll.
      // Here we could check if the dll is present, but that wouldn't be the right check to do in all cases, so we avoid it (note: the current DX11 version of FSR has no dll, it's statically linked).

      return custom_data != nullptr && custom_data->is_supported;
   }

   void FidelityFX::FSR::Deinit(SR::InstanceData*& data, ID3D11Device* optional_device)
   {
      auto& custom_data = reinterpret_cast<FSRInstanceData*&>(data);

      ReleaseResources(custom_data);

      delete custom_data;
      custom_data = nullptr;
   }

   // The context (FSR's own textures). The next "UpdateSettings()" creates it again.
   void FidelityFX::FSR::ReleaseResources(SR::InstanceData* data)
   {
      auto* custom_data = static_cast<FSRInstanceData*>(data);
      if (!custom_data || !custom_data->has_context)
      {
         return;
      }
      FfxErrorCode err_code = ffxFsr3ContextDestroy(&custom_data->context);
      if (err_code != FFX_OK)
      {
         printf_s("FSR3: ffxFsr3ContextDestroy failed, error = %d\n", static_cast<int>(err_code));
      }
      memset(&custom_data->context, 0, sizeof(FfxFsr3Context));
      custom_data->has_context = false;

      free(custom_data->scratch_buffer);
      custom_data->scratch_buffer = nullptr;
   }

   bool FidelityFX::FSR::HasInit(const SR::InstanceData* data) const
   {
      return true;
   }

   bool FidelityFX::FSR::IsSupported(const SR::InstanceData* data) const
   {
      return data->is_supported;
   }

   bool FidelityFX::FSR::UpdateSettings(SR::InstanceData* data, ID3D11DeviceContext* command_list, const SR::SettingsData& settings_data)
   {
      auto& custom_data = reinterpret_cast<FSRInstanceData*&>(data);

      // Early exit if FSR is not supported by hardware or driver.
      if (!command_list || !custom_data || !custom_data->is_supported)
         return false;

      // No need to re-instantiate FSR "features" if all the params are the same
      if (settings_data == custom_data->settings_data && custom_data->has_context) // Not memcmp: the struct's padding is indeterminate
      {
         return true;
      }

      const size_t scratch_buffer_size = ffxGetScratchMemorySizeDX11(1);
      void* scratch_buffer = calloc(scratch_buffer_size, 1);

      Microsoft::WRL::ComPtr<ID3D11Device> device;
      command_list->GetDevice(device.GetAddressOf());

      FfxFsr3ContextDescription context_desc{};

      FfxErrorCode err_code = ffxGetInterfaceDX11(&context_desc.backendInterfaceUpscaling, ffxGetDeviceDX11(device.Get()), scratch_buffer, scratch_buffer_size, 1);
      if (err_code != FFX_OK)
      {
         printf_s("FSR3: ffxGetInterfaceDX11 failed, error = %d\n", static_cast<int>(err_code));
         ASSERT_ONCE(false);
         free(scratch_buffer);
         return false;
      }

      context_desc.displaySize.width = settings_data.output_width;
      context_desc.displaySize.height = settings_data.output_height;
      context_desc.maxUpscaleSize.width = settings_data.output_width;
      context_desc.maxUpscaleSize.height = settings_data.output_height;
      if (settings_data.dynamic_resolution)
      {
         context_desc.maxRenderSize.width = settings_data.output_width;
         context_desc.maxRenderSize.height = settings_data.output_height;
      }
      else
      {
         context_desc.maxRenderSize.width = settings_data.render_width;
         context_desc.maxRenderSize.height = settings_data.render_height;
      }

      context_desc.flags |= FFX_FSR3_ENABLE_UPSCALING_ONLY; // No FG for now (there's none in DX11 anyway)
      if (settings_data.auto_exposure)
      {
         context_desc.flags |= FFX_FSR3_ENABLE_AUTO_EXPOSURE;
      }
      if (settings_data.hdr)
      {
         context_desc.flags |= FFX_FSR3_ENABLE_HIGH_DYNAMIC_RANGE;
      }
      if (settings_data.dynamic_resolution)
      {
         context_desc.flags |= FFX_FSR3_ENABLE_DYNAMIC_RESOLUTION;
      }
      if (settings_data.inverted_depth)
      {
         context_desc.flags |= FFX_FSR3_ENABLE_DEPTH_INVERTED | FFX_FSR3_ENABLE_DEPTH_INFINITE;
      }
      if (settings_data.mvs_jittered)
      {
         context_desc.flags |= FFX_FSR3_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
      }

      context_desc.backBufferFormat = FfxSurfaceFormat::FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT; // Just guessed for now, for Frame Gen only. Luma classic format hardcoded.

#if DEVELOPMENT
      if (true) // For now we always do this in dev mode
      {
         context_desc.fpMessage = [](FfxMsgType type, const wchar_t* message)
         {
            LogMessage(type == FFX_MESSAGE_TYPE_ERROR, message);
         };
         context_desc.flags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
      }
#endif

      // Destroy any possible previously created context
      ReleaseResources(custom_data);

      err_code = ffxFsr3ContextCreate(&custom_data->context, &context_desc);
      if (err_code != FFX_OK)
      {
         // No retry policy here: without a context "Draw()" fails, and the games then suppress SR until an upscaler is picked again
         printf_s("FSR3: ffxFsr3ContextCreate failed, error = %d\n", static_cast<int>(err_code));
         ASSERT_ONCE_MSG(false, "Couldn't create FSR3 context");
         free(scratch_buffer);
         return false;
      }

      custom_data->settings_data = settings_data;
      custom_data->scratch_buffer = scratch_buffer;
      custom_data->has_context = true;

      custom_data->phase_count = ffxFsr3GetJitterPhaseCount(settings_data.render_width, settings_data.output_width);

      return true;
   }

   int FidelityFX::FSR::GetJitterPhases(const SR::InstanceData* data) const
   {
      auto& custom_data = reinterpret_cast<const FSRInstanceData*&>(data);
      return custom_data->phase_count;
   }

   bool FidelityFX::FSR::Draw(const SR::InstanceData* data, ID3D11DeviceContext* command_list, const DrawData& draw_data)
   {
      auto& custom_data = reinterpret_cast<const FSRInstanceData*&>(data);
      // No context: "UpdateSettings()" failed or wasn't called since "ReleaseResources()"
      if (!custom_data->has_context)
      {
         return false;
      }

      FfxFsr3DispatchUpscaleDescription dispatch_upscale{};

      dispatch_upscale.commandList = ffxGetCommandListDX11(command_list);
      dispatch_upscale.color = ffxGetResourceDX11_local(draw_data.source_color, GetFfxResourceDescriptionDX11(draw_data.source_color), L"FSR3_color");
      dispatch_upscale.upscaleOutput = ffxGetResourceDX11_local(draw_data.output_color, GetFfxResourceDescriptionDX11(draw_data.output_color), L"FSR3_upscaleOutput");
      dispatch_upscale.depth = ffxGetResourceDX11_local(draw_data.depth_buffer, GetFfxResourceDescriptionDX11(draw_data.depth_buffer), L"FSR3_depth");
      dispatch_upscale.motionVectors = ffxGetResourceDX11_local(draw_data.motion_vectors, GetFfxResourceDescriptionDX11(draw_data.motion_vectors), L"FSR3_motionVectors");
      if (draw_data.exposure)
      {
         dispatch_upscale.exposure = ffxGetResourceDX11_local(draw_data.exposure, GetFfxResourceDescriptionDX11(draw_data.exposure), L"FSR3_exposure");
      }
      if (draw_data.bias_mask)
      {
         dispatch_upscale.reactive = ffxGetResourceDX11_local(draw_data.bias_mask, GetFfxResourceDescriptionDX11(draw_data.bias_mask), L"FSR3_reactive");
      }
      if (draw_data.transparency_alpha)
      {
         dispatch_upscale.transparencyAndComposition = ffxGetResourceDX11_local(draw_data.transparency_alpha, GetFfxResourceDescriptionDX11(draw_data.transparency_alpha), L"FSR3_transparencyAndComposition");
      }

#if DEVELOPMENT
      D3D11_TEXTURE2D_DESC output_desc;
      ((ID3D11Texture2D*)draw_data.output_color)->GetDesc(&output_desc);
      ASSERT_ONCE(draw_data.output_color && custom_data->settings_data.output_width == (int)output_desc.Width && custom_data->settings_data.output_height == (int)output_desc.Height);
#endif
      dispatch_upscale.upscaleSize.width = custom_data->settings_data.output_width;
      dispatch_upscale.upscaleSize.height = custom_data->settings_data.output_height;
      // 0: the settings' render size (like DLSS)
      dispatch_upscale.renderSize.width = draw_data.render_width ? draw_data.render_width : custom_data->settings_data.render_width;
      dispatch_upscale.renderSize.height = draw_data.render_height ? draw_data.render_height : custom_data->settings_data.render_height;
      dispatch_upscale.jitterOffset.x = draw_data.jitter_x;
      dispatch_upscale.jitterOffset.y = draw_data.jitter_y;
      dispatch_upscale.motionVectorScale.x = custom_data->settings_data.mvs_x_scale;
      dispatch_upscale.motionVectorScale.y = custom_data->settings_data.mvs_y_scale;

      dispatch_upscale.preExposure = draw_data.pre_exposure == 0.f ? 1.f : draw_data.pre_exposure;

      dispatch_upscale.cameraFovAngleVertical = draw_data.vert_fov;
      if (custom_data->settings_data.inverted_depth)
      {
         dispatch_upscale.cameraFar = draw_data.near_plane;
         dispatch_upscale.cameraNear = draw_data.far_plane;
      }
      else
      {
         dispatch_upscale.cameraFar = draw_data.far_plane;
         dispatch_upscale.cameraNear = draw_data.near_plane;
      }
      // dispatch_upscale.cameraFar = draw_data.near_plane; // TODO: why does FSR complain depth is too small with inverse depth? Even with 100000
      // dispatch_upscale.cameraNear = draw_data.far_plane;
      dispatch_upscale.viewSpaceToMetersFactor = 1.f; // This can be left at zero and still work. Most engines units should be meters.

      dispatch_upscale.reset = draw_data.reset;

      dispatch_upscale.enableSharpening = draw_data.user_sharpness >= 0.f;
      dispatch_upscale.sharpness = dispatch_upscale.enableSharpening ? draw_data.user_sharpness : 0.f;

      float time_delta = draw_data.time_delta > 0.f ? draw_data.time_delta : (1.f / 60.f);
      dispatch_upscale.frameTimeDelta = static_cast<float>(time_delta * 1000.f); // FSR expects milliseconds. Unused by upscaling.
      dispatch_upscale.frameID = draw_data.frame_index;                          // This is ignored so it doesn't matter (it's for FG)

      dispatch_upscale.flags = 0;
#if ENABLE_FSR_DEBUG_VIEW
      if (SR::debug_view)
      {
         dispatch_upscale.flags |= FFX_FSR3_UPSCALER_FLAG_DRAW_DEBUG_VIEW;
      }
#endif

      FfxErrorCode err_code = ffxFsr3ContextDispatchUpscale(const_cast<FfxFsr3Context*>(&custom_data->context), &dispatch_upscale);
      if (err_code != FFX_OK)
      {
         printf_s("FSR3: ffxFsr3ContextDispatchUpscale failed, error = %d\n", static_cast<int>(err_code));
      }
      return err_code == FFX_OK;
   }
} // namespace FidelityFX

#endif // DX12

#endif
