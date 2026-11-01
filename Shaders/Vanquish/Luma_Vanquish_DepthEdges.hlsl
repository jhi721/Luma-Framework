// SMAA predication signal from the depth prepass target (zmapscr 0x5F3E9D55 / zmapscrnki 0xBA502672, R32F): (c14.w / (z/w + c14.z) -
// c201.x) / c201.y, MGR:R's: linear view depth less the near plane over the depth range, the value the soft particles, SSAO and DoF
// read. Affine in view depth, so the shared tangent-plane test takes it directly.

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0); // A copy of the prepass target, the scene's size
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
   // Not "DepthExtractTolerance": its 1e-3 floor is above most of this depth (MGR:R's measurement, same encoding)
   uav[id.xy] = PlaneDeviationEdgenessMirrored(id.xy, center, left, right, top, bottom, max(center, 1e-7) * tolerance);
}
