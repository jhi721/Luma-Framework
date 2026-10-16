// SMAA predication signal (TW2 port). No depth texture: UE3 packs LINEAR depth in the fp16 scene ALPHA, and a plain
// difference fails (a plane's change grows as z^2), so this is tangent-plane deviation (XeGTAO_CalculateEdges), [0,1].

#include "../Includes/DepthEdges.hlsl"

Texture2D<float4> scene : register(t0); // fp16 scene colour; .w carries LINEAR depth in Unreal units
RWTexture2D<float> uav : register(u0);  // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   float4 P; // P.x = relative tolerance: plane deviation counted as a full edge, as a fraction of view depth
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = scene.Load(p).w;
   // Out-of-bounds Loads return 0 (D3D11-defined), so mirror the centre: a flat border beats a false edge.
   const float leftZ = (id.x > 0) ? scene.Load(p - int3(1, 0, 0)).w : centerZ;
   const float rightZ = scene.Load(p + int3(1, 0, 0)).w;
   const float topZ = (id.y > 0) ? scene.Load(p - int3(0, 1, 0)).w : centerZ;
   const float bottomZ = scene.Load(p + int3(0, 1, 0)).w;

   // Depth-proportional tolerance: what matters scales with distance. Both terms are required - the slope adjustment
   // alone degrades toward the vanishing point, a depth-proportional threshold alone cannot reject a grazing plane.
   uav[id.xy] = PlaneDeviationEdgeness(centerZ, leftZ, rightZ, topZ, bottomZ, max(centerZ, 1e-3) * max(P.x, 1e-4));
}
