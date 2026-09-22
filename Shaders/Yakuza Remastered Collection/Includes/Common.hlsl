// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// Linear (1.0 = paper white) -> the gamma-space post-process output of the scene grade and the videos. With UI_DRAW_TYPE 2
// the HUD is drawn on top in gamma SDR, so the output is pre-scaled to land at UI paper white after composition.
float3 Y3_EncodeOutput(float3 color)
{
#if UI_DRAW_TYPE >= 2
   color *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return linear_to_gamma(color);
}