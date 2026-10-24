#define GAME_GENERIC 1

#define DISABLE_AUTO_DEBUGGER 1

#define DISABLE_FOCUS_LOSS_SUPPRESSION 1

#define CHECK_GRAPHICS_API_COMPATIBILITY 1

#define AVOID_INPUT_LOSS 1

#if DEVELOPMENT
// For the D3D12 smoke test of Luma's passes on the game's command lists ("test_rcas_hash", "test_smaa_hash", "test_bloom_hash") and of its sampler upgrade ("test_mip_lod_bias"),
// and the D3D11 side of their parity check ("TestPass")
#define ENABLE_RCAS 1
#define ENABLE_SMAA 1
#define ENABLE_BLOOM 1
#endif

#include "..\..\Core\core.hpp"

namespace
{
   std::set<reshade::api::format> toggleable_texture_upgrade_formats;
#if DX12 && DEVELOPMENT
   // Driven by the D3D12 smoke tests through the dev MCP, to exercise the game callback paths of Core: pixel shader hashes (written as signed ints), 0 is off
   int test_skip_hash = 0;        // The draw is skipped
   int test_custom_data_hash = 0; // The draw gets Luma's cbuffers set by the callback, with CustomData1 = 7
   int test_replaced_hash = 0;    // The callback draws it itself and reports it replaced
   int test_rcas_hash = 0;        // The draw becomes RCAS (sharpness 1) of its t0 (the table of root parameter 0) into its render target
   int test_smaa_hash = 0;        // The draw becomes SMAA of its t0 into its render target (with a "Luma_SMAA_impl.hlsl" in the game's shader folder)
   int test_smaa_rcas = 0;        // 1: the SMAA of "test_smaa_hash" is followed by RCAS (sharpness 1)
   int test_bloom_hash = 0;       // The draw becomes the Karis average and the 3 level bloom of its t0, whose mip 0 is copied into the top left of its render target
   float test_mip_lod_bias = 0.f; // The mip bias of the upgraded samplers, applied at present: it reaches the samplers the game writes after it
   int test_viewport_scale_hash = 0; // The draw's viewports and scissors get half their width through "SetGameViewportsD3D12()", and keep it for the draws after it (as a render scale's code does)
   int test_skip_copy_width = 0;     // The game copies ("OverrideCopyResource()", "OverrideCopyTextureRegion()") into a texture of this width are skipped. "TestSkipCopyWidth" of "[Luma Advanced]" sets it on boot
   int test_smaa_predication = 0;    // The SMAA of "test_smaa_hash" gets the edge threshold 0.3 as game constant x (the smoke's "Luma_SMAA_impl.hlsl" has predication on: 0 keeps its 0.05): 1 without a predication texture, 2 with its t0 as the predication texture
#endif
#if DX11 && DEVELOPMENT
   // The D3D11 side of the parity check of the D3D12 passes (the same input drawn by both, compared per pixel), from "[Luma Advanced]": the draw of the pixel shader "TestPassHash" (hex) becomes
   // "TestPass" of its t0 into its render target 0, with the arguments of the D3D12 knobs above
   enum class TestPass : uint8_t
   {
      None,
      RCAS,
      SMAA,
      SMAAThenRCAS,
      Bloom
   };
   TestPass test_pass = TestPass::None;
   uint32_t test_pass_hash = 0;
#endif
} // namespace

struct GenericGameDeviceData final : public GameDeviceData
{
#if DX11 && DEVELOPMENT
   com_ptr<ID3D11Buffer> test_smaa_constants; // b1 of the parity check's "Luma_SMAA_impl.hlsl": the metrics of the target, then 4 game floats (0)
   com_ptr<ID3D11Texture2D> test_smaa_output; // SMAA's target when RCAS follows it, at the target's size and format
   com_ptr<ID3D11RenderTargetView> test_smaa_output_rtv;
   com_ptr<ID3D11ShaderResourceView> test_smaa_output_srv;
#endif
};

// Generic mod that should work in many games out of the box, configurable at runtime.
// Texture upgrades and linearized scRGB HDR should work. An SDR to HDR upgrade can be done at the end.
class GenericGame final : public Game
{
   static GenericGameDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<GenericGameDeviceData*>(device_data.game);
   }

public:
   void OnInit(bool async) override
   {
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
#if DX11 && DEVELOPMENT
      char test_pass_name[32] = {};
      size_t test_pass_name_size = sizeof(test_pass_name);
      if (reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "TestPass", test_pass_name, &test_pass_name_size))
      {
         const std::string_view name = test_pass_name;
         test_pass = (name == "rcas" ? TestPass::RCAS : (name == "smaa" ? TestPass::SMAA : (name == "smaa_rcas" ? TestPass::SMAAThenRCAS : (name == "bloom" ? TestPass::Bloom : TestPass::None))));
      }
      char test_pass_hash_text[16] = {};
      size_t test_pass_hash_size = sizeof(test_pass_hash_text);
      if (reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "TestPassHash", test_pass_hash_text, &test_pass_hash_size))
      {
         test_pass_hash = uint32_t(std::strtoul(test_pass_hash_text, nullptr, 16));
      }
#endif
#if DX12 && DEVELOPMENT
      // "TestUIHash" (hex, section "<NAME> Advanced"): a pixel shader whose draws count as UI, so "core.hide_ui" skips them (the D3D12 smoke test of Hide UI)
      char test_ui_hash_text[16] = {};
      size_t test_ui_hash_size = sizeof(test_ui_hash_text);
      if (reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "TestUIHash", test_ui_hash_text, &test_ui_hash_size))
      {
         shader_hashes_UI.pixel_shaders.emplace(uint32_t(std::strtoul(test_ui_hash_text, nullptr, 16)));
      }
      reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "TestSkipCopyWidth", test_skip_copy_width);
      Mcp::RegisterInts({{"test_skip_copy_width", &test_skip_copy_width, 0, INT_MAX}, {"test_skip_hash", &test_skip_hash, INT_MIN, INT_MAX}, {"test_custom_data_hash", &test_custom_data_hash, INT_MIN, INT_MAX}, {"test_replaced_hash", &test_replaced_hash, INT_MIN, INT_MAX}, {"test_rcas_hash", &test_rcas_hash, INT_MIN, INT_MAX}, {"test_smaa_hash", &test_smaa_hash, INT_MIN, INT_MAX}, {"test_smaa_rcas", &test_smaa_rcas, 0, 1}, {"test_smaa_predication", &test_smaa_predication, 0, 2}, {"test_bloom_hash", &test_bloom_hash, INT_MIN, INT_MAX}, {"test_viewport_scale_hash", &test_viewport_scale_hash, INT_MIN, INT_MAX}});
      Mcp::RegisterValues({{"test_mip_lod_bias", &test_mip_lod_bias, -8.f, 8.f}});
#endif
   }

#if DX12
   // "TextureUpgradeWriterShaders" (section "<NAME> Advanced" of ReShade.ini): pixel shader hashes in hex, with an optional ":slot" for the render target they write (0 by default),
   // separated by commas ("0x167D96E5,0x7561D004:1"). On D3D12 the textures a listed shader renders into are the only placed ones upgraded (see "includes/d3d12_clones.h").
   // "IndirectTextureUpgrades" (0 or 1) gives the matching committed textures a clone instead of their in place upgrade, "ChainTextureUpgrades" (0 to 2, see
   // "ChainTextureFormatUpgradesType") promotes the clone of a texture an upgraded one is copied into. "TextureCustomDimensions" gives the textures of a size a clone of another
   // ("texture_custom_dimensions_upgrades"), as "WxH>WxH" pairs separated by commas ("640x360>1280x720").
   // "LoadConfigs()" runs on boot, before any device exists, so the list is known when the game creates its textures.
   void LoadConfigs() override
   {
      // ReShade returns a value with commas as a list, one null terminated entry per element
      const auto read_list = [](const char* key)
      {
         std::string list(1024, '\0');
         size_t list_size = list.size();
         std::vector<std::string> tokens;
         if (!reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), key, list.data(), &list_size))
         {
            return tokens;
         }
         list.resize(list_size);
         std::ranges::replace(list, '\0', ',');
         for (size_t start = 0; start < list.size();)
         {
            const size_t end = (std::min)(list.find(',', start), list.size());
            tokens.push_back(list.substr(start, end - start));
            start = end + 1;
         }
         return tokens;
      };

      for (const std::string& token : read_list("TextureCustomDimensions"))
      {
         uint32_t width = 0, height = 0, upgraded_width = 0, upgraded_height = 0;
         if (sscanf_s(token.c_str(), "%ux%u>%ux%u", &width, &height, &upgraded_width, &upgraded_height) == 4)
         {
            texture_custom_dimensions_upgrades.emplace_back(uint4(width, height, 0, 0), reshade::api::format::unknown, uint4(upgraded_width, upgraded_height, 0, 0));
            reshade::log::message(reshade::log::level::info, std::format("[Texture upgrades] Custom dimensions (TextureCustomDimensions): {}x{} to {}x{}", width, height, upgraded_width, upgraded_height).c_str());
         }
      }

      reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "IndirectTextureUpgrades", enable_indirect_texture_format_upgrades);
      int chain_upgrades = int(enable_chain_indirect_texture_format_upgrades);
      if (reshade::get_config_value(nullptr, NAME_ADVANCED_SETTINGS.c_str(), "ChainTextureUpgrades", chain_upgrades))
      {
         enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType(std::clamp(chain_upgrades, 0, int(ChainTextureFormatUpgradesType::DirectAndIndirectDependencies)));
      }
      if (enable_indirect_texture_format_upgrades || enable_chain_indirect_texture_format_upgrades != ChainTextureFormatUpgradesType::None)
      {
         reshade::log::message(reshade::log::level::info, std::format("[Texture upgrades] Indirect upgrades: {}, chain upgrades: {}", enable_indirect_texture_format_upgrades, int(enable_chain_indirect_texture_format_upgrades)).c_str());
      }

      std::string parsed;
      for (const std::string& token : read_list("TextureUpgradeWriterShaders"))
      {
         char* hash_end = nullptr;
         const uint32_t hash = uint32_t(std::strtoul(token.c_str(), &hash_end, 16));
         if (hash_end == token.c_str())
         {
            continue;
         }
         const size_t colon = token.find(':');
         const unsigned long slot = (colon != std::string::npos ? std::strtoul(token.c_str() + colon + 1, nullptr, 10) : 0);
         if (slot >= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)
         {
            continue;
         }
         std::vector<uint8_t>& rtv_slots = auto_texture_format_upgrade_shader_hashes[hash].rtv_slots;
         if (std::ranges::find(rtv_slots, uint8_t(slot)) == rtv_slots.end())
         {
            rtv_slots.push_back(uint8_t(slot));
            parsed += std::format("{}0x{:08X}:{}", (parsed.empty() ? "" : ", "), hash, slot);
         }
      }
      if (!parsed.empty())
      {
         reshade::log::message(reshade::log::level::info, std::format("[Texture upgrades] Writer shaders of the textures to upgrade (TextureUpgradeWriterShaders): {}", parsed).c_str());
      }
   }
#endif

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new GenericGameDeviceData;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(NativeDevice* native_device, NativeCommandList* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);

#if DX12 && DEVELOPMENT
      const auto is_test_hash = [&](int test_hash)
      { return test_hash != 0 && original_shader_hashes.Contains(static_cast<uint32_t>(test_hash), reshade::api::shader_stage::pixel); };
      if (is_test_hash(test_skip_hash))
      {
         return DrawOrDispatchOverrideType::Skip;
      }
      if (is_test_hash(test_viewport_scale_hash))
      {
         std::vector<reshade::api::viewport> viewports = cmd_list_data.d3d12_state.viewports;
         for (reshade::api::viewport& viewport : viewports)
         {
            viewport.width *= 0.5f;
         }
         std::vector<reshade::api::rect> scissors = cmd_list_data.d3d12_state.scissor_rects;
         for (reshade::api::rect& scissor : scissors)
         {
            scissor.right = scissor.left + (scissor.right - scissor.left) / 2;
         }
         SetGameViewportsD3D12(cmd_list_data, uint32_t(viewports.size()), viewports.data());
         SetGameScissorRectsD3D12(cmd_list_data, uint32_t(scissors.size()), scissors.data());
      }
      if (is_custom_pass && is_test_hash(test_custom_data_hash))
      {
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, 7);
         updated_cbuffers = true;
      }
      if (original_draw_dispatch_func && is_test_hash(test_replaced_hash))
      {
         (*original_draw_dispatch_func)();
         return DrawOrDispatchOverrideType::Replaced;
      }
      if (const bool rcas = is_test_hash(test_rcas_hash), bloom = is_test_hash(test_bloom_hash); rcas || bloom || is_test_hash(test_smaa_hash))
      {
         // The source is resolved through the development builds' shadow of the descriptor heaps
         const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
         D3D12State::ViewInfo source;
         if (!state.graphics.bindings.empty() && state.graphics.bindings[0].table.handle != 0)
         {
            const std::shared_lock lock(device_data.descriptor_heaps.mutex);
            if (const D3D12State::Descriptor* descriptor = D3D12State::FindDescriptor(device_data.descriptor_heaps, cmd_list_data.cmd_list->get_device(), state.graphics.bindings[0].table, 0))
            {
               source = descriptor->view;
            }
         }
         if (source.resource == 0 || state.render_target_views.empty())
         {
            return DrawOrDispatchOverrideType::None;
         }
         const reshade::api::resource target = {state.render_target_views[0].resource};
         bool drawn = false;
         if (bloom)
         {
            // The smoke's source is in a pixel shader resource state there, its target in a render target state
            static constexpr float sigmas[] = {2.f, 2.f, 2.f};
            reshade::api::resource karis_average = {};
            reshade::api::resource bloom_texture = {};
            drawn = DrawKarisAverage(cmd_list_data, device_data, {source.resource}, source.format, reshade::api::resource_usage::shader_resource_pixel, &karis_average) &&
                    DrawBloom(cmd_list_data, device_data, karis_average, reshade::api::format::r16g16b16a16_float, uint32_t(std::size(sigmas)), sigmas, &bloom_texture);
            if (drawn)
            {
               reshade::api::command_list* const cmd_list = cmd_list_data.cmd_list;
               cmd_list->barrier(bloom_texture, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_source);
               cmd_list->barrier(target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_dest);
               cmd_list->copy_texture_region(bloom_texture, 0, nullptr, target, 0, nullptr);
               cmd_list->barrier(target, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::render_target);
               cmd_list->barrier(bloom_texture, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::shader_resource);
            }
         }
         else if (rcas)
         {
            drawn = DrawRCAS(cmd_list_data, device_data, {source.resource}, source.format, target, state.render_target_descs[0], 1.f);
         }
         else
         {
            const float smaa_constants[4] = {(test_smaa_predication != 0 ? 0.3f : 0.f), 0.f, 0.f, 0.f};
            const reshade::api::resource predication = {(test_smaa_predication == 2 ? source.resource : 0)};
            drawn = DrawSMAA(cmd_list_data, device_data, {source.resource}, source.format, predication, source.format, target, state.render_target_descs[0], smaa_constants, float(test_smaa_rcas));
         }
         if (drawn)
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
#endif
#if DX11 && DEVELOPMENT
      if (test_pass != TestPass::None && original_shader_hashes.Contains(test_pass_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11ShaderResourceView> source;
         native_device_context->PSGetShaderResources(0, 1, &source);
         com_ptr<ID3D11RenderTargetView> target;
         native_device_context->OMGetRenderTargets(1, &target, nullptr);
         if (!source || !target)
         {
            return DrawOrDispatchOverrideType::None;
         }
         com_ptr<ID3D11Resource> target_resource;
         target->GetResource(&target_resource);
         com_ptr<ID3D11Texture2D> target_texture;
         target_resource->QueryInterface(&target_texture);
         D3D11_TEXTURE2D_DESC target_desc;
         target_texture->GetDesc(&target_desc);
         if (test_pass == TestPass::Bloom)
         {
            static constexpr float sigmas[] = {2.f, 2.f, 2.f};
            com_ptr<ID3D11ShaderResourceView> karis_average;
            DrawKarisAverage(native_device, native_device_context, device_data, source.get(), &karis_average);
            com_ptr<ID3D11ShaderResourceView> bloom;
            DrawBloom(native_device, native_device_context, device_data, karis_average.get(), int(std::size(sigmas)), sigmas, &bloom);
            com_ptr<ID3D11Resource> bloom_resource;
            bloom->GetResource(&bloom_resource);
            native_device_context->CopySubresourceRegion(target_resource.get(), 0, 0, 0, 0, bloom_resource.get(), 0, nullptr);
            return DrawOrDispatchOverrideType::Replaced;
         }
         if (test_pass == TestPass::RCAS)
         {
            if (!PrepareRCAS(native_device, device_data))
            {
               return DrawOrDispatchOverrideType::None;
            }
            DrawRCAS(native_device_context, device_data, source.get(), target.get(), 1.f);
            return DrawOrDispatchOverrideType::Replaced;
         }
         if (!game_device_data.test_smaa_constants)
         {
            CD3D11_BUFFER_DESC constants_desc(sizeof(float) * 8, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
            native_device->CreateBuffer(&constants_desc, nullptr, &game_device_data.test_smaa_constants);
         }
         if (D3D11_MAPPED_SUBRESOURCE mapped; SUCCEEDED(native_device_context->Map(game_device_data.test_smaa_constants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
         {
            const float constants[8] = {1.f / float(target_desc.Width), 1.f / float(target_desc.Height), float(target_desc.Width), float(target_desc.Height), 0.f, 0.f, 0.f, 0.f};
            std::memcpy(mapped.pData, constants, sizeof(constants));
            native_device_context->Unmap(game_device_data.test_smaa_constants.get(), 0);
         }
         ID3D11RenderTargetView* smaa_target = target.get();
         if (test_pass == TestPass::SMAAThenRCAS)
         {
            if (!PrepareRCAS(native_device, device_data))
            {
               return DrawOrDispatchOverrideType::None;
            }
            D3D11_TEXTURE2D_DESC output_desc = {};
            if (game_device_data.test_smaa_output)
            {
               game_device_data.test_smaa_output->GetDesc(&output_desc);
            }
            if (!game_device_data.test_smaa_output || output_desc.Width != target_desc.Width || output_desc.Height != target_desc.Height || output_desc.Format != target_desc.Format)
            {
               output_desc = {.Width = target_desc.Width, .Height = target_desc.Height, .MipLevels = 1, .ArraySize = 1, .Format = target_desc.Format, .SampleDesc = {1, 0}, .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE};
               game_device_data.test_smaa_output = nullptr;
               game_device_data.test_smaa_output_rtv = nullptr;
               game_device_data.test_smaa_output_srv = nullptr;
               native_device->CreateTexture2D(&output_desc, nullptr, &game_device_data.test_smaa_output);
               native_device->CreateRenderTargetView(game_device_data.test_smaa_output.get(), nullptr, &game_device_data.test_smaa_output_rtv);
               native_device->CreateShaderResourceView(game_device_data.test_smaa_output.get(), nullptr, &game_device_data.test_smaa_output_srv);
            }
            smaa_target = game_device_data.test_smaa_output_rtv.get();
         }
         DrawSMAA(native_device, native_device_context, device_data, smaa_target, source.get(), source.get(), nullptr, game_device_data.test_smaa_constants.get());
         if (test_pass == TestPass::SMAAThenRCAS)
         {
            DrawRCAS(native_device_context, device_data, game_device_data.test_smaa_output_srv.get(), target.get(), 1.f);
         }
         return DrawOrDispatchOverrideType::Replaced;
      }
#endif

      return DrawOrDispatchOverrideType::None; // Don't cancel the original draw call
   }
#if DX12 && DEVELOPMENT
   // The D3D12 smoke test of the game copy overrides: a copy into a texture of "test_skip_copy_width" is skipped, the destination keeps what it had
   bool OverrideCopyResource(NativeDevice* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      return test_skip_copy_width != 0 && reinterpret_cast<ID3D12Resource*>(dst_resource)->GetDesc().Width == UINT64(test_skip_copy_width);
   }
   bool OverrideCopyTextureRegion(NativeDevice* native_device, DeviceData& device_data, uint64_t& dst_resource, uint32_t dst_subresource, const NativeBox* dst_box, uint64_t& src_resource, uint32_t src_subresource, const NativeBox* src_box) override
   {
      return test_skip_copy_width != 0 && reinterpret_cast<ID3D12Resource*>(dst_resource)->GetDesc().Width == UINT64(test_skip_copy_width);
   }
#endif
   void OnPresent(NativeDevice* native_device, DeviceData& device_data) override
   {
#if DX12 && DEVELOPMENT
      if (!custom_texture_mip_lod_bias_offset)
      {
         const std::unique_lock lock(s_mutex_samplers);
         device_data.texture_mip_lod_bias_offset = test_mip_lod_bias;
      }
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      reshade::api::effect_runtime* runtime = nullptr;

      ImGui::NewLine();
      // TODO: hide these in development given they are already shown in the dev settings?
      // Requires a change in resolution to (~fully) apply (no texture cloning yet)
      if (swapchain_format_upgrade_type > TextureFormatUpgradesType::None)
      {
         if (swapchain_format_upgrade_type == TextureFormatUpgradesType::AllowedEnabled ? ImGui::Button("Disable Swapchain Upgrade") : ImGui::Button("Enable Swapchain Upgrade"))
         {
            swapchain_format_upgrade_type = swapchain_format_upgrade_type == TextureFormatUpgradesType::AllowedEnabled ? TextureFormatUpgradesType::AllowedDisabled : TextureFormatUpgradesType::AllowedEnabled;
         }
      }
      if (texture_format_upgrades_type > TextureFormatUpgradesType::None)
      {
         if (texture_format_upgrades_type == TextureFormatUpgradesType::AllowedEnabled ? ImGui::Button("Disable Texture Format Upgrades") : ImGui::Button("Enable Texture Format Upgrades"))
         {
            texture_format_upgrades_type = texture_format_upgrades_type == TextureFormatUpgradesType::AllowedEnabled ? TextureFormatUpgradesType::AllowedDisabled : TextureFormatUpgradesType::AllowedEnabled;
         }

         // TODO: serialize all this stuff!
         ImGui::NewLine();
         ImGui::Text("Texture Format Upgrades:");
         for (auto toggleable_texture_upgrade_format : toggleable_texture_upgrade_formats)
         {
            // Dumb stream conversion
            std::ostringstream oss;
            oss << toggleable_texture_upgrade_format;
            std::string toggleable_texture_upgrade_format_name = oss.str();

            bool enabled = texture_upgrade_formats.contains(toggleable_texture_upgrade_format);

            int mode = enabled ? 1 : 0;
            const char* settings_name_strings[2] = { "Off", "On" };
            if (ImGui::SliderInt(toggleable_texture_upgrade_format_name.c_str(), &mode, 0, 1, settings_name_strings[mode], ImGuiSliderFlags_NoInput))
            {
               if (mode >= 1)
               {
                  texture_upgrade_formats.emplace(toggleable_texture_upgrade_format);
               }
               else
               {
                  texture_upgrade_formats.erase(toggleable_texture_upgrade_format);
               }
            }
         }
      }

      ImGui::NewLine();
      if (prevent_fullscreen_state ? ImGui::Button("Allow Fullscreen State") : ImGui::Button("Disallow Fullscreen State"))
      {
         prevent_fullscreen_state = !prevent_fullscreen_state;
      }
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Generic Luma mod - Developed by Pumbo", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      std::filesystem::path file_path = System::GetModulePath(hModule);
      std::string file_name = file_path.stem().string();
      // Retrieve the dll name and use it as the addon name, so we can re-use this template mod for all games by simply renaming the executable (as long as the mod just involves replacing shaders).
      bool use_custom_game_name = false;
      if (file_name.starts_with(std::string(Globals::MOD_NAME) + "-"))
      {
         file_name.erase(0, std::string(Globals::MOD_NAME).length() + 1);
         if (file_name.starts_with('_'))
            file_name.erase(0, 1);
         if (file_name != "Generic" && file_name != "Generic Mod" && file_name != "GenericMod" && file_name != "Generic Mod DX12")
            use_custom_game_name = true;
      }

      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name; // Remove the potential "_" at the beginning

      const char* game_name = use_custom_game_name ? file_name.c_str() : cleared_project_name; // Can include spaces!
      std::string mod_description = "Generic Luma mod";
      if (use_custom_game_name)
      {
         sub_game_shaders_appendix = file_name;
         mod_description += " for " + file_name;
      }
      else
      {
         sub_game_shaders_appendix = System::GetProcessExecutableName();
      }

      uint32_t mod_version = 1;
      Globals::SetGlobals(game_name, mod_description.c_str(), "https://github.com/Filoppi/Luma-Framework/", mod_version);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // On D3D12 the texture upgrades are direct and only for the textures the game creates committed (the placed ones are cloned in a later phase, which also adds the mirrors
      // of the indirect and chain upgrades)
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
#if DX11
      enable_indirect_texture_format_upgrades = true; // This is generally safer so enable it in the generic mod
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectDependencies; // Indirect dependencies are probably not needed as they'd already be upgraded too
#endif
#if DX12 && DEVELOPMENT
      enable_samplers_upgrade = true; // Boot-time only. For the smoke test ("test_mip_lod_bias")
#endif
      toggleable_texture_upgrade_formats = {
            reshade::api::format::r8g8b8a8_unorm,
            reshade::api::format::r8g8b8a8_unorm_srgb,
            reshade::api::format::r8g8b8a8_typeless,
            reshade::api::format::r8g8b8x8_unorm,
            reshade::api::format::r8g8b8x8_unorm_srgb,
            reshade::api::format::b8g8r8a8_unorm,
            reshade::api::format::b8g8r8a8_unorm_srgb,
            reshade::api::format::b8g8r8a8_typeless,
            reshade::api::format::b8g8r8x8_unorm,
            reshade::api::format::b8g8r8x8_unorm_srgb,
            reshade::api::format::b8g8r8x8_typeless,

            reshade::api::format::r10g10b10a2_unorm,
            reshade::api::format::r10g10b10a2_typeless,

            reshade::api::format::r16g16b16a16_unorm,

            reshade::api::format::r11g11b10_float,
      };
      texture_upgrade_formats.insert(toggleable_texture_upgrade_formats.begin(), toggleable_texture_upgrade_formats.end());
      texture_upgrade_formats.erase(reshade::api::format::r16g16b16a16_unorm); // This might be more likely to cause damage than not generally, so remove by default
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      game = new GenericGame();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}