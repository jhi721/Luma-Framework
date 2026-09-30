// Include this instead of the shared "../Includes/Common.hlsl" from any shader that reads the per-game LumaGameSettings: it defines
// LUMA_GAME_CB_STRUCTS through GameCBuffers.hlsl before the shared Settings.hlsl declares the LumaSettings cbuffer, so GameSettings
// is the real grade struct rather than the empty default.

#include "GameCBuffers.hlsl"
// Keep after GameCBuffers.hlsl (see above); this comment also stops clang-format from sorting the two
#include "../../Includes/Common.hlsl"

// UI_DRAW_TYPE 2, for the linear content the passes write under the gamma-SDR HUD (the grade, the movies): pre-scaled so it lands at
// GamePaperWhite after the composition rescales the canvas by UIPaperWhite
float3 PreScaleForUIPaperWhite(float3 linearColor)
{
#if UI_DRAW_TYPE >= 2
   linearColor *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return linearColor;
}
