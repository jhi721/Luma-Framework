#define GAME_PERSONA_5_STRIKERS 1

// Core only checks whether this is defined (any value skips the "attach the debugger" popup)
#define DISABLE_AUTO_DEBUGGER 1

// Movies play through a separate DX9 device (Media Foundation), same as Nioh
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

#include "..\..\Core\core.hpp"

namespace
{
   // Katana engine PostEffect3 composite: exposure, lens effects, vignette, then the baked HDR 3D LUT (tonemap + grade).
   // Every captured scene (menus, dialogue, hub, field) runs it once, writing straight into the swapchain.
   constexpr uint32_t composite_hash = 0x45A96F2D;
   constexpr uint32_t fxaa_hash = 0xED2D9823;

   bool g_hide_ui = false; // Session only, so a restart never comes back without a HUD
} // namespace

class Persona5Strikers final : public Game
{
public:
   void OnInit(bool async) override
   {
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('1'); // The composite, UI and FXAA write the swapchain through sRGB views, so in linear
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('0'); // sRGB (implicit, through the swapchain views)
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2'); // The UI blends straight onto the swapchain after the composite

      // The game binds b0-b4
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;

      default_luma_global_game_settings.Dithering = cb_luma_global_settings.GameSettings.Dithering = 1.f;
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      // Everything the game draws onto the swapchain after the composite is UI (HUD, menus, dialogue boxes, fades), except FXAA
      if (g_hide_ui && device_data.has_drawn_main_post_processing && (stages & reshade::api::shader_stage::pixel) == reshade::api::shader_stage::pixel && !original_shader_hashes.Contains(fxaa_hash, reshade::api::shader_stage::pixel))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> rtv_resource;
         if (rtv)
            rtv->GetResource(&rtv_resource);
         if (rtv_resource)
         {
            const std::shared_lock lock(device_data.mutex);
            if (device_data.back_buffers.contains(reinterpret_cast<uint64_t>(rtv_resource.get())))
               return DrawOrDispatchOverrideType::Replaced;
         }
         return DrawOrDispatchOverrideType::None;
      }

      if (original_shader_hashes.Contains(composite_hash, reshade::api::shader_stage::pixel))
         device_data.has_drawn_main_post_processing = true;

      return DrawOrDispatchOverrideType::None;
   }

   void LoadConfigs() override
   {
      reshade::get_config_value(nullptr, NAME, "Dithering", cb_luma_global_settings.GameSettings.Dithering);
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& settings = cb_luma_global_settings.GameSettings;

      ImGui::SeparatorText("Effects");
      bool dithering = settings.Dithering > 0.5f;
      if (ImGui::Checkbox("Dithering", &dithering))
      {
         settings.Dithering = dithering ? 1.f : 0.f;
         device_data.cb_luma_global_settings_dirty = true;
         reshade::set_config_value(nullptr, NAME, "Dithering", settings.Dithering);
      }
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Reduces gradient banding.");
      if (DrawResetButton(settings.Dithering, default_luma_global_game_settings.Dithering, "Dithering"))
         device_data.cb_luma_global_settings_dirty = true;

      ImGui::SeparatorText("UI");
      ImGui::Checkbox("Hide Gameplay UI", &g_hide_ui);
      if (ImGui::IsItemHovered())
         ImGui::SetTooltip("Disables the in-game UI.");
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Persona 5 Strikers\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR.\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      ImGui::PopStyleColor(3);

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
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nDICE (HDR tonemapper)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Persona 5 Strikers Luma mod", "", 1);

      // The composite, UI and FXAA all write the swapchain
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;
      // FXAA reads a BGRA8 copy of the swapchain (after UI), which has to hold HDR too.
      // ponytail: this also upgrades every other swapchain sized BGRA8 target (e.g. the G-buffer albedo), a copy-only chain upgrade would be tighter if VRAM matters
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      texture_upgrade_formats = {reshade::api::format::b8g8r8a8_typeless};
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution;

#if DEVELOPMENT
      forced_shader_names.emplace(composite_hash, "Composite");
      forced_shader_names.emplace(0x90C6B12E, "UI");
      forced_shader_names.emplace(fxaa_hash, "FXAA");
      forced_shader_names.emplace(0x63435B03, "SSAO");
      forced_shader_names.emplace(0x691D080F, "Deferred Lighting");
#endif

      game = new Persona5Strikers();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
