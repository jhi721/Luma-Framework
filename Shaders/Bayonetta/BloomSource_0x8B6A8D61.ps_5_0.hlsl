// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// Bayonetta bloom source "filter05" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x8B6A8D61): the scene into the fixed size 320x176
// bloom target, rgb * 2 and alpha = (a * c192.x + Y' - c192.y) * c192.w (the threshold the blurs multiply in). Vanilla reads one
// bilinear tap per target texel: 4x reduction at 720p, 12x at 4K, where small highlights flicker and drop out of the bloom.
// In Luma Bloom's chain (main.cpp "DrawBloomPass", LumaData.CustomData1 1) it averages the outputs of every source texel under the
// target texel. Each tap is clamped as the 8-bit scene stored it: the scene is fp16 and HDR now, and the bloom stays vanilla bounded.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: c192 (.x alpha weight, .y threshold, .w scale)
}

Texture2D<float4> t0 : register(t0); // scene
SamplerState s0_s : register(s0);

// The vanilla output of one source sample, clamped as the 8-bit target stores it
float4 BloomSource(float4 sample)
{
   const float4 scene = saturate(ApplyDgvMask(sample, cb3[44], cb3[45]));
   return saturate(float4(scene.rgb + scene.rgb, (scene.a * cb4[192].x + dot(scene.rgb, kLumaWeights) - cb4[192].y) * cb4[192].w));
}

void main(DGV_MAIN_SIGNATURE)
{
   [branch] if (LumaData.CustomData1 == 1)
   {
      float2 size;
      t0.GetDimensions(size.x, size.y);
      // The source texels under one target texel (4 x 4.1 from the 4K scene into Luma Bloom's 960x528), one tap each. The mean of
      // the clamped outputs, not the output of the mean: the threshold's floor at 0 would let a dark part cancel a bright one.
      const float2 footprint = float2(abs(ddx(v5.x)), abs(ddy(v5.y))) * size;
      const int2 taps = clamp(int2(ceil(footprint)), 1, 16);
      const float2 step = footprint / (taps * size);
      const float2 first = v5.xy - 0.5 * (taps - 1) * step;
      float4 sum = 0.0;
      for (int y = 0; y < taps.y; y++)
      {
         for (int x = 0; x < taps.x; x++)
         {
            sum += BloomSource(t0.SampleLevel(s0_s, first + float2(x, y) * step, 0.0));
         }
      }
      o0 = sum / (taps.x * taps.y);
   }
   else
   {
      o0 = BloomSource(t0.Sample(s0_s, v5.xy));
   }
}
