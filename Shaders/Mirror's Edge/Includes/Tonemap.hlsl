// The two grades that write the gamma-encoded canvas (TdToneMapping, and GammaCorrection when the TdTonemapping system setting is
// off): dgVoodoo's D3D9 constants, the original's pow, the HDR display map and the canvas encode they share.
#ifndef LUMA_MIRRORS_EDGE_TONEMAP
#define LUMA_MIRRORS_EDGE_TONEMAP

// clang-format off
#include "../../Includes/GammaCanvasDisplayMap.hlsl" // GammaCanvasDisplayMap, EncodeGammaCanvas
#include "../../Includes/DgVoodoo.hlsl"
// clang-format on

// 1 = HDR, 0 = vanilla clamped SDR.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// b3/b4 are dgVoodoo's state and the D3D9 pixel constants (c0 at row 8).
cbuffer DgVoodooState : register(b3)
{
   float4 DgvConstants[77] : packoffset(c0);
}
cbuffer PixelShaderConstants : register(b4)
{
   float4 PsConstants[236] : packoffset(c0);
}

// The original's pow: |x| floored at 1e-4, dgVoodoo's guarded log.
float3 PowTd(float3 base, float3 exponent)
{
   const float3 b = max(abs(base), 0.0001);
   return exp2(exponent * float3(DgVoodooLog2(b.x), DgVoodooLog2(b.y), DgVoodooLog2(b.z)));
}

// The extended (unclipped) grade, linear BT.709 (1 = paper white), to the display (see "GammaCanvasDisplayMap")
float3 DisplayMapTd(float3 linearBT709)
{
   return GammaCanvasDisplayMap(linearBT709, LumaSettings.GameSettings.Contrast, LumaSettings.GameSettings.Saturation, LumaSettings.GameSettings.HighlightsDesaturation);
}

// A linear frame as the canvas holds it: gamma space (POST_PROCESS_SPACE_TYPE 0), so the HUD blends onto it like vanilla.
float3 EncodeTdCanvas(float3 linearColor, float2 uv)
{
   return EncodeGammaCanvas(linearColor, uv, LumaSettings.GameSettings.Dithering > 0.5);
}

#endif // LUMA_MIRRORS_EDGE_TONEMAP
