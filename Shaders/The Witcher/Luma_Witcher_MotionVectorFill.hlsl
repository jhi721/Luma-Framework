// The upscaler's device depth (a sharable R32_FLOAT copy of the scene's D24S8, see SR bridge), and the camera motion (see
// "Includes/MotionVectorFill.hlsl"; the view projection derived from the draws' constants, column vectors). Depth isn't reversed.
// BL GOTY's fill, with the depth written out as ME1's.

#include "../Includes/MotionVectorFill.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 padding;
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   const float scene_depth = depth.Load(int3(id.xy, 0));
   device_depth[id.xy] = scene_depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, reprojection, jitter_ndc, scene_depth);
}
