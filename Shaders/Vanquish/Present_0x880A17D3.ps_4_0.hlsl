#include "Includes/Common.hlsl"

// dgVoodoo's present blit (ps_4_0 0x880A17D3, MGR:R's and Bayonetta's too): dgVoodoo's D3D9 back buffer (the canvas: the game's FXAA or
// edge AA output with the UI on it) loaded 1:1 into the swapchain. dgVoodoo draws it once the swapchain's format differs from the
// canvas' (else it copies). The UI blends additively on the fp16 canvas, which no longer clamps at 1: main.cpp flags this pass
// (LumaData.CustomData1 1) and binds the gather pass' output (t1, the scene before the UI), and the UI is clamped to max(that, 1).

Texture2D<float4> t0 : register(t0); // the canvas
Texture2D<float4> t1 : register(t1); // the scene before the UI (main.cpp)

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   const int2 pixel = int2(v1);
   o0 = t0.Load(int3(pixel, 0));
   uint2 canvas_size;
   t0.GetDimensions(canvas_size.x, canvas_size.y);
   if (LumaData.CustomData1 == 1)
   {
      uint2 scene_size;
      t1.GetDimensions(scene_size.x, scene_size.y);
      o0.rgb = clamp(o0.rgb, 0.0, max(t1.Load(int3(uint2(pixel) * scene_size / canvas_size, 0)).rgb, 1.0));
      o0.a = saturate(o0.a);
   }
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR at the paper white the
   // composition scales the canvas by (UI_DRAW_TYPE 2), as MGR:R's
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(o0.rgb, v0.xy / canvas_size, true, LumaSettings.DisplayMode != 0, UIPaperWhiteNits);
   }
}
