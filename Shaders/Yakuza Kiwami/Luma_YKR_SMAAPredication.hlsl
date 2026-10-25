// SMAA predication signal for Yakuza Kiwami from the scene depth (D32S8, standard Z: see NOTES.md "Exe RE"), drawn by main.cpp
// at the SSAO draw, where the opaque depth is complete and readable (the game clears it again before the post chain).
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "../Includes/DepthEdges.hlsl"

// "DrawFullscreenPassD3D12()": the target's width and height, then the relative tolerance and whether the depth is reversed
cbuffer PredicationConstants : register(b0)
{
   float4 predication_params;
}

Texture2D<float> depth : register(t0);

float main(float4 position : SV_Position) : SV_Target
{
   return DepthPredicationEdgeness(depth, uint2(position.xy), predication_params.w != 0.0, predication_params.z);
}
