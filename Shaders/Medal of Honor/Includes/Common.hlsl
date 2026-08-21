// Medal of Honor (2010) — game-local Common. Include this (instead of the shared "../Includes/Common.hlsl")
// from any game shader that needs the per-game LumaGameSettings: it defines LUMA_GAME_CB_STRUCTS via
// GameCBuffers.hlsl BEFORE the shared Settings.hlsl declares the LumaSettings cbuffer, so GameSettings is the
// real grade struct rather than the empty default.

// Define the game custom cbuffer structs.
#include "GameCBuffers.hlsl"
// Shared global common (pulls in the shared Settings.hlsl -> LumaSettings cbuffer with our GameSettings).
#include "../../Includes/Common.hlsl"

// The tonemap's UI pre-scale, applied to the canvas after the display map (UI_DRAW_TYPE 2, tail of FinishMOH).
// 1.0 whenever UI Paper White equals Game Paper White; deriving it keeps both anchors honest on any setting.
float UiPreScale()
{
#if UI_DRAW_TYPE >= 2
   if (LumaSettings.GamePaperWhiteNits > 0.0)
      return LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return 1.0;
}

// Where scene paper white lands in this canvas' encoding: the value the 8-bit post chain treated as 1.0, and the
// ceiling its UNORM targets used to enforce for free. Shared by the bright pass and the composite ON PURPOSE --
// the two disagreeing about this anchor is exactly how the vanilla bloom path grows halos it never had.
float PaperWhiteOnCanvas()
{
   return linear_to_gamma1(UiPreScale());
}
