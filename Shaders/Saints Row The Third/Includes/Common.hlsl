// Saints Row: The Third - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real settings struct.

// clang-format off
// ORDER IS LOAD-BEARING - GameCBuffers must define LUMA_GAME_CB_STRUCTS before Settings.hlsl is pulled in below.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on

// Linear (1.0 = paper white) -> the gamma-encoded post-process space. With UI_DRAW_TYPE 2 the scene is pre-scaled by
// GamePaperWhite / UIPaperWhite: the composition rescales the whole image by UIPaperWhite, which puts the gamma-space
// UI drawn on top at the UI paper white and the scene back at its own.
float3 SR3_EncodeOutput(float3 color)
{
#if UI_DRAW_TYPE >= 2
   color *= GamePaperWhiteNits / max(UIPaperWhiteNits, 1.0);
#endif
   return linear_to_gamma(color, GCT_POSITIVE);
}
