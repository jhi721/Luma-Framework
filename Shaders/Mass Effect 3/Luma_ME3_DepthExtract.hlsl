// SMAA predication signal (ME1 2007 / TW2 port). No depth texture: UE3 packs DEVICE Z in the fp16 scene alpha, which the
// uber turns into view depth as 1 / (min(a, 65504) * MinZ_MaxZRatio.z - MinZ_MaxZRatio.w) with MinZ_MaxZRatio = cb4[10]
// (measured (9.99, 0.001, 0.1001, 0.0001)). ME1's alpha is already linear; here every tap is decoded the same way before
// the test. A plain difference fails (a plane's change grows as z^2), so this is tangent-plane deviation
// (XeGTAO_CalculateEdges), [0,1].

Texture2D<float4> scene : register(t0); // fp16 scene colour; .w carries device Z
RWTexture2D<float> uav : register(u0);  // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float4 P; // P.x = relative tolerance: plane deviation counted as a full edge, as a fraction of view depth
}

// A GPU copy of the uber's own PS constants (dgVoodoo b4), taken at the uber draw of the same frame.
cbuffer UberConstants : register(b1)
{
   float4 UberPsConstants[236];
}

float ViewDepth(int3 p)
{
   const float z = min(scene.Load(p).w, 65504.0) * UberPsConstants[10].z - UberPsConstants[10].w;
   return z > 0.0 ? rcp(z) : 65504.0; // the uber's rcp guard maps to "far"; clamp it to a finite fp16-scale value
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = ViewDepth(p);
   // Out-of-bounds Loads return 0 (D3D11-defined), so mirror the centre: a flat border beats a false edge.
   const float leftZ = (id.x > 0) ? ViewDepth(p - int3(1, 0, 0)) : centerZ;
   const float rightZ = ViewDepth(p + int3(1, 0, 0));
   const float topZ = (id.y > 0) ? ViewDepth(p - int3(0, 1, 0)) : centerZ;
   const float bottomZ = ViewDepth(p + int3(0, 1, 0));

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
