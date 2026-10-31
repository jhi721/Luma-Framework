// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings (VideoAutoHDR*) before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// MGR:R CRI movie pass "hwshaderpftyuv" (shader2.dat, dgVoodoo 2.87.5 -> ps_5_0 0x43A3099D without alpha test; the GREATEREQUAL /
// GREATER translations and "hwshaderpftyuva", which adds an alpha plane, forward here): BT.601 limited range Y'CbCr -> R'G'B' (dgVoodoo
// reads the planes from .w) times g_MatrialColor (c62 = cb4[70], the fade). Transcribed from the translated disassembly. In game the
// cutscenes draw it into the canvas, the title screen's movie into the scene target before the UI (the alpha plane variant).
// Then the vanilla clamp (the 8-bit targets gave it: below 0 the conversion's undershoot, the fp16 mirrors kept it) and, in HDR, a
// light PumboAutoHDR. The targets are gamma encoded with 1 = UI paper white (UI_DRAW_TYPE 2: the composition scales the canvas by it).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: one (and, or) mask pair per sampler from [44], [8].z the alpha test reference (uint)
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0); // Y'
Texture2D<float4> t1 : register(t1); // Cb
Texture2D<float4> t2 : register(t2); // Cr
Texture2D<float4> t3 : register(t3); // alpha (VIDEO_ALPHA_PLANE)

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);
SamplerState s3_s : register(s3);

// 1: "hwshaderpftyuva" (Video_0x71B4A727), alpha from a fourth plane instead of 1
#ifndef VIDEO_ALPHA_PLANE
#define VIDEO_ALPHA_PLANE 0
#endif
// 0 none, 1 GREATEREQUAL, 2 GREATER
#ifndef VIDEO_ALPHA_TEST
#define VIDEO_ALPHA_TEST 0
#endif

void main(DGV_MAIN_SIGNATURE)
{
   const float y_prime = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).w - 0.0625;
   const float cb = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]).w - 0.5;
   const float cr = ApplyDgvMask(t2.Sample(s2_s, v5.xy), cb3[48], cb3[49]).w - 0.5;
   float4 color;
   color.r = y_prime * 1.164 + cr * 1.596;
   color.g = y_prime * 1.164 + (cb * -0.392 - cr * 0.813);
   color.b = y_prime * 1.164 + cb * 2.017;
#if VIDEO_ALPHA_PLANE
   color.a = ApplyDgvMask(t3.Sample(s3_s, v5.xy), cb3[50], cb3[51]).w;
#else
   color.a = 1.0;
#endif
   o0 = color * cb4[70];

#if VIDEO_ALPHA_TEST
   const uint alpha = uint(o0.a * 255.0 + 0.0001);
   const uint reference = asuint(cb3[8].z);
#if VIDEO_ALPHA_TEST == 1
   if (alpha < reference)
#else
   if (alpha <= reference)
#endif
   {
      discard;
   }
#endif

   o0 = saturate(o0);
   [branch] if (LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      o0.rgb = linear_to_gamma(VideoAutoHDR(gamma_to_linear(o0.rgb), true, LumaSettings.GameSettings.VideoAutoHDRBoost, UIPaperWhiteNits));
   }
}
