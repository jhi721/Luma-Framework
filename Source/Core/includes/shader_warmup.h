#pragma once

#include <d3d11.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include <source/com_ptr.hpp>
#include "dxbc.h"
#include "../utils/shader_compiler.hpp"
#include "../utils/system.h"

// Driver shader cache warm-up. D3D11 drivers compile a shader at its first draw, not at its creation, so a game that creates shaders
// lazily (every dgVoodoo game: it translates each D3D9 shader at its first draw) stutters through its first run, and again after a
// driver update. The pack, one per wrapper version (dgVoodoo's translation changes with it), holds the game's shaders as created, before
// Luma's patches, built offline ("Scripts/build_warmup_pack.py") from the game's shader dumps and, for a dgVoodoo game, every D3D9
// shader pushed through "_tools/dgv_hashgen --pairs", so it stays valid when the patches change. Every build draws each entry once at
// launch, as the driver will receive it and with the game mod's own variants ("Expand"), then Luma's own shaders, on extra devices on
// background threads: another device in the same process warms the driver's cache for the game's (NVIDIA and AMD, see
// "docs/Shader-Precompilation.md").
namespace ShaderWarmup
{
   struct Entry
   {
      reshade::api::pipeline_subobject_type type = reshade::api::pipeline_subobject_type::pixel_shader;
      std::vector<uint8_t> code;
   };

   // The warm-up threads create their devices through ReShade (it hooks the D3D11 entry points): Core's device and command list events
   // skip those devices, and each thread draws with the native device "OnInitDevice" hands over, which no addon event reaches
   inline thread_local bool g_is_warmup_thread = false;
   inline thread_local ID3D11Device* g_warmup_native_device = nullptr;

   constexpr uint32_t PACK_MAGIC = DXBC::FourCC("LSW1");

   // The wrapper the game's shaders come from: dgVoodoo's D3D9 module ("d3d9_chain.dll" behind the D3D9 VA Fix proxy) describes itself
   // as "dgVoodoo 2.87.5 - Direct3D9". Anything else runs its own D3D11 bytecode.
   inline std::string GetWrapperName()
   {
      for (const wchar_t* module_name : {L"d3d9_chain.dll", L"d3d9.dll"})
      {
         const HMODULE module = GetModuleHandleW(module_name);
         if (!module)
            continue;
         const std::filesystem::path path = System::GetModulePath(module);
         DWORD handle = 0;
         const DWORD info_size = GetFileVersionInfoSizeW(path.c_str(), &handle);
         std::vector<uint8_t> info(info_size);
         struct Translation
         {
            WORD language;
            WORD code_page;
         }* translation = nullptr;
         UINT length = 0;
         if (info_size == 0 || !GetFileVersionInfoW(path.c_str(), 0, info_size, info.data()) || !VerQueryValueW(info.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translation), &length) || length < sizeof(Translation))
            continue;
         const wchar_t* description = nullptr;
         if (!VerQueryValueW(info.data(), std::format(L"\\StringFileInfo\\{:04x}{:04x}\\FileDescription", translation->language, translation->code_page).c_str(), reinterpret_cast<void**>(const_cast<wchar_t**>(&description)), &length))
            continue;
         std::wstring_view text = description;
         if (!text.starts_with(L"dgVoodoo "))
            continue;
         text = text.substr(0, text.find(L" - "));
         std::string name;
         for (const wchar_t c : text)
         {
            name += (c == L' ' ? '-' : char(c));
         }
         return name;
      }
      return "D3D11";
   }

   // Pack: "LSW1", the entry count, then per entry its stage (0 vertex, 1 pixel), size and DXBC container. A damaged pack keeps its valid
   // entries.
   inline std::vector<Entry> LoadPack(const std::filesystem::path& path)
   {
      std::vector<Entry> entries;
      std::ifstream file(path, std::ios::binary);
      uint32_t header[2] = {};
      if (!file.read(reinterpret_cast<char*>(header), sizeof(header)) || header[0] != PACK_MAGIC)
         return entries;
      for (uint32_t i = 0; i < header[1]; i++)
      {
         uint32_t entry_header[2] = {}; // Stage, size
         if (!file.read(reinterpret_cast<char*>(entry_header), sizeof(entry_header)) || entry_header[0] > 1 || entry_header[1] < sizeof(Shader::DXBCHeader) || entry_header[1] > (16u << 20))
            break;
         Entry entry = {.type = (entry_header[0] == 0 ? reshade::api::pipeline_subobject_type::vertex_shader : reshade::api::pipeline_subobject_type::pixel_shader), .code = std::vector<uint8_t>(entry_header[1])};
         if (!file.read(reinterpret_cast<char*>(entry.code.data()), entry.code.size()))
            break;
         const auto* container = reinterpret_cast<const Shader::DXBCHeader*>(entry.code.data());
         if (std::memcmp(container->format_name, "DXBC", 4) != 0 || container->file_size != entry.code.size())
            break;
         entries.push_back(std::move(entry));
      }
      return entries;
   }

   // The bytecode to warm for a pack entry (the game's shader as created): what the driver receives for it after Luma's patches, and the
   // game mod's own variants of it. Called on the warm-up threads.
   using Expand = void (*)(reshade::api::pipeline_subobject_type type, const std::vector<uint8_t>& code, std::vector<std::vector<uint8_t>>* codes);
   // Luma's own compiled shaders (replacements and native ones), drawn as they are after the pack. Called on a warm-up thread, may wait
   // for their compilation.
   using LumaEntries = std::vector<Entry> (*)();

   enum class Status : uint8_t
   {
      IDLE,
      RUNNING,
      STOPPING, // "Stop" asked, the threads finish their current entry
      STOPPED,  // A later "Start" warms it all again
      DONE,
   };

   struct Worker
   {
      // For "thread", "pack_directory", "expand" and "luma_entries": the device events and the settings UI start and stop the warm-up
      // ("Detach" goes without: under the loader lock no other thread runs)
      std::shared_mutex mutex;
      std::thread thread;
      std::filesystem::path pack_directory;
      Expand expand = nullptr;
      LumaEntries luma_entries = nullptr;
      std::atomic<Status> status = Status::IDLE;
      // Progress for the settings UI, each value read on its own
      std::atomic<size_t> entry_count = 0; // The pack's (0 without one), then Luma's shaders too
      std::atomic<size_t> processed = 0;
      std::atomic<size_t> drawn = 0; // Shaders: the entries and their variants ("Expand")
      std::atomic<size_t> failed = 0;
      std::atomic<uint32_t> run_ms = 0; // The last run's
   };
   inline Worker g_worker;

   // A vertex shader writing every input of a pixel shader (same order and semantics) that the rasterizer doesn't generate itself, and a
   // position after them if the pixel shader doesn't read it (after: the inputs keep their registers)
   inline com_ptr<ID3DBlob> CompilePassThroughVertexShader(std::vector<DXBC::SignatureElement> pixel_inputs)
   {
      std::ranges::sort(pixel_inputs, {}, &DXBC::SignatureElement::reg);
      std::string members;
      bool has_position = false;
      for (size_t i = 0; i < pixel_inputs.size(); i++)
      {
         const DXBC::SignatureElement& input = pixel_inputs[i];
         if (input.system_value != D3D_NAME_UNDEFINED && input.system_value != D3D_NAME_POSITION)
            continue;
         has_position |= (input.system_value == D3D_NAME_POSITION);
         const char* type = (input.component_type == D3D_REGISTER_COMPONENT_UINT32 ? "uint" : (input.component_type == D3D_REGISTER_COMPONENT_SINT32 ? "int" : "float"));
         const std::string semantic = (input.system_value == D3D_NAME_POSITION ? std::string("SV_Position") : std::format("{}{}", input.name, input.semantic_index));
         members += std::format("{}{} v{} : {}; ", type, std::popcount(uint32_t(input.mask)), i, semantic);
      }
      if (!has_position)
      {
         members += "float4 position : SV_Position; ";
      }
      const std::string source = std::format("struct Output {{ {}}}; Output main() {{ return (Output)0; }}", members);
      com_ptr<ID3DBlob> code;
      if (Shader::d3d_compile)
      {
         Shader::d3d_compile(source.data(), source.size(), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &code, nullptr);
      }
      return code;
   }

   // Draws the pack's entries ("expand"ed), then Luma's own shaders, on two threads with a device each (one more halved the time on NVIDIA
   // and AMD; with four, AMD left part of the cache cold), until done or stopped
   inline void Run(com_ptr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL feature_level, std::filesystem::path pack_directory, Expand expand, LumaEntries luma_entries)
   {
      const auto start = std::chrono::steady_clock::now();
      // The wrapper's pack, else one of the same dgVoodoo branch (2.87.5 translates like 2.87.3)
      const std::string wrapper = GetWrapperName();
      std::filesystem::path pack_path = pack_directory / (wrapper + ".bin");
      std::error_code error;
      if (!std::filesystem::exists(pack_path, error) && wrapper.starts_with("dgVoodoo-"))
      {
         const std::string branch = wrapper.substr(0, wrapper.rfind('.') + 1);
         for (const auto& file : std::filesystem::directory_iterator(pack_directory, error))
         {
            if (file.path().filename().string().starts_with(branch) && file.path().extension() == ".bin")
            {
               pack_path = file.path();
               break;
            }
         }
      }
      const std::vector<Entry> entries = LoadPack(pack_path);
      g_worker.entry_count = entries.size();
      g_worker.processed = 0;
      g_worker.drawn = 0;
      g_worker.failed = 0;
      // Only for a game with a pack (other mods never warm, Luma's shaders alone don't stutter much)
      if (entries.empty())
      {
         g_worker.status = Status::DONE;
         return;
      }

      const auto stopping = []
      {
         return g_worker.status == Status::STOPPING;
      };
      std::atomic<size_t> next_entry = 0;
      std::vector<Entry> luma_shaders;
      std::once_flag luma_shaders_fetched;
      std::atomic<size_t> next_luma_shader = 0;
      std::shared_mutex pass_through_mutex;
      std::unordered_map<std::string, com_ptr<ID3DBlob>> pass_through_codes; // By pixel shader input signature
      auto work = [&]
      {
         g_is_warmup_thread = true;
         using CreateDevice = decltype(&D3D11CreateDevice);
         const auto create_device = reinterpret_cast<CreateDevice>(GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "D3D11CreateDevice"));
         // Declared before its children, so they go first: ReShade checks the native device is gone with its proxy
         com_ptr<ID3D11Device> reshade_device;
         if (!create_device || FAILED(create_device(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &feature_level, 1, D3D11_SDK_VERSION, &reshade_device, nullptr, nullptr)))
         {
            reshade::log::message(reshade::log::level::warning, "[Shader warm-up] Failed to create a device");
            return;
         }
         // Without a ReShade proxy (it skips some adapters) the device is native already
         ID3D11Device* const device = (g_warmup_native_device ? g_warmup_native_device : reshade_device.get());
         com_ptr<ID3D11DeviceContext> context;
         device->GetImmediateContext(&context);

         const D3D11_TEXTURE2D_DESC target_desc = {.Width = 1, .Height = 1, .MipLevels = 1, .ArraySize = 1, .Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .SampleDesc = {1, 0}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_RENDER_TARGET};
         com_ptr<ID3D11Texture2D> target;
         com_ptr<ID3D11RenderTargetView> target_view;
         // Zero instances or thread groups: the driver compiles, nothing runs
         const UINT indirect_arguments[4] = {};
         const D3D11_BUFFER_DESC arguments_desc = {.ByteWidth = sizeof(indirect_arguments), .Usage = D3D11_USAGE_DEFAULT, .MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS};
         const D3D11_SUBRESOURCE_DATA arguments_data = {.pSysMem = indirect_arguments};
         com_ptr<ID3D11Buffer> arguments;
         const std::vector<uint8_t> zeros(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT * 16);
         const D3D11_BUFFER_DESC vertices_desc = {.ByteWidth = UINT(zeros.size()), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_VERTEX_BUFFER};
         const D3D11_SUBRESOURCE_DATA vertices_data = {.pSysMem = zeros.data()};
         com_ptr<ID3D11Buffer> vertices;
         const D3D11_QUERY_DESC query_desc = {.Query = D3D11_QUERY_EVENT};
         com_ptr<ID3D11Query> query;
         if (FAILED(device->CreateTexture2D(&target_desc, nullptr, &target)) || FAILED(device->CreateRenderTargetView(target.get(), nullptr, &target_view)) || FAILED(device->CreateBuffer(&arguments_desc, &arguments_data, &arguments)) || FAILED(device->CreateBuffer(&vertices_desc, &vertices_data, &vertices)) || FAILED(device->CreateQuery(&query_desc, &query)))
            return;
         const D3D11_VIEWPORT viewport = {.Width = 1.f, .Height = 1.f, .MaxDepth = 1.f};

         // Most pixel shaders share their input signature with others (the bytecode is shared by the threads)
         std::unordered_map<std::string, com_ptr<ID3D11VertexShader>> pass_through_shaders;
         auto wait_for_draws = [&]
         {
            context->End(query.get());
            context->Flush();
            while (context->GetData(query.get(), nullptr, 0, 0) == S_FALSE && !stopping())
            {
               std::this_thread::yield();
            }
         };
         auto draw = [&](reshade::api::pipeline_subobject_type type, const std::vector<uint8_t>& code)
         {
            context->ClearState();
            if (type == reshade::api::pipeline_subobject_type::compute_shader)
            {
               com_ptr<ID3D11ComputeShader> compute_shader;
               device->CreateComputeShader(code.data(), code.size(), nullptr, &compute_shader);
               if (!compute_shader)
                  return false;
               context->CSSetShader(compute_shader.get(), nullptr, 0);
               context->DispatchIndirect(arguments.get(), 0);
            }
            else
            {
               std::vector<DXBC::Chunk> chunks;
               std::vector<DXBC::SignatureElement> inputs;
               const DXBC::Chunk* input_signature = (DXBC::ReadChunks(code.data(), code.size(), &chunks) ? DXBC::FindChunk(&chunks, DXBC::FourCC("ISGN")) : nullptr);
               if (!input_signature || !DXBC::ReadSignature(input_signature->data, &inputs))
                  return false;
               context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
               context->RSSetViewports(1, &viewport);
               com_ptr<ID3D11VertexShader> vertex_shader;
               com_ptr<ID3D11PixelShader> pixel_shader;
               com_ptr<ID3D11InputLayout> input_layout;
               if (type == reshade::api::pipeline_subobject_type::pixel_shader)
               {
                  const std::string signature(input_signature->data.begin(), input_signature->data.end());
                  auto& pass_through_shader = pass_through_shaders[signature];
                  if (!pass_through_shader)
                  {
                     com_ptr<ID3DBlob> pass_through_code;
                     {
                        const std::shared_lock lock(pass_through_mutex);
                        if (const auto it = pass_through_codes.find(signature); it != pass_through_codes.end())
                        {
                           pass_through_code = it->second;
                        }
                     }
                     if (!pass_through_code)
                     {
                        pass_through_code = CompilePassThroughVertexShader(inputs);
                        const std::unique_lock lock(pass_through_mutex);
                        pass_through_codes.emplace(signature, pass_through_code); // Two threads may compile one: harmless
                     }
                     if (pass_through_code)
                     {
                        device->CreateVertexShader(pass_through_code->GetBufferPointer(), pass_through_code->GetBufferSize(), nullptr, &pass_through_shader);
                     }
                  }
                  vertex_shader = pass_through_shader;
                  device->CreatePixelShader(code.data(), code.size(), nullptr, &pixel_shader);
                  if (!vertex_shader || !pixel_shader)
                     return false;
                  ID3D11RenderTargetView* const target_views[] = {target_view.get()};
                  context->OMSetRenderTargets(1, target_views, nullptr);
               }
               else
               {
                  // One layout of the inputs: AMD compiles a vertex shader again for each layout it's drawn with, so there only the
                  // game's own layout is warm
                  std::erase_if(inputs, [](const DXBC::SignatureElement& input)
                     { return input.system_value != D3D_NAME_UNDEFINED; });
                  std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
                  for (const DXBC::SignatureElement& input : inputs)
                  {
                     const DXGI_FORMAT format = (input.component_type == D3D_REGISTER_COMPONENT_UINT32 ? DXGI_FORMAT_R32G32B32A32_UINT : (input.component_type == D3D_REGISTER_COMPONENT_SINT32 ? DXGI_FORMAT_R32G32B32A32_SINT : DXGI_FORMAT_R32G32B32A32_FLOAT));
                     elements.push_back({.SemanticName = input.name.c_str(), .SemanticIndex = input.semantic_index, .Format = format, .AlignedByteOffset = UINT(elements.size() * 16), .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA});
                  }
                  device->CreateVertexShader(code.data(), code.size(), nullptr, &vertex_shader);
                  if (!vertex_shader || elements.size() > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT || (!elements.empty() && FAILED(device->CreateInputLayout(elements.data(), UINT(elements.size()), code.data(), code.size(), &input_layout))))
                     return false;
                  const UINT stride = UINT(zeros.size());
                  const UINT offset = 0;
                  ID3D11Buffer* const vertex_buffers[] = {vertices.get()};
                  context->IASetVertexBuffers(0, 1, vertex_buffers, &stride, &offset);
                  context->IASetInputLayout(input_layout.get());
               }
               context->VSSetShader(vertex_shader.get(), nullptr, 0);
               context->PSSetShader(pixel_shader.get(), nullptr, 0);
               context->DrawInstancedIndirect(arguments.get(), 0);
            }
            // Waited on in batches (of both threads' draws, roughly): the driver's compile threads stay busy without a deep queue
            if (++g_worker.drawn % 16 == 0)
            {
               wait_for_draws();
            }
            return true;
         };
         // Draws the codes of one entry; false once the device is gone (a shader the driver's compiler crashed on: every later creation
         // would fail)
         auto warm = [&](reshade::api::pipeline_subobject_type type, const std::vector<std::vector<uint8_t>>& codes, size_t entry)
         {
            for (const std::vector<uint8_t>& code : codes)
            {
               if (!draw(type, code))
               {
                  g_worker.failed++;
               }
            }
            g_worker.processed++;
            if (const HRESULT removed = device->GetDeviceRemovedReason(); FAILED(removed))
            {
               reshade::log::message(reshade::log::level::error, std::format("[Shader warm-up] The device was removed (0x{:08X}) by entry {}", uint32_t(removed), entry).c_str());
               return false;
            }
            return true;
         };

         std::vector<std::vector<uint8_t>> codes;
         bool device_alive = true;
         for (size_t i = next_entry++; i < entries.size() && !stopping() && device_alive; i = next_entry++)
         {
            codes.clear();
            expand(entries[i].type, entries[i].code, &codes);
            device_alive = warm(entries[i].type, codes, i);
         }
         std::call_once(luma_shaders_fetched, [&]
            {
               if (luma_entries && !stopping())
               {
                  luma_shaders = luma_entries();
                  g_worker.entry_count += luma_shaders.size();
               } });
         for (size_t i = next_luma_shader++; i < luma_shaders.size() && !stopping() && device_alive; i = next_luma_shader++)
         {
            device_alive = warm(luma_shaders[i].type, {luma_shaders[i].code}, entries.size() + i);
         }
         wait_for_draws();
         context->ClearState();
         context->Flush();
      };

      // A second device costs address space: an x86 process keeps one when less than 1 GB is left
      MEMORYSTATUSEX memory = {.dwLength = sizeof(MEMORYSTATUSEX)};
      const bool two_threads = (sizeof(void*) == 8 || (GlobalMemoryStatusEx(&memory) && memory.ullAvailVirtual >= (1ull << 30)));
      std::thread helper;
      if (two_threads)
      {
         helper = std::thread(work);
      }
      work();
      if (helper.joinable())
      {
         helper.join();
      }
      Status running = Status::RUNNING;
      if (!g_worker.status.compare_exchange_strong(running, Status::DONE))
      {
         g_worker.status = Status::STOPPED;
      }
      g_worker.run_ms = uint32_t(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
      reshade::log::message(reshade::log::level::info, std::format("[Shader warm-up] {} shaders drawn ({} failed){} in {} ms on {} threads, from \"{}\" and {} Luma shaders", size_t(g_worker.drawn), size_t(g_worker.failed), (g_worker.status == Status::DONE ? "" : ", stopped early"), uint32_t(g_worker.run_ms), (two_threads ? 2 : 1), pack_path.string(), luma_shaders.size()).c_str());
   }

   // "Start" and "Restart" with "g_worker.mutex" held: a new run (a finished thread is joined first)
   inline void Launch(ID3D11Device* game_device)
   {
      if (g_worker.thread.joinable())
      {
         g_worker.thread.join();
      }
      com_ptr<IDXGIDevice> dxgi_device;
      com_ptr<IDXGIAdapter> adapter;
      if (FAILED(game_device->QueryInterface(&dxgi_device)) || FAILED(dxgi_device->GetAdapter(&adapter)))
         return;
      g_worker.status = Status::RUNNING;
      g_worker.thread = std::thread(Run, std::move(adapter), game_device->GetFeatureLevel(), g_worker.pack_directory, g_worker.expand, g_worker.luma_entries);
   }

   // From the game device's "OnInitDevice": warms the pack once per process (the driver's cache is); a run a device destruction stopped
   // ("Stop") starts over
   inline void Start(ID3D11Device* game_device, const std::filesystem::path& pack_directory, Expand expand, LumaEntries luma_entries)
   {
      const std::unique_lock lock(g_worker.mutex);
      g_worker.pack_directory = pack_directory;
      g_worker.expand = expand;
      g_worker.luma_entries = luma_entries;
      if (const Status status = g_worker.status; status == Status::IDLE || status == Status::STOPPED)
      {
         Launch(game_device);
      }
   }

   // From the settings UI: warms it all again
   inline void Restart(ID3D11Device* game_device)
   {
      const std::unique_lock lock(g_worker.mutex);
      if (const Status status = g_worker.status; status != Status::RUNNING && status != Status::STOPPING && g_worker.expand)
      {
         Launch(game_device);
      }
   }

   // From the game device's "OnDestroyDevice": the next device's "Start" warms it again
   inline void Stop()
   {
      const std::unique_lock lock(g_worker.mutex);
      if (!g_worker.thread.joinable())
         return;
      Status running = Status::RUNNING;
      g_worker.status.compare_exchange_strong(running, Status::STOPPING);
      g_worker.thread.join();
   }

   // From "DLL_PROCESS_DETACH" (no join under the loader lock): games often exit without destroying their device, and the process ends the
   // thread
   inline void Detach()
   {
      if (!g_worker.thread.joinable())
         return;
      Status running = Status::RUNNING;
      g_worker.status.compare_exchange_strong(running, Status::STOPPING);
      g_worker.thread.detach();
   }

   // The settings' "Shader Compilation" section, for games with a pack
   inline void DrawImGuiSection(ID3D11Device* game_device)
   {
      const Status status = g_worker.status;
      const bool running = (status == Status::RUNNING || status == Status::STOPPING);
      const size_t entry_count = g_worker.entry_count;
      if (!running && entry_count == 0)
         return;

      ImGui::SeparatorText("Shader Compilation");
      const size_t processed = (std::min)(size_t(g_worker.processed), entry_count);
      const size_t failed = g_worker.failed;
      if (running)
      {
         ImGui::ProgressBar(entry_count != 0 ? float(processed) / float(entry_count) : 0.f, ImVec2(-FLT_MIN, 0.f), std::format("Compiling {} / {}", processed, entry_count).c_str());
      }
      else if (status == Status::DONE)
      {
         ImGui::Text("%zu shaders compiled from %zu entries in %u ms%s", size_t(g_worker.drawn), entry_count, uint32_t(g_worker.run_ms), failed != 0 ? std::format(" ({} failed)", failed).c_str() : "");
      }
      else
      {
         ImGui::Text("Stopped at %zu / %zu", processed, entry_count);
      }

      ImGui::BeginDisabled(running);
      if (ImGui::Button("Compile Shaders"))
      {
         Restart(game_device);
      }
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
         ImGui::SetTooltip("Luma compiles the game's shaders in the background at launch, so they don't stutter the first time they are drawn\n(the first launch, or after a driver update clears the driver's shader cache).\nPress to compile them again.");
      }
   }
} // namespace ShaderWarmup
