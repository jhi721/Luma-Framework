// SMAA predication signal from the game's R32 scene depth (Y3R standard, sky = 1: ASSAO unpacks it as
// 0.10001 / (1.0001 - d); Y4R/Y5R reversed, sky = 0: about 0.1 / (d + 0.0001), measured; LumaData.CustomData1 != 0).
// Copied from Luma_SR3_SMAAPredication.hlsl (a port of Luma_TW2_DepthExtract.hlsl).
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "Includes/Common.hlsl"

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

float LinearDepth(int3 p)
{
   return RelativeViewDepth(depth.Load(p), LumaData.CustomData1 != 0u);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = LinearDepth(p);
   // Out-of-bounds Loads return 0; mirroring the centre there keeps the border flat instead of a false edge rim.
   const float leftZ = (id.x > 0) ? LinearDepth(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = LinearDepth(p + int3(1, 0, 0));
   const float topZ = (id.y > 0) ? LinearDepth(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = LinearDepth(p + int3(0, 1, 0));

   // Plane deviation counted as a full edge, as a fraction of view depth (LumaData.CustomData3: TW2's validated 0.02,
   // a DEV slider for calibration).
   edgeness[id.xy] = PlaneDeviationEdgeness(centerZ, leftZ, rightZ, topZ, bottomZ, centerZ * LumaData.CustomData3);
}
