// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "../../Includes/Reinhard.hlsl"
#include "Settings.hlsl"

// The game's Y' weights, on encoded values
static const float3 kLumaWeights = float3(0.298912, 0.586611, 0.114478);
// The grades' (filter00, filter04) HDR fit into their 256x1 curve's [0,1] domain: the scale for a value x, 1 up to the shoulder,
// above it a Reinhard shoulder (C1 at the seam, asymptotic to 1) that fits the whole HDR range into the curve's last 10% (~25 texels)
static const float kCurveShoulder = 0.9;

float CurveDomainScale(float x)
{
   return (x <= kCurveShoulder) ? 1.0 : (Reinhard::ReinhardRanged(x, kCurveShoulder, 1.0).x / x);
}