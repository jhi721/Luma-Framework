#include "../Includes/DgVoodoo.hlsl"
#include "Includes/Common.hlsl"

// MGR:R "filtershadercopytex" (shader2.dat, dgVoodoo 2.87.5 -> ps_5_0 0x91A6C437, the same hash as Bayonetta's): t0 * g_MatrialColor
// (c184 = cb4[192]). After the gather pass the game draws the UI (HUD, menus, subtitles) on the scene target, resolves it and copies it
// with this into dgVoodoo's D3D9 back buffer (the canvas). Many UI layers blend additively or subtractively: the 8-bit target clamped
// every blend to [0, 1], the fp16 mirror keeps summing (above the peak, below 0). main.cpp flags that copy (LumaData.CustomData1 1) and
// binds the scene target as the gather pass left it (t1, before the UI): the UI is clamped to [0, max(that, 1)], so it stays within its
// paper white like vanilla while the HDR scene under it is untouched.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]
}

Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1); // the scene target before the UI (main.cpp)
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   float4 color = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]);
   if (LumaData.CustomData1 == 1)
   {
      color.rgb = clamp(color.rgb, 0.0, max(t1.Sample(s0_s, v5.xy).rgb, 1.0));
      color.a = saturate(color.a);
   }
   o0 = color * cb4[192];
}
