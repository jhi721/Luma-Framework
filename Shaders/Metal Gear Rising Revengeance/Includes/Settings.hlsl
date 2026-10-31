#ifndef SRC_GAME_SETTINGS_HLSL
#define SRC_GAME_SETTINGS_HLSL

// Include this after the global "Settings.hlsl" file.

// 1 = HDR (the grade's shoulder wrap and the C-infinity display map in Gather_0x726E6E78), 0 = vanilla clamped SDR in every display mode
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

#endif // SRC_GAME_SETTINGS_HLSL
