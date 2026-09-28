// RCAS on the SMAA output (ME1 2007 / BL2 / TPS / TW2 shape): on the gamma canvas, before the UE3 Canvas HUD.
// paperWhite 1.0; RCAS_LIMIT bounds the lobe, since the canvas is fp16 and no longer clamps at 1.0.

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
