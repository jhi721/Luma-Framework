// FSR inputs from the scene's alpha, which UE3 fills with the linear view depth (the sky 65504): the device depth, the camera motion
// (vc4 view projection, turned into column vectors on the CPU) and, when enabled, FSR's masks (see "Includes/MotionVectorFill.hlsl").
// Depth isn't reversed. The game has no depth view to read.

#include "../Includes/MotionVectorFill.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 depth_from_view;          // The projection's depth row: device depth = x + y / view depth
   float reactive_scale;
   float reactive_threshold;
   float reactive_enabled;
   float padding;
};

Texture2D<float4> scene : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition
RWTexture2D<float> reactive : register(u2);
RWTexture2D<float> transparency : register(u3);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   const float4 color = scene.Load(int3(id.xy, 0));
   if (reactive_enabled != 0.0)
   {
      WriteFsrMasks(reactive, transparency, id.xy, mask_input.Load(int3(id.xy, 0)), reactive_scale, reactive_threshold);
   }
   // max() also drops a NaN; a cleared (0) alpha lands on the near plane
   const float depth = saturate(depth_from_view.x + depth_from_view.y / max(color.a, 1e-4));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, reprojection, jitter_ndc, depth);
}
