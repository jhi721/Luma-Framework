// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// Linear (1.0 = paper white) -> the gamma-space post-process output of the scene grade and the videos. With UI_DRAW_TYPE 2
// the HUD is drawn on top in gamma SDR, so the output is pre-scaled to land at UI paper white after composition.
float3 YRC_EncodeOutput(float3 color)
{
#if UI_DRAW_TYPE >= 2
   color *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   return linear_to_gamma(color);
}

// The engine-wide material alpha test: `reference` (cb11[0].z) in 1/255 units, 0 = off.
void YRC_AlphaTest(float alpha, uint reference)
{
   if (reference > 0u && (alpha - float(reference) * 0.00392156886) < 0.0)
      discard;
}

// Y5R lit materials end in the tone curve sqrt(1 - exp(-u)), which the addon continues past u = pivot along its tangent
// (see "PatchY5MaterialToneCurve" in main.cpp; constants in GameCBuffers.hlsl). Maps the extended output back to the
// vanilla one: with t = curved^2 and t_p = 1 - slope, above the pivot t = t_p + slope * (u - p), so vanilla 1 - exp(-u) is
// 1 - slope * exp(-(t - t_p) / slope). Exact for the materials; anything else above 1 (emissive, particles) lands just
// under 1, near vanilla's UNORM clip. Per channel, like the curve.
float4 YRC_Y5VanillaMaterialCurve(float4 curved)
{
   const float4 t = sqr(max(0.0, curved));
   const float tPivot = 1.0 - YRC_Y5_MATERIAL_CURVE_SLOPE;
   return sqrt(t > tPivot ? 1.0 - YRC_Y5_MATERIAL_CURVE_SLOPE * exp((tPivot - t) / YRC_Y5_MATERIAL_CURVE_SLOPE) : t);
}

// A bilinear sample of a texture that vanilla stored as 8-bit UNORM (every texel clamped to [0,1] before filtering) and
// that is fp16 now: saturates the four texels, then filters. Sample() would blend values above 1 first. Mip 0; the sampler
// only sets the footprint's addressing, the filter is assumed linear. `y5MaterialCurve` (a Y5R scene source) maps the
// color texels back through the vanilla material curve instead, as vanilla stored them there.
float4 SampleSaturatedBilinear(Texture2D<float4> tex, SamplerState s, float2 uv, bool y5MaterialCurve = false)
{
   float2 size;
   tex.GetDimensions(size.x, size.y);
   const float2 f = frac(uv * size - 0.5);
   // Rows are the channels; Gather returns the texels (-,+), (+,+), (+,-), (-,-).
   float4 r = tex.GatherRed(s, uv), g = tex.GatherGreen(s, uv), b = tex.GatherBlue(s, uv);
   r = y5MaterialCurve ? YRC_Y5VanillaMaterialCurve(r) : saturate(r);
   g = y5MaterialCurve ? YRC_Y5VanillaMaterialCurve(g) : saturate(g);
   b = y5MaterialCurve ? YRC_Y5VanillaMaterialCurve(b) : saturate(b);
   const float4x4 texels = float4x4(r, g, b, saturate(tex.GatherAlpha(s, uv)));
   return mul(texels, float4((1.0 - f.x) * f.y, f.x * f.y, f.x * (1.0 - f.y), (1.0 - f.x) * (1.0 - f.y)));
}

// The glow downsamples clamp their source like vanilla only on the draws the addon flags (LumaData.CustomData1):
// Y3R's downsample hash 0x54A5E7AC is also Y4R's ps_cubic resample, which must stay unclamped. `y5MaterialCurve`: see
// SampleSaturatedBilinear.
float4 SampleGlowSource(Texture2D<float4> tex, SamplerState s, float2 uv, bool y5MaterialCurve = false)
{
   return LumaData.CustomData1 != 0u ? SampleSaturatedBilinear(tex, s, uv, y5MaterialCurve) : tex.Sample(s, uv);
}