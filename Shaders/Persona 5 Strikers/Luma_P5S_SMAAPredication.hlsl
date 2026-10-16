// SMAA predication signal from the game's full resolution D32 scene depth (the copy the SSAO downsample reads; reversed Z,
// sky = 0). Port of Saints Row: The Third's Luma_SR3_SMAAPredication.hlsl (itself from Luma_TW2_DepthExtract.hlsl).
// Method and the SMAA_PREDICATION_THRESHOLD 0.5 contract: see "PlaneDeviationEdgeness" and "RelativeViewDepth" in
// Includes/DepthEdges.hlsl.

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

// Reversed Z: clears to 0 (main menu dump); a closed room reads 0.036-0.52 (Velvet Room dialogue dump)
float LinearDepth(int3 p)
{
   return RelativeViewDepth(depth.Load(p), true);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = LinearDepth(p);
   // Out-of-bounds Loads return 0; the center stands in there to avoid a false edge rim.
   const float leftZ = (id.x > 0) ? LinearDepth(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = LinearDepth(p + int3(1, 0, 0));
   const float topZ = (id.y > 0) ? LinearDepth(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = LinearDepth(p + int3(0, 1, 0));

   // Plane deviation counted as a full edge, as a fraction of view depth (TW2's validated 0.02).
   edgeness[id.xy] = PlaneDeviationEdgeness(centerZ, leftZ, rightZ, topZ, bottomZ, centerZ * 0.02);
}
