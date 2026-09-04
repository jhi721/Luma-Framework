#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User-facing grade controls, drawn in DrawImGuiSettings (main.cpp) and read in Luma_ME1_Tonemap.hlsl.
// All apply only on the HDR tonemap path.
struct LumaGameSettings
{
   float Exposure;          // exposure multiplier (1 = vanilla). Applied scene-referred, pre-grade.
   float Saturation;        // 1 = vanilla. Saturation multiplier on the final HDR color (lerp against luminance).
   float HighlightDechroma; // 0 = off (default; keep color, only mandatory gamut desat applies); higher = bright sources fade to white sooner.
   float BloomIntensity;    // 1 = default. Scales the Luma pyramid ONLY; the game's own glow shares a buffer with the DoF blur and is never scaled.
   float Contrast;          // 1 = vanilla. Slope contrast around 18% mid-gray on the final HDR color.
   float Dithering;         // 0/1 toggle. Animated triangular dither at output to break gradient banding.
   // Appended, never reordered: this struct is a C++/HLSL ABI mirror.
   float LumaBloomEnable;    // 0/1. 1 = the Luma multi-scale HDR pyramid REPLACES the game's bloom (which the gather replacement then stops writing).
   float BloomThreshold;     // linear scene brightness where bloom starts. 1.0 matches the game's own bright-pass; near 0 makes the whole scene glow.
   float VideoAutoHDREnable; // 0/1. 1 = light PumboAutoHDR on the Bink movie pass (HDR only); 0 = flat SDR at paper white.
   float VideoAutoHDRBoost;  // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   // Not a user setting: the inverse display gamma the FGammaCorrection pass applies (cb4[11].x, measured 0.625), read
   // back by main.cpp and folded into the grade's SDR reference, because the two live in different passes.
   float DisplayGammaInverse;
   // Not a user setting: the engine's BloomScale (DOFAndBloomGather cb4[11].x, measured 0.1), read back by main.cpp.
   // The vanilla glow adds BloomScale x blur(bright pass); the Luma pyramid is energy-preserving, so this is the gain
   // that makes BloomIntensity 1 mean vanilla strength.
   float BloomScaleLive;
};

// Game specific cbuffer (instance/pass) data, uploaded at the gamma-correction draw.
struct LumaGameData
{
   // 1 when UberPostProcessBlend ran earlier this frame (its replacement left LINEAR HDR in the intermediate), 0 when
   // the engine skipped it (elevators, some loading/UI scenes) and the gamma-correction pass reads the RAW fp16 scene:
   // then that replacement must run the whole HDR block itself instead of passing hundreds of nits straight through.
   float UberRanThisFrame;
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
