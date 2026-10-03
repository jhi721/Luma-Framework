// The upscalers' inputs from the G-buffer's linear view depth (its fourth target, written by every opaque draw; 0 where none drew,
// the sky): the device depth, the camera motion (vc4 WorldToScreen, column vectors) and, when enabled, FSR's masks (see
// "Includes/MotionVectorFill.hlsl"). Depth isn't reversed. The game's depth buffer has no view to read.
// Under the render scale (main.cpp "RenderArea") the scene covers only the top-left "render_size" of the targets: only that area
// is filled, its NDC over the area. The upscaler runs before the exposure, on the linear scene (at every scale, see main.cpp
// "RenderArea"): the exposure it needs is the tonemap's multiplier from the last frame's exposure pass (see "Luma_TW2_Tonemap.hlsl"):
// the adaptation texel's gain (.z), capped and post-scaled by that pass's constants.
// Ported from Mass Effect 2007 ("Luma_ME1_MotionVectorFill.hlsl"), which reads the depth from the scene's alpha.

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
};

Texture2D<float> view_depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition
RWTexture2D<float> reactive : register(u2);
RWTexture2D<float> transparency : register(u3);
Texture2D<float4> adaptation : register(t2);
RWTexture2D<float> exposure : register(u4);
// A copy of the last exposure pass's vc4: c49 PSC_LumRanges (cb4[57], the static perms' levels), c51 PSC_LumRanges2 (cb4[59], .x the
// exposure cap m_maxMultiplier, .y the post-scale)
cbuffer ExposurePass : register(b1)
{
   float4 exposure_pass[60];
};

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const uint2 size = uint2(fill.render_size);
   if (any(id.xy >= size))
      return;
   // Bound whenever an upscaler runs (the SR bridge restarts its helper when an input appears or goes): 1 on the exposed scene. FSR
   // needs the tonemap's exact value (FSR-Best-Practices FIN-4); its per pixel black level can't be one number and is left out.
   if (all(id.xy == 0))
   {
      float value = 1.0;
      if (fill.exposure_enabled != 0.0)
      {
         const float gain = fill.exposure_static != 0.0 ? exposure_pass[57].z : adaptation.Load(int3(0, 0, 0)).z;
         value = fill.user_exposure * min(gain, exposure_pass[59].x) * exposure_pass[59].y;
      }
      exposure[uint2(0, 0)] = value;
   }
   if (fill.reactive_enabled != 0.0)
   {
      WriteFsrMasks(reactive, transparency, id.xy, mask_input.Load(int3(id.xy, 0)), fill.reactive_scale, fill.reactive_threshold);
   }
   // No visible surface has a view depth <= 0 (nor NaN): the cleared sky goes to the far plane
   const float linear_depth = view_depth.Load(int3(id.xy, 0));
   const float depth = linear_depth > 0.0 ? saturate(fill.depth_from_view.x + fill.depth_from_view.y / linear_depth) : 1.0;
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, depth);
}
