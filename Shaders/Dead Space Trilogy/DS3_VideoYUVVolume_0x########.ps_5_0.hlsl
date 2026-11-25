// Hashes registered from C++ ("redirected_shader_hashes"): originally 0xA4AEB579.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 3: the video pass with Y'/U/V volume planes (blob cadd80485581; dgVoodoo 2.87.6 -> ps_5_0 0xA4AEB579, 2.81.3 -> ps_4_0
// 0x61AED491), BT.601 limited range Y'CbCr -> R'G'B', alpha 1. Transcribed from the translation, then the vanilla clamp (the 8 bit
// frame clipped the limited range undershoot, the fp16 mirror doesn't), a light PumboAutoHDR and the UI_DRAW_TYPE 2 pre-scale (the
// video at the game paper white).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.x lodBias (the planes' mip bias), c1 YUV_UVscale
}

Texture3D<float4> t32 : register(t32); // YTexture (Y')
Texture3D<float4> t33 : register(t33); // UTexture (Cb)
Texture3D<float4> t34 : register(t34); // VTexture (Cr)
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

void main(DGV_MAIN_SIGNATURE)
{
   // As the D3D9 original: Y' and U at YUV_UVscale.zw, V at YUV_UVscale.xy
   const float3 uv_yu = float3(cb4[9].zw * v5.xy, v5.z);
   const float3 uv_v = float3(cb4[9].xy * v5.xy, v5.z);
   const float y = ApplyDgvMask(t32.SampleBias(s0_s, uv_yu, cb4[8].x), cb3[44], cb3[45]).x - 0.0625;
   const float cb = ApplyDgvMask(t33.SampleBias(s1_s, uv_yu, cb4[8].x), cb3[46], cb3[47]).x - 0.5;
   const float cr = ApplyDgvMask(t34.SampleBias(s2_s, uv_v, cb4[8].x), cb3[48], cb3[49]).x - 0.5;
   float3 rgb = float3(dot(float2(1.164383, 1.596027), float2(y, cr)), dot(float3(1.164383, -0.391762, -0.812968), float3(y, cb, cr)), dot(float2(1.164383, 2.017232), float2(y, cb)));

   rgb = saturate(rgb);
   rgb = gamma_to_linear(rgb);
   rgb = VideoAutoHDR(rgb, LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   // LumaData.CustomData3 != 0: after the post chain, in Luma's UI layer. Else an in-world screen the display map later maps with the scene.
   rgb = linear_to_gamma(rgb * (LumaData.CustomData3 != 0 ? GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true) : 1.0));

   o0 = float4(rgb, 1.0);
}
