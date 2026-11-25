// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x94EDB896.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 3: the video pass with 2D Y'/Cb/Cr planes (blob 3a36f472f053; dgVoodoo 2.87.6 -> ps_5_0 0x94EDB896, 2.81.3 -> ps_4_0
// 0xC747B9DD), BT.601 limited range Y'CbCr -> R'G'B', alpha 0. Transcribed from the translation, then the vanilla clamp (the 8 bit
// frame clipped the limited range undershoot, the fp16 mirror doesn't), a light PumboAutoHDR and the UI_DRAW_TYPE 2 pre-scale (the
// video at the game paper white).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

Texture2D<float4> t0 : register(t0); // gTexture_Y (Y')
Texture2D<float4> t1 : register(t1); // gTexture_Cr
Texture2D<float4> t2 : register(t2); // gTexture_Cb
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

void main(DGV_MAIN_SIGNATURE)
{
   const float y = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).x - 0.0625;
   const float cr = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]).x - 0.5;
   const float cb = ApplyDgvMask(t2.Sample(s2_s, v5.xy), cb3[48], cb3[49]).x - 0.5;
   float3 rgb = float3(dot(float2(1.164383, 1.596027), float2(y, cr)), dot(float3(1.164383, -0.391762, -0.812968), float3(y, cb, cr)), dot(float2(1.164383, 2.017232), float2(y, cb)));

   rgb = saturate(rgb);
   rgb = gamma_to_linear(rgb);
   rgb = VideoAutoHDR(rgb, LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   // LumaData.CustomData3 != 0: after the post chain, in Luma's UI layer. Else an in-world screen the display map later maps with the scene.
   rgb = linear_to_gamma(rgb * (LumaData.CustomData3 != 0 ? GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true) : 1.0));

   o0 = float4(rgb, 0.0);
}
