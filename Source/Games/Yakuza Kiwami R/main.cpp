#define GAME_YAKUZA_KIWAMI_R 1

#define DISABLE_AUTO_DEBUGGER 1

// The engine's hardware probe creates a D3D11 device before the D3D12 ones: skip it
#define CHECK_GRAPHICS_API_COMPATIBILITY 1

#include "..\..\Core\core.hpp"

// RGG "Old Engine" (the Kiwami 1 / Yakuza 3-5 family), D3D12 only (DXIL SM 6.0). See NOTES.md.

struct YakuzaKiwamiRDeviceData final : public GameDeviceData
{
};

class YakuzaKiwamiR final : public Game
{
public:
   void OnInit(bool async) override
   {
      // The game's own cbuffers are b9, b10 and b12
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 11;
   }

   void OnCreateDevice(NativeDevice* native_device, DeviceData& device_data) override
   {
      device_data.game = new YakuzaKiwamiRDeviceData;
   }

   void PrintImGuiAbout() override
   {
      ImGui::Text("Yakuza Kiwami R Luma mod", "");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      Globals::SetGlobals(PROJECT_NAME, "Yakuza Kiwami R Luma mod", "https://github.com/Filoppi/Luma-Framework/", 1);

      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB; // B8G8R8A8 back buffer -> R16G16B16A16 FLOAT
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
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

      game = new YakuzaKiwamiR();
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
