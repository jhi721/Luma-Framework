// Mass Effect 3 (2012) - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real struct.

// clang-format off
// ORDER IS LOAD-BEARING - do not sort.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on
