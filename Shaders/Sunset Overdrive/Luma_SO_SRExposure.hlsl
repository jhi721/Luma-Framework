// FSR's exposure (FSR-Best-Practices FIN-4). It's the multiplier the tonemap applies to the scene SR reads, g_AdaptedLumBuffer[1]
// (last frame's adapted exposure) times the user's "Exposure", written into the 1x1 texture FSR takes.

#include "Includes/Common.hlsl"

StructuredBuffer<float> exposureBuffer : register(t0); // g_AdaptedLumBuffer
RWTexture2D<float> exposure : register(u0);

[numthreads(1, 1, 1)] void main() {
   exposure[uint2(0, 0)] = exposureBuffer[1] * LumaSettings.GameSettings.Exposure;
}
