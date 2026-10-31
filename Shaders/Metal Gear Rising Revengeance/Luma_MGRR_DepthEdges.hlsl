// SMAA predication signal from the depth prepass target (0x522D69B1, R32F): (c14.w / (z/w + c14.z) - c201.x) / c201.y, linear view
// depth less the near plane over the depth range, the value the soft particles and the deferred passes read. Affine in view depth,
// so the shared tangent-plane test takes it directly (its device depth form flips 1 - z/w back to view depth first).

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0); // The resolved prepass target, the scene's size
RWTexture2D<float> uav : register(u0); // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float tolerance; // The plane deviation counted as a full edge, as a fraction of view depth
   float3 padding;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float center = depth.Load(p);
   const float left = depth.Load(p - int3(1, 0, 0));
   const float right = depth.Load(p + int3(1, 0, 0));
   const float top = depth.Load(p - int3(0, 1, 0));
   const float bottom = depth.Load(p + int3(0, 1, 0));
   // Not "DepthExtractTolerance": its 1e-3 floor is above most of this depth (median ~6e-4 of the range in the prologue city)
   uav[id.xy] = PlaneDeviationEdgenessMirrored(id.xy, center, left, right, top, bottom, max(center, 1e-7) * tolerance);
}
