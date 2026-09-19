// Mass Effect 3 (2012) - GFx multiply-blend vertex colour lerped toward the texture by COLOR1.z, extra alpha COLOR1.w.
// See Includes/GFxUI.hlsl: vanilla body plus the alpha clamp.
#include "Includes/GFxUI.hlsl"

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   const float4 texel = ApplyDgvMask(t0.Sample(s0_s, v5.xy), 0);
   o0 = GFxMultiplyOutput(lerp(v2, texel, v3.z), v3.w);
}
