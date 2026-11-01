// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// Vanquish exposure meter "filter03" (shader.dat, dgVoodoo 2.87.6 -> ps_5_0 0xEA483785): 8 x 9 taps 0.05 apart from (0.3, 0.3), summed
// and divided by 64, into an 8x8 R8G8B8A8 target the CPU reads back. Transcribed from the translated disassembly. Its source is fp16
// now (HDR colour, alpha above 1 from additive draws): each tap is clamped as the 8-bit target stored it, so the average and the
// game's exposure stay vanilla.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   float4 sum = 0.0;
   for (int y = 0; y < 9; y++)
   {
      for (int x = 0; x < 8; x++)
      {
         sum += saturate(ApplyDgvMask(t0.Sample(s0_s, float2(0.3 + 0.05 * x, 0.3 + 0.05 * y)), cb3[44], cb3[45]));
      }
   }
   o0 = sum * 0.015625;
}
