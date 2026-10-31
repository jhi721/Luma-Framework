// The upscalers' inputs from the depth prepass (an R32_FLOAT target the game fills with 1 - z/w, z/w being the D3D9 device depth, sky
// cleared to 0): the device depth, the camera motion (vc4 view projection, column vectors) and, when enabled, the reactive masks. The
// Witcher's fill with Mirror's Edge's masks. The game has no depth view to read.

#include "../Includes/MotionVectorFill.hlsl"
#include "Includes/GameCBuffers.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   CB::MotionVectorFillConstants fill;
};

Texture2D<float> prepass : register(t0);
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition (both max blended)
RWTexture2D<float2> motion_vectors : register(u0);
RWTexture2D<float> device_depth : register(u1);
RWTexture2D<float> reactive : register(u2);
RWTexture2D<float> transparency : register(u3);
RWTexture2D<float> previous_additive : register(u4); // The additive draws' mask of the previous frame, replaced here

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   uint2 size;
   motion_vectors.GetDimensions(size.x, size.y);
   if (any(id.xy >= size))
      return;
   if (fill.reactive_enabled != 0.0)
   {
      // "WriteFsrMasks" with the additive and the alpha blended draws apart: the additive ones write y = 0, so x > y is theirs
      // (lasers, glows: cores ~0.3, glows 0.05-0.2), scaled and capped at 0.9 without a threshold, else they smear; the alpha
      // blended ones (x = y = a: smoke, haze) only over the threshold, as a continuous mask over them shook the whole sprite
      // in the TAA lab (_tools/taa_lab/reactive_bayo_eval.py, 2026-10-31). The additive part keeps the previous frame's: a blinking
      // laser that went out writes nothing, but the upscaler's history still has it lit (lab: it blinked at about half its range
      // without, three quarters with)
      const float2 mask = mask_input.Load(int3(id.xy, 0));
      const float current_additive = (mask.x > mask.y ? mask.x : 0.0);
      const float additive = min(max(current_additive, previous_additive[id.xy]) * fill.reactive_scale, 0.9);
      previous_additive[id.xy] = current_additive;
      const float alpha_blended = (mask.y < fill.reactive_threshold ? 0.0 : 0.9);
      reactive[id.xy] = max(additive, alpha_blended);
      transparency[id.xy] = mask.y;
   }
   // saturate() also drops a NaN
   const float depth = saturate(1.0 - prepass.Load(int3(id.xy, 0)));
   device_depth[id.xy] = depth;
   if (motion_vectors[id.xy].x < MOTION_VECTOR_MARKER)
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, fill.reprojection, fill.jitter_ndc, depth);
}
