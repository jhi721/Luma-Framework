// Saints Row IV - game-local Common. Include this instead of "../Includes/Common.hlsl": it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE Settings.hlsl, so GameSettings is the real settings struct.

// clang-format off
// ORDER IS LOAD-BEARING - GameCBuffers must define LUMA_GAME_CB_STRUCTS before Settings.hlsl is pulled in below.
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on

// The distortion map (r8g8b8a8 in vanilla, FP16 once Luma upgrades it with the swapchain-sized targets) at the screen uv, as the
// 8-bit target stored it: each texel clamped and re-quantized before a bilinear filter. The writers' 0.5 neutral then reads 128/255
// again, as the readers' decode bias expects: rl_distortion_01 centres it exactly, the finals keep vanilla's 1 + 0.5/255 bias. At scale 1 the pixels sit on texel centres, so this is the texel itself. Under
// the upscaling sub-rect (main.cpp "g_render_scale") the scene drew the map into the target's top-left share, LumaData.CustomData3/4
// (0 = the whole target): read at that share, clamped to it, and interpolated without re-quantization steps.
float4 SR4_LoadDistortionTexel(Texture2D<float4> map, int2 texel, int2 limit)
{
   return round(saturate(map.Load(int3(clamp(texel, 0, limit - 1), 0))) * 255.0) / 255.0;
}

float4 SR4_SampleDistortionMap(Texture2D<float4> map, float2 uv)
{
   int2 size;
   map.GetDimensions(size.x, size.y);
   const float2 share = LumaData.CustomData3 > 0.0 ? float2(LumaData.CustomData3, LumaData.CustomData4) : 1.0;
   const int2 limit = int2(round(share * size));
   const float2 texel = uv * share * size - 0.5;
   const int2 base = int2(floor(texel));
   const float2 f = texel - base;
   return lerp(lerp(SR4_LoadDistortionTexel(map, base, limit), SR4_LoadDistortionTexel(map, base + int2(1, 0), limit), f.x), lerp(SR4_LoadDistortionTexel(map, base + int2(0, 1), limit), SR4_LoadDistortionTexel(map, base + int2(1, 1), limit), f.x), f.y);
}

// Linear (1.0 = paper white) -> the gamma-encoded post-process space. With UI_DRAW_TYPE 2 the scene is pre-scaled by
// GamePaperWhite / UIPaperWhite: the composition rescales the whole image by UIPaperWhite, which puts the gamma-space
// UI drawn on top at the UI paper white and the scene back at its own.
float3 SR4_EncodeOutput(float3 color)
{
#if UI_DRAW_TYPE >= 2
   color *= GamePaperWhiteNits / max(UIPaperWhiteNits, 1.0);
#endif
   return linear_to_gamma(color, GCT_POSITIVE);
}
