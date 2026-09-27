// rl_distortion_01: offsets the scene by the distortion map and blends in the blurred scene by its z. Its decode (vanilla's
// -1.003922, the 1 + 1/255 bias) centres the offset on the 8-bit 128/255, so the map is read as its 8-bit target stored it (see
// "SR4_SampleDistortionMap"): Luma's FP16 upgrade would store the writers' 0.5 neutral as exactly 0.5 and shift the whole screen.
#include "Includes/Common.hlsl"

cbuffer vc0 : register(b0)
{
   float Blur_scale : packoffset(c0);
   float3 Distortion_scale : packoffset(c1);
}

cbuffer vc4 : register(b4)
{
   float4 Tint_color : packoffset(c1);
}

SamplerState base_samplerSampler : register(s0);
SamplerState blurred_samplerSampler : register(s2);
Texture2D<float4> base_samplerTexture : register(t0);
Texture2D<float4> distortion_samplerTexture : register(t1);
Texture2D<float4> blurred_samplerTexture : register(t2);

void main(float4 pos : SV_Position, float2 texcoord : TEXCOORD0, out float4 o0 : SV_Target0)
{
   const float3 distortion = SR4_SampleDistortionMap(distortion_samplerTexture, texcoord).xyz * Tint_color.xyz;
   const float2 uv = (distortion.xy * 2.0 - (1.0 + 1.0 / 255.0)) * Distortion_scale.xy + texcoord;
   const float blur = min(distortion.z * Blur_scale, 1.0);
   const float4 base = base_samplerTexture.Sample(base_samplerSampler, uv);
   o0 = float4(lerp(base.rgb, blurred_samplerTexture.Sample(blurred_samplerSampler, uv).rgb, blur), base.a);
}
