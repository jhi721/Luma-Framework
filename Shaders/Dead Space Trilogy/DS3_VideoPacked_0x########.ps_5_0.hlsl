// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x8AE909CA.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 3: the video pass with one packed Y'CbCr volume (blob 7d31cb074543; dgVoodoo 2.87.6 -> ps_5_0 0x8AE909CA, 2.81.3 ->
// ps_4_0 0xBA227CA7), BT.601 limited range Y'CbCr -> R'G'B' with 3 decimal coefficients, alpha 1. Transcribed from the translation,
// then the vanilla clamp (the 8 bit frame clipped the limited range undershoot, the fp16 mirror doesn't), a light PumboAutoHDR and the
// UI_DRAW_TYPE 2 pre-scale (the video at the game paper white).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.x lodBias (the volume's mip bias)
}

Texture3D<float4> t32 : register(t32); // YUVTexture: Y' in x, Cb in y, Cr in w
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   const float4 yuv = ApplyDgvMask(t32.SampleBias(s0_s, v5.xyz, cb4[8].x), cb3[44], cb3[45]);
   const float y = yuv.x - 0.0625;
   const float cb = yuv.y - 0.5;
   const float cr = yuv.w - 0.5;
   float3 rgb = float3(1.164 * y + 1.596 * cr, 1.164 * y - 0.391 * cb - 0.813 * cr, 1.164 * y + 2.018 * cb);

   rgb = saturate(rgb);
   rgb = gamma_to_linear(rgb);
   rgb = VideoAutoHDR(rgb, LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   // LumaData.CustomData3 != 0: after the post chain, in Luma's UI layer. Else an in-world screen the display map later maps with the scene.
   rgb = linear_to_gamma(rgb * (LumaData.CustomData3 != 0 ? GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true) : 1.0));

   o0 = float4(rgb, 1.0);
}
