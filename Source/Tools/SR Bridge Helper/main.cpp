// x64 helper for 32-bit Luma mods: DLSS (NGX) and FSR 3 (FidelityFX DX11) are x64 only, so the game's SR bridge
// ("Source/Core/sr_bridge/SRBridge.cpp") starts this next to the game's exe and hands it NT-handle shared textures and
// two shared fences. It runs the upscaler on its own device of the same adapter. Protocol: "SRBridgeProtocol.h".
#include "../../Core/sr_bridge/SRBridgeProtocol.h"

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "nvsdk_ngx_helpers.h"
#include "FidelityFX/host/backends/dx11/ffx_dx11.h"
#include "FidelityFX/host/ffx_fsr3.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace SRBridgeProtocol;

namespace
{
   struct Settings
   {
      int upscaler = kDlss;
      uint32_t render_width = 0, render_height = 0, output_width = 0, output_height = 0;
      int hdr = 1, inverted_depth = 0, mvs_jittered = 0, auto_exposure = 0, dynamic_resolution = 0;
      float mvs_x_scale = 1.f, mvs_y_scale = 1.f;
      int render_preset = 0;
   };

   // The protocol's "<settings>"
   std::istream& operator>>(std::istream& stream, Settings& settings)
   {
      return stream >> settings.render_width >> settings.render_height >> settings.output_width >> settings.output_height >> settings.hdr >> settings.inverted_depth >>
             settings.mvs_jittered >> settings.auto_exposure >> settings.dynamic_resolution >> settings.mvs_x_scale >> settings.mvs_y_scale >> settings.render_preset;
   }

   struct Frame
   {
      uint64_t n = 0;
      float jitter_x = 0.f, jitter_y = 0.f;
      int reset = 0;
      uint32_t render_width = 0, render_height = 0;
      float pre_exposure = 0.f, sharpness = -1.f, near_plane = 0.f, far_plane = 0.f, vert_fov = 0.f;
   };

   // As Luma's Core runs DLSS in x64 games (Source/Core/dlss/DLSS.cpp)
   struct Dlss
   {
      static constexpr const char* project_id = "d8238c51-1f2f-438d-a309-38c16e33c716"; // Luma's
      static constexpr int quality_modes = 6;

      ID3D11Device* device = nullptr; // Set once NGX is initialized on it
      NVSDK_NGX_Parameter* capabilities = nullptr;
      NVSDK_NGX_Parameter* parameters = nullptr;
      NVSDK_NGX_Handle* feature = nullptr;
      Settings settings;

      bool Init(ID3D11Device* device_)
      {
         const NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init_with_ProjectID(project_id, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".", device_);
         if (NVSDK_NGX_FAILED(result))
            return printf("[Luma Upscaler] NGX init 0x%08X\n", unsigned(result)), false;
         device = device_;
         NVSDK_NGX_D3D11_GetCapabilityParameters(&capabilities);
         int available = 0;
         if (capabilities)
            capabilities->Get(NVSDK_NGX_EParameter_SuperSampling_Available, &available);
         if (!available)
            return printf("[Luma Upscaler] DLSS not available\n"), false;
         return true;
      }

      // Replaces the previous feature
      bool CreateFeature(ID3D11DeviceContext* context, const Settings& settings_)
      {
         settings = settings_;
         ReleaseFeature();

         // The mode whose optimal render resolution matches, else the closest one in range
         int quality = NVSDK_NGX_PerfQuality_Value_Balanced;
         unsigned int best_delta = (std::numeric_limits<unsigned int>::max)();
         for (int i = 0; i < quality_modes; i++)
         {
            unsigned int optimal_width = 0, optimal_height = 0, min_width = 0, max_width = 0, min_height = 0, max_height = 0;
            float sharpness = 0.f;
            if (NVSDK_NGX_FAILED(NGX_DLSS_GET_OPTIMAL_SETTINGS(capabilities, settings.output_width, settings.output_height, NVSDK_NGX_PerfQuality_Value(i),
                   &optimal_width, &optimal_height, &max_width, &max_height, &min_width, &min_height, &sharpness)) ||
                optimal_width == 0 || optimal_height == 0)
               continue;
            if (i == NVSDK_NGX_PerfQuality_Value_DLAA)
            {
               optimal_width = max_width = settings.output_width;
               optimal_height = max_height = settings.output_height;
            }
            const unsigned int delta = std::abs(int(settings.render_width) - int(optimal_width)) + std::abs(int(settings.render_height) - int(optimal_height));
            if (!settings.dynamic_resolution && delta == 0)
            {
               quality = i;
               break;
            }
            if (settings.render_width >= min_width && settings.render_width <= max_width && settings.render_height >= min_height &&
                settings.render_height <= max_height && delta < best_delta)
            {
               quality = i;
               best_delta = delta;
            }
         }
         if (settings.render_width >= settings.output_width && settings.render_height >= settings.output_height)
            quality = NVSDK_NGX_PerfQuality_Value_DLAA;

         NVSDK_NGX_D3D11_AllocateParameters(&parameters);
         if (!parameters)
            return printf("[Luma Upscaler] NGX parameters\n"), false;
         for (const char* preset : {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraQuality,
                 NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced,
                 NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance})
            NVSDK_NGX_Parameter_SetUI(parameters, preset, unsigned(settings.render_preset));
         NVSDK_NGX_DLSS_Create_Params create = {};
         create.Feature.InWidth = settings.render_width;
         create.Feature.InHeight = settings.render_height;
         create.Feature.InTargetWidth = settings.output_width;
         create.Feature.InTargetHeight = settings.output_height;
         create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value(quality);
         create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | (settings.inverted_depth ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0) |
                                       (settings.mvs_jittered ? NVSDK_NGX_DLSS_Feature_Flags_MVJittered : 0) |
                                       (settings.auto_exposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0) | (settings.hdr ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0);
         NVSDK_NGX_Result result = NGX_D3D11_CREATE_DLSS_EXT(context, &feature, parameters, &create);
         if (NVSDK_NGX_FAILED(result))
         {
            // The quality mode can be refused (only the resolutions matter): the default preset and Balanced
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_Balanced;
            result = NGX_D3D11_CREATE_DLSS_EXT(context, &feature, parameters, &create);
         }
         if (NVSDK_NGX_FAILED(result))
            return printf("[Luma Upscaler] DLSS create 0x%08X\n", unsigned(result)), false;
         return true;
      }

      bool Evaluate(ID3D11DeviceContext* context, ID3D11Texture2D* const* textures, const Frame& frame) const
      {
         NVSDK_NGX_D3D11_DLSS_Eval_Params evaluate = {};
         evaluate.Feature.pInColor = textures[kColor];
         evaluate.Feature.pInOutput = textures[kOutput];
         evaluate.pInDepth = textures[kDepth];
         evaluate.pInMotionVectors = textures[kMotion];
         evaluate.pInExposureTexture = textures[kExposure];
         evaluate.pInBiasCurrentColorMask = textures[kReactive];
         evaluate.InRenderSubrectDimensions = {frame.render_width, frame.render_height};
         if (frame.pre_exposure != 0.f)
            evaluate.InPreExposure = frame.pre_exposure;
         evaluate.InReset = frame.reset;
         evaluate.InMVScaleX = settings.mvs_x_scale;
         evaluate.InMVScaleY = settings.mvs_y_scale;
         evaluate.InJitterOffsetX = frame.jitter_x;
         evaluate.InJitterOffsetY = frame.jitter_y;
         return NVSDK_NGX_SUCCEED(NGX_D3D11_EVALUATE_DLSS_EXT(context, feature, parameters, &evaluate));
      }

      void ReleaseFeature()
      {
         if (feature)
            NVSDK_NGX_D3D11_ReleaseFeature(feature);
         if (parameters)
            NVSDK_NGX_D3D11_DestroyParameters(parameters);
         feature = nullptr;
         parameters = nullptr;
      }

      ~Dlss()
      {
         ReleaseFeature();
         if (capabilities)
            NVSDK_NGX_D3D11_DestroyParameters(capabilities);
         if (device)
            NVSDK_NGX_D3D11_Shutdown1(device);
      }
   };

   // As Luma's Core runs FSR 3 in x64 games (Source/Core/fsr/FSR.cpp)
   struct Fsr
   {
      FfxFsr3Context context = {};
      std::vector<unsigned char> scratch;
      bool created = false;
      Settings settings;

      // Replaces the previous context
      bool Create(ID3D11Device* device, const Settings& settings_)
      {
         settings = settings_;
         if (created)
            ffxFsr3ContextDestroy(&context);
         context = {};
         created = false;
         scratch.resize(ffxGetScratchMemorySizeDX11(1));
         FfxFsr3ContextDescription create = {};
         FfxErrorCode result = ffxGetInterfaceDX11(&create.backendInterfaceUpscaling, ffxGetDeviceDX11(device), scratch.data(), scratch.size(), 1);
         if (result != FFX_OK)
            return printf("[Luma Upscaler] FSR interface %d\n", int(result)), false;
         create.displaySize = create.maxUpscaleSize = {settings.output_width, settings.output_height};
         create.maxRenderSize = settings.dynamic_resolution ? create.displaySize : FfxDimensions2D{settings.render_width, settings.render_height};
         create.flags = FFX_FSR3_ENABLE_UPSCALING_ONLY | (settings.hdr ? FFX_FSR3_ENABLE_HIGH_DYNAMIC_RANGE : 0) |
                        (settings.dynamic_resolution ? FFX_FSR3_ENABLE_DYNAMIC_RESOLUTION : 0) |
                        (settings.inverted_depth ? FFX_FSR3_ENABLE_DEPTH_INVERTED | FFX_FSR3_ENABLE_DEPTH_INFINITE : 0) |
                        (settings.mvs_jittered ? FFX_FSR3_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION : 0) | (settings.auto_exposure ? FFX_FSR3_ENABLE_AUTO_EXPOSURE : 0);
         create.backBufferFormat = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT; // Frame generation's only
         result = ffxFsr3ContextCreate(&context, &create);
         if (result != FFX_OK)
            return printf("[Luma Upscaler] FSR create %d\n", int(result)), false;
         created = true;
         return true;
      }

      static FfxResource Resource(ID3D11Texture2D* texture)
      {
         FfxResource resource = {};
         if (texture)
         {
            resource.resource = texture;
            resource.state = FFX_RESOURCE_STATE_COMPUTE_READ;
            resource.description = GetFfxResourceDescriptionDX11(texture);
         }
         return resource;
      }

      bool Evaluate(ID3D11DeviceContext* device_context, ID3D11Texture2D* const* textures, const Frame& frame)
      {
         FfxFsr3DispatchUpscaleDescription dispatch = {};
         dispatch.commandList = ffxGetCommandListDX11(device_context);
         dispatch.color = Resource(textures[kColor]);
         dispatch.depth = Resource(textures[kDepth]);
         dispatch.motionVectors = Resource(textures[kMotion]);
         dispatch.exposure = Resource(textures[kExposure]);
         dispatch.reactive = Resource(textures[kReactive]);
         dispatch.transparencyAndComposition = Resource(textures[kTransparency]);
         dispatch.upscaleOutput = Resource(textures[kOutput]);
         dispatch.jitterOffset = {frame.jitter_x, frame.jitter_y};
         dispatch.motionVectorScale = {settings.mvs_x_scale, settings.mvs_y_scale};
         dispatch.renderSize = {frame.render_width, frame.render_height};
         dispatch.upscaleSize = {settings.output_width, settings.output_height};
         dispatch.enableSharpening = frame.sharpness >= 0.f;
         dispatch.sharpness = dispatch.enableSharpening ? frame.sharpness : 0.f;
         dispatch.frameTimeDelta = 1000.f / 60.f; // Unused by upscaling
         dispatch.preExposure = frame.pre_exposure == 0.f ? 1.f : frame.pre_exposure;
         dispatch.reset = frame.reset != 0;
         dispatch.cameraNear = settings.inverted_depth ? frame.far_plane : frame.near_plane;
         dispatch.cameraFar = settings.inverted_depth ? frame.near_plane : frame.far_plane;
         dispatch.cameraFovAngleVertical = frame.vert_fov;
         dispatch.viewSpaceToMetersFactor = 1.f;
         return ffxFsr3ContextDispatchUpscale(&context, &dispatch) == FFX_OK;
      }

      ~Fsr()
      {
         if (created)
            ffxFsr3ContextDestroy(&context);
      }
   };

   ComPtr<IDXGIAdapter1> FindAdapter(const LUID& luid)
   {
      ComPtr<IDXGIFactory1> factory;
      if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
         return nullptr;
      ComPtr<IDXGIAdapter1> adapter;
      for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
      {
         DXGI_ADAPTER_DESC1 desc;
         if (SUCCEEDED(adapter->GetDesc1(&desc)) && desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart)
            return adapter;
      }
      return nullptr;
   }
} // namespace

int main()
{
   std::string mode;
   int version = 0;
   Settings settings;
   LUID luid = {};
   uint64_t handles[kCount] = {}, fence_handles[2] = {};
   std::cin >> mode >> version;
   if (mode != "bridge" || version != kVersion)
      return printf("[Luma Upscaler] FAIL protocol \"%s\" %d, expected \"bridge\" %d\n", mode.c_str(), version, kVersion), 1;
   std::cin >> settings.upscaler >> luid.LowPart >> luid.HighPart >> settings;
   for (uint64_t& handle : handles)
      std::cin >> handle;
   std::cin >> fence_handles[0] >> fence_handles[1];
   if (!std::cin)
      return printf("[Luma Upscaler] FAIL bad start line\n"), 1;

   const ComPtr<IDXGIAdapter1> adapter = FindAdapter(luid);
   if (!adapter)
      return printf("[Luma Upscaler] FAIL no adapter with the game's LUID\n"), 1;
   const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
   ComPtr<ID3D11Device> device;
   ComPtr<ID3D11DeviceContext> context;
   ComPtr<ID3D11Device1> device1;
   ComPtr<ID3D11Device5> device5;
   ComPtr<ID3D11DeviceContext4> context4;
   if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, levels, UINT(std::size(levels)), D3D11_SDK_VERSION, &device, nullptr, &context)) ||
       FAILED(device.As(&device1)) || FAILED(device.As(&device5)) || FAILED(context.As(&context4)))
      return printf("[Luma Upscaler] FAIL device\n"), 1;

   ComPtr<ID3D11Texture2D> textures[kCount];
   ID3D11Texture2D* raw[kCount] = {};
   for (int i = 0; i < kCount; i++)
   {
      if (handles[i] && FAILED(device1->OpenSharedResource1(HANDLE(uintptr_t(handles[i])), IID_PPV_ARGS(&textures[i]))))
         return printf("[Luma Upscaler] FAIL open texture %d\n", i), 1;
      raw[i] = textures[i].Get();
   }
   ComPtr<ID3D11Fence> fence_in, fence_out;
   if (!raw[kColor] || !raw[kMotion] || !raw[kDepth] || !raw[kOutput] || FAILED(device5->OpenSharedFence(HANDLE(uintptr_t(fence_handles[0])), IID_PPV_ARGS(&fence_in))) ||
       FAILED(device5->OpenSharedFence(HANDLE(uintptr_t(fence_handles[1])), IID_PPV_ARGS(&fence_out))))
      return printf("[Luma Upscaler] FAIL inputs or fences\n"), 1;

   std::unique_ptr<Dlss> dlss;
   std::unique_ptr<Fsr> fsr; // Its context is far larger than the stack
   if (settings.upscaler == kDlss)
   {
      dlss = std::make_unique<Dlss>();
      if (!dlss->Init(device.Get()) || !dlss->CreateFeature(context.Get(), settings))
         return 1;
   }
   else
   {
      fsr = std::make_unique<Fsr>();
      if (!fsr->Create(device.Get(), settings))
         return 1;
   }
   context4->Signal(fence_out.Get(), kReady);
   context->Flush();
   printf("[Luma Upscaler] ready: %s %ux%u -> %ux%u\n", dlss ? "DLSS" : "FSR 3", settings.render_width, settings.render_height, settings.output_width, settings.output_height);
   fflush(stdout);

   Frame frame;
   uint32_t frames = 0, failures = 0;
   std::string message;
   while (std::cin >> message)
   {
      if (message == "settings")
      {
         if (!(std::cin >> settings))
            return printf("[Luma Upscaler] FAIL bad settings line\n"), 1;
         if (dlss ? !dlss->CreateFeature(context.Get(), settings) : !fsr->Create(device.Get(), settings))
            return 1;
         printf("[Luma Upscaler] recreated: %ux%u -> %ux%u, preset %d\n", settings.render_width, settings.render_height, settings.output_width, settings.output_height,
            settings.render_preset);
         fflush(stdout);
         continue;
      }
      if (message != "frame" || !(std::cin >> frame.n >> frame.jitter_x >> frame.jitter_y >> frame.reset >> frame.render_width >> frame.render_height >> frame.pre_exposure >>
                                   frame.sharpness >> frame.near_plane >> frame.far_plane >> frame.vert_fov))
         return printf("[Luma Upscaler] FAIL bad line \"%s\"\n", message.c_str()), 1;
      context4->Wait(fence_in.Get(), frame.n);
      const bool drawn = dlss ? dlss->Evaluate(context.Get(), raw, frame) : fsr->Evaluate(context.Get(), raw, frame);
      context4->Signal(fence_out.Get(), frame.n);
      context->Flush();
      frames++;
      if (!drawn && failures++ < 5)
      {
         printf("[Luma Upscaler] frame %llu: the upscaler failed\n", (unsigned long long)frame.n);
         fflush(stdout);
      }
   }
   printf("[Luma Upscaler] the game closed the pipe after %u frames (%u failed)\n", frames, failures);
   return 0;
}
