#define GAME_BAYONETTA 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1

#include "..\..\Core\core.hpp"

// Bayonetta (PC, 2017) is x86 D3D9Ex: it runs through dgVoodoo (D3D9 -> D3D11), so every hash below is a dgVoodoo translation.
// Hashes come from the offline mapping of the game's shader.dat (_tools/bayonetta/NOTES.md) and still need an in-game check.
// filter00: per-channel 1D curve LUT + saturation + vertical colour gradient (the grade, drawn before bloom). dgVoodoo 2.87.5, 2.81.3.
const ShaderHashesList shader_hashes_Grade = {.pixel_shaders = {0x719339A9, 0x3BE8E47F}};

class Bayonetta final : public Game
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
      if (original_shader_hashes.Contains(shader_hashes_Grade))
      {
         device_data.has_drawn_main_post_processing = true;
      }
      return DrawOrDispatchOverrideType::None;
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Bayonetta\" is open source and free.\n"
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
      Globals::SetGlobals(cleared_project_name, "Bayonetta Luma mod", "", mod_version);

      // Every scene target is A8R8G8B8 (see NOTES): render targets are upgraded to check the scene range, the swapchain isn't yet
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedDisabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
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
      // Offline map of the post shaders (both dgVoodoo builds), for traces and the DevKit
      for (const auto& [hash, name] : std::initializer_list<std::pair<const char*, const char*>>{
              {"719339A9", "Grade (filter00)"},
              {"3BE8E47F", "Grade (filter00)"},
              {"B7A08D4A", "Grade Luma Curve (filter04)"},
              {"2D4416C4", "Grade Luma Curve (filter04)"},
              {"8B6A8D61", "Bloom Source (filter05)"},
              {"29FE50C2", "Bloom Source (filter05)"},
              {"49231C65", "Bloom Blur H (filter06)"},
              {"FB14B417", "Bloom Blur H (filter06)"},
              {"F758B2DC", "Bloom Blur V (filter07)"},
              {"1F962E82", "Bloom Blur V (filter07)"},
              {"CE62BA4B", "Blur H (filter01)"},
              {"4B605EEB", "Blur H (filter01)"},
              {"91CC01D3", "Blur V (filter02)"},
              {"A5207DF1", "Blur V (filter02)"},
              {"68B566FE", "Centre Average (filter03)"},
              {"AB34862D", "Centre Average (filter03)"},
              {"9D71029B", "DoF CoC (filter08)"},
              {"CCDA0B04", "DoF CoC (filter08)"},
              {"40AF18CC", "SSAO"},
              {"C32759B1", "SSAO"},
              {"91A6C437", "Copy"},
              {"7584B641", "Copy"},
              {"B4CE6220", "Copy No Alpha"},
              {"F70BABE8", "Copy No Alpha"},
              {"87A169B9", "Object Motion Blur"},
              {"5BF04DF8", "Object Motion Blur"},
              {"651AC335", "Skinned Velocity (BlurSrcWeight)"},
              {"988429CF", "Skinned Velocity (BlurSrcWeight)"},
              {"C60649FD", "Video YUV"},
              {"ED0DA20F", "Video YUV"},
           })
      {
         forced_shader_names.emplace(std::stoul(hash, nullptr, 16), name);
      }
#endif

      game = new Bayonetta();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
