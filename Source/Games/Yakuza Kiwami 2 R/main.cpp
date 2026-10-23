#define GAME_YAKUZA_KIWAMI_2_R 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe ("pxd::DeviceDetectWindowClass") creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

// SMAA or RCAS sharpening in place of the copy of the grade into the UI target (see "OnDrawOrDispatch()")
#define ENABLE_RCAS 1
#define ENABLE_SMAA 1

#include "..\..\Core\core.hpp"

// Dragon Engine, D3D12 only (DXIL SM 6.0). The scene ends in 0xD0869ED8 (log10(1 + scene), the only tone curve), the grade
// 0x167D96E5 (HDR replacement in Shaders/Yakuza Kiwami 2 R) writes an R8G8B8A8 target in gamma, 0x7561D004 copies it into the
// B8G8R8A8 UI target, the HUD blends over it in gamma, and 0x8578A29E blits that to the swapchain (pow 2.2 decode, sRGB encode).

namespace
{
   constexpr uint32_t PIXEL_SHADER_GRADE = 0x167D96E5;
   constexpr uint32_t PIXEL_SHADER_COPY = 0x7561D004; // The copy of the grade into the UI target, and the bloom chain's downsample

   bool g_smaa = false;          // SMAA on the graded image (before the HUD), then RCAS if that is on too
   float g_rcas_sharpness = 0.f; // RCAS on the graded image (before the HUD), 0 = off (the vanilla copy)
} // namespace

struct YakuzaKiwami2RDeviceData final : public GameDeviceData
{
   // What the grade rendered into (its fp16 clone), for the copy into the UI target to read with RCAS. Written and read by the
   // threads that record the grade and the UI, cleared when the swapchain (and with it the game's targets) is made again.
   std::atomic<uint64_t> grade_output = 0;
   std::atomic<reshade::api::format> grade_output_format = reshade::api::format::unknown;
};

class YakuzaKiwami2R final : public Game
{
   static YakuzaKiwami2RDeviceData& GetGameDeviceData(DeviceData& device_data)
   {
      return *static_cast<YakuzaKiwami2RDeviceData*>(device_data.game);
   }

public:
   void OnInit(bool async) override
   {
      // The grade pre-scales the scene by Game/UI Paper White, the HUD keeps its gamma blend, the composition scales it all back
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      // Manual paper white sliders, not the OS reference level. Core shows the UI one on UI_DRAW_TYPE >= 1.
      use_os_reference_white_level = false;
   }

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new YakuzaKiwami2RDeviceData;
   }

   void OnInitSwapchain(reshade::api::swapchain* swapchain) override
   {
      GetGameDeviceData(*swapchain->get_device()->get_private_data<DeviceData>()).grade_output = 0;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(NativeDevice* native_device, NativeCommandList* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      auto& game_device_data = GetGameDeviceData(device_data);
      const D3D12State::CommandListState& state = cmd_list_data.d3d12_state;
      if (state.render_target_views.empty())
      {
         return DrawOrDispatchOverrideType::None;
      }
      const D3D12State::ViewInfo& render_target = state.render_target_views[0];
      if (original_shader_hashes.Contains(PIXEL_SHADER_GRADE, reshade::api::shader_stage::pixel))
      {
         game_device_data.grade_output_format = render_target.format;
         game_device_data.grade_output = render_target.resource;
         return DrawOrDispatchOverrideType::None;
      }
      // The copy into the UI target is the one at the swapchain's width (the bloom chain's are smaller). Its source, the grade output, is in a
      // shader resource state there and the UI target in a render target state, which SMAA and RCAS need.
      const uint64_t grade_output = game_device_data.grade_output;
      if ((g_smaa || g_rcas_sharpness > 0.f) && grade_output != 0 && original_shader_hashes.Contains(PIXEL_SHADER_COPY, reshade::api::shader_stage::pixel) && render_target.resource != 0)
      {
         reshade::api::device* const device = cmd_list_data.cmd_list->get_device();
         if (device->get_resource_desc({render_target.resource}).texture.width != device_data.output_resolution.x)
         {
            return DrawOrDispatchOverrideType::None;
         }
         bool drawn = false;
         if (g_smaa)
         {
            drawn = DrawSMAA(cmd_list_data, device_data, {grade_output}, game_device_data.grade_output_format, {}, {}, {render_target.resource}, state.render_target_descs[0], {}, g_rcas_sharpness);
         }
         else
         {
            drawn = DrawRCAS(cmd_list_data, device_data, {grade_output}, game_device_data.grade_output_format, {render_target.resource}, state.render_target_descs[0], g_rcas_sharpness);
         }
         if (drawn)
         {
            return DrawOrDispatchOverrideType::Replaced;
         }
      }
      return DrawOrDispatchOverrideType::None;
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa, false, "Smooths the jagged edges of the image under the HUD.");
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening of the image under the HUD (0 = off).");
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Yakuza Kiwami 2 R Luma mod", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza Kiwami 2 R Luma mod", "https://github.com/Filoppi/Luma-Framework/", 1);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // B8G8R8A8 back buffer -> R16G16B16A16 FLOAT
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      // R16G16B16A16 UNORM stays: it isn't an SDR image format here
      texture_upgrade_formats = {
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

         reshade::api::format::r11g11b10_float,
      };
      texture_format_upgrades_2d_size_filters = 0 | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;
      // The image targets are placed, and only the placed textures these shaders write get upgraded (see "includes/d3d12_clones.h"). An
      // R8G8B8A8 target of the frame holds packed IDs (0x473780B3 writes, 0xACD93541 decodes uint(x * 255)) that fp16 breaks.
      auto_texture_format_upgrade_shader_hashes[0x167D96E5] = {{0}, {}}; // Grade output (R8G8B8A8)
      auto_texture_format_upgrade_shader_hashes[0x7561D004] = {{0}, {}}; // UI target (B8G8R8A8), also the bloom chain's downsample

      game = new YakuzaKiwami2R();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
