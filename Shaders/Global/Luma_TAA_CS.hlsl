#include "../Includes/Math.hlsl"

// Luma TAA: native-resolution temporal anti-aliasing resolve (no upscaling), for games without (usable) TAA or as a
// vendor-agnostic alternative to DLAA/FSR Native AA. Each component was picked by measurement against 64 spp ground
// truth over moving, HDR, noisy and sub-pixel test scenes (see "docs/Luma-TAA.md"):
// - 5-tap Catmull-Rom history (Jimenez, "Dynamic Temporal Antialiasing and Upsampling in Call of Duty"; Bevy)
// - motion vector of the closest depth in the 3x3 neighborhood (Karis, "High Quality Temporal Supersampling")
// - "rounded" 3x3 + cross neighborhood box, history clipped toward the clamped box average (Playdead, INSIDE)
// - per-channel Reinhard around the final blend only (Godot/Spartan)
// - accumulated history weight w' = 1 / (2 - w) (MiniEngine, Intel), capped, attenuated by speed and reset by a
//   depth disocclusion test (MiniEngine, Intel)
//
// Inputs follow the "SR::SuperResolutionImpl::DrawData" contract: device depth, motion vectors in any unit that
// "MotionVectorScale" converts to pixels such that previous position = current position + motion vector, and
// colors in linear (HDR) space. The history is a Luma owned RGBA16F texture: rgb is linear color, alpha the weight.

// How much the history weight is reduced per pixel of motion; motion vectors resampled every frame blur the history,
// so faster pixels converge to the current frame (8 pixels/frame and faster use no history).
#ifndef TAA_SPEED_LIMIT
#define TAA_SPEED_LIMIT 8.0
#endif
// Upper bound of the history weight: at least 6% of every frame is kept (an effective window of ~16 frames, the
// Halton 16 jitter period), which bounds lag on lighting and shading changes that the neighborhood box can't catch.
#ifndef TAA_MAX_HISTORY_WEIGHT
#define TAA_MAX_HISTORY_WEIGHT 0.94
#endif
// Rejects history where the closest depth is behind everything that was there last frame. It catches disocclusions
// whose colors fall inside the neighborhood box; disabling it removes the previous depth input (and its copy).
#ifndef TAA_DEPTH_DISOCCLUSION
#define TAA_DEPTH_DISOCCLUSION 1
#endif
// Relative linear depth tolerance of the disocclusion test.
#ifndef TAA_DEPTH_TOLERANCE
#define TAA_DEPTH_TOLERANCE 0.01
#endif

#define TAA_FLAG_RESET          (1u << 0)
#define TAA_FLAG_INVERTED_DEPTH (1u << 1)

cbuffer LumaTAAData : register(b0)
{
   float2 RenderResolution;
   float2 InvRenderResolution;
   float2 MotionVectorScale;
   float2 DepthNearFar;
   uint Flags;
   float3 Padding;
}

Texture2D<float3> SourceColor : register(t0);
Texture2D<float> DeviceDepth : register(t1);
Texture2D<float2> MotionVectors : register(t2);
Texture2D<float4> History : register(t3);
Texture2D<float> PreviousDeviceDepth : register(t4);

RWTexture2D<float4> OutputHistory : register(u0);
RWTexture2D<float4> OutputColor : register(u1);

SamplerState LinearClampSampler : register(s0);

float LinearizeDepth(float device_depth)
{
   const float near = DepthNearFar.x;
   const float far = DepthNearFar.y;
   if (Flags & TAA_FLAG_INVERTED_DEPTH)
   {
      return near * far / (near + device_depth * (far - near));
   }
   return near * far / (far - device_depth * (far - near));
}

// Sign preserving, so scRGB colors outside of BT.709 (negative channels) survive the round trip.
float3 Reinhard(float3 color)
{
   return color / (1.0 + abs(color));
}

float3 InverseReinhard(float3 color)
{
   return color / max(1.0 - abs(color), 1e-6);
}

// 5-tap Catmull-Rom (the 9-tap bilinear formulation without its 4 corner taps, weights not renormalized).
// "position" is in pixels, with texel centers at integer + 0.5.
float4 SampleHistoryCatmullRom(float2 position)
{
   const float2 texel_center = floor(position - 0.5) + 0.5;
   const float2 f = position - texel_center;
   const float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
   const float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
   const float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
   const float2 w3 = f * f * (-0.5 + 0.5 * f);
   const float2 w12 = w1 + w2;
   const float2 uv0 = (texel_center - 1.0) * InvRenderResolution;
   const float2 uv3 = (texel_center + 2.0) * InvRenderResolution;
   const float2 uv12 = (texel_center + w2 / w12) * InvRenderResolution;
   float4 history = History.SampleLevel(LinearClampSampler, float2(uv12.x, uv0.y), 0) * (w12.x * w0.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv0.x, uv12.y), 0) * (w0.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, uv12, 0) * (w12.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv3.x, uv12.y), 0) * (w3.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv12.x, uv3.y), 0) * (w12.x * w3.y);
   return history;
}

// Moves "history" along the segment towards "target" until it is inside the box (Playdead "clip_aabb").
float3 ClipToBox(float3 history, float3 target, float3 box_min, float3 box_max)
{
   const float3 delta = history - target;
   const float3 limit = (delta > 0.0) ? (box_max - target) : (box_min - target);
   const float3 scale = (abs(delta) > 1e-8) ? saturate(limit / delta) : 1.0;
   return target + delta * min3(scale);
}

[numthreads(8, 8, 1)] void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
   const int2 pixel = dispatch_thread_id.xy;
   if (any(pixel >= int2(RenderResolution)))
      return;

   const int2 max_pixel = int2(RenderResolution) - 1;
   const bool inverted_depth = (Flags & TAA_FLAG_INVERTED_DEPTH) != 0;

   // 3x3 neighborhood: color box (3x3 and cross, averaged into a "rounded" box) and the closest depth.
   float3 current = 0.0;
   float3 min_3x3 = FLT_MAX;
   float3 max_3x3 = -FLT_MAX;
   float3 sum_3x3 = 0.0;
   float3 min_cross = FLT_MAX;
   float3 max_cross = -FLT_MAX;
   float3 sum_cross = 0.0;
   float closest_depth = inverted_depth ? 0.0 : 1.0;
   int2 closest_pixel = pixel;
   [unroll] for (int y = -1; y <= 1; y++)
   {
      [unroll] for (int x = -1; x <= 1; x++)
      {
         const int2 sample_pixel = clamp(pixel + int2(x, y), 0, max_pixel);
         const float3 color = SourceColor.Load(int3(sample_pixel, 0));
         min_3x3 = min(min_3x3, color);
         max_3x3 = max(max_3x3, color);
         sum_3x3 += color;
         if (x == 0 || y == 0)
         {
            min_cross = min(min_cross, color);
            max_cross = max(max_cross, color);
            sum_cross += color;
         }
         if (x == 0 && y == 0)
         {
            current = color;
         }

         const float depth = DeviceDepth.Load(int3(sample_pixel, 0));
         const bool closer = inverted_depth ? (depth > closest_depth) : (depth < closest_depth);
         if (closer)
         {
            closest_depth = depth;
            closest_pixel = sample_pixel;
         }
      }
   }
   const float3 box_min = 0.5 * (min_3x3 + min_cross);
   const float3 box_max = 0.5 * (max_3x3 + max_cross);
   const float3 box_average = 0.5 * (sum_3x3 / 9.0 + sum_cross / 5.0);

   const float2 motion = MotionVectors.Load(int3(closest_pixel, 0)) * MotionVectorScale;
   const float2 history_position = pixel + 0.5 + motion;
   const bool offscreen = any(history_position < 0.0) || any(history_position >= RenderResolution);

#if TAA_DEPTH_DISOCCLUSION
   // Disocclusion: the closest surface is farther than everything that was around the reprojected position.
   const float4 previous_depths = PreviousDeviceDepth.GatherRed(LinearClampSampler, history_position * InvRenderResolution);
   const float previous_farthest = inverted_depth ? min(min(previous_depths.x, previous_depths.y), min(previous_depths.z, previous_depths.w))
                                                  : max(max(previous_depths.x, previous_depths.y), max(previous_depths.z, previous_depths.w));
   const bool disoccluded = LinearizeDepth(closest_depth) > LinearizeDepth(previous_farthest) * (1.0 + TAA_DEPTH_TOLERANCE);
#else
   const bool disoccluded = false;
#endif

   const float4 history_sample = SampleHistoryCatmullRom(history_position);
   // Clipping into the current neighborhood also removes the Catmull-Rom ringing.
   const float3 history = ClipToBox(history_sample.rgb, clamp(box_average, box_min, box_max), box_min, box_max);

   float history_weight = min(history_sample.a * saturate(1.0 - length(motion) / TAA_SPEED_LIMIT), TAA_MAX_HISTORY_WEIGHT);
   if (offscreen || disoccluded || (Flags & TAA_FLAG_RESET))
   {
      history_weight = 0.0;
   }

   // Blending in a compressed space keeps single bright samples from dominating (fireflies, HDR edges).
   const float3 resolved = InverseReinhard(lerp(Reinhard(current), Reinhard(history), history_weight));

   OutputHistory[pixel] = float4(resolved, rcp(2.0 - history_weight));
   // Written separately so the output can be the game's own texture (any format), while the history stays RGBA16F.
   OutputColor[pixel] = float4(resolved, 1.0);
}
