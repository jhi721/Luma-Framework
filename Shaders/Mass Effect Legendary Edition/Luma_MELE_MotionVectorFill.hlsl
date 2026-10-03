// Camera motion for the motion vector pixels no patched draw wrote (sky, unpatched draws), which still hold the frame-start
// marker 65504, the largest float16: scene depth reprojected from the current to the previous frame's world camera (b1
// ViewProjectionMatrix and the PreViewTranslation change, transposed for column vectors on the CPU). Depth isn't reversed.

#include "../Includes/MotionVectorFill.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size) || motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, reprojection, jitter_ndc, depth.Load(int3(id.xy, 0)));
}
