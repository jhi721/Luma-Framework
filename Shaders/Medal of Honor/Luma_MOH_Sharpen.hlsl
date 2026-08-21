// RCAS sharpening for the SMAA output (Medal of Honor 2010; same shape as the shipped Airborne, BL2/TPS and
// Witcher 2 passes). Runs after the SMAA neighborhood-blend pass, on the gamma canvas colour SMAA itself consumed,
// and writes the canvas — so it lands before the bloom chain reads it, before the HUD is drawn on it, and before
// the core Display Composition applies paper white and the scRGB encode. paperWhite is passed as 1.0; the
// sharpness slider is the only knob.
// RCAS_LIMIT (in the shared header) bounds the lobe so bright pixels do not over-sharpen — which matters here
// because the canvas is fp16 and carries display-mapped values above 1.0.

#include "../Includes/RCAS.hlsl"

cbuffer SharpenCB : register(b0)
{
   float4 SharpenParams; // (width, height, sharpness[0..1], unused)
}

Texture2D<float4> tex0 : register(t0);    // SMAA output (gamma canvas)
Texture2D<float2> dummyMV : register(t1); // unused (dynamicSharpening = false)

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   int2 p = int2(pos.xy);
   int2 maxPixel = int2((int)SharpenParams.x - 1, (int)SharpenParams.y - 1);
   return RCAS(p, int2(0, 0), maxPixel, SharpenParams.z, tex0, dummyMV, 1.0, false, (float4)0, false);
}
