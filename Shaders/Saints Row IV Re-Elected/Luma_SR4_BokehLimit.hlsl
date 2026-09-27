// The ceiling of the additive bokeh sprites (rl_bokeh_sprite_01, blend ONE + ONE onto the gamma-encoded HDR output). Drawn after
// them with blend op MIN, so each channel ends at min(with the sprites, max(before them, 1)). Vanilla's UNORM target clipped
// the sum at 1: a sprite still brightens a pixel up to paper white, but no longer adds on top of an HDR highlight.

Texture2D<float4> base : register(t0); // the target before the sprites

float4 bokeh_limit_ps(float4 pos : SV_Position) : SV_Target
{
   return float4(max(base.Load(int3(pos.xy, 0)).rgb, 1.0), 1.0);
}
