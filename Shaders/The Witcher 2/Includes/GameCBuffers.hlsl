#ifndef LUMA_GAME_CB_STRUCTS
#define LUMA_GAME_CB_STRUCTS

#ifdef __cplusplus
// This include is needed to allow reading shader types from c++.
#include "../../../Source/Core/includes/shader_types.h"
#endif

// Mirrors c++ name spaces.
namespace CB
{
// User grade settings, mirrored to c++ ("OnInit" defaults / ImGui in main.cpp) and read through
// LumaSettings.GameSettings: Exposure in the tonemap replacement (Luma_TW2_Tonemap.hlsl), the video knobs in
// Video_0x30BE6D87, everything else in the final grade. Defaults are vanilla no-ops, apart from Dithering and Video AutoHDR.
struct LumaGameSettings
{
   float Exposure;              // scene multiplier before the game grade (1 = vanilla). SDR + HDR
   float Saturation;            // saturation around luminance, after the display map (1 = vanilla). HDR display path only
   float HighlightDechroma;     // 0 = off. DICE highlight desaturation: sources above a third of peak fade toward white, mid-tones untouched. HDR display path only
   float Dithering;             // 1 = animated triangular output dither (anti-banding). SDR + HDR
   float VideoAutoHDREnable;    // 0/1. 1 = light PumboAutoHDR on pre-rendered videos (HDR only); 0 = flat SDR at paper white
   float VideoAutoHDRBoost;     // 0..1. Highlight-expansion strength; peak = lerp(sRGB white, 250 nits, boost). 0 = off
   float VignetteIntensity;     // 1 = vanilla vignette darkening, 0 = none. Applies in SDR and HDR (the vignette lives in the vanilla grade tail)
   float Contrast;              // 1 = vanilla. Multiplicative contrast around 18% mid-gray, applied before the display map. HDR display path only
   float BloomIntensity;        // 1 = vanilla, 0 = none. Scales the engine's glow where it enters the screen blend (SDR + HDR)
   float ColorGradingIntensity; // 1 = vanilla, 0 = no split toning. Fades the vanilla shadow/highlight split toning out of the grade (SDR + HDR)
};

// Define the game specific cbuffer (instance/pass) data here
struct LumaGameData
{
   float Dummy; // hlsl doesn't support empty structs
};

// The motion vector fill's constants (Luma_TW2_MotionVectorFill.hlsl, b0), written by "EndScene"
struct MotionVectorFillConstants
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 depth_from_view;          // The projection's depth row: device depth = x + y / view depth
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled;
   float exposure_enabled; // The upscaler's exposure from the exposure pass's levels (linear scene, see main.cpp), else 1
   float2 render_size;     // The scene's top-left render area (pixels): the output size, or smaller under the render scale
   float user_exposure;    // Luma's Exposure, the tonemap's scene multiplier (1 in the vanilla SDR tonemap)
   float exposure_static;  // The static exposure perms: the gain is the pass's c49 levels, not an adaptation texel
};

// XeGTAO's runtime knobs (Luma_TW2_XeGTAO.hlsl, b9), written by "RunXeGTAO"
struct GTAOKnobs
{
   float final_value_power;    // primary darkness dial (2.2 a preference; 1.0 matches the vanilla AO histogram)
   float depth_scale;          // viewZ divisor (game units -> ~meters); 1 here, the depth is in meters already
   float radius_override;      // > 0 overrides EFFECT_RADIUS (view units after depth_scale)
   float debug_view;           // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float2 viewport_pixel_size; // 1 / AO target resolution (half render res)
   float2 area_scale;          // The scene's share of the AO target under the render scale (main.cpp "RenderArea"), else 1
   float noise_index;          // XeGTAO's temporal noise: frame % 64 while DLSS/FSR accumulates the AO, else 0
   float3 padding;
};
} // namespace CB

#ifdef __cplusplus
static_assert(sizeof(CB::MotionVectorFillConstants) == 112 && sizeof(CB::GTAOKnobs) == 48); // The HLSL cbuffers' sizes
#endif

#endif // LUMA_GAME_CB_STRUCTS
