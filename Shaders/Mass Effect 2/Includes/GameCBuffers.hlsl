#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// Mass Effect 2 (2010) user-facing controls, drawn in DrawImGuiSettings and read across the ME2 shaders. A C++/HLSL
// ABI: fields are APPENDED, never reordered. ⚠ Growing it needs a REBUILD AND REDEPLOY, not just Reload Shaders.
struct LumaGameSettings
{
   float Exposure;          // scene-referred multiplier applied pre-grade on the uber. 1 = vanilla.
   float Saturation;        // 1 = vanilla. Lerp against BT.709 luminance, applied LAST, after the display map.
   float Contrast;          // 1 = vanilla. Multiplicative contrast around 18% mid-gray, applied before the display map.
   float HighlightDechroma; // 0 = off (default). DICE highlight desaturation: sources above a third of peak fade toward
                            // white, mid-tones untouched.
   float Dithering;         // 0/1. Animated triangular dither at the material's encode, to break gradient banding.
   // Native look controls. Both scale ONLY the radial/temporal part of a native effect, never its white point: the
   // vignette's centre value (1.010363, 1.000006, 1.163092) is part of the vanilla grade, not a darkening.
   float VignetteIntensity;  // 1 = vanilla radial darkening, 0 = flat (white point kept).
   float FilmGrainIntensity; // 1 = vanilla grain amplitude, 0 = off. Only the grain material perm reads it.
   // Bink movies bypass the scene passes entirely, so without this they sit flat at paper white while gameplay has
   // highlights. Appended after the fields above, never reordered.
   float VideoAutoHDREnable; // 0/1. 1 = light PumboAutoHDR on the Bink pass (HDR only); 0 = flat SDR at paper white.
   float VideoAutoHDRBoost;  // 0..1 highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off.
   // Luma HDR bloom pyramid. Appended after the fields above, never reordered.
   float BloomIntensity;  // 1 = vanilla STRENGTH (see BloomScaleLive). Scales the Luma pyramid ONLY: the game's own
                          // glow shares a buffer with the DoF blur and can only be switched off, never scaled.
   float BloomThreshold;  // linear scene brightness where bloom starts. 1.0 is exactly where the gather's own
                          // bright-pass sits; near 0 the whole scene glows, which is the failure mode.
   float LumaBloomEnable; // 0/1. 1 = the pyramid REPLACES the game's bloom, which the gather replacement then
                          // stops writing (Includes/DofBloomGather.hlsl reads this as a BOOLEAN).
   float BloomScaleLive;  // not a user setting: the engine's BloomScale (gather cb4[11].x), read back by main.cpp.
                          // The vanilla glow adds BloomScale x blur(bright pass) and the pyramid is
                          // energy-preserving, so this is the gain that makes BloomIntensity 1 mean vanilla.
   // DEVELOPMENT tuning control, appended after the fields above and never reordered. Read ONLY by the hard-clip
   // uber permutation, and ONLY under DEVELOPMENT - shipping builds compile the HueOnly wrapper and never touch it,
   // so a Publishing build is bit-exact whatever this holds.
   float HardClipHighlightPurity; // MacLeod-Boynton chrominance strength. 0 = the target keeps its own purity (the
                                  // canonical HueOnly contract, and the shipped look); 1 = adopt the Reinhard
                                  // reference's purity, which is lower above its shoulder, so highlights wash out.
};

// Game specific cbuffer (instance/pass) data, uploaded per replaced draw.
struct LumaGameData
{
   // hlsl doesn't support empty structs. Nothing needs per-draw data yet: ME2's gamma-only pass (FGammaCorrection
   // 0x0160196C) has never been observed drawing, so it needs no "did the uber run" flag (see NOTES.md).
   float Dummy;
};
} // namespace CB

#endif // LUMA_GAME_CB_STRUCTS
