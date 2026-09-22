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

// A bilinear sample of a texture that vanilla stored as 8-bit UNORM (every texel clamped to [0,1] before filtering) and
// that is fp16 now: saturates the four texels, then filters. Sample() would blend values above 1 first. Mip 0; the sampler
// only sets the footprint's addressing, the filter is assumed linear.
float4 SampleSaturatedBilinear(Texture2D<float4> tex, SamplerState s, float2 uv)
{
   float2 size;
   tex.GetDimensions(size.x, size.y);
   const float2 f = frac(uv * size - 0.5);
   // Rows are the channels; Gather returns the texels (-,+), (+,+), (+,-), (-,-).
   const float4x4 texels = float4x4(saturate(tex.GatherRed(s, uv)), saturate(tex.GatherGreen(s, uv)), saturate(tex.GatherBlue(s, uv)), saturate(tex.GatherAlpha(s, uv)));
   return mul(texels, float4((1.0 - f.x) * f.y, f.x * f.y, f.x * (1.0 - f.y), (1.0 - f.x) * (1.0 - f.y)));
}

// The glow downsamples clamp their source like vanilla only on the draws the addon flags (LumaData.CustomData1): Y3's
// downsample hash is also Y4's ps_cubic resample, which must stay unclamped.
float4 SampleGlowSource(Texture2D<float4> tex, SamplerState s, float2 uv)
{
   return LumaData.CustomData1 != 0u ? SampleSaturatedBilinear(tex, s, uv) : tex.Sample(s, uv);
}