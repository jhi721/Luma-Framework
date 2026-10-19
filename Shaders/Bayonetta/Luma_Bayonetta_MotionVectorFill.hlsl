// The upscalers' inputs from the depth prepass (an R32_FLOAT target the game fills with 1 - z/w, z/w being the D3D9 device depth, sky
// cleared to 0): the device depth, the camera motion (vc4 view projection, column vectors) and, when enabled, the reactive masks (see
// "Includes/MotionVectorFill.hlsl"). The Witcher's fill with Mirror's Edge's masks. The game has no depth view to read.

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
};

Texture2D<float> prepass : register(t0);
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);
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
   // saturate() also drops a NaN
   const float depth = saturate(1.0 - prepass.Load(int3(id.xy, 0)));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, depth);
}
