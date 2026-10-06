// DLSS / FSR: the depth of field blur amount of every scene pixel, accumulated over frames like the upscaler accumulates color, for
// the DoF/bloom gather (DofBloomGather_0x33BD72CF.ps_5_0.hlsl, t1). The gather takes its amount from the mean depth of its 4
// bilinear taps (a 4x4 block), and the depth in the scene's alpha is jittered: at a silhouette against something far, the block's
// mean flips across the focus curve every frame and the block flips between sharp and blurred after the upscaler (measured 0 <->
// full blur on the edge of a window, aiming). Each pixel's amount (the gather's formula and constants) is blended into its history,
// reprojected with the motion vectors and clamped to the current 3x3 range; the gather averages the history at its taps. BL GOTY's
// DOF history (docs/DLAA-From-Scratch-Lessons.md, "Block-level screen terms flip under the jitter").

// clang-format off
#include "Includes/GameCBuffers.hlsl"
#include "Includes/GameBindings.hlsl" // b3/b4 (the gather's, bound here), the dgVoodoo masks, DoFBlurAmount
// clang-format on

#define DoFParams  PsConstants[8] // As the gather reads them
#define DoFMaxBlur PsConstants[9]

cbuffer DOFHistory : register(b0)
{
   CB::DOFHistoryConstants constants;
};

Texture2D<float4> scene : register(t0);          // What the gather reads: fp16, alpha = linear depth
Texture2D<float2> motion_vectors : register(t1); // UV, previous minus current
Texture2D<float> previous_history : register(t2);
RWTexture2D<float> history : register(u0);
SamplerState linear_sampler : register(s0);

static const uint TILE = 8;                     // A group's pixels per side
static const uint APRON = TILE + 2;             // With the 1 pixel border the 3x3 range reads
groupshared float group_amounts[APRON * APRON]; // Each amount computed once

[numthreads(TILE, TILE, 1)] void main(uint3 id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID, uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) {
   uint2 history_size;
   history.GetDimensions(history_size.x, history_size.y);
   const int2 size = int2(history_size);
   // No DoF (every amount is capped to 0): nothing to read or reproject
   if (max(DoFMaxBlur.x, DoFMaxBlur.y) <= 0.0)
   {
      if (all(id.xy < history_size))
      {
         history[id.xy] = 0.0;
      }
      return;
   }
   const int2 tile_origin = int2(group_id.xy * TILE) - 1;
   for (uint i = group_index; i < APRON * APRON; i += TILE * TILE)
   {
      const int2 pixel = clamp(tile_origin + int2(i % APRON, i / APRON), 0, size - 1);
      const float depth = ApplyDgvMask(scene.Load(int3(pixel, 0)), DgvMaskT0, DgvFillT0).a;
      group_amounts[i] = DoFBlurAmount(depth, DoFParams, DoFMaxBlur);
   }
   GroupMemoryBarrierWithGroupSync();
   if (any(id.xy >= history_size))
      return;

   const int2 tile_pixel = int2(group_thread_id.xy) + 1;
   const float current = group_amounts[tile_pixel.y * APRON + tile_pixel.x];
   float result = current;
   const float2 uv = (float2(id.xy) + 0.5) / float2(size) + motion_vectors.Load(int3(id.xy, 0));
   if (constants.history_weight < 1.0 && all(uv >= 0.0) && all(uv <= 1.0))
   {
      float lowest = current, highest = current;
      [unroll] for (int y = -1; y <= 1; y++)
      {
         [unroll] for (int x = -1; x <= 1; x++)
         {
            const float neighbor = group_amounts[(tile_pixel.y + y) * APRON + tile_pixel.x + x];
            lowest = min(lowest, neighbor);
            highest = max(highest, neighbor);
         }
      }
      const float previous = clamp(previous_history.SampleLevel(linear_sampler, uv, 0), lowest, highest);
      result = lerp(previous, current, constants.history_weight);
   }
   history[id.xy] = result;
}
