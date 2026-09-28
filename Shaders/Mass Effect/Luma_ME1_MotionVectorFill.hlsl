// FSR inputs from the scene's alpha, which UE3 fills with the linear view depth (the sky 65504): the device depth, and the camera
// motion for the motion vector pixels no patched draw wrote (still the marker 65504 from the frame start, the largest float16: sky,
// unpatched draws), that depth reprojected from the current to the previous frame's camera (vc4 view projection, turned into
// column vectors on the CPU). Depth isn't reversed. The game has no depth view to read.
// Also FSR's masks, when enabled, from what the alpha blended draws wrote themselves (their pixel shaders patched, max blended).
// They draw without motion vectors of their own, so FSR would keep the history of what's behind them (ghosting). The reactive
// one from all of them (additive sparks and glows: max(r, g, b) * a with the color tonemapped x / (1 + x), the others a):
// scaled, then 0 under the threshold and 0.9 over it (a low one shakes static glows), or without a threshold capped at 0.9 so
// FSR still keeps a little history. The transparency & composition one from the non-additive ones (smoke, glass, water: a), as
// is (AMD's sample passes the alpha).

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
      const float2 mask = mask_input.Load(int3(id.xy, 0));
      const float reactivity = mask.x * reactive_scale;
      reactive[id.xy] = reactive_threshold > 0.0 ? (reactivity < reactive_threshold ? 0.0 : 0.9) : min(reactivity, 0.9);
      transparency[id.xy] = mask.y;
   }
   // max() also drops a NaN; a cleared (0) alpha lands on the near plane
   const float depth = saturate(depth_from_view.x + depth_from_view.y / max(color.a, 1e-4));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < 65504.0)
      return;
   const float4 current = float4((id.xy + 0.5) / size * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter_ndc, depth, 1.0);
   const float4 previous = mul(reprojection, current);
   // Previous minus current, UV space, like the patched draws
   motion_vectors[id.xy] = (previous.xy / previous.w - current.xy) * float2(0.5, -0.5);
}
