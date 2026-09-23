// Game-local Common: defines LumaGameSettings before the shared Settings.hlsl declares the LumaSettings cbuffer.
// clang-format off
// ORDER MATTERS: sorted, the shared Common comes first and its dummy LumaGameSettings wins (X3018 on every field).
#include "GameCBuffers.hlsl"
#include "../../Includes/Common.hlsl"
// clang-format on

// The vanilla 4x4 Bayer dither: the tonemap multiplies its SDR output by "Bayer * scale + 1", compose divides it back out in the darks.
static const float kBayer4x4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static const float3 kBayerScale = float3(1.0 / 32.0, 1.0 / 32.0, 1.0 / 16.0);

float SRTTR_Bayer(uint2 pixel)
{
   return kBayer4x4[((pixel.x & 3u) << 2) | (pixel.y & 3u)] * 0.125 - 1.0;
}

// UI_DRAW_TYPE 2: the scene is scaled by this before the GUI layer is blended over it, so that the final UI paper white scaling lands it at the scene paper white
float SRTTR_SceneToUIScale()
{
   return LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
}

// Anti-banding dither at the last scene pass, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020
// PQ in HDR. The input is gamma code with 1 = UI paper white (UI_DRAW_TYPE 2).
void SRTTR_DitherOutput(inout float3 color, float2 uv)
{
   if (LumaSettings.GameSettings.Dithering <= 0.5)
   {
      return;
   }
   if (LumaSettings.DisplayMode != 1)
   {
      ApplyDithering(color, uv, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      return;
   }
   const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
   float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(color, GCT_MIRROR) * pqScale), GCT_MIRROR);
   ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
   color = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
}
