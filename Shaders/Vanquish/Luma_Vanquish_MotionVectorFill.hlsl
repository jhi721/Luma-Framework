// The upscalers' depth and the camera motion of the texels no patched mesh draw wrote (Core's "SceneMotionVectors": the sky and
// background layers, translucent-only materials), from the scene's hardware depth (D24) and the frame camera's reprojection (this
// frame's view and projection rows of vc4 against last frame's, built on the CPU). With "reactive_enabled", FSR's and Luma TAA's masks
// from what the alpha blended draws wrote, as Bayonetta's.

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
};

Texture2D<float> depth : register(t0);       // R24_UNORM_X8_TYPELESS view of the scene depth
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition (both max blended)
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);
RWTexture2D<float> reactive : register(u2);
RWTexture2D<float> transparency : register(u3);
RWTexture2D<float> previous_additive : register(u4); // The additive and alpha blended draws' masks of the previous frame, replaced here

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   if (fill.reactive_enabled != 0.0)
   {
      // The additive draws write y = 0, so x > y is theirs (sparks, glows). They are scaled and capped at 0.9 without a threshold,
      // else they smear, and kept from the previous frame, because a blinking one that went out is still lit in the upscaler's history.
      // The alpha blended draws (x = y = a: smoke, haze) count only over the threshold, as a continuous mask over them shakes the
      // whole sprite, and only where their alpha changed since the previous frame by more than "alpha_change_threshold". The game also
      // draws static glass panes and corridor end caps as large alpha blended particle quads (037EC99A), and masking those threw away
      // the history behind them, so its aliasing shimmered under Luma TAA and FSR.
      const float alpha_change_threshold = 0.1;
      const float2 mask = mask_input.Load(int3(id.xy, 0));
      const float current_additive = (mask.x > mask.y ? mask.x : 0.0);
      // Both previous masks in the R32_FLOAT texture, as 8-bit integers (exact in a float): additive * 256 + alpha
      const float previous_masks = previous_additive[id.xy];
      const float previous_alpha = fmod(previous_masks, 256.0) / 255.0;
      const float additive = min(max(current_additive, floor(previous_masks / 256.0) / 255.0) * fill.reactive_scale, 0.9);
      previous_additive[id.xy] = round(current_additive * 255.0) * 256.0 + round(mask.y * 255.0);
      const float alpha_blended = (mask.y < fill.reactive_threshold || abs(mask.y - previous_alpha) <= alpha_change_threshold ? 0.0 : 0.9);
      reactive[id.xy] = max(additive, alpha_blended);
      transparency[id.xy] = mask.y;
   }
   // saturate() also drops a NaN
   const float z = saturate(depth.Load(int3(id.xy, 0)));
   device_depth[id.xy] = z;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, z);
}
