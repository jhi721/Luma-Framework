// RCAS on the gamma canvas after SMAA or the upscaler, into the swapchain target (or a temp main.cpp copies into it). It runs on
// the gamma values as they are (paperWhite 1) with FidelityFX's SDR-tuned RCAS_LIMIT lobe bound, so highlights above 1 may sharpen
// less uniformly.

#include "../Includes/RCAS.hlsl"

cbuffer SharpenCB : register(b0)
{
   float4 SharpenParams; // (width, height, sharpness[0..1], unused)
}

Texture2D<float4> tex0 : register(t0);    // SMAA or upscaler output (gamma canvas)
Texture2D<float2> dummyMV : register(t1); // unused (dynamicSharpening = false)

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   int2 p = int2(pos.xy);
   int2 maxPixel = int2((int)SharpenParams.x - 1, (int)SharpenParams.y - 1);
   return RCAS(p, int2(0, 0), maxPixel, SharpenParams.z, tex0, dummyMV, 1.0, false, (float4)0, false);
}
