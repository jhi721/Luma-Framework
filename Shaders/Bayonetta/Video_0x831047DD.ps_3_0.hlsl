// Bayonetta Sofdec movie pass "smp_video_yuv_rgb_conv_shader" (raw ps_3_0 CRC32
// 0x831047DD = dgVoodoo 2.87.5 0xC60649FD): BT.601 limited range Y'CbCr (the
// planes' alpha channel, s0 Y', s1 Cb, s2 Cr) to R'G'B', gamma encoded, 1 = UI
// paper white. Luma: the vanilla clamp (the 8-bit canvas gave it, the fp16 one
// does not) and a light PumboAutoHDR (TW1's, SDR movies): linearize with
// gamma 2.2, expand highlights, encode again. g_LumaVideo.x = enable, .y =
// AutoHDRMaxWhite (CPU, see the register block); disabled = the vanilla
// conversion plus the clamp. Shoulder 2.75 and saturation expansion 0.2 are
// PumboAutoHDR's defaults.
#include "Includes/Luma_Block_sm3.hlsli"

sampler2D s_texture_y : register(s0);
sampler2D s_texture_u : register(s1);
sampler2D s_texture_v : register(s2);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
   const float y_prime = tex2D(s_texture_y, uv).w - 0.0625;
   const float cb = tex2D(s_texture_u, uv).w - 0.5;
   const float cr = tex2D(s_texture_v, uv).w - 0.5;
   float3 rgb;
   rgb.r = y_prime * 1.164 + cr * 1.596;
   rgb.g = y_prime * 1.164 + (cb * -0.392 - cr * 0.813);
   rgb.b = y_prime * 1.164 + cb * 2.017;
   rgb = saturate(rgb);
   [branch] if (g_LumaVideo.x > 0.5)
   {
      const float3 linear_rgb = pow(abs(rgb), 2.2);
      const float extra = g_LumaVideo.y - 1.0;
      const float ratio = max(linear_rgb.r, max(linear_rgb.g, linear_rgb.b));
      const float single_scale =
          (ratio == 0.0) ? 1.0
                         : (ratio + pow(saturate(ratio), 2.75) * extra) / ratio;
      const float3 channel_total =
          linear_rgb + pow(saturate(linear_rgb), 2.75) * extra;
      const float3 channel_scale =
          float3((linear_rgb.r == 0.0) ? 1.0 : channel_total.r / linear_rgb.r,
                 (linear_rgb.g == 0.0) ? 1.0 : channel_total.g / linear_rgb.g,
                 (linear_rgb.b == 0.0) ? 1.0 : channel_total.b / linear_rgb.b);
      rgb = pow(abs(linear_rgb * lerp(single_scale, channel_scale, 0.2)), 1.0 / 2.2);
   }
   return float4(rgb, 1.0);
}
