// DLAA/FSR inputs from the scene's alpha, which UE3 fills with the linear view depth (the sky 65504): the device depth, the camera
// motion (vc4 view projection, turned into column vectors on the CPU) for the pixels no motion vector draw wrote and, when enabled,
// FSR's masks (see "Includes/MotionVectorFill.hlsl"). Depth isn't reversed. The game has no depth view to read. Mass Effect 2007's
// fill.

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
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
   if (fill.reactive_enabled != 0.0)
   {
      WriteFsrMasks(reactive, transparency, id.xy, mask_input.Load(int3(id.xy, 0)), fill.reactive_scale, fill.reactive_threshold);
   }
   // max() also drops a NaN; a cleared (0) alpha lands on the near plane
   const float depth = saturate(fill.depth_from_view.x + fill.depth_from_view.y / max(scene.Load(int3(id.xy, 0)).a, 1e-4));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, depth);
}
