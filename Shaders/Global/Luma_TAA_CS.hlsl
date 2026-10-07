#include "../Includes/Math.hlsl"

// Luma TAA: native-resolution temporal anti-aliasing resolve (no upscaling), for games without (usable) TAA or as a
// vendor-agnostic alternative to DLAA/FSR Native AA. Each component was picked by measurement against 64 spp ground
// truth over moving, HDR, noisy and sub-pixel test scenes (see "docs/Luma-TAA.md"):
// - 5-tap Catmull-Rom history (Jimenez, "Dynamic Temporal Antialiasing and Upsampling in Call of Duty"; Bevy)
// - motion vector of the closest depth in the neighborhood (Karis, "High Quality Temporal Supersampling")
// - "rounded" 3x3 + cross neighborhood box, history clipped toward the clamped box average (Playdead, INSIDE)
// - per-channel Reinhard around the final blend only (Godot/Spartan), scaled to compress HDR highlights only
// - accumulated history weight w' = 1 / (2 - w) (MiniEngine, Intel), capped by the resampling blur of the fractional
//   per frame displacement (Yang et al., "Amortized Supersampling")
// - at Ultra quality, a depth clip against the previous depth reconstructed from this frame (AMD FSR 2)
//
// Inputs follow the "SR::SuperResolutionImpl::DrawData" contract: device depth, motion vectors in any unit that
// "MotionVectorScale" converts to pixels such that previous position = current position + motion vector, and
// colors in linear (HDR) space. The history is a Luma owned RGBA16F texture: rgb is linear color, alpha the weight.

// Quality levels (texture fetches per pixel and composite score train / stress from docs/Luma-TAA.md; lower is better):
// 0 Low: 3x3 color box, motion vector of the closest depth in the cross, bilinear history (16, 8.79 / 17.02)
// 1 Medium: + 5-tap Catmull-Rom history (20, 8.62 / 15.69)
// 2 High: + closest depth over the full 3x3 (24, 8.38 / 15.69)
// 3 Ultra: + depth clip, which needs "reconstruct_previous_depth_cs" dispatched first (28, 8.01 / 15.69)
#ifndef TAA_QUALITY
#define TAA_QUALITY 2
#endif
// Upper bound of the history weight: at least 4% of every frame is kept (an effective window of ~25 frames), which
// bounds lag on lighting and shading changes that the neighborhood box can't catch.
#ifndef TAA_MAX_HISTORY_WEIGHT
#define TAA_MAX_HISTORY_WEIGHT 0.96
#endif
// Resampling the history at a fractional position blurs it every frame, by an amount set by the fractional part of the
// displacement (none at whole pixels, most at half pixels), not by the speed. The history weight is capped at
// tolerance / (tolerance + blur) so that blur stays bounded (Yang et al. 2009). The cap applies to this frame only: the
// accumulated weight keeps converging to it, so motion never compounds into the stored weight.
#ifndef TAA_RESAMPLING_BLUR_TOLERANCE
#define TAA_RESAMPLING_BLUR_TOLERANCE 0.4
#endif
// The blend runs on Reinhard(color * scale) / scale, so single bright samples (fireflies, sub-pixel HDR highlights) can't
// dominate the average while values well below 1 / scale (in the input's linear units) blend almost linearly: an
// unscaled Reinhard darkens every high contrast edge, as averaging in a compressed space loses energy.
#ifndef TAA_TONEMAP_SCALE
#define TAA_TONEMAP_SCALE 0.25
#endif
// Ultra quality depth clip: history is kept in proportion to tolerance * depth / separation when the current closest
// surface is farther than the nearest surface that moved to the reprojected position (a disocclusion). Relative linear
// depth, tuned on the lab's layered scenes; AMD FSR 2 derives a resolution-scaled one (~5% of depth at 1080p).
#ifndef TAA_DEPTH_TOLERANCE
#define TAA_DEPTH_TOLERANCE 0.3
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
// Linear depth bits (asuint), written by "reconstruct_previous_depth_cs" and read by the Ultra quality resolve.
Texture2D<uint> ReconstructedPreviousDepth : register(t4);

RWTexture2D<float4> OutputHistory : register(u0);
RWTexture2D<float4> OutputColor : register(u1);
RWTexture2D<uint> OutputReconstructedPreviousDepth : register(u2);

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

// The 4 bilinear taps around "position" (pixels, texel centers at integer + 0.5): the top left texel and the
// fractional weights. Taps are "base + int2(tap & 1, tap >> 1)" with weight lerp(1 - f, f, offset) per axis.
void BilinearFootprint(float2 position, out int2 base, out float2 f)
{
   const float2 corner = position - 0.5;
   base = int2(floor(corner));
   f = corner - base;
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

// Bilinear at Low quality, otherwise 5-tap Catmull-Rom (the 9-tap bilinear formulation without its 4 corner taps,
// weights not renormalized). "position" is in pixels, with texel centers at integer + 0.5.
float4 SampleHistory(float2 position)
{
#if TAA_QUALITY == 0
   return History.SampleLevel(LinearClampSampler, position * InvRenderResolution, 0);
#else
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
#endif
}

// Moves "history" along the segment towards "target" until it is inside the box (Playdead "clip_aabb").
float3 ClipToBox(float3 history, float3 target, float3 box_min, float3 box_max)
{
   const float3 delta = history - target;
   const float3 limit = (delta > 0.0) ? (box_max - target) : (box_min - target);
   const float3 scale = (abs(delta) > 1e-8) ? saturate(limit / delta) : 1.0;
   return target + delta * min3(scale);
}

#define RECONSTRUCT_TILE 16
groupshared uint reconstruct_tile[RECONSTRUCT_TILE * RECONSTRUCT_TILE];

[numthreads(8, 8, 1)] void reconstruct_previous_depth_cs(uint3 dispatch_thread_id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) {
   // Ultra quality first pass (AMD FSR 2 "reconstruct previous depth"): every pixel's closest 3x3 depth is scattered
   // along its motion vector into the previous frame's bilinear footprint (taps weighing more than 1%), keeping the
   // nearest. "OutputReconstructedPreviousDepth" must be cleared to asuint(FLT_MAX) (0x7F7FFFFF) before the dispatch;
   // positive floats order like their bits, so InterlockedMin keeps the nearest linear depth.
   const int2 pixel = dispatch_thread_id.xy;
   const int2 max_pixel = int2(RenderResolution) - 1;
   const bool inside = all(pixel <= max_pixel);

   // The group's taps usually land in one small area (neighbors share motion): reduce them in groupshared memory
   // first, anchored around where the group's center pixel lands, and write one global atomic per touched texel.
   const int2 group_center = min(int2(group_id.xy) * 8 + 4, max_pixel);
   const float2 center_previous = group_center + 0.5 + MotionVectors.Load(int3(group_center, 0)) * MotionVectorScale;
   const int2 tile_origin = int2(floor(center_previous)) - RECONSTRUCT_TILE / 2;
   [unroll] for (uint i = group_index; i < RECONSTRUCT_TILE * RECONSTRUCT_TILE; i += 64)
   {
      reconstruct_tile[i] = 0x7F7FFFFFu;
   }
   GroupMemoryBarrierWithGroupSync();

   if (inside)
   {
      const bool inverted_depth = (Flags & TAA_FLAG_INVERTED_DEPTH) != 0;
      float closest_depth = inverted_depth ? 0.0 : 1.0;
      int2 closest_pixel = pixel;
      [unroll] for (int y = -1; y <= 1; y++)
      {
         [unroll] for (int x = -1; x <= 1; x++)
         {
            const int2 sample_pixel = clamp(pixel + int2(x, y), 0, max_pixel);
            const float depth = DeviceDepth.Load(int3(sample_pixel, 0));
            const bool closer = inverted_depth ? (depth > closest_depth) : (depth < closest_depth);
            if (closer)
            {
               closest_depth = depth;
               closest_pixel = sample_pixel;
            }
         }
      }

      const uint linear_depth_bits = asuint(LinearizeDepth(closest_depth));
      const float2 previous_position = pixel + 0.5 + MotionVectors.Load(int3(closest_pixel, 0)) * MotionVectorScale;
      int2 footprint_base;
      float2 footprint_fraction;
      BilinearFootprint(previous_position, footprint_base, footprint_fraction);
      [unroll] for (uint tap = 0; tap < 4; tap++)
      {
         const int2 offset = int2(tap & 1, tap >> 1);
         const float2 axis_weights = offset ? footprint_fraction : (1.0 - footprint_fraction);
         const int2 tap_pixel = footprint_base + offset;
         if (axis_weights.x * axis_weights.y > 0.01 && all(tap_pixel >= 0) && all(tap_pixel <= max_pixel))
         {
            const int2 tile_pixel = tap_pixel - tile_origin;
            if (all(tile_pixel >= 0) && all(tile_pixel < RECONSTRUCT_TILE))
            {
               InterlockedMin(reconstruct_tile[tile_pixel.y * RECONSTRUCT_TILE + tile_pixel.x], linear_depth_bits);
            }
            else
            {
               InterlockedMin(OutputReconstructedPreviousDepth[tap_pixel], linear_depth_bits);
            }
         }
      }
   }
   GroupMemoryBarrierWithGroupSync();

   [unroll] for (uint j = group_index; j < RECONSTRUCT_TILE * RECONSTRUCT_TILE; j += 64)
   {
      const uint value = reconstruct_tile[j];
      if (value != 0x7F7FFFFFu)
      {
         InterlockedMin(OutputReconstructedPreviousDepth[tile_origin + int2(j % RECONSTRUCT_TILE, j / RECONSTRUCT_TILE)], value);
      }
   }
   // clang-format off
}

// clang-format parses a second "[numthreads] void f() {}" in a file as a lambda and indents it.
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
// clang-format on
{
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

         // Below High quality only the cross is searched for the closest depth.
         if (TAA_QUALITY < 2 && x != 0 && y != 0)
         {
            continue;
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

   const float4 history_sample = SampleHistory(history_position);
   // Clipping into the current neighborhood also removes the Catmull-Rom ringing.
   const float3 history = ClipToBox(history_sample.rgb, clamp(box_average, box_min, box_max), box_min, box_max);

#if TAA_QUALITY >= 3
   // Depth clip (AMD FSR 2): both depths come from this frame, so a camera moving along its view axis, which changes
   // every depth, can't make the test fail.
   const float current_linear_depth = LinearizeDepth(closest_depth);
   int2 footprint_base;
   float2 footprint_fraction;
   BilinearFootprint(history_position, footprint_base, footprint_fraction);
   float depth_clip_kept = 0.0;
   float depth_clip_weights = 0.0;
   [unroll] for (uint tap = 0; tap < 4; tap++)
   {
      const int2 offset = int2(tap & 1, tap >> 1);
      const float2 axis_weights = offset ? footprint_fraction : (1.0 - footprint_fraction);
      const float weight = axis_weights.x * axis_weights.y;
      const int2 tap_pixel = footprint_base + offset;
      if (weight <= 0.01 || any(tap_pixel < 0) || any(tap_pixel > max_pixel))
      {
         continue;
      }
      const float separation = current_linear_depth - asfloat(ReconstructedPreviousDepth.Load(int3(tap_pixel, 0)));
      depth_clip_kept += weight * ((separation > 0.0) ? saturate(TAA_DEPTH_TOLERANCE * current_linear_depth / separation) : 1.0);
      depth_clip_weights += weight;
   }
   const float depth_clip = (depth_clip_weights > 0.0) ? (depth_clip_kept / depth_clip_weights) : 1.0;
#else
   const float depth_clip = 1.0;
#endif

   const float2 fraction = frac(abs(motion));
   const float resampling_blur = 0.5 * (fraction.x * (1.0 - fraction.x) + fraction.y * (1.0 - fraction.y));
   const float blur_cap = TAA_RESAMPLING_BLUR_TOLERANCE / (TAA_RESAMPLING_BLUR_TOLERANCE + resampling_blur);
   float history_weight = min(history_sample.a, min(blur_cap, TAA_MAX_HISTORY_WEIGHT)) * depth_clip;
   if (offscreen || (Flags & TAA_FLAG_RESET))
   {
      history_weight = 0.0;
   }

   // Blending in a compressed space keeps single bright samples from dominating (fireflies, HDR edges).
   const float3 blended = lerp(Reinhard(current * TAA_TONEMAP_SCALE), Reinhard(history * TAA_TONEMAP_SCALE), history_weight);
   const float3 resolved = InverseReinhard(blended) / TAA_TONEMAP_SCALE;

   OutputHistory[pixel] = float4(resolved, rcp(2.0 - history_weight));
   // Written separately so the output can be the game's own texture (any format), while the history stays RGBA16F.
   OutputColor[pixel] = float4(resolved, 1.0);
}
