// Mass Effect 3 (2012) - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real struct.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on

// UI_DRAW_TYPE 2: the gamma HUD blends onto the same canvas after FXAA and Core's composition then scales the whole
// canvas by UI Paper White, so the scene is pre-scaled by Game/UI Paper White to land at Game Paper White. This is the
// ratio MELE divides out as R = UI/Game in its stage 1; here Core's composition plays MELE's stage 2. Guarded: an
// unset GamePaperWhiteNits would black the scene and leave the HUD.
float GetUIPaperWhitePreScale()
{
#if UI_DRAW_TYPE >= 2
   if (GamePaperWhiteNits > 0.0)
      return GamePaperWhiteNits / max(UIPaperWhiteNits, 1.0);
#endif
   return 1.0;
}
