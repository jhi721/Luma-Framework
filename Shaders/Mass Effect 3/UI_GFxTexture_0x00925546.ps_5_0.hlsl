// Mass Effect 3 (2012) - GFx textured fill. See Includes/GFxUI.hlsl: vanilla body plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   o0 = GFxOutput(ApplyDgvMask(t0.Sample(s0_s, v5.xy), 0), 1.0);
}
