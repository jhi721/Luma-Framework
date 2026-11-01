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
// The display map's rolloff strength: RenoDX's Control Resonant uses 1 too, and the paper white stays within 1%. Matches MGR:R's.
static const float kDisplayMapStrength = 1.0;