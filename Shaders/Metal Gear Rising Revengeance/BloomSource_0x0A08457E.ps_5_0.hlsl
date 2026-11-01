#include "../Includes/DgVoodoo.hlsl"
#include "Includes/Common.hlsl"

// MGR:R bloom source "filter05" (shader2.dat, dgVoodoo 2.87.6 -> ps_5_0 0x0A08457E): two scene taps 2 * g_AddColor.zw (c185 =
// cb4[193]) apart, rgb premultiplied by alpha (the emissive mask), their max blended over the raw second tap by its alpha, then
// rgb * 2 times the threshold saturate((max channel + Y') / 2 - lerp(c184.x, c184.z, a)) * lerp(c184.y, c184.w, a) (c184 = cb4[192]).
// Transcribed from the translated disassembly. Into the 8-bit 320x176 bloom target. The scene is fp16 now: HDR colour and additive
// alpha above 1 (the mask multiplies rgb), so each tap is clamped as the 8-bit scene stored it and the bloom stays vanilla bounded.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0); // scene
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   const float2 offset = 2.0 * cb4[193].zw;
   const float4 first = saturate(ApplyDgvMask(t0.Sample(s0_s, v5.xy - offset), cb3[44], cb3[45]));
   const float4 second = saturate(ApplyDgvMask(t0.Sample(s0_s, v5.xy + offset), cb3[44], cb3[45]));
   const float4 brightest = max(float4(second.rgb * second.a, second.a), float4(first.rgb * first.a, first.a));
   const float4 color = lerp(second, brightest, brightest.a);
   const float level = (max3(color.rgb) + dot(color.rgb, kLumaWeights)) * 0.5;
   const float2 threshold = lerp(cb4[192].xy, cb4[192].zw, color.a);
   o0.rgb = saturate((level - threshold.x) * threshold.y) * (color.rgb + color.rgb);
   o0.a = color.a;
}
