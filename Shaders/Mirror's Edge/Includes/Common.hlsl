// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// UI_DRAW_TYPE 2: the HUD draws onto the tonemap's canvas, so the scene is pre-scaled to land at GamePaperWhite after
// composition rescales the canvas by UIPaperWhite.
float3 PreScaleForUIPaperWhite(float3 linearColor)
{
#if UI_DRAW_TYPE >= 2
   if (LumaSettings.GamePaperWhiteNits > 0.0)
      linearColor *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return linearColor;
}
