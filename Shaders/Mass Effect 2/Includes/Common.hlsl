// Mass Effect 2 (2010) - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real game struct.

// Define the game custom cbuffer structs.
#include "GameCBuffers.hlsl"
// Shared global common (pulls in the shared Settings.hlsl -> LumaSettings cbuffer with our GameSettings).
#include "../../Includes/Common.hlsl"
