// SMAA of the grade output (0x167D96E5, a gamma 2.2 canvas) into the UI target, in place of 0x7561D004's copy (see main.cpp).
// Reference: https://github.com/iryoku/smaa
// The edge detection reads the gamma canvas; the neighborhood blend filters it in linear light and re-encodes it.

#include "../Includes/Common.hlsl"

// Root constants of Core's D3D12 "DrawSMAA()"
cbuffer SmaaConstants : register(b1)
{
   float4 SmaaRtMetrics;
   float4 SmaaGameConstants; // Unused
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA

#define SMAA_NEIGHBORHOOD_GAMMA_IN_LINEAR         1
#define SMAA_NEIGHBORHOOD_OUTPUT(color, position) color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);

#include "../Includes/SMAA_Passes.hlsl"
