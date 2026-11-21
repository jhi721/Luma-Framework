// The scene is gamma 2.2 encoded (1 = SDR white, the values above it kept by the fp16 mirror), the upscalers' HDR mode takes linear
// light (DLSS-Best-Practices COL-1, FSR-Best-Practices FIN-2): decoded before them, encoded back after. Mirrored, so negative
// values survive the round trip.

#include "../Includes/Color.hlsl"

Texture2D<float4> source : register(t0);

float4 decode_ps(float4 position : SV_Position) : SV_Target
{
   return float4(gamma_to_linear(source.Load(int3(position.xy, 0)).rgb, GCT_MIRROR), 1.0);
}

float4 encode_ps(float4 position : SV_Position) : SV_Target
{
   return float4(linear_to_gamma(source.Load(int3(position.xy, 0)).rgb, GCT_MIRROR), 1.0);
}
