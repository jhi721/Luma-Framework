// SMAA's predication signal from the scene's hardware depth (D24, the motion vector fill's view of it): each tap linearized to view
// depth, then the plane deviation edge-ness of "DepthEdges.hlsl". The depth is rescaled by 1 / A first (device depth / A is the
// infinite projection's with the same near; Dead Space 2's and 3's custom depth range puts infinity at A ~0.99).

#include "../Includes/DepthEdges.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer DepthEdges : register(b0)
{
   CB::DepthEdgesConstants constants;
};

Texture2D<float> depth : register(t0); // R24_UNORM_X8_TYPELESS view of the scene depth
RWTexture2D<float> uav : register(u0); // R16_FLOAT predication signal (0 = on the local plane, 1 = edge)

float ViewDepth(int3 p)
{
   return constants.near_plane * RelativeViewDepth(saturate(depth.Load(p) * constants.depth_scale), false);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const int3 p = int3(id.xy, 0);
   const float centerZ = ViewDepth(p);
   uav[id.xy] = PlaneDeviationEdgenessMirrored(id.xy, centerZ, ViewDepth(p - int3(1, 0, 0)), ViewDepth(p + int3(1, 0, 0)), ViewDepth(p - int3(0, 1, 0)), ViewDepth(p + int3(0, 1, 0)),
                                               DepthExtractTolerance(centerZ, constants.tolerance));
}
