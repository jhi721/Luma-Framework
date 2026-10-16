// SMAA predication signal from the game's R24 scene depth (non-reversed, sky = 1), taken from the ambient pass.
// Port of Saints Row: The Third's Luma_SR3_SMAAPredication.hlsl (itself from Luma_TW2_DepthExtract.hlsl).
// In a measured SR3 frame a raw R24 depth threshold only caught nearby silhouettes, hence the plane deviation measure.
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

float LinearDepth(int3 p)
{
   return RelativeViewDepth(depth.Load(p), false);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = LinearDepth(p);
   // Out-of-bounds Loads return 0; mirroring the centre there keeps the border flat instead of a false edge rim.
   const float leftZ = (id.x > 0) ? LinearDepth(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = LinearDepth(p + int3(1, 0, 0));
   const float topZ = (id.y > 0) ? LinearDepth(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = LinearDepth(p + int3(0, 1, 0));

   // Plane deviation counted as a full edge, as a fraction of view depth (TW2's validated 0.02).
   edgeness[id.xy] = PlaneDeviationEdgeness(centerZ, leftZ, rightZ, topZ, bottomZ, centerZ * 0.02);
}
