// Borderlands GOTY Enhanced — ScreenPercentage < 100 stretch of the uber's sub-rect onto the swapchain (PS 0xB431B8F7).
//
// The shipped game leaves this pass's inputs unset: its $Globals (ColorScale, OverlayColor, InverseGamma) and its quad UVs,
// so the screen is black below 100% even without Luma. The uber already applied the grade, overlay and gamma, so this
// copies its sub-rect, placed from the pixel position and the uber's viewport share (LumaData, see main.cpp).

#include "Includes/Common.hlsl" // game-local: LumaData.GameData

SamplerState SceneColorTextureSampler_s : register(s0);
Texture2D<float4> SceneColorTexture : register(t0);

void main(
  float2 v0 : TEXCOORD0,
  float4 pos : SV_Position,
  out float4 o0 : SV_Target0)
{
  float2 size;
  SceneColorTexture.GetDimensions(size.x, size.y);
  o0 = float4(SceneColorTexture.SampleLevel(SceneColorTextureSampler_s, pos.xy / size * LumaData.GameData.ScreenPercentageScale, 0).rgb, 1.0);
}
