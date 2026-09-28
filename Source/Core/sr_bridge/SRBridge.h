#pragma once

#include "../includes/super_resolution.h"

// DLSS and FSR 3 for 32-bit games: both are x64 only, so they run in "sr_bridge_helper.exe" next to the game's exe
// (Source/Tools/SR Bridge Helper) on shared copies of the inputs. Games opt in with "UseLumaSRBridge" (their package then
// ships the helper and DLSS's dll), and use it like the in-process implementations.
#if defined(_WIN64) || !defined(ENABLE_SR_BRIDGE)
#undef ENABLE_SR_BRIDGE
#define ENABLE_SR_BRIDGE 0
#endif

#if ENABLE_SR_BRIDGE

namespace SRBridge
{
   // One per SR type, registered as that type's implementation
   class Bridge : public SR::SuperResolutionImpl
   {
   public:
      explicit Bridge(SR::Type type) : type(type)
      {
      }

      virtual bool HasInit(const SR::InstanceData* data) const override;
      virtual bool IsSupported(const SR::InstanceData* data) const override;

      virtual bool Init(SR::InstanceData*& data, ID3D11Device* device, IDXGIAdapter* adapter = nullptr) override;
      virtual void Deinit(SR::InstanceData*& data, ID3D11Device* optional_device = nullptr) override;

      virtual bool UpdateSettings(SR::InstanceData* data, ID3D11DeviceContext* command_list, const SR::SettingsData& settings_data) override;

      // Until the helper is ready, the color is copied to the output (so the game doesn't treat it as a failure)
      virtual bool Draw(const SR::InstanceData* data, ID3D11DeviceContext* command_list, const DrawData& draw_data) override;

      virtual int GetJitterPhases(const SR::InstanceData* data) const override;

   private:
      SR::Type type;
   };
} // namespace SRBridge

#endif
