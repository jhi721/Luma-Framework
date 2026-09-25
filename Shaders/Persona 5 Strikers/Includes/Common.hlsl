// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Whether the composite and ApplyFxaa passes take the Luma HDR path (HDR display mode only). TONEMAP_TYPE 0 keeps the vanilla SDR output
// on HDR displays too: no extension, DICE or dither (the scene sliders still apply).
#define P5S_HDR_SCENE (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1)

// Luma anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR and SDR on HDR.
// The color is linear, with 1 = UI paper white (UI_DRAW_TYPE 2). Not in the vanilla reference (TONEMAP_TYPE 0), which has no dither.
void P5S_DitherOutput(inout float3 color, float2 uv)
{
   if (TONEMAP_TYPE < 1 || LumaSettings.GameSettings.Dithering <= 0.5)
      return;
   if (LumaSettings.DisplayMode == 0)
   {
      ApplyDithering(color, uv, false, 1.0, 8u, LumaSettings.FrameIndex, true);
   }
   else
   {
      const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
      float3 pq = Linear_to_PQ(BT709_To_BT2020(color * pqScale), GCT_MIRROR);
      ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
      color = BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale;
   }
}