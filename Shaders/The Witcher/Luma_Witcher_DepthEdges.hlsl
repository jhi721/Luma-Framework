// SMAA predication signal from the scene depth (D24, not reversed): tangent-plane deviation (ME1's Luma_ME1_DepthExtract,
// XeGTAO_CalculateEdges), [0, 1]. Device depth d = A + B / z is affine in 1 / z, which is affine in screen space on a plane, so the
// plane test works on d itself. A deviation of "tolerance" x view depth is B / z x tolerance = (d - A) x tolerance; A = far / (far -
// near) is 1 within 2e-5 here (near 0.1, far ~5000), hence (1 - d).

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

   // Deviation from the local plane: compare each one-sided delta against the slope implied by the opposite neighbour
   // and keep the smaller. On a plane the two agree and cancel to ~0; at a discontinuity neither does.
   float4 edgesLRTB = float4(leftD, rightD, topD, bottomD) - centerD;
   const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
   const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
   const float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
   edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

   // Depth-proportional tolerance (see the top). The floor avoids a division by 0 at the sky (d = 1).
   const float tolerance = max(1.0 - centerD, 1e-6) * max(P.x, 1e-4);
   // Left and top only: this is the axis pairing SMAA's predication actually compares.
   uav[id.xy] = saturate(max(edgesLRTB.x, edgesLRTB.z) / tolerance);
}
