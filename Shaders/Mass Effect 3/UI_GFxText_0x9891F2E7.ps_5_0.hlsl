// Mass Effect 3 (2012) - GFx glyphs: vertex colour, coverage from the atlas red channel. See Includes/GFxUI.hlsl:
// vanilla body plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   const float coverage = ApplyDgvMask(t0.Sample(s0_s, v5.xy), 0).x;
   o0 = GFxOutput(float4(v2.rgb, coverage * v2.a), 1.0);
}
