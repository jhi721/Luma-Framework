// Camera motion for motion vector pixels no patched draw wrote (still the frame start's marker, the largest float16: sky, unpatched
// draws): the G-buffer depth reprojected from the current to the previous frame's projTM (vc2 c0-c3, column vectors). Depth isn't
// reversed.
#include "../Includes/MotionVectorFill.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 render_size;              // The scene's pixels (the render scale's sub-rect, else the whole target)
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const uint2 size = uint2(render_size);
   if (any(id.xy >= size) || IsObjectMotion(motion_vectors[id.xy]))
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, reprojection, jitter_ndc, depth.Load(int3(id.xy, 0)));
}
