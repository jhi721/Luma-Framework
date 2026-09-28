// Mass Effect 3 (2012) - GFx two-texture fill: t1 lerped toward t0 by COLOR1.z. See Includes/GFxUI.hlsl: vanilla body
// plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1);
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   const float4 texel0 = ApplyDgvMask(t0.Sample(s0_s, v5.xy), 0);
   const float4 texel1 = ApplyDgvMask(t1.Sample(s1_s, v6.xy), 1);
   o0 = GFxOutput(lerp(texel1, texel0, v3.z), 1.0);
}
