// SMAA predication signal from the game's full resolution D32 scene depth (the copy the SSAO downsample reads; reversed Z,
// sky = 0): it clears to 0 (main menu dump); a closed room reads 0.036-0.52 (Velvet Room dialogue dump).
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   edgeness[id.xy] = DepthPredicationEdgeness(depth, id.xy, true);
}
