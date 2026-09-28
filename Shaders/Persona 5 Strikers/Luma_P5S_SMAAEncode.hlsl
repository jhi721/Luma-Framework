// Gamma encoded copy of the linear canvas for RCAS and SMAA edge detection (its thresholds assume perceptual input).
// HDR values above 1 take the same power curve; GCT_MIRROR mirrors negatives around 0.
#include "../Includes/Color.hlsl"

Texture2D<float4> linear_in : register(t0);
RWTexture2D<float4> encoded : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const float4 c = linear_in.Load(int3(id.xy, 0));
   encoded[id.xy] = float4(linear_to_gamma(c.rgb, GCT_MIRROR), c.a);
}
