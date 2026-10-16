// Saints Row: The Third and Gat out of Hell - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real settings struct.

// clang-format off
// ORDER IS LOAD-BEARING - GameCBuffers must define LUMA_GAME_CB_STRUCTS before Settings.hlsl is pulled in below.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on

// Linear (1.0 = paper white) -> the gamma-encoded post-process space, with the "GetUIPaperWhitePreScale" pre-scale.
float3 SR_EncodeOutput(float3 color)
{
   color *= GetUIPaperWhitePreScale(GamePaperWhiteNits, UIPaperWhiteNits, false);
   return linear_to_gamma(color, GCT_POSITIVE);
}
