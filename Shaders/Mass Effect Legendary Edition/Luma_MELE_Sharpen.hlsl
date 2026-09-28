// RCAS on the gamma post buffer after SMAA or DLSS/FSR, before it is copied back and decoded by stage 2. It uses
// paperWhite=1 and an SDR-tuned lobe limiter bounded by RCAS_LIMIT, so gamma highlights above 1 may sharpen less
// uniformly.

#include "../Includes/RCAS.hlsl"

cbuffer SharpenCB : register(b0)
{
   float4 SharpenParams; // (width, height, sharpness [0,1], unused).
}

Texture2D<float4> tex0 : register(t0);    // Gamma SMAA output, or a copy of the upscaled post buffer.
Texture2D<float2> dummyMV : register(t1); // Unused because dynamicSharpening is false.

float4 sharpen_ps(float4 pos : SV_Position) : SV_Target
{
   int2 p = int2(pos.xy);
   int2 maxPixel = int2((int)SharpenParams.x - 1, (int)SharpenParams.y - 1);
   return RCAS(p, int2(0, 0), maxPixel, SharpenParams.z, tex0, dummyMV, 1.0, false, (float4)0, false);
}
