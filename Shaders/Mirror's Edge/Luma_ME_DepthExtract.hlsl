// SMAA predication signal, Mass Effect 2007's (TW2 port). No depth texture: UE3 packs LINEAR depth in the fp16 scene ALPHA, and a plain
// difference fails (a plane's change grows as z^2), so this is tangent-plane deviation (XeGTAO_CalculateEdges), [0,1].

#include "../Includes/DepthEdges.hlsl"
#include "Includes/GameCBuffers.hlsl"

Texture2D<float4> scene : register(t0); // fp16 scene colour; .w carries LINEAR depth in Unreal units
RWTexture2D<float> uav : register(u0);  // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   CB::DepthExtractConstants extract;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = scene.Load(p).w;
   const float leftZ = scene.Load(p - int3(1, 0, 0)).w;
   const float rightZ = scene.Load(p + int3(1, 0, 0)).w;
   const float topZ = scene.Load(p - int3(0, 1, 0)).w;
   const float bottomZ = scene.Load(p + int3(0, 1, 0)).w;

   // Depth-proportional tolerance: what matters scales with distance. Both terms are required - the slope adjustment
   // alone degrades toward the vanishing point, a depth-proportional threshold alone cannot reject a grazing plane.
   uav[id.xy] = PlaneDeviationEdgenessMirrored(id.xy, centerZ, leftZ, rightZ, topZ, bottomZ, DepthExtractTolerance(centerZ, extract.tolerance));
}
