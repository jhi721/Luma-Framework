#define GAME_CALL_OF_DUTY_2 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1

#include "..\..\Core\core.hpp"

// Call of Duty 2 (Steam v1.3) is x86 D3D9: it runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a dgVoodoo 2.87.5
// translation. The game compiles its shipped HLSL (iw_07.iwd materials/shaders) at load; hashes come from the offline mapping of
// those sources through the game's own compiler (_tools/cod2/NOTES.md) and still need an in-game check.
// Glow apply (bloom or sky bleed): the last scene pass before the 2D/UI draws, when r_glow is on.
const ShaderHashesList shader_hashes_GlowApply = {.pixel_shaders = {0xFD269FEB, 0x8162B64F}};

class CallOfDuty2 final : public Game
{
public:
   void OnInit(bool async) override
   {
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('0');

      // dgVoodoo's translated shaders use b3 (its state) and b4 (the D3D9 constants)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      if (original_shader_hashes.Contains(shader_hashes_GlowApply))
      {
         device_data.has_drawn_main_post_processing = true;
      }
      return DrawOrDispatchOverrideType::None;
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Call of Duty 2\" is open source and free.\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Thanks to the Luma team and contributors.");
      ImGui::PopTextWrapPos();

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
      {
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);
      }

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\ndgVoodoo2 (Dege)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = (project_name[0] == '_') ? (project_name + 1) : project_name;

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Call of Duty 2 Luma mod", "", mod_version);

      // The scene renders into the A8R8G8B8 back buffer and its resolve copies (see NOTES). Upgrades stay off until the in-game
      // capture proves the targets; the format list is ready for toggling them in the dev settings.
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedDisabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedDisabled;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_unorm,
         reshade::api::format::r8g8b8a8_unorm_srgb,
         reshade::api::format::r8g8b8a8_typeless,
         reshade::api::format::b8g8r8a8_unorm,
         reshade::api::format::b8g8r8a8_unorm_srgb,
         reshade::api::format::b8g8r8a8_typeless,
         reshade::api::format::b8g8r8x8_unorm,
         reshade::api::format::b8g8r8x8_typeless,
      };
      // "No1Px" as in the other dgVoodoo mods: the wrapper binds 1x1 placeholders in unused sampler slots, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

#if DEVELOPMENT
      // Offline map of the full screen shaders (materials/shaders/<name>.hlsl), for traces and the DevKit
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"85C3356C", "Glow Setup"},
              {"DEABD25C", "Glow Blur (filter_symmetric_1)"},
              {"1B7E9395", "Glow Blur (filter_symmetric_2)"},
              {"29036ECF", "Glow Blur (filter_symmetric_3)"},
              {"A730C9E7", "Glow Blur (filter_symmetric_4)"},
              {"CE67E42F", "Glow Blur (filter_symmetric_5)"},
              {"86B33B01", "Glow Blur (filter_symmetric_6)"},
              {"D2923F3D", "Glow Blur (filter_symmetric_7)"},
              {"C437C53E", "Glow Blur (filter_symmetric_8)"},
              {"FD269FEB", "Glow Apply Bloom"},
              {"8162B64F", "Glow Apply Sky Bleed"},
              {"23408E24", "Color Channel Mixer"},
              {"7D469725", "Grain Overlay"},
              {"CC85E39F", "Shell Shock"},
              {"4A39D75C", "Distortion"},
              {"0F8B56F5", "Distortion (Float Z)"},
              {"72326605", "Float Z Build"},
              {"EC7F3DD0", "Float Z Build (Alpha Test)"},
              {"57D6ED70", "Shadow Cookie Blur"},
              {"2660D466", "Shadow Cookie Caster"},
              {"9E96C712", "Shadow Cookie Display"},
              {"A9F5BDD9", "Shadow Cookie Receiver"},
              {"03CCE68B", "Sky"},
              {"BEC5D203", "Objective"},
           })
      {
         forced_shader_names.emplace(std::stoul(hash, nullptr, 16), name);
      }
#endif

      game = new CallOfDuty2();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
