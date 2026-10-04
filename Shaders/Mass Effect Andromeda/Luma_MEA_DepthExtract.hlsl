// SMAA predication signal. Source: the game's linear view depth (r32_float, metres, ~20000 on the sky), written by PS
// 0xDE1C9EB9 from the D24 reverse-Z depth (near / device Z) and read by the AO, lighting and motion blur passes. A plain
// difference fails (a plane's change grows with distance), so this is tangent-plane deviation (XeGTAO_CalculateEdges), [0,1].

Texture2D<float> depth : register(t0); // view depth in metres
RWTexture2D<float> uav : register(u0); // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float4 P; // P.x = relative tolerance: plane deviation counted as a full edge, as a fraction of view depth
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint w, h;
   depth.GetDimensions(w, h);
   if (id.x >= w || id.y >= h)
      return;
   const int3 p = int3(id.xy, 0);
   const float centerZ = depth.Load(p);
   // Out-of-bounds Loads return 0, so mirror the centre on every border: a flat border beats a false edge.
   const float leftZ = (id.x > 0) ? depth.Load(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = (id.x + 1 < w) ? depth.Load(p + int3(1, 0, 0)) : centerZ;
   const float topZ = (id.y > 0) ? depth.Load(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = (id.y + 1 < h) ? depth.Load(p + int3(0, 1, 0)) : centerZ;

   // Deviation from the local plane: compare each one-sided delta against the slope implied by the opposite neighbour
   // and keep the smaller. On a plane the two agree and cancel to ~0; at a discontinuity neither does.
   float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
   const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
   const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
   const float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
   edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

   // Depth-proportional tolerance: what matters scales with distance. Both terms are required - the slope adjustment
   // alone degrades toward the vanishing point, a depth-proportional threshold alone cannot reject a grazing plane.
   const float tolerance = max(centerZ, 1e-3) * max(P.x, 1e-4);
   // Left and top only: this is the axis pairing SMAA's predication actually compares.
   uav[id.xy] = saturate(max(edgesLRTB.x, edgesLRTB.z) / tolerance);
}
