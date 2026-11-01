// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// MGR:R bloom source "filter05" (shader2.dat, dgVoodoo 2.87.6 -> ps_5_0 0x0A08457E): two scene taps 2 * g_AddColor.zw (c185 =
// cb4[193]) apart, rgb premultiplied by alpha (the emissive mask), their max blended over the raw second tap by its alpha, then
// rgb * 2 times the threshold saturate((max channel + Y') / 2 - lerp(c184.x, c184.z, a)) * lerp(c184.y, c184.w, a) (c184 = cb4[192]).
// Transcribed from the translated disassembly. Into the 8-bit 320x176 bloom target, from the scene and then from each level into the
// next. The scene is fp16 now: HDR colour and additive alpha above 1 (the mask multiplies rgb), so each tap is clamped as the 8-bit
// scene stored it and the bloom stays vanilla bounded.
// In Luma Bloom's chain (main.cpp "DrawBloomPass", LumaData.CustomData1 1) it averages the outputs centred on every source texel under
// the target texel: vanilla's 2 taps per texel (12x reduction at 4K) let small highlights flicker and drop out of the bloom.

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

// The vanilla output from its two taps
float4 BloomSource(float4 firstSample, float4 secondSample)
{
   const float4 first = saturate(ApplyDgvMask(firstSample, cb3[44], cb3[45]));
   const float4 second = saturate(ApplyDgvMask(secondSample, cb3[44], cb3[45]));
   const float4 brightest = max(float4(second.rgb * second.a, second.a), float4(first.rgb * first.a, first.a));
   const float4 color = lerp(second, brightest, brightest.a);
   const float level = (max3(color.rgb) + dot(color.rgb, kLumaWeights)) * 0.5;
   const float2 threshold = lerp(cb4[192].xy, cb4[192].zw, color.a);
   return float4(saturate((level - threshold.x) * threshold.y) * (color.rgb + color.rgb), color.a);
}

void main(DGV_MAIN_SIGNATURE)
{
   const float2 offset = 2.0 * cb4[193].zw;
   [branch] if (LumaData.CustomData1 == 1)
   {
      float2 size;
      t0.GetDimensions(size.x, size.y);
      // The source texels under one target texel (4 x 4.1 from the 4K scene into Luma Bloom's 960x528), one output each. The mean of
      // the clamped outputs, not the output of the mean: the threshold's floor at 0 would let a dark part cancel a bright one.
      const float2 footprint = float2(abs(ddx(v5.x)), abs(ddy(v5.y))) * size;
      const int2 taps = clamp(int2(ceil(footprint)), 1, 16);
      const float2 step = footprint / (taps * size);
      const float2 firstTap = v5.xy - 0.5 * (taps - 1) * step;
      float4 sum = 0.0;
      for (int y = 0; y < taps.y; y++)
      {
         for (int x = 0; x < taps.x; x++)
         {
            const float2 uv = firstTap + float2(x, y) * step;
            sum += BloomSource(t0.SampleLevel(s0_s, uv - offset, 0.0), t0.SampleLevel(s0_s, uv + offset, 0.0));
         }
      }
      o0 = sum / (taps.x * taps.y);
   }
   else
   {
      o0 = BloomSource(t0.Sample(s0_s, v5.xy - offset), t0.Sample(s0_s, v5.xy + offset));
   }
}
