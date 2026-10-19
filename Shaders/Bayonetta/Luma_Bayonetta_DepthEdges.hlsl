// Bayonetta: SMAA predication signal at the canvas size, from the game's depth prepass target (R32F "1 - z/w", the depth DoF and SSAO
// read; main.cpp takes it from the SSAO pass). 1 - z/w is proportional to 1 / z (z/w's constant term is 1 within 2e-5), as a
// reversed device depth, so the shared tangent-plane test applies. With PreserveAspect the scene fills a 16:9 rect of the canvas
// (the scene copy's viewport): texels outside it are flat.

#include "../Includes/DepthEdges.hlsl"
#include "Includes/GameCBuffers.hlsl"

Texture2D<float> depth : register(t0); // R32F 1 - z/w, the scene rect's size
RWTexture2D<float> uav : register(u0); // R16_FLOAT predication signal at the canvas size (0 = on the local plane, 1 = edge)

cbuffer PredCB : register(b0)
{
   CB::DepthEdgesConstants edges;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   depth.GetDimensions(size.x, size.y);
   const int2 pixel = int2(id.xy) - int2(edges.scene_origin);
   if (any(pixel < 0) || any(pixel >= int2(size)))
   {
      uav[id.xy] = 0.0;
      return;
   }
   uav[id.xy] = DepthPredicationEdgeness(depth, uint2(pixel), true, max(edges.tolerance, 1e-4));
}
