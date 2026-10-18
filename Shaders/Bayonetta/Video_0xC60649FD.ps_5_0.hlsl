// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings (VideoAutoHDR*) before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Bayonetta Sofdec movie pass ("smp_video_yuv_rgb_conv_shader", dgVoodoo 2.87.5 -> ps_5_0 0xC60649FD), BT.601 limited range
// Y'CbCr -> R'G'B' straight onto the canvas. Transcribed from the translated disassembly (dgVoodoo reads the planes from .w), then the
// vanilla clamp (the 8-bit canvas gave it, the fp16 mirror doesn't) and a light PumboAutoHDR (TW1's). The canvas is gamma encoded
// with 1 = UI paper white (UI_DRAW_TYPE 2: the composition scales the canvas by it); main.cpp keeps the present blit from clamping
// the AutoHDR back.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo's texture format emulation masks, one (and, or) pair per sampler, s0 at 44/45
}

Texture2D<float4> t0 : register(t0); // Y'
Texture2D<float4> t1 : register(t1); // Cb
Texture2D<float4> t2 : register(t2); // Cr

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

void main(DGV_MAIN_SIGNATURE)
{
   const float y_prime = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).w - 0.0625;
   const float cb = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]).w - 0.5;
   const float cr = ApplyDgvMask(t2.Sample(s2_s, v5.xy), cb3[48], cb3[49]).w - 0.5;
   float3 rgb;
   rgb.r = y_prime * 1.164 + cr * 1.596;
   rgb.g = y_prime * 1.164 + (cb * -0.392 - cr * 0.813);
   rgb.b = y_prime * 1.164 + cb * 2.017;

   rgb = saturate(rgb);
   [branch] if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      rgb = linear_to_gamma(VideoAutoHDR(gamma_to_linear(rgb), true, LumaSettings.GameSettings.VideoAutoHDRBoost, UIPaperWhiteNits));
   }
   o0 = float4(rgb, 1.0);
}
