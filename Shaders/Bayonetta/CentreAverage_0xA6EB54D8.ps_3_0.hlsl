// Bayonetta "filter03" (raw ps_3_0 CRC32 0xA6EB54D8 = dgVoodoo 2.87.5
// 0x68B566FE): the auto exposure's input, the mean of an 8x8 grid of the
// previous frame over UV 0.3..0.65 (step 0.05), read back by the CPU from an
// 8x8 A8R8G8B8 target (it must stay 8-bit). Gamma encoded R'G'B'. Luma: the
// frame is fp16 and HDR, so each tap is clamped as the 8-bit frame stored it,
// after "Exposure" (the encoded gain in c208.x, applied on the grades'
// g_MatrialColor) is taken back out, or the auto exposure would cancel it.
// Vanilla (c208.x = 1, taps <= 1) is the original sum / 64.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D Texture : register(s0);

float4 main() : COLOR0
{
   const float4 exposure_rcp = float4((1.0 / g_LumaGrade.x).xxx, 1.0);
   float4 sum = 0.0;
   [loop] for (int y = 0; y < 8; y++)
   {
      [loop] for (int x = 0; x < 8; x++)
      {
         sum += saturate(
             tex2Dlod(Texture, float4(0.3 + x * 0.05, 0.3 + y * 0.05, 0.0, 0.0)) *
             exposure_rcp);
      }
   }
   return sum * (1.0 / 64.0);
}
