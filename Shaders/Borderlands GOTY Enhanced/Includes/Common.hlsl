// Include this instead of the shared "../Includes/Common.hlsl" from any shader that reads the per-game LumaGameSettings: it defines
// LUMA_GAME_CB_STRUCTS through GameCBuffers.hlsl before the shared Settings.hlsl declares the LumaSettings cbuffer, so GameSettings
// is the real grade struct rather than the empty default.

#include "GameCBuffers.hlsl"
// Keep after GameCBuffers.hlsl (see above); this comment also stops clang-format from sorting the two
#include "../../Includes/Common.hlsl"
