// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 2: the VP6 video pass (in-world video logs and full screen videos, dgVoodoo 2.87.6 -> ps_5_0 0xD49CB77E), BT.601 limited
// range Y'CbCr -> R'G'B', alpha blended onto the frame after the SMAA neighborhood pass, i.e. over the display mapped scene (UI side).
// Transcribed from the translation, then the vanilla clamp (the 8 bit frame clipped the limited range undershoot, the fp16 mirror
// doesn't), a light PumboAutoHDR and the UI_DRAW_TYPE 2 pre-scale (the video at the game paper white, as Mirror's Edge's).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.x the planes' mip bias, c1 the planes' UV scales (Y' xy, chroma zw)
}

Texture2D<float4> t0 : register(t0); // The frame mask (alpha)
Texture2D<float4> t1 : register(t1); // Y'
Texture2D<float4> t2 : register(t2); // Cb
Texture2D<float4> t3 : register(t3); // Cr
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);
SamplerState s3_s : register(s3);

void main(DGV_MAIN_SIGNATURE_CENTROID)
{
   const float4 uv = cb4[9] * v5.xyxy;
   const float y = ApplyDgvMask(t1.SampleBias(s1_s, uv.xy, cb4[8].x), cb3[46], cb3[47]).x - 0.0625;
   const float cb = ApplyDgvMask(t2.SampleBias(s2_s, uv.zw, cb4[8].x), cb3[48], cb3[49]).x - 0.5;
   const float cr = ApplyDgvMask(t3.SampleBias(s3_s, uv.zw, cb4[8].x), cb3[50], cb3[51]).x - 0.5;
   float3 rgb = float3(dot(float2(1.164383, 1.596027), float2(y, cr)), dot(float3(1.164383, -0.391762, -0.812968), float3(y, cb, cr)), dot(float2(1.164383, 2.017232), float2(y, cb)));

   rgb = saturate(rgb);
   rgb = gamma_to_linear(rgb);
   rgb = VideoAutoHDR(rgb, LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5, LumaSettings.GameSettings.VideoAutoHDRBoost, LumaSettings.GamePaperWhiteNits);
   // LumaData.CustomData3 != 0: over the display mapped frame (UI side). Else an in-world monitor the display map later maps with the scene.
   rgb = linear_to_gamma(rgb * (LumaData.CustomData3 != 0 ? GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true) : 1.0));

   const float4 tint = v2 * ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);
   // The unorm frame clamped the output before blending: the in-world monitors' tint above 1 clipped at 1 (keep the AutoHDR level,
   // identical below 1), an alpha above 1 would make the inverse source alpha negative
   o0 = float4(min(rgb * tint.rgb, max(rgb, 1.0)), saturate(tint.a));
}
