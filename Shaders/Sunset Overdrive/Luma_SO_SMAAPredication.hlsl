// SMAA predication signal from the scene depth (D32S8, reversed-Z with near at 1 and the sky at 0), taken from the camera velocity
// pass. This is Saints Row: The Third Remastered's "Luma_SRTTR_SMAAPredication.hlsl" with the depth reversed, see there for the method.
// It measures the deviation from the local tangent plane, a slope-adjusted second difference with a depth-proportional tolerance (as
// "XeGTAO_CalculateEdges").
//
// Depth: the projection gives d = near / z minus a far term of ~1e-5, so 1 / d is about proportional to linear view depth, and the
// relative tolerance cancels the unknown near plane. Sky clamps at 2^-24.
//
// Output is edge-ness in [0,1] against the LEFT and TOP neighbours only (SMAA compares centre-vs-left and centre-vs-top), so
// SMAA_PREDICATION_THRESHOLD is 0.5.

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0);
RWTexture2D<float> edgeness : register(u0);

float LinearDepth(int3 p)
{
   return RelativeViewDepth(depth.Load(p), true);
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
