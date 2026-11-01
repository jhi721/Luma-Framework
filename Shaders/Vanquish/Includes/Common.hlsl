// Always include this instead of the global "Common.hlsl" if you made any changes to the game shaders/cbuffers

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/CInfinityRolloff.hlsl"
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// The game's Y' weights, on encoded values (FilterShaderGather, filter05)
static const float3 kLumaWeights = float3(0.298912, 0.586611, 0.114478);
// The display map's rolloff strength (MGR:R's: the paper white stays within 1%)
static const float kDisplayMapStrength = 1.0;

// The game's 8-bit targets clamped the scene passes' output (the lighting composition, its blur and the DoF, fp16 now). Kept in SDR
// and for the vanilla reference (TONEMAP_TYPE 0): unclamped, the blurs, DoF and haze after them spread the highlights above 1 and the
// gather's clip covers more than vanilla's (the bright, hazy main menu).
float4 ClampSceneOutput(float4 color)
{
   return (TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1) ? color : saturate(color);
}
