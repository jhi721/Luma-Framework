// Camera motion for motion vector pixels no patched draw wrote (still the frame start's marker, the largest float16: sky, unpatched
// draws): the G-buffer depth reprojected from the current to the previous frame's projTM (vc2 c0-c3, column vectors). Depth isn't
// reversed. When enabled, also FSR's and Luma TAA's masks from what the blended forward draws wrote (see
// "Includes/MotionVectorFill.hlsl").
#include "../Includes/MotionVectorFill.hlsl"

cbuffer MotionVectorFill : register(b0)
{
   row_major float4x4 reprojection; // Current clip space to the previous frame's
   float2 jitter_ndc;               // This frame's projection jitter: in the depth, not in the motion vectors
   float2 render_size;              // The scene's pixels (the render scale's sub-rect, else the whole target)
   float reactive_scale;            // The blended draws' reactivity, scaled (main.cpp "g_sr_reactive_scale")
   float reactive_threshold;        // Under it 0, over it 0.9; 0: the scaled reactivity, capped (see "WriteFsrMasks")
   float reactive_enabled;          // Not 0: the masks are written (Luma TAA or FSR with the reactive mask on)
   float padding;
};

Texture2D<float> depth : register(t0);
RWTexture2D<float2> motion_vectors : register(u0);
Texture2D<float2> mask_input : register(t1); // x reactive, y transparency & composition
RWTexture2D<float> reactive : register(u1);
RWTexture2D<float> transparency : register(u2);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
   const uint2 size = uint2(render_size);
   if (any(id.xy >= size))
      return;
   if (reactive_enabled != 0.0)
   {
      WriteFsrMasks(reactive, transparency, id.xy, mask_input.Load(int3(id.xy, 0)), reactive_scale, reactive_threshold);
   }
   if (IsObjectMotion(motion_vectors[id.xy]))
      return;
   WriteCameraMotion(motion_vectors, id.xy, size, reprojection, jitter_ndc, depth.Load(int3(id.xy, 0)));
}
