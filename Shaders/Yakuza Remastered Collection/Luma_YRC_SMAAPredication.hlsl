// SMAA predication signal from the game's R32 scene depth (Y3R standard, sky = 1: ASSAO unpacks it as
// 0.10001 / (1.0001 - d); Y4R/Y5R reversed, sky = 0: about 0.1 / (d + 0.0001), measured; LumaData.CustomData1 != 0).
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "Includes/Common.hlsl"

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   // LumaData.CustomData3: TW2's validated 0.02, a DEV slider for calibration.
   edgeness[id.xy] = DepthPredicationEdgeness(depth, id.xy, LumaData.CustomData1 != 0u, LumaData.CustomData3);
}
