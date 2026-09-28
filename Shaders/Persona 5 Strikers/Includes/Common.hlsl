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

// Whether the composite and ApplyFxaa take the Luma HDR path (HDR display mode only). TONEMAP_TYPE 0 keeps the vanilla SDR output on
// HDR displays too: no extension, DICE or dither (scene sliders still apply).
#define P5S_HDR_SCENE (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1)

// The Katana post process's RGB weights (bloom threshold, Karis averages, DOF): a weighted sum of the linear scene RGB
static const float3 P5S_PostWeights = float3(0.222015, 0.706655, 0.071330);

// Karis average weight (against fireflies): 1 / (1 + weighted RGB)
float P5S_KarisWeight(float3 color)
{
   return 1.0 / (dot(color, P5S_PostWeights) + 1.0);
}

// Luma anti-banding dither of one output quantizer step: an 8-bit code in SDR, 10-bit BT.2020 PQ in HDR and SDR on HDR.
// The color is linear, 1 = UI paper white (UI_DRAW_TYPE 2). Off in the vanilla reference (TONEMAP_TYPE 0).
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