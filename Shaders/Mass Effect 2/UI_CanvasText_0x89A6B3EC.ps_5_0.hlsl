// Mass Effect 2 (2010) UE3 Canvas text: colour from the vertex colour, alpha from the glyph texture's red channel.
// See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// This shader reads COLOR0 (v2) and TEXCOORD0 (v5.xy).
void main(ME2_MAIN_SIGNATURE)
{
   const float4 glyph = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0);
   o0 = saturate(ME2_CanvasTransform(float4(v2.rgb, glyph.r * v2.a)));
}
