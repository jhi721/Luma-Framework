// XeGTAO for Yakuza Kiwami (D3D12): replaces the game's SSAO.
// Source: https://github.com/GameTechDev/XeGTAO
//
// Game specifics (see NOTES.md "XeGTAO"):
// - main.cpp ("DrawXeGTAO()") runs the 4 dispatches at fx_ssao_test 0x5F373FFF, at the scene depth's full resolution, and skips the game's chain
//   (fx_ssao_test, fx_ssao_temporal_filter 0xC806318D, ps_ssao_blur 0x2AA0CC2B x8). At ps_ssao_upsampling 0x18B4D161, "apply_ps" writes the final AO
//   in its place, into the alpha of the screen shadow mask (the materials' t13.w) as the game does (color write mask A, FUN_140864d90).
// - tex0 = an R32 copy of the scene depth (D32S8, standard Z), t1 = an RGBA8 copy of the depth prepass' view normals (ps_normal: n * 0.498 + 0.498),
//   both made at that draw.
// - The view space is the main camera's projection (the exe's camera slot 0, never jittered there), right-handed: XeGTAO's looks down +z.
// - No TAA: NoiseIndex is frozen at 0 and denoise runs twice. GT-VBAO under DLAA or SMAA T2x (which average it over frames) takes one slice
//   whose noise moves every frame instead (main.cpp's "YKR GT-VBAO Temporal Main Pass CS").
// - The radius is the game's: fx_ssao_test samples two rings in the tangent plane at view distance max(c.z * noise, c.w * view z), c_ps_user[8]
//   and [10] (the stage's, live), noise in [0, 1] (c.x on) or 1. XeGTAO spreads its steps over its radius as the noise spreads the game's, so its
//   radius is the outer extent of both rings (as the Remastered Collection's matches ASSAO's).

// Root constants, set by main.cpp
cbuffer LumaGTAO : register(b0)
{
   float2 ViewportPixelSizeRT; // 1 / the depth's size
   float2 DepthUnpackRT;       // The projection's _33 and _43: view z = _43 / (d + _33)
   float2 NDCToViewMulRT;      // (2 / _11, -2 / _22)
   float2 NDCToViewAddRT;      // ((_31 - 1) / _11, (1 + _32) / _22)
   float FinalValuePowerRT;    // Primary darkness dial
   float RadiusOverrideRT;     // > 0 overrides the game's radius (view units, meters)
   float DebugViewRT;          // DEVELOPMENT debug view (legend in Includes/XeGTAO.hlsl)
   float NoiseIndexRT;         // The frame (mod 64) while a temporal anti-aliasing averages the noise, else 0
   float4 VanillaRadiusRT;     // The game's rings: c_ps_user[8].zw, c_ps_user[10].zw
}

// The game's SSAO darkens far wider than its rings: each ring sample counts any depth in front of it as a full occluder, so its halos reach
// past the radius. Matched live on a street at night (AO alpha vs the game's, static pixels): 7.5x its radius (1.5 m near the camera) at power 1
// gives the game's darkening percentiles (total 1.07x, its darkest 10% at 0.79x: GTAO's softer contacts); an office scene fits it too, with
// another power (main.cpp picks the power per algorithm)
#define YKR_GTAO_RADIUS_SCALE             7.5
#define XE_GTAO_EFFECT_RADIUS(viewspaceZ) (RadiusOverrideRT > 0.0 ? RadiusOverrideRT : YKR_GTAO_RADIUS_SCALE * max(max(VanillaRadiusRT.x, VanillaRadiusRT.y * (viewspaceZ)), max(VanillaRadiusRT.z, VanillaRadiusRT.w * (viewspaceZ))))

#define VIEWPORT_PIXEL_SIZE               ViewportPixelSizeRT
#define NDC_TO_VIEW_MUL                   NDCToViewMulRT
#define NDC_TO_VIEW_ADD                   NDCToViewAddRT

#define XE_GTAO_FINAL_OUTPUT_TYPE         unorm float
#define XE_GTAO_ENCODE_FINAL(v)           (v)

// Sky (d = 1) stays finite: the far plane's view z
float XeGTAO_ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
   return DepthUnpackRT.y / min(screenDepth + DepthUnpackRT.x, -1e-7);
}

Texture2D<float4> tex1 : register(t1);

float3 XeGTAO_LoadViewspaceNormal(uint2 pixCoord)
{
   // fx_ssao's decode (x * 2.0079 - 1, normalized), then z flipped from the game's right-handed view space
   const float3 n = normalize(tex1.Load(int3(pixCoord, 0)).xyz * 2.007874 - 1.0);
   return float3(n.xy, -n.z);
}

// GT-VBAO's main pass ("gtvbao_main_pass_cs", "GT-VBAO Enable") on top of XeGTAO's, which it includes. Steps set its accuracy (fewer
// miss occluders: the AO comes out brighter), slices its noise, which nothing averages over time here (NoiseIndex 0): one slice is
// blotchy after the denoise (_tools/ykr/gtao_bench, a street frame vs 8 x 32). In game (4080 SUPER, 4K) over XeGTAO: 2 x 16 +2.6 ms, 2 x 8
// +1.0 to 1.9 ms
#ifndef GTVBAO_SLICE_COUNT
#define GTVBAO_SLICE_COUNT 2.0
#endif
#ifndef GTVBAO_STEP_COUNT
#define GTVBAO_STEP_COUNT 8.0
#endif
// The thickness behind each depth sample is VBAO's strength (thicker reaches GTAO's darkening), kept small as its paper advises: at power 1
// and YKR_GTAO_RADIUS_SCALE a sweep against the game's SSAO fit 0.2 x the radius (0.3 m near the camera) best over an office (total 1.06x,
// darkest 10% 0.83x) and a street at night (0.77x, 0.69x), see NOTES.md "Calibration"
#define GTVBAO_THICKNESS(effectRadius) (0.2 * (effectRadius))
#include "../Includes/GTVBAO.hlsl"

// At ps_ssao_upsampling, drawn with an alpha only write mask: the final AO (tex0, the depth's size, the target's)
float4 apply_ps(float4 pos : SV_Position) : SV_Target
{
   return tex0.Load(int3(pos.xy, 0)).xxxx;
}
