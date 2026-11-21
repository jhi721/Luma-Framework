// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DisplayMap.hlsl"
// clang-format on

// Dead Space 2: dgVoodoo 2.87.6's present blit (ps_4_0 0x880A17D3), the frame copied 1:1 into the swapchain after the UI. The display
// map normally ran where the scene ended (MarkerPatch's SMAA neighborhood or FXAA replacement, or Luma_DS2_DisplayMap before the
// UI; LumaData.CustomData1 != 0 here): then this only keeps the fp16 frame in range. Otherwise the whole frame, UI included, is mapped
// here.

Texture2D<float4> t0 : register(t0); // The frame

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   o0 = t0.Load(int3(int2(v1), 0));
   o0.a = saturate(o0.a); // Blends pushed the fp16 frame's alpha up to 2; the unorm swapchain kept it in range
   [branch] if (LumaData.CustomData1 != 0)
   {
      o0.rgb = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
      // SDR: the unorm swapchain clipped the UI drawn after the display map
      if (LumaSettings.DisplayMode != 1)
      {
         o0.rgb = saturate(o0.rgb);
      }
      return;
   }
   uint2 size;
   t0.GetDimensions(size.x, size.y);
   o0.rgb = DisplayMapCanvas(o0.rgb, v0.xy / size);
}
