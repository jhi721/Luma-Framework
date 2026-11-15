// FSR's exposure (FSR-Best-Practices FIN-4). It's the multiplier the tonemap applies to the scene SR reads, g_AdaptedLumBuffer[1]
// (last frame's adapted exposure) times the user's "Exposure", written into the 1x1 texture FSR takes.

#include "Includes/Common.hlsl"

StructuredBuffer<float> exposureBuffer : register(t0); // g_AdaptedLumBuffer
RWTexture2D<float> exposure : register(u0);

[numthreads(1, 1, 1)] void main() {
   const float value = exposureBuffer[1] * LumaSettings.GameSettings.Exposure;
   // FSR divides by it: 0 (the first frame's or a reset's adapted exposure, the Exposure slider at 0), NaN or Inf would turn its output
   // to NaN
   exposure[uint2(0, 0)] = (value > 0.0 && value < 65504.0) ? value : 1.0;
}
