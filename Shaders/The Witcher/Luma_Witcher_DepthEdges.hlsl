// SMAA predication signal from the scene depth (D24, not reversed): tangent-plane deviation (ME1's Luma_ME1_DepthExtract,
// XeGTAO_CalculateEdges), [0, 1]. Device depth d = A + B / z is affine in 1 / z, which is affine in screen space on a plane, so the
// plane test works on d itself. A deviation of "tolerance" x view depth is B / z x tolerance = (d - A) x tolerance; A = far / (far -
// near) is 1 within 2e-5 here (near 0.1, far ~5000), hence (1 - d).

#include "../Includes/DepthEdges.hlsl"

Texture2D<float> depth : register(t0); // the scene depth (R24_UNORM_X8)
RWTexture2D<float> uav : register(u0); // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float4 P; // P.x = relative tolerance: plane deviation counted as a full edge, as a fraction of view depth
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerD = depth.Load(p);
   // Out-of-bounds Loads return 0 (D3D11-defined), so mirror the centre: a flat border beats a false edge.
   uint2 size;
   depth.GetDimensions(size.x, size.y);
   const float leftD = (id.x > 0) ? depth.Load(p - int3(1, 0, 0)) : centerD;
   const float rightD = (id.x + 1 < size.x) ? depth.Load(p + int3(1, 0, 0)) : centerD;
   const float topD = (id.y > 0) ? depth.Load(p - int3(0, 1, 0)) : centerD;
   const float bottomD = (id.y + 1 < size.y) ? depth.Load(p + int3(0, 1, 0)) : centerD;

   // Depth-proportional tolerance (see the top). The floor avoids a division by 0 at the sky (d = 1).
   uav[id.xy] = PlaneDeviationEdgeness(centerD, leftD, rightD, topD, bottomD, max(1.0 - centerD, 1e-6) * max(P.x, 1e-4));
}
