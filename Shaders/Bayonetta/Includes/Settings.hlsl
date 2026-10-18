#ifndef SRC_GAME_SETTINGS_HLSL
#define SRC_GAME_SETTINGS_HLSL

// Include this after the global "Settings.hlsl" file.

// 1 = HDR (the grades' shoulder wrap, the scene copy's PsychoV31 display map), 0 = vanilla clamped SDR in every display mode
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

#endif // SRC_GAME_SETTINGS_HLSL
