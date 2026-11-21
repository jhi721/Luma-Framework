// The upscalers' depth and the camera motion of the texels no patched mesh draw wrote (Core's "SceneMotionVectors": the sky, the
// unpatched draws), from the scene's hardware depth (D24) and the frame camera's reprojection. With "reactive_enabled", FSR's and Luma
// TAA's masks from what the alpha blended draws wrote (see "Includes/MotionVectorFill.hlsl").

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
};

Texture2D<float> depth : register(t0);       // R24_UNORM_X8_TYPELESS view of the scene depth
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
   const float z = depth.Load(int3(id.xy, 0));
   device_depth[id.xy] = saturate(z * fill.depth_scale);
   if (IsObjectMotion(motion_vectors[id.xy]))
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, z);
}
