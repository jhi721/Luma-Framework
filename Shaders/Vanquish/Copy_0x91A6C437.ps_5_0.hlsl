// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings (VideoAutoHDR*) before the shared Settings.hlsl, keep FIRST
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// Vanquish "filtershadercopytex" (shader2.dat, dgVoodoo 2.87.6 -> ps_5_0 0x91A6C437, MGR:R's and Bayonetta's hash): t0 * g_MatrialColor
// (c184 = cb4[192]). It runs in the bloom chain, after the gather pass and for the movies: Sofdec decodes on the CPU, dgVoodoo unpacks the
// frame (UnpackVideo 0x471059BE) and this copy draws it into the final target. main.cpp flags that copy (LumaData.CustomData1 1): the
// vanilla clamp (the 8-bit target gave it) and, in HDR, a light PumboAutoHDR. The target is gamma encoded with 1 = UI paper white
// (UI_DRAW_TYPE 2: the composition scales the canvas by it).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   o0 = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]) * cb4[192];
   if (LumaData.CustomData1 == 1)
   {
      o0 = saturate(o0);
      [branch] if (LumaSettings.DisplayMode == 1 && LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
      {
         o0.rgb = linear_to_gamma(VideoAutoHDR(gamma_to_linear(o0.rgb), true, LumaSettings.GameSettings.VideoAutoHDRBoost, UIPaperWhiteNits));
      }
   }
}
