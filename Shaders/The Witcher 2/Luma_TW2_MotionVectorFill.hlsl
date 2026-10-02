// The upscalers' inputs from the G-buffer's linear view depth (its fourth target, written by every opaque draw; 0 where none drew,
// the sky): the device depth, and the camera motion for the motion vector pixels no patched draw wrote (still the marker 65504 from
// the frame start, the largest float16: sky, unpatched draws), that depth reprojected from the current to the previous frame's
// camera (vc4 WorldToScreen, column vectors). Depth isn't reversed. The game's depth buffer has no view to read.
// Also FSR's masks, when enabled, from what the alpha blended draws wrote themselves (see
// "MotionVectorPatch::PatchPixelShaderReactive", max blended). They draw without motion vectors of their own, so FSR would keep
// the history of what's behind them (ghosting). The reactive one from all of them: scaled, then 0 under the threshold and 0.9
// over it (a low one shakes static glows), or without a threshold capped at 0.9 so FSR still keeps a little history. The
// transparency & composition one from the non-additive ones (smoke, glass, water), as is (AMD's sample passes the alpha).
// Ported from Mass Effect 2007 ("Luma_ME1_MotionVectorFill.hlsl"), which reads the depth from the scene's alpha.

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

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   if (fill.reactive_enabled != 0.0)
   {
      const float2 mask = mask_input.Load(int3(id.xy, 0));
      const float reactivity = mask.x * fill.reactive_scale;
      reactive[id.xy] = fill.reactive_threshold > 0.0 ? (reactivity < fill.reactive_threshold ? 0.0 : 0.9) : min(reactivity, 0.9);
      transparency[id.xy] = mask.y;
   }
   // No visible surface has a view depth <= 0 (nor NaN): the cleared sky goes to the far plane
   const float linear_depth = view_depth.Load(int3(id.xy, 0));
   const float depth = linear_depth > 0.0 ? saturate(fill.depth_from_view.x + fill.depth_from_view.y / linear_depth) : 1.0;
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < 65504.0)
      return;
   const float4 current = float4((id.xy + 0.5) / size * float2(2.0, -2.0) + float2(-1.0, 1.0) - fill.jitter_ndc, depth, 1.0);
   const float4 previous = mul(fill.reprojection, current);
   // Previous minus current, UV space, like the patched draws
   motion_vectors[id.xy] = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
}
