#include "SRBridge.h"

#if ENABLE_SR_BRIDGE

#include "SRBridgeProtocol.h"
#include "../utils/system.h"

#include <include/reshade.hpp>

#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>

namespace SRBridge
{
   using Microsoft::WRL::ComPtr;
   using namespace SRBridgeProtocol;

   namespace
   {
      void Log(reshade::log::level level, const std::string& message)
      {
         reshade::log::message(level, ("[SR Bridge] " + message).c_str());
      }

      // Before trying again after the helper failed, so a game that keeps drawing doesn't start one per frame
      constexpr auto retry_delay = std::chrono::seconds(2);
   } // namespace

   struct BridgeInstanceData : public SR::InstanceData
   {
      SR::Type type = SR::Type::None;
      ComPtr<ID3D11Device5> device;
      LUID luid = {};

      // The running helper
      HANDLE job = nullptr; // Ends the helper with the game ("JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE")
      HANDLE process = nullptr;
      HANDLE pipe = nullptr; // Its stdin
      ComPtr<ID3D11DeviceContext4> context;
      ComPtr<ID3D11Fence> fences[2];          // "in": a frame's inputs copied, "out": its output written (first "kReady")
      ID3D11Resource* sources[kCount] = {};   // The game's resources it was started for (compared only)
      ComPtr<ID3D11Texture2D> shared[kCount]; // What it opened: the game's own when NT-handle shared already, else copies
      bool copied[kCount] = {};
      uint64_t frame = 0;
      bool ready = false;

      bool failed = false;
      std::chrono::steady_clock::time_point failure_time;

      ~BridgeInstanceData()
      {
         Stop();
      }

      void Stop();
      bool Fail(const std::string& reason);
      bool Start(ID3D11DeviceContext* command_list, ID3D11Resource* const* resources);
   };

   // One helper at a time: Core only deinitializes the selected type, so a previous selection's instance stops its own here
   BridgeInstanceData* running = nullptr;

   void BridgeInstanceData::Stop()
   {
      if (pipe)
         CloseHandle(pipe); // EOF: the helper ends by itself
      if (process)
      {
         WaitForSingleObject(process, 500);
         CloseHandle(process);
      }
      if (job)
         CloseHandle(job); // Ends it if it's still running
      job = process = pipe = nullptr;
      for (auto& fence : fences)
         fence.Reset();
      for (int i = 0; i < kCount; i++)
      {
         sources[i] = nullptr;
         shared[i].Reset();
         copied[i] = false;
      }
      frame = 0;
      ready = false;
      if (running == this)
         running = nullptr;
   }

   bool BridgeInstanceData::Fail(const std::string& reason)
   {
      Stop();
      failed = true;
      failure_time = std::chrono::steady_clock::now();
      Log(reshade::log::level::warning, reason);
      return false;
   }

   bool BridgeInstanceData::Start(ID3D11DeviceContext* command_list, ID3D11Resource* const* resources)
   {
      if (running && running != this)
         running->Stop();
      Stop();
      if (FAILED(command_list->QueryInterface(IID_PPV_ARGS(&context))))
         return Fail("no D3D11 fences on this device context");
      if (!resources[kColor] || !resources[kMotion] || !resources[kDepth] || !resources[kOutput])
         return Fail("missing color, motion vectors, depth or output");

      HANDLE handles[kCount + 2] = {}; // The textures', then the fences'
      const auto fail = [&](const std::string& reason)
      {
         for (HANDLE handle : handles)
            if (handle)
               CloseHandle(handle);
         return Fail(reason);
      };
      for (int i = 0; i < kCount; i++)
      {
         sources[i] = resources[i];
         if (!resources[i])
            continue;
         ComPtr<ID3D11Texture2D> texture;
         if (FAILED(resources[i]->QueryInterface(IID_PPV_ARGS(&texture))))
            return fail(std::format("resource {} isn't a 2D texture", i));
         D3D11_TEXTURE2D_DESC desc;
         texture->GetDesc(&desc);
         copied[i] = (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0;
         if (copied[i])
         {
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (i == kOutput ? D3D11_BIND_UNORDERED_ACCESS : 0);
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture)))
               return fail(std::format("resource {} (format {}) can't be shared", i, int(desc.Format)));
         }
         shared[i] = texture;
         ComPtr<IDXGIResource1> dxgi_resource;
         if (FAILED(texture.As(&dxgi_resource)) ||
             FAILED(dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handles[i])))
            return fail(std::format("resource {} has no shared handle", i));
      }
      for (int i = 0; i < 2; i++)
      {
         if (FAILED(device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fences[i]))) ||
             FAILED(fences[i]->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handles[kCount + i])))
            return fail("no shared fences");
      }

      job = CreateJobObjectW(nullptr, nullptr);
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
      limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
         return fail("no job object");

      // The helper next to the exe: its stdin a pipe, its output the log file there
      const std::filesystem::path directory = System::GetModulePath().parent_path();
      const std::filesystem::path helper_path = directory / kHelperName;
      SECURITY_ATTRIBUTES inherit = {sizeof(inherit), nullptr, TRUE};
      HANDLE pipe_read = nullptr;
      const HANDLE log_file = CreateFileW((directory / kLogName).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, CREATE_ALWAYS, 0, nullptr);
      PROCESS_INFORMATION process_info = {};
      bool started = false;
      if (log_file != INVALID_HANDLE_VALUE && CreatePipe(&pipe_read, &pipe, &inherit, 0))
      {
         SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0);
         STARTUPINFOW startup = {sizeof(startup)};
         startup.dwFlags = STARTF_USESTDHANDLES;
         startup.hStdInput = pipe_read;
         startup.hStdOutput = startup.hStdError = log_file;
         std::wstring command = L"\"" + helper_path.wstring() + L"\"";
         // Suspended until it's in the job, so it can't outlive the game even if it's killed right away
         started = CreateProcessW(helper_path.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, directory.c_str(), &startup, &process_info);
         if (started)
         {
            process = process_info.hProcess;
            started = AssignProcessToJobObject(job, process) && ResumeThread(process_info.hThread) != DWORD(-1);
            CloseHandle(process_info.hThread);
         }
      }
      const DWORD error = GetLastError();
      if (!started && process)
         TerminateProcess(process, 0); // Created, but not in the job
      for (HANDLE handle : {pipe_read, log_file})
         if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
      if (!started)
         return fail(std::format("the helper couldn't start (error {}): {}", error, helper_path.string()));

      const SR::SettingsData& settings = settings_data;
      std::string message = std::format("bridge {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}", kVersion, type == SR::Type::DLSS ? int(kDlss) : int(kFsr), luid.LowPart,
         luid.HighPart, settings.render_width, settings.render_height, settings.output_width, settings.output_height, int(settings.hdr), int(settings.inverted_depth),
         int(settings.mvs_jittered), int(settings.auto_exposure), int(settings.dynamic_resolution), settings.mvs_x_scale, settings.mvs_y_scale, settings.render_preset);
      for (HANDLE handle : handles)
      {
         HANDLE remote = nullptr;
         if (handle)
            DuplicateHandle(GetCurrentProcess(), handle, process, &remote, 0, FALSE, DUPLICATE_SAME_ACCESS);
         message += std::format(" {}", uint64_t(uintptr_t(remote)));
         if (handle)
            CloseHandle(handle);
      }
      message += "\n";
      DWORD written = 0;
      if (!WriteFile(pipe, message.data(), DWORD(message.size()), &written, nullptr))
         return Fail("the helper's pipe broke");
      running = this;
      Log(reshade::log::level::info, std::format("started the helper for {} ({}x{} -> {}x{})", type == SR::Type::DLSS ? "DLSS" : "FSR 3", settings.render_width,
                                        settings.render_height, settings.output_width, settings.output_height));
      return true;
   }

   bool Bridge::HasInit(const SR::InstanceData* data) const
   {
      return data != nullptr;
   }

   bool Bridge::IsSupported(const SR::InstanceData* data) const
   {
      return data && data->is_supported;
   }

   // No process yet (Core initializes every type at device creation): supported if the helper (and for DLSS, NVIDIA's dll and GPU)
   // is there
   bool Bridge::Init(SR::InstanceData*& data, ID3D11Device* device, IDXGIAdapter* adapter)
   {
      if (data)
         Deinit(data);
      auto* const custom_data = new BridgeInstanceData();
      data = custom_data;
      custom_data->type = type;
      custom_data->min_resolution = 32;
      custom_data->requires_unordered_access_output_texture = false; // The helper writes its own copy
      if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&custom_data->device))))
         return false;
      ComPtr<IDXGIAdapter> device_adapter = adapter;
      ComPtr<IDXGIDevice> dxgi_device;
      if (!device_adapter && SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
         dxgi_device->GetAdapter(&device_adapter);
      DXGI_ADAPTER_DESC adapter_desc = {};
      if (!device_adapter || FAILED(device_adapter->GetDesc(&adapter_desc)))
         return false;
      custom_data->luid = adapter_desc.AdapterLuid;
      const std::filesystem::path directory = System::GetModulePath().parent_path();
      std::error_code error;
      custom_data->is_supported = std::filesystem::exists(directory / kHelperName, error) &&
                                  (type != SR::Type::DLSS || (adapter_desc.VendorId == 0x10DE && std::filesystem::exists(directory / L"nvngx_dlss.dll", error)));
      return custom_data->is_supported;
   }

   void Bridge::Deinit(SR::InstanceData*& data, ID3D11Device* optional_device)
   {
      delete static_cast<BridgeInstanceData*>(data);
      data = nullptr;
   }

   // The helper (re)starts on the next draw with these
   bool Bridge::UpdateSettings(SR::InstanceData* data, ID3D11DeviceContext* command_list, const SR::SettingsData& settings_data)
   {
      auto* const custom_data = static_cast<BridgeInstanceData*>(data);
      if (!custom_data || !custom_data->is_supported || !command_list)
         return false;
      if (settings_data == custom_data->settings_data)
         return !custom_data->failed;
      custom_data->Stop();
      custom_data->settings_data = settings_data;
      custom_data->failed = false;
      return true;
   }

   bool Bridge::Draw(const SR::InstanceData* data, ID3D11DeviceContext* command_list, const DrawData& draw_data)
   {
      auto& custom_data = *const_cast<BridgeInstanceData*>(static_cast<const BridgeInstanceData*>(data));
      if (custom_data.failed)
      {
         if (std::chrono::steady_clock::now() - custom_data.failure_time < retry_delay)
            return false;
         custom_data.failed = false;
      }
      ID3D11Resource* const resources[kCount] = {draw_data.source_color, draw_data.motion_vectors, draw_data.depth_buffer, draw_data.bias_mask, draw_data.transparency_alpha,
         draw_data.exposure, draw_data.output_color};
      if (custom_data.process)
      {
         // A dead process's fences read UINT64_MAX, so no GPU wait is left hanging
         if (WaitForSingleObject(custom_data.process, 0) == WAIT_OBJECT_0 || custom_data.fences[1]->GetCompletedValue() == UINT64_MAX)
            return custom_data.Fail("the helper exited (see sr_bridge.log next to the game's exe)");
         if (!std::equal(std::begin(resources), std::end(resources), std::begin(custom_data.sources)))
            custom_data.Stop(); // Recreated: the helper restarts on them
      }
      if (!custom_data.process && !custom_data.Start(command_list, resources))
         return false;

      if (!custom_data.ready)
      {
         if (custom_data.fences[1]->GetCompletedValue() < kReady)
         {
            // Still creating the upscaler: the color as it is (at the top left, if it's smaller)
            ComPtr<ID3D11Texture2D> source, output;
            D3D11_TEXTURE2D_DESC source_desc, output_desc;
            if (SUCCEEDED(draw_data.source_color->QueryInterface(IID_PPV_ARGS(&source))) && SUCCEEDED(draw_data.output_color->QueryInterface(IID_PPV_ARGS(&output))))
            {
               source->GetDesc(&source_desc);
               output->GetDesc(&output_desc);
               const D3D11_BOX box = {0, 0, 0, (std::min)(source_desc.Width, output_desc.Width), (std::min)(source_desc.Height, output_desc.Height), 1};
               if (source_desc.Format == output_desc.Format)
                  command_list->CopySubresourceRegion(output.Get(), 0, 0, 0, 0, source.Get(), 0, &box);
            }
            return true;
         }
         custom_data.ready = true;
         custom_data.frame = kReady;
         Log(reshade::log::level::info, custom_data.type == SR::Type::DLSS ? "running DLSS" : "running FSR 3");
      }

      for (int i = 0; i < kCount; i++)
      {
         if (i != kOutput && custom_data.copied[i])
            command_list->CopyResource(custom_data.shared[i].Get(), resources[i]);
      }
      const uint64_t frame = ++custom_data.frame;
      custom_data.context->Signal(custom_data.fences[0].Get(), frame);
      command_list->Flush();
      const std::string line = std::format("{} {} {} {} {} {} {} {} {} {} {}\n", frame, draw_data.jitter_x, draw_data.jitter_y, draw_data.reset ? 1 : 0,
         draw_data.render_width ? draw_data.render_width : custom_data.settings_data.render_width,
         draw_data.render_height ? draw_data.render_height : custom_data.settings_data.render_height, draw_data.pre_exposure, draw_data.user_sharpness,
         draw_data.near_plane, draw_data.far_plane, draw_data.vert_fov);
      DWORD written = 0;
      if (!WriteFile(custom_data.pipe, line.data(), DWORD(line.size()), &written, nullptr))
         return custom_data.Fail("the helper's pipe broke");
      custom_data.context->Wait(custom_data.fences[1].Get(), frame);
      if (custom_data.copied[kOutput])
         command_list->CopyResource(draw_data.output_color, custom_data.shared[kOutput].Get());
      return true;
   }

   // NVIDIA's formula (as Core's DLSS), which FSR's matches
   int Bridge::GetJitterPhases(const SR::InstanceData* data) const
   {
      const float scale = (std::max)(float(data->settings_data.output_height) / float(data->settings_data.render_height), 1.f);
      return std::lrintf(float(SR::GetDefaultJitterPhases()) * scale * scale);
   }
} // namespace SRBridge

#endif
