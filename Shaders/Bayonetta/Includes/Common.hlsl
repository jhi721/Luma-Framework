// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "../../Includes/CInfinityRolloff.hlsl"
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// The game's Y' weights, on encoded values
static const float3 kLumaWeights = float3(0.298912, 0.586611, 0.114478);
// The grades' (filter00, filter04) HDR fit into their 256x1 curve's [0,1] domain: identity up to the shoulder, a C-infinity rolloff
// above it, asymptotic to 1. Then the display map's rolloff strength: RenoDX's Control Resonant uses 1 too, and the paper white stays
// within 1%. All three match MGR:R's (same engine code).
static const float kCurveShoulder = 0.9;
static const float kCurveShoulderStrength = 1.5;
static const float kDisplayMapStrength = 1.0;

// The scale for a value x, 1 up to the shoulder
float CurveDomainScale(float x)
{
   return (x <= kCurveShoulder) ? 1.0 : (Tonemap::CInfinityRollOff::Apply(x, 1.0, kCurveShoulder, kCurveShoulderStrength) / x);
}