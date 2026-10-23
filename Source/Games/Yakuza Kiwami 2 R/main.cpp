#define GAME_YAKUZA_KIWAMI_2_R 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe ("pxd::DeviceDetectWindowClass") creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

#include "..\..\Core\core.hpp"

// Dragon Engine, D3D12 only (DXIL SM 6.0). The scene ends in 0xD0869ED8 (log10(1 + scene), the only tone curve), the grade
// 0x167D96E5 (HDR replacement in Shaders/Yakuza Kiwami 2 R) writes an R8G8B8A8 target in gamma, 0x7561D004 copies it into the
// B8G8R8A8 UI target, the HUD blends over it in gamma, and 0x8578A29E blits that to the swapchain (pow 2.2 decode, sRGB encode).
// Pipeline record: _tools/yk2r/NOTES.md.

class YakuzaKiwami2R final : public Game
{
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
