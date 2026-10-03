// RCAS sharpening of a texture drawn over a target of its size (games draw it with Core's "Copy VS" and "DrawCustomPixelShader",
// usually on the SMAA output). paperWhite = 1, the input sharpened as is: most games' gamma SDR canvas, MEA's display encoded color,
// ME1's and MoH's fp16 canvas (above 1.0 where the scene is).

#include "../Includes/RCAS.hlsl"

cbuffer SharpenCB : register(b0)
{
   float4 SharpenParams; // (width, height, sharpness [0..1], unused)
}

Texture2D<float4> tex0 : register(t0);
Texture2D<float2> dummyMV : register(t1); // Unused (no dynamic sharpening)

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   int2 p = int2(pos.xy);
   int2 maxPixel = int2((int)SharpenParams.x - 1, (int)SharpenParams.y - 1);
   return RCAS(p, int2(0, 0), maxPixel, SharpenParams.z, tex0, dummyMV, 1.0, false, (float4)0, false);
}
