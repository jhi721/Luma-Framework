// Mass Effect (2007) - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real grade struct.

// clang-format off
// ORDER IS LOAD-BEARING - GameCBuffers must define LUMA_GAME_CB_STRUCTS before Settings.hlsl is pulled in below.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on
