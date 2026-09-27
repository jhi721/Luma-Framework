// DLSS / FSR: the DOF blur amount per render pixel, accumulated over frames like the upscaler accumulates color. The game's gather
// takes it from the jittered depth, so a thin object near a 4x4 block edge flips the block between sharp and blurred every frame (a
// flickering halo around wires against the sky). Each pixel's amount (the gather's formula, same constants) is blended into its
// history, reprojected with the motion vectors and clamped to the current 3x3 range; Luma_BL_OutputGather.hlsl averages it per block.

cbuffer OutputPost : register(b5)
{
   float2 share;
   float2 inv_share;
   float2 padding0;
   float2 render_size;   // The scene's sub-rect, pixels
   float history_weight; // Weight of the current frame, 1 = no history
   float3 padding1;
};

// The gather's constants (bound for it: this runs at its dispatch)
cbuffer GatherGlobals : register(b0)
{
   float4 gather_constants[4];
};
cbuffer PSOffsetConstants : register(b2)
{
   float4 offset_constants[7];
};

Texture2D<float> depth : register(t0);
Texture2D<float2> motion_vectors : register(t1); // UV of the scene's sub-rect, previous minus current
Texture2D<float> previous_history : register(t2);
RWTexture2D<float> history : register(u0);
SamplerState linear_sampler : register(s0);

#include "Includes/DOFBlurAmount.hlsli"

// The group's 8x8 pixels and a 1 pixel border (edge clamped), each amount computed once instead of by up to 9 threads
groupshared float group_amounts[10 * 10];

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) {
   const int2 tile_origin = int2(group_id.xy) * 8 - 1;
   for (uint i = group_index; i < 10 * 10; i += 8 * 8)
      group_amounts[i] = BlurAmount(depth.Load(int3(clamp(tile_origin + int2(i % 10, i / 10), 0, int2(render_size) - 1), 0)));
   GroupMemoryBarrierWithGroupSync();

   if (any(float2(id.xy) >= render_size))
      return;
   const int2 pixel = int2(id.xy);
   const int2 tile_pixel = pixel - tile_origin;
   const float current = group_amounts[tile_pixel.y * 10 + tile_pixel.x];
   float result = current;
   const float2 uv = (float2(pixel) + 0.5) / render_size + motion_vectors.Load(int3(pixel, 0));
   if (history_weight < 1.0 && all(uv >= 0.0) && all(uv <= 1.0))
   {
      float lowest = current, highest = current;
      [unroll] for (int y = -1; y <= 1; y++)
      {
         [unroll] for (int x = -1; x <= 1; x++)
         {
            const float neighbor = group_amounts[(tile_pixel.y + y) * 10 + tile_pixel.x + x];
            lowest = min(lowest, neighbor);
            highest = max(highest, neighbor);
         }
      }
      float2 history_size;
      previous_history.GetDimensions(history_size.x, history_size.y);
      const float previous = clamp(previous_history.SampleLevel(linear_sampler, uv * render_size / history_size, 0), lowest, highest);
      result = lerp(previous, current, history_weight);
   }
   history[pixel] = result;
}
