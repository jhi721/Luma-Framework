// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/CInfinityRolloff.hlsl"
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// The game's Y' weights, on encoded values (FilterShaderGather)
static const float3 kLumaWeights = float3(0.298912, 0.586611, 0.114478);
// The grade's HDR fit into its 256x1 curve's [0,1] domain (identity up to the shoulder, a C-infinity rolloff above it) and the
// display map's rolloff strength (RenoDX's Control Resonant uses 1 too: the paper white stays within 1%)
static const float kCurveShoulder = 0.9;
static const float kCurveShoulderStrength = 1.5;
static const float kDisplayMapStrength = 1.0;
