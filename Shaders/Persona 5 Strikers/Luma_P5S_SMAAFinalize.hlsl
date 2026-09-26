// Last post-composite pass when RCAS is on (after SMAA, or alone after DLSS/FSR; without RCAS, SMAA writes the canvas), from the
// gamma copy into the linear canvas: RCAS on the gamma color (perceptual sharpening), the decode, then the Luma dither the composite
// skipped. The UI draws after this.
// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/RCAS.hlsl"
// clang-format on

Texture2D<float4> smaaOutput : register(t0);
Texture2D<float2> dummyMV : register(t1); // Unused (no dynamic sharpening)

float4 main(float4 pos : SV_Position) : SV_Target
{
   // RCAS peak: the display's peak relative to UI paper white, gamma encoded like the input
   const float peak = LumaSettings.DisplayMode == 1 ? pow(LumaSettings.PeakWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0), 1.0 / DefaultGamma) : 1.0;
   float4 color = RCAS(int2(pos.xy), 0, int2(LumaSettings.SwapchainSize) - 1, LumaSettings.GameSettings.RCASSharpness, smaaOutput, dummyMV, peak);
   color.rgb = gamma_to_linear(color.rgb, GCT_MIRROR);
   P5S_DitherOutput(color.rgb, pos.xy * LumaSettings.SwapchainInvSize);
   return color;
}
