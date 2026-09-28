// The ceiling or floor of the additive and subtractive draws onto the gamma-encoded HDR output (the vint UI's additive,
// additive_alpha and subtractive render modes, the rl_bokeh_sprite_01 sprites). Vanilla's UNORM swapchain clipped their result to
// [0,1]; drawn after them with blend op MIN or MAX from a copy of the target taken before them, each channel ends at
// min(after, max(before, 1)) or max(after, min(before, 0)). They still brighten a pixel up to paper white or darken it down to
// black, but no longer add on top of an HDR highlight or go negative.

Texture2D<float4> base : register(t0); // the target before the draw

float4 additive_limit_ps(float4 pos : SV_Position) : SV_Target
{
   return float4(max(base.Load(int3(pos.xy, 0)).rgb, 1.0), 1.0);
}

float4 subtractive_limit_ps(float4 pos : SV_Position) : SV_Target
{
   return float4(min(base.Load(int3(pos.xy, 0)).rgb, 0.0), 0.0);
}
