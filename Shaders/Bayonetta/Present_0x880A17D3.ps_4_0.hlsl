#include "Includes/Common.hlsl"

// Bayonetta: dgVoodoo's present blit (ps_4_0 0x880A17D3 on both builds, as in TW1), dgVoodoo's copy of its D3D9 back buffer (the
// canvas) loaded 1:1 into the swapchain.
// The UI draws on the canvas after the scene, many of its layers additively (SRC_ALPHA, ONE): the 8-bit canvas clamped every blend
// at 1, the fp16 mirror keeps summing. main.cpp binds the canvas as the scene copy (ResetRender) or the last movie draw left it, so
// everything the UI added is clamped back to max(that, 1): the UI stays within its paper white like vanilla, the HDR scene and the
// movies' AutoHDR under it are untouched. Frames without either get t1 unbound, which reads 0: the whole canvas is clamped to 1.

Texture2D<float4> t0 : register(t0); // the canvas
Texture2D<float4> t1 : register(t1); // the canvas before the UI (main.cpp)

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
   const int3 pixel = int3(int2(v1), 0);
   o0 = t0.Load(pixel);
   o0.rgb = min(o0.rgb, max(t1.Load(pixel).rgb, 1.0));
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR at the paper white the
   // composition scales the canvas by (UI_DRAW_TYPE 2)
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      uint2 size;
      t0.GetDimensions(size.x, size.y);
      ApplyOutputDithering(o0.rgb, v0.xy / size, true, LumaSettings.DisplayMode == 1, UIPaperWhiteNits);
   }
}
