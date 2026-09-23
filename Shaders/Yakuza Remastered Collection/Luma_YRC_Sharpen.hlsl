// RCAS sharpening for the SMAA output (same shape as the SR3, TW2 and BL2/TPS passes). It stands in for the game's
// FidelityFX CAS, which only copies its input while SMAA runs. Runs on the gamma canvas color after the SMAA
// neighborhood blend. LumaData.CustomData3 = the RCAS Sharpness slider.

#include "../Includes/RCAS.hlsl"
#include "Includes/Common.hlsl"

Texture2D<float4> tex0 : register(t0);    // SMAA output (gamma canvas)
Texture2D<float2> dummyMV : register(t1); // unused (no dynamic sharpening)

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   return RCAS(int2(pos.xy), int2(0, 0), int2(LumaSettings.SwapchainSize) - 1, LumaData.CustomData3, tex0, dummyMV);
}
