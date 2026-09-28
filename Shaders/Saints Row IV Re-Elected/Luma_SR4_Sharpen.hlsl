// RCAS sharpening of the gamma canvas after SMAA, or alone after DLSS/FSR (same shape as the TW2 and BL2/TPS passes).
// Reads the canvas snapshot and writes straight into the canvas RTV. LumaData.CustomData3 is the RCAS Sharpness slider.

#include "../Includes/RCAS.hlsl"
#include "Includes/Common.hlsl"

Texture2D<float4> tex0 : register(t0);    // Gamma canvas snapshot, SMAA's output when it ran
Texture2D<float2> dummyMV : register(t1); // unused (no dynamic sharpening)

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   return RCAS(int2(pos.xy), int2(0, 0), int2(LumaSettings.SwapchainSize) - 1, LumaData.CustomData3, tex0, dummyMV);
}
