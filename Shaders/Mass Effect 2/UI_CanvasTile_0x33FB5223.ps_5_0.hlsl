// Mass Effect 2 (2010) UE3 Canvas textured tile: all four channels come from the texture. See Includes/CanvasUI.hlsl -
// vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// This shader reads only TEXCOORD0 (v5.xy).
void main(ME2_MAIN_SIGNATURE)
{
   const float4 tile = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0);
   o0 = saturate(ME2_CanvasTransform(tile));
}
