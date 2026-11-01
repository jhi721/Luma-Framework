// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// Bayonetta "filter03" (dgVoodoo 2.87.5 -> ps_5_0 0x68B566FE): the auto exposure's input, an 8x8 average of the previous frame over
// UV 0.3..0.65, read back by the CPU as the luma of one 8-bit texel (the game's auto exposure). The frame is fp16 and HDR now: each
// sample is clamped as the 8-bit frame stored it, so values above 1 don't lower the exposure. "Exposure" (on the grades' gain) is taken
// back out first, or the auto exposure, which adds to that same gain, would cancel it within a few frames (the bloom composited after
// the grade is divided too: close enough for a feedback loop's input).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   // As the grades apply it (encoded: its linear scale through the 2.2 decode)
   const float4 exposure_rcp = float4((1.0 / pow(LumaSettings.GameSettings.Exposure, 1.0 / DefaultGamma)).xxx, 1.0);
   float4 sum = 0.0;
   [unroll] for (int y = 0; y < 8; y++)
   {
      [unroll] for (int x = 0; x < 8; x++)
      {
         sum += saturate(ApplyDgvMask(t0.Sample(s0_s, float2(0.3 + x * 0.05, 0.3 + y * 0.05)), cb3[44], cb3[45]) * exposure_rcp);
      }
   }
   o0 = sum / 64.0;
}
