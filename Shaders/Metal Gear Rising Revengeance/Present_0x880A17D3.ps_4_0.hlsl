#include "Includes/Common.hlsl"

// dgVoodoo's present blit (ps_4_0 0x880A17D3, Bayonetta's too): dgVoodoo's D3D9 back buffer (the canvas) loaded 1:1 into the swapchain.
// With the output at the render resolution the game draws no canvas copy (Copy_0x91A6C437), so the UI's clamp to the scene before it
// (see that shader) happens here: main.cpp flags it (LumaData.CustomData1 1) and binds that scene (t1, at the render resolution).

Texture2D<float4> t0 : register(t0); // the canvas
Texture2D<float4> t1 : register(t1); // the scene target before the UI (main.cpp)

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   const int2 pixel = int2(v1);
   o0 = t0.Load(int3(pixel, 0));
   if (LumaData.CustomData1 == 1)
   {
      uint2 canvas_size;
      uint2 scene_size;
      t0.GetDimensions(canvas_size.x, canvas_size.y);
      t1.GetDimensions(scene_size.x, scene_size.y);
      o0.rgb = clamp(o0.rgb, 0.0, max(t1.Load(int3(uint2(pixel) * scene_size / canvas_size, 0)).rgb, 1.0));
      o0.a = saturate(o0.a);
   }
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR at the paper white the
   // composition scales the canvas by (UI_DRAW_TYPE 2), as Bayonetta's
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      uint2 size;
      t0.GetDimensions(size.x, size.y);
      ApplyOutputDithering(o0.rgb, v0.xy / size, true, LumaSettings.DisplayMode == 1, UIPaperWhiteNits);
   }
}
