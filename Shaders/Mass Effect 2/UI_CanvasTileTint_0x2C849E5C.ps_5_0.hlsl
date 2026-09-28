// Mass Effect 2 (2010) UE3 Canvas tile with a tint and a fade: the sampled tile is lerped toward the vertex colour
// by COLOR1.z, then alpha is scaled by COLOR1.w. See Includes/CanvasUI.hlsl.
#include "Includes/CanvasUI.hlsl"
Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// This shader reads COLOR0 (v2), COLOR1.zw (v3.zw) and TEXCOORD0 (v5.xy).
void main(ME2_MAIN_SIGNATURE)
{
   const float4 tile = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0);
   // `add r0, r0, -v2` then `mad r0, v3.zzzz, r0, v2` - a lerp from the vertex colour to the tile.
   const float4 c = (lerp(v2, tile, v3.z) * CanvasMul) + CanvasAdd;
   // This perm is the one that carries a `mov_sat` before the gamma pow, so the saturate on RGB is vanilla's own.
   o0 = saturate(float4(PowUE3(saturate(c.rgb), CanvasGamma.xxx), c.w * v3.w));
}
