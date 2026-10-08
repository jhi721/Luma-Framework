#include "../Includes/Color.hlsl"
#include "../Includes/Math.hlsl"
#include "../Includes/Reinhard.hlsl"

// Luma TAA: temporal anti-aliasing resolve, at native resolution or upscaling ("TAA_UPSCALE"), for games without (usable)
// TAA or as a vendor-agnostic alternative to DLAA/FSR Native AA. Each component was picked by measurement against 64 spp
// ground truth over moving, HDR, noisy and sub-pixel test scenes (see "docs/Luma-TAA.md"):
// - 5-tap bicubic history: Catmull-Rom (sharpness c = 0.5), c = 0.4 for the 2x history and TAAU (Jimenez, "Dynamic
//   Temporal Antialiasing and Upsampling in Call of Duty"; Bevy)
// - motion vector of the closest depth in the neighborhood (Karis, "High Quality Temporal Supersampling")
// - "rounded" 3x3 + cross neighborhood box, history clipped toward the clamped box average (Playdead, INSIDE)
// - a linear blend, which keeps the energy of HDR highlights (optionally per-channel Reinhard around it, Godot/Spartan)
// - accumulated history weight w' = 1 / (2 - w) (MiniEngine, Intel), capped by the resampling blur of the fractional
//   per frame displacement (Yang et al., "Amortized Supersampling")
// - a thin feature lock (AMD FSR 2 "locks"): history isn't clipped where a sub-pixel highlight was seen recently
// - from Medium quality, a flickering analysis that widens the clip where the current frame aliases (UE TSR), and an
//   optional reactive mask (AMD FSR 2)
// - from High quality, the history at 2x2 the render resolution (UE TSR's history screen percentage 200)
// - at Ultra quality, a depth clip against the previous depth reconstructed from this frame (AMD FSR 2)
//
// Inputs follow the "SR::SuperResolutionImpl::DrawData" contract: device depth, motion vectors in any unit that
// "MotionVectorScale" converts to pixels such that previous position = current position + motion vector, and
// colors in linear BT.709 (scRGB: HDR values and negative channels are fine; the relative luminance below uses BT.709
// coefficients). The history is a Luma owned RGBA16F texture (2x2 the render resolution from High
// quality): rgb is linear color, alpha the accumulated weight.

// Quality levels (HDR-FLIP train / stress from docs/Luma-TAA.md, lower is better; GPU ms at 1080p / 4K on an RTX 4080
// SUPER, "taaperf" with coherent inputs and no reactive mask):
// 0 Low: 3x3 color box, motion vector of the closest depth in the cross, bilinear history (7.19 / 9.60; 0.12 / 0.52)
// 1 Medium: + 5-tap bicubic history, closest depth over the full 3x3, flickering analysis, reactive mask
//   (6.13 / 8.16; 0.14 / 0.59)
// 2 High: + history at 2x2 the render resolution (5.04 / 6.84; 0.29 / 1.19)
// 3 Ultra: + depth clip, which needs "reconstruct_previous_depth_cs" dispatched first (4.56 / 6.82; 0.34 / 1.40)
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
// 0 blends linearly: the history converges to the mean of the jittered samples, so sub-pixel HDR highlights keep their
// energy (77-90% in the lab's glint scenes with the lock below, the rest is lost to clipping on frames where the jitter
// misses them), at the cost of more twinkle. 1 blends in Reinhard space, with a peak of 1 / "TAA_TONEMAP_SCALE".
// Bright samples (sub-pixel HDR highlights, fireflies) then barely move the average, which steadies them but dims them
// (16-31% of their energy is left at a scale of 0.25), as averaging in a compressed space biases toward darker samples.
// This only applies to the render resolution history.
#ifndef TAA_REINHARD_BLEND
#define TAA_REINHARD_BLEND 0
#endif
#ifndef TAA_TONEMAP_SCALE
#define TAA_TONEMAP_SCALE 0.25
#endif
// Ultra quality depth clip: history is kept in proportion to tolerance * depth / separation when the current closest
// surface is farther than the nearest surface that moved to the reprojected position (a disocclusion). Relative linear
// depth, tuned on the lab's layered scenes; AMD FSR 2 derives a resolution-scaled one (~5% of depth at 1080p).
#ifndef TAA_DEPTH_TOLERANCE
#define TAA_DEPTH_TOLERANCE 0.3
#endif
// Thin feature lock: the neighborhood box is built from the current frame only, so a sub-pixel highlight that this frame's
// jitter missed is clipped out of the history, and the highlight twinkles and loses energy. A pixel whose relative
// luminance is above TAA_LOCK_MIN_LUMINANCE (in the input's linear units) and TAA_LOCK_RATIO times its neighbors' highest
// locks for TAA_LOCK_FRAMES frames (the jitter period: a highlight hit once per cycle stays locked), reprojected with the
// history, and locked history isn't clipped. Only on nearly static pixels (under TAA_LOCK_MAX_SPEED pixels per frame,
// both to create and to use a lock): a moving feature uncovers background whose reprojection lands on its old lock, and
// unclipped history would trail behind it.
#ifndef TAA_LOCK
#define TAA_LOCK 1
#endif
#ifndef TAA_LOCK_RATIO
#define TAA_LOCK_RATIO 2.0
#endif
#ifndef TAA_LOCK_MIN_LUMINANCE
#define TAA_LOCK_MIN_LUMINANCE 1.0
#endif
#ifndef TAA_LOCK_FRAMES
#define TAA_LOCK_FRAMES 16
#endif
#ifndef TAA_LOCK_MAX_SPEED
#define TAA_LOCK_MAX_SPEED 0.1
#endif
// Temporal upscaling (TAAU): the inputs are at "RenderResolution", while the history, the per pixel states and the
// output are at "OutputResolution", with one thread per output pixel. Each output pixel takes this frame's sample only
// when it fell inside it (the box splat of the 2x history, at the output pixel's size), otherwise a jitter-aware
// Gaussian of the 3x3 render pixels ("TAA_UPSCALE_SIGMA" output pixels, weighted by "TAA_UPSCALE_FALLBACK_WEIGHT").
// The history weight caps are scaled by the share of frames in which an output pixel receives a sample (render /
// output pixel area).
#ifndef TAA_UPSCALE
#define TAA_UPSCALE 0
#endif
// An output pixel without a sample this frame still takes the Gaussian estimate, at this weight times its nearest
// sample's Gaussian weight. This leaves fewer frames without any current information in motion, at the cost of a little
// sharpness (see "Temporal upscaling" in "docs/Luma-TAA.md").
#ifndef TAA_UPSCALE_FALLBACK_WEIGHT
#define TAA_UPSCALE_FALLBACK_WEIGHT 0.5
#endif
// The upscaled history's resampling blur tolerance, in output pixels (the 2x history's is 0.2)
#ifndef TAA_UPSCALE_BLUR_TOLERANCE
#define TAA_UPSCALE_BLUR_TOLERANCE 0.4
#endif
#ifndef TAA_UPSCALE_SIGMA
#define TAA_UPSCALE_SIGMA 0.5
#endif
// History at 2x2 the render resolution (UE TSR's "r.TSR.History.ScreenPercentage" 200), from High quality: repeated
// reprojection blurs it far less. Each thread resolves its pixel's 4 history texels and outputs their mean. A texel
// takes this frame's sample only when it fell in its quarter pixel. This is a box splat, so the mean of the 4 texels
// converges to the pixel's box filter. A texel without a sample falls back to a jitter-aware Gaussian of the 3x3 (sigma
// "TAA_HISTORY_2X_SIGMA" render pixels), but only where its history is rejected or under the reactive mask. The mean of
// the 4 history texels is clipped, and the texels keep their deviations from it (the sub-pixel detail), scaled by the
// mean's clip factor. The history textures must match Core's "LumaTAA::history_2x_quality".
#ifndef TAA_HISTORY_2X
#define TAA_HISTORY_2X (TAA_QUALITY >= 2 && !TAA_UPSCALE)
#endif
// Bicubic history sharpness c (Catmull-Rom is 0.5). The 2x history and TAAU resample at a higher frequency, where 0.4
// rings less and measured better in the lab (see "docs/Luma-TAA.md").
#ifndef TAA_HISTORY_SHARPNESS
#if TAA_HISTORY_2X || TAA_UPSCALE
#define TAA_HISTORY_SHARPNESS 0.4
#else
#define TAA_HISTORY_SHARPNESS 0.5
#endif
#endif
// The 2x history's resampling blur tolerance, in history texels. This is the lab optimum, the render resolution history
// uses "TAA_RESAMPLING_BLUR_TOLERANCE".
#ifndef TAA_HISTORY_2X_BLUR_TOLERANCE
#define TAA_HISTORY_2X_BLUR_TOLERANCE 0.2
#endif
// Share of frames in which a history texel receives a sample (1/4 for the box splat). The history weight caps are
// defined for a texel sampled every frame, so they are scaled by this to keep the same time constant for every texel.
#ifndef TAA_HISTORY_2X_RATE
#define TAA_HISTORY_2X_RATE 0.25
#endif
#ifndef TAA_HISTORY_2X_SIGMA
#define TAA_HISTORY_2X_SIGMA 0.3
#endif
// Under the reactive mask, a texel without a sample this frame also takes the Gaussian, with a weight of this value
// times the mask times the Gaussian's peak tap. The mask only lowers the history's cap, so such a texel would otherwise
// keep its history in full ("history / (history + 0)" is 1) until a sample lands in it, every 4th frame on average.
#ifndef TAA_HISTORY_2X_REACTIVE_FALLBACK
#define TAA_HISTORY_2X_REACTIVE_FALLBACK 0.25
#endif
// Flickering analysis (after UE TSR's): the 3x3 clip against a current frame that aliases (detail near the Nyquist
// limit, sub-pixel shading, alpha tested cutouts) throws away the history that would average the aliasing out, and the
// output flickers. Each pixel keeps the range (min/max) its current sample's luminance has been seen in, reprojected with
// the history and decaying toward the current value by TAA_FLICKER_ENVELOPE_DECAY per frame, and the clip box is widened
// by TAA_FLICKER_GAMMA times that range: on a static pixel that alternates between two values (a cutout appearing and
// vanishing between jitter phases), the history average of the two stays inside the box.
#ifndef TAA_FLICKER
#define TAA_FLICKER (TAA_QUALITY >= 1)
#endif
#ifndef TAA_FLICKER_GAMMA
#define TAA_FLICKER_GAMMA 1.0
#endif
#ifndef TAA_FLICKER_ENVELOPE_DECAY
#define TAA_FLICKER_ENVELOPE_DECAY 0.25
#endif
// The range starts over where the lighting changed, which a range alone can't tell from aliasing. That is when the
// luminance of the 3x3 neighborhood's darkest sample changes by more than "TAA_FLICKER_RESET_CHANGE" (relative) against
// its own exponential average ("TAA_FLICKER_RESET_SMOOTHING" per frame). 0 disables the test. A cutout or sub-pixel detail
// moves the 3x3 mean and max by up to 30x as it appears and vanishes, but the background around it keeps the min, while a
// lighting change moves the min too.
#ifndef TAA_FLICKER_RESET_CHANGE
#define TAA_FLICKER_RESET_CHANGE 0.9
#endif
#ifndef TAA_FLICKER_RESET_SMOOTHING
#define TAA_FLICKER_RESET_SMOOTHING 0.25
#endif
// The range also starts over where the pixel moved faster than this (pixels per frame) in the previous frame, so an object
// that passed over a static background leaves no trace in its range.
#ifndef TAA_FLICKER_MOTION_RESET_SPEED
#define TAA_FLICKER_MOTION_RESET_SPEED 0.5
#endif
// The widening fades out with motion, reaching none at this speed (pixels per frame): moving content uncovers and
// reprojects history whose clip must stay tight, or it trails (UE TSR disables its analysis on moving pixels too).
// 0 disables the fade.
#ifndef TAA_FLICKER_MAX_SPEED
#define TAA_FLICKER_MAX_SPEED 0.5
#endif
// Reactive mask input (AMD FSR 2 semantics, t7), drawn for example by a game's alpha blended particles. Where it is set,
// the history weight cap is scaled by 1 - mask (with the mask capped at "TAA_REACTIVE_MAX"), the flickering range starts
// over and nothing locks. It's only read when "TAA_FLAG_REACTIVE_MASK" says one is bound: loads from an unbound slot
// return 0 but aren't free (~0.15-0.2 ms at 4K on an RTX 4080 SUPER, against ~0.02 ms for a bound mask).
#ifndef TAA_REACTIVE
#define TAA_REACTIVE (TAA_QUALITY >= 1)
#endif
#ifndef TAA_REACTIVE_MAX
#define TAA_REACTIVE_MAX 0.9
#endif

#define TAA_FLAG_RESET          (1u << 0)
#define TAA_FLAG_INVERTED_DEPTH (1u << 1)
#define TAA_FLAG_REACTIVE_MASK  (1u << 2)

cbuffer LumaTAAData : register(b0)
{
   float2 RenderResolution;
   float2 InvRenderResolution;
   float2 MotionVectorScale;
   float2 DepthNearFar;
   uint Flags;
   float2 Jitter; // This frame's sample offset from the pixel center, in pixels
   float Padding;
   float2 OutputResolution; // Only used with "TAA_UPSCALE"
   float2 InvOutputResolution;
}

Texture2D<float3> SourceColor : register(t0);
Texture2D<float> DeviceDepth : register(t1);
Texture2D<float2> MotionVectors : register(t2);
Texture2D<float4> History : register(t3);
// Linear depth bits (asuint), written by "reconstruct_previous_depth_cs" and read by the Ultra quality resolve.
Texture2D<uint> ReconstructedPreviousDepth : register(t4);
Texture2D<uint> PreviousLock : register(t5); // Thin feature lock frames left (R8_UINT, alternating like the history)
// Flickering analysis state (R11G11B10_FLOAT, alternating like the history), see "OutputFlicker" in "main"
Texture2D<float3> PreviousFlicker : register(t6);
// Optional reactive mask (AMD FSR 2's): 1 where the color has no matching motion vectors (particles, VFX). See
// "TAA_REACTIVE".
Texture2D<float> ReactiveMask : register(t7);

RWTexture2D<float4> OutputHistory : register(u0);
RWTexture2D<float4> OutputColor : register(u1);
RWTexture2D<uint> OutputReconstructedPreviousDepth : register(u2);
RWTexture2D<uint> OutputLock : register(u3);
RWTexture2D<float3> OutputFlicker : register(u4);

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

// History weight cap for a displacement in history texels (see TAA_RESAMPLING_BLUR_TOLERANCE)
float ResamplingBlurCap(float2 displacement, float tolerance)
{
   const float2 fraction = frac(abs(displacement));
   const float resampling_blur = 0.5 * (fraction.x * (1.0 - fraction.x) + fraction.y * (1.0 - fraction.y));
   return tolerance / (tolerance + resampling_blur);
}

// Bilinear at Low quality, otherwise the 5-tap bicubic (the 9-tap bilinear formulation without its 4 corner taps,
// weights not renormalized). "position" is in pixels, with texel centers at integer + 0.5.
float4 SampleHistory(float2 position, float2 inv_resolution)
{
#if TAA_QUALITY == 0
   return History.SampleLevel(LinearClampSampler, position * inv_resolution, 0);
#else
   const float2 texel_center = floor(position - 0.5) + 0.5;
   const float2 f = position - texel_center;
   const float c = TAA_HISTORY_SHARPNESS;
   const float2 w0 = f * (-c + f * (2.0 * c - c * f));
   const float2 w1 = 1.0 + f * f * (-(3.0 - c) + (2.0 - c) * f);
   const float2 w2 = f * (c + f * ((3.0 - 2.0 * c) - (2.0 - c) * f));
   const float2 w3 = f * f * (-c + c * f);
   const float2 w12 = w1 + w2;
   const float2 uv0 = (texel_center - 1.0) * inv_resolution;
   const float2 uv3 = (texel_center + 2.0) * inv_resolution;
   const float2 uv12 = (texel_center + w2 / w12) * inv_resolution;
   float4 history = History.SampleLevel(LinearClampSampler, float2(uv12.x, uv0.y), 0) * (w12.x * w0.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv0.x, uv12.y), 0) * (w0.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, uv12, 0) * (w12.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv3.x, uv12.y), 0) * (w3.x * w12.y);
   history += History.SampleLevel(LinearClampSampler, float2(uv12.x, uv3.y), 0) * (w12.x * w3.y);
   // The negative lobes can take the weight below 0 next to texels without history (2x2 history and upscaling)
   history.a = max(history.a, 0.0);
   return history;
#endif
}

// Moves "history" along the segment towards "target" until it is inside the box (Playdead "clip_aabb").
float3 ClipToBox(float3 history, float3 target, float3 box_min, float3 box_max)
{
   const float3 delta = history - target;
   const float3 limit = ((delta > 0.0) ? (box_max - target) : (box_min - target));
   const float3 scale = ((abs(delta) > 1e-8) ? saturate(limit / delta) : 1.0);
   return target + delta * min3(scale);
}

// The pixel of the 3x3 neighborhood (only its cross with "cross_only") with the closest device depth, and that depth
void FindClosestDepth(int2 pixel, int2 max_pixel, bool inverted_depth, bool cross_only, out float closest_depth,
                      out int2 closest_pixel)
{
   closest_depth = (inverted_depth ? 0.0 : 1.0);
   closest_pixel = pixel;
   [unroll] for (int y = -1; y <= 1; y++)
   {
      [unroll] for (int x = -1; x <= 1; x++)
      {
         if (cross_only && x != 0 && y != 0)
         {
            continue;
         }
         const int2 sample_pixel = clamp(pixel + int2(x, y), 0, max_pixel);
         const float depth = DeviceDepth.Load(int3(sample_pixel, 0));
         const bool closer = (inverted_depth ? (depth > closest_depth) : (depth < closest_depth));
         if (closer)
         {
            closest_depth = depth;
            closest_pixel = sample_pixel;
         }
      }
   }
}

// Threads per group side (Core dispatches ceil(size / 8) groups per axis, see "LumaTAA.hpp")
#define TAA_GROUP_SIZE   8
#define RECONSTRUCT_TILE 16
groupshared uint reconstruct_tile[RECONSTRUCT_TILE * RECONSTRUCT_TILE];

[numthreads(TAA_GROUP_SIZE, TAA_GROUP_SIZE, 1)] void reconstruct_previous_depth_cs(uint3 dispatch_thread_id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) {
   // Ultra quality first pass (AMD FSR 2 "reconstruct previous depth"): every pixel's closest 3x3 depth is scattered
   // along its motion vector into the previous frame's bilinear footprint (taps weighing more than 1%), keeping the
   // nearest. "OutputReconstructedPreviousDepth" must be cleared to asuint(FLT_MAX) (0x7F7FFFFF) before the dispatch;
   // positive floats order like their bits, so InterlockedMin keeps the nearest linear depth.
   const int2 pixel = dispatch_thread_id.xy;
   const int2 max_pixel = int2(RenderResolution) - 1;
   const bool inside = all(pixel <= max_pixel);

   // The group's taps usually land in one small area (neighbors share motion): reduce them in groupshared memory
   // first, anchored around where the group's center pixel lands, and write one global atomic per touched texel.
   const int2 group_center = min(int2(group_id.xy) * TAA_GROUP_SIZE + TAA_GROUP_SIZE / 2, max_pixel);
   const float2 center_previous = group_center + 0.5 + MotionVectors.Load(int3(group_center, 0)) * MotionVectorScale;
   const int2 tile_origin = int2(floor(center_previous)) - RECONSTRUCT_TILE / 2;
   [unroll] for (uint i = group_index; i < RECONSTRUCT_TILE * RECONSTRUCT_TILE; i += TAA_GROUP_SIZE * TAA_GROUP_SIZE)
   {
      reconstruct_tile[i] = asuint(FLT_MAX);
   }
   GroupMemoryBarrierWithGroupSync();

   if (inside)
   {
      float closest_depth;
      int2 closest_pixel;
      FindClosestDepth(pixel, max_pixel, (Flags & TAA_FLAG_INVERTED_DEPTH) != 0, false, closest_depth, closest_pixel);

      const uint linear_depth_bits = asuint(LinearizeDepth(closest_depth));
      const float2 previous_position = pixel + 0.5 + MotionVectors.Load(int3(closest_pixel, 0)) * MotionVectorScale;
      int2 footprint_base;
      float2 footprint_fraction;
      BilinearFootprint(previous_position, footprint_base, footprint_fraction);
      [unroll] for (uint tap = 0; tap < 4; tap++)
      {
         const int2 offset = int2(tap & 1, tap >> 1);
         const float2 axis_weights = (offset ? footprint_fraction : (1.0 - footprint_fraction));
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

   [unroll] for (uint j = group_index; j < RECONSTRUCT_TILE * RECONSTRUCT_TILE; j += TAA_GROUP_SIZE * TAA_GROUP_SIZE)
   {
      const uint value = reconstruct_tile[j];
      if (value != asuint(FLT_MAX))
      {
         InterlockedMin(OutputReconstructedPreviousDepth[tile_origin + int2(j % RECONSTRUCT_TILE, j / RECONSTRUCT_TILE)], value);
      }
   }
   // clang-format off
}

// clang-format parses a second "[numthreads] void f() {}" in a file as a lambda and indents it.
[numthreads(TAA_GROUP_SIZE, TAA_GROUP_SIZE, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
// clang-format on
{
#if TAA_UPSCALE
   const int2 output_pixel = dispatch_thread_id.xy;
   if (any(output_pixel >= int2(OutputResolution)))
      return;
   const float2 render_scale = OutputResolution * InvRenderResolution; // Output pixels per render pixel
   // The output pixel's center in render pixels, and the render pixel it falls in
   const float2 sample_center = (output_pixel + 0.5) / render_scale;
   const int2 pixel = min(int2(sample_center), int2(RenderResolution) - 1);
#else
   const int2 pixel = dispatch_thread_id.xy;
   if (any(pixel >= int2(RenderResolution)))
      return;
#endif

   const int2 max_pixel = int2(RenderResolution) - 1;
   const bool inverted_depth = (Flags & TAA_FLAG_INVERTED_DEPTH) != 0;

   // 3x3 neighborhood color box (the 3x3 and the cross, averaged into a "rounded" box)
   float3 current = 0.0;
   float3 min_3x3 = FLT_MAX;
   float3 max_3x3 = -FLT_MAX;
   float3 sum_3x3 = 0.0;
   float3 min_cross = FLT_MAX;
   float3 max_cross = -FLT_MAX;
   float3 sum_cross = 0.0;
   float max_neighbor_luminance = 0.0; // For the thin feature lock
   // The flickering analysis uses the darkest sample's luminance. The luminance of the per channel min would be ~0 wherever
   // the samples have near-zero channels in different places, and hide lighting changes there.
   float min_luminance = FLT_MAX;
   float reactive = 0.0;
#if TAA_HISTORY_2X || TAA_UPSCALE
   float3 neighborhood[9];
#endif
   [unroll] for (int y = -1; y <= 1; y++)
   {
      [unroll] for (int x = -1; x <= 1; x++)
      {
         const int2 sample_pixel = clamp(pixel + int2(x, y), 0, max_pixel);
         const float3 color = SourceColor.Load(int3(sample_pixel, 0));
         const float luminance = dot(color, Rec709_Luminance);
         min_luminance = min(min_luminance, luminance);
#if TAA_LOCK
         if (x != 0 || y != 0)
         {
            max_neighbor_luminance = max(max_neighbor_luminance, luminance);
         }
#endif
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
#if TAA_HISTORY_2X || TAA_UPSCALE
         neighborhood[(y + 1) * 3 + (x + 1)] = color;
#endif
      }
   }
   float closest_depth;
   int2 closest_pixel;
   // At Low quality only the cross is searched
   FindClosestDepth(pixel, max_pixel, inverted_depth, TAA_QUALITY == 0, closest_depth, closest_pixel);
#if TAA_REACTIVE
   [branch] if (Flags & TAA_FLAG_REACTIVE_MASK)
   {
      [unroll] for (int y = -1; y <= 1; y++)
      {
         [unroll] for (int x = -1; x <= 1; x++)
         {
            reactive = max(reactive, ReactiveMask.Load(int3(clamp(pixel + int2(x, y), 0, max_pixel), 0)));
         }
      }
   }
#endif
   float3 box_min = 0.5 * (min_3x3 + min_cross);
   float3 box_max = 0.5 * (max_3x3 + max_cross);
   const float3 box_average = 0.5 * (sum_3x3 / 9.0 + sum_cross / 5.0);
   const float current_luminance = dot(current, Rec709_Luminance);

   const float2 motion = MotionVectors.Load(int3(closest_pixel, 0)) * MotionVectorScale;
   const float speed = length(motion);
#if TAA_UPSCALE
   const float2 history_position = sample_center + motion;
#else
   const float2 history_position = pixel + 0.5 + motion;
#endif
   const bool offscreen = any(history_position < 0.0) || any(history_position >= RenderResolution);
   // The per pixel states (lock, flickering analysis) are reprojected with the nearest texel, at the output resolution
   // when upscaling
#if TAA_UPSCALE
   const int2 state_pixel = output_pixel;
   const float2 state_history_position = history_position * render_scale;
   const bool state_offscreen = any(state_history_position < 0.0) || any(state_history_position >= OutputResolution);
#else
   const int2 state_pixel = pixel;
   const float2 state_history_position = history_position;
   const bool state_offscreen = offscreen;
#endif
   const bool has_previous_state = !state_offscreen && !(Flags & TAA_FLAG_RESET);
   const int2 previous_pixel = int2(floor(state_history_position));

#if TAA_FLICKER
   {
      // The state is R11G11B10_FLOAT, half the bytes of RGBA16F. It's unsigned with ~1.5% precision, which the range and
      // the 0.9 relative reset tolerate. x and y are the current sample luminance's range, where x > y means the pixel
      // moved faster than "TAA_FLICKER_MOTION_RESET_SPEED" and the range starts over. z is the average luminance of the 3x3
      // darkest sample, at least 2^-14 (the smallest normal), so the 0 of a cleared texture means no state. Values are
      // stored as they are, because D3D11 rounds float conversions toward zero and an offset (like "+ 1") would lose
      // small values.
      const float3 previous_state = (has_previous_state ? PreviousFlicker.Load(int3(previous_pixel, 0)) : 0.0);
      const bool has_state = previous_state.z > 0.0;
      const float lighting_tolerance = TAA_FLICKER_RESET_CHANGE * max(max(abs(min_luminance), abs(previous_state.z)), 1e-4);
      const bool lighting_changed = TAA_FLICKER_RESET_CHANGE > 0.0 && abs(min_luminance - previous_state.z) > lighting_tolerance;
      const bool moved = previous_state.x > previous_state.y;
      float range_min = current_luminance;
      float range_max = current_luminance;
      if (has_state && !lighting_changed && !moved && reactive <= 0.0)
      {
         range_min = min(current_luminance, lerp(previous_state.x, current_luminance, TAA_FLICKER_ENVELOPE_DECAY));
         range_max = max(current_luminance, lerp(previous_state.y, current_luminance, TAA_FLICKER_ENVELOPE_DECAY));
      }
      const float min_luminance_ema = lerp(previous_state.z, min_luminance, TAA_FLICKER_RESET_SMOOTHING);
      const float min_luminance_average = max((has_state ? min_luminance_ema : min_luminance), 6.103515625e-5);
      const float3 range_state = float3(max(range_min, 0.0), max(range_max, 0.0), min_luminance_average);
      // A range min above any max (R11G11B10 holds up to 65024) marks the pixel as moved
      OutputFlicker[state_pixel] = ((speed > TAA_FLICKER_MOTION_RESET_SPEED) ? float3(65000.0, 0.0, min_luminance_average) : range_state);
      float widening = TAA_FLICKER_GAMMA * (range_max - range_min);
      if (TAA_FLICKER_MAX_SPEED > 0.0)
      {
         widening *= saturate(1.0 - speed / TAA_FLICKER_MAX_SPEED);
      }
      box_min -= widening;
      box_max += widening;
   }
#endif
   const float max_history_weight = TAA_MAX_HISTORY_WEIGHT * (1.0 - min(reactive, TAA_REACTIVE_MAX));
   // Clipping into the current neighborhood also removes the bicubic's ringing.
   const float3 clip_target = clamp(box_average, box_min, box_max);

#if TAA_LOCK
   const bool lock_static = speed < TAA_LOCK_MAX_SPEED && reactive <= 0.0;
   uint lock_frames = 0;
   if (lock_static && current_luminance > TAA_LOCK_MIN_LUMINANCE && current_luminance > TAA_LOCK_RATIO * max_neighbor_luminance)
   {
      lock_frames = TAA_LOCK_FRAMES;
   }
   else if (has_previous_state)
   {
      const uint previous_lock = PreviousLock.Load(int3(previous_pixel, 0));
      lock_frames = ((previous_lock > 0) ? (previous_lock - 1) : 0);
   }
   const bool locked = lock_static && lock_frames > 0;
#else
   const bool locked = false;
#endif

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
      const float2 axis_weights = (offset ? footprint_fraction : (1.0 - footprint_fraction));
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
   const float depth_clip = ((depth_clip_weights > 0.0) ? (depth_clip_kept / depth_clip_weights) : 1.0);
#else
   const float depth_clip = 1.0;
#endif

#if TAA_UPSCALE
   const float cap_fraction = min(ResamplingBlurCap(motion * render_scale, TAA_UPSCALE_BLUR_TOLERANCE), max_history_weight);
   const float cap_samples = cap_fraction / (1.0 - cap_fraction) / (render_scale.x * render_scale.y);
   const float4 history_sample = SampleHistory(state_history_position, InvOutputResolution);
   const float3 history = (locked ? history_sample.rgb : ClipToBox(history_sample.rgb, clip_target, box_min, box_max));
   float3 current_sum = 0.0;
   float current_weight = 0.0;
   float3 gauss_sum = 0.0;
   float gauss_weight = 0.0;
   float gauss_max = 0.0;
   [unroll] for (int n = 0; n < 9; n++)
   {
      // This sample's offset from the output pixel's center, in output pixels
      const float2 offset = (pixel + float2(n % 3 - 1, n / 3 - 1) + 0.5 + Jitter - sample_center) * render_scale;
      const float box_weight = (all(abs(offset) < 0.5) ? 1.0 : 0.0);
      current_sum += neighborhood[n] * box_weight;
      current_weight += box_weight;
      const float gauss = exp(-0.5 * dot(offset, offset) / (TAA_UPSCALE_SIGMA * TAA_UPSCALE_SIGMA));
      gauss_sum += neighborhood[n] * gauss;
      gauss_weight += gauss;
      gauss_max = max(gauss_max, gauss);
   }
   const float3 current_output = ((current_weight > 0.0) ? (current_sum / current_weight) : (gauss_sum / gauss_weight));
   const float sample_weight = ((current_weight > 0.0) ? min(current_weight, 1.0) : (TAA_UPSCALE_FALLBACK_WEIGHT * gauss_max));
   const float history_samples = min(history_sample.a / max(1.0 - history_sample.a, 1e-4), cap_samples);
   const float history_validity = (has_previous_state ? depth_clip : 0.0);
   const float history_weight_output = history_samples * history_validity;
   // Rejected history (depth clip, offscreen) is replaced even in pixels that got no sample this frame
   const float blend = ((history_samples > 0.0) ? (history_samples / (history_samples + sample_weight)) : 0.0) * history_validity;
   const float3 resolved = lerp(current_output, history, blend);
   const float accumulated = history_weight_output + sample_weight;
   OutputHistory[output_pixel] = float4(resolved, accumulated / (accumulated + 1.0));
#if TAA_LOCK
   OutputLock[output_pixel] = ((history_weight_output > 0.0) ? lock_frames : 0);
#endif
   OutputColor[output_pixel] = float4(resolved, 1.0);
#elif TAA_HISTORY_2X
   const float cap_fraction = min(ResamplingBlurCap(motion * 2.0, TAA_HISTORY_2X_BLUR_TOLERANCE), max_history_weight);
   const float cap_samples = cap_fraction / (1.0 - cap_fraction) * TAA_HISTORY_2X_RATE;
   float2 history_positions_2x[4];
   float4 history_samples_2x[4];
   float3 history_mean = 0.0;
   [unroll] for (uint texel = 0; texel < 4; texel++)
   {
      history_positions_2x[texel] = (pixel + 0.25 + 0.5 * int2(texel & 1, texel >> 1) + motion) * 2.0;
      history_samples_2x[texel] = SampleHistory(history_positions_2x[texel], 0.5 * InvRenderResolution);
      history_mean += history_samples_2x[texel].rgb * 0.25;
   }
   const float3 clipped_mean = ClipToBox(history_mean, clip_target, box_min, box_max);
   const float3 mean_delta = history_mean - clip_target;
   const float mean_clip_ratio = saturate(length(clipped_mean - clip_target) / max(length(mean_delta), 1e-8));
   const float mean_clip_scale = ((dot(abs(mean_delta), 1.0) > 1e-8) ? mean_clip_ratio : 1.0);
   float3 output_sum = 0.0;
   bool any_history = false;
   [unroll] for (uint sub = 0; sub < 4; sub++)
   {
      const int2 sub_offset = int2(sub & 1, sub >> 1);
      const float2 center = pixel + 0.25 + 0.5 * sub_offset; // Render pixels
      const bool offscreen_2x = any(history_positions_2x[sub] < 0.0) || any(history_positions_2x[sub] >= RenderResolution * 2.0);
      const float4 history_sample_2x = history_samples_2x[sub];
      const float3 history_2x_clipped = clipped_mean + (history_sample_2x.rgb - history_mean) * mean_clip_scale;
      const float3 history_2x = (locked ? history_sample_2x.rgb : history_2x_clipped);
      float3 current_sum = 0.0;
      float current_weight = 0.0;
      float3 gauss_sum = 0.0;
      float gauss_weight = 0.0;
      float gauss_max = 0.0;
      [unroll] for (int n = 0; n < 9; n++)
      {
         const float2 offset = pixel + float2(n % 3 - 1, n / 3 - 1) + 0.5 + Jitter - center;
         const float box_weight = (all(abs(offset) < 0.25) ? 1.0 : 0.0);
         current_sum += neighborhood[n] * box_weight;
         current_weight += box_weight;
         const float gauss = exp(-0.5 * dot(offset, offset) / (TAA_HISTORY_2X_SIGMA * TAA_HISTORY_2X_SIGMA));
         gauss_sum += neighborhood[n] * gauss;
         gauss_weight += gauss;
         gauss_max = max(gauss_max, gauss);
      }
      const float3 current_2x = ((current_weight > 0.0) ? (current_sum / current_weight) : (gauss_sum / gauss_weight));
      const float reactive_fallback_weight = reactive * TAA_HISTORY_2X_REACTIVE_FALLBACK * gauss_max;
      const float sample_weight = ((current_weight > 0.0) ? min(current_weight, 1.0) : reactive_fallback_weight);
      const float history_samples = min(history_sample_2x.a / max(1.0 - history_sample_2x.a, 1e-4), cap_samples);
      const float history_validity = ((offscreen_2x || (Flags & TAA_FLAG_RESET)) ? 0.0 : depth_clip);
      const float history_weight_2x = history_samples * history_validity;
      // Rejected history (depth clip, offscreen) is replaced even in texels that got no sample this frame
      const float blend = ((history_samples > 0.0) ? (history_samples / (history_samples + sample_weight)) : 0.0) * history_validity;
      const float3 resolved_2x = lerp(current_2x, history_2x, blend);
      const float accumulated = history_weight_2x + sample_weight;
      OutputHistory[pixel * 2 + sub_offset] = float4(resolved_2x, accumulated / (accumulated + 1.0));
      output_sum += resolved_2x;
      any_history = any_history || (history_weight_2x > 0.0);
   }
#if TAA_LOCK
   OutputLock[pixel] = (any_history ? lock_frames : 0);
#endif
   OutputColor[pixel] = float4(output_sum * 0.25, 1.0);
#else
   const float4 history_sample = SampleHistory(history_position, InvRenderResolution);
   const float3 history = (locked ? history_sample.rgb : ClipToBox(history_sample.rgb, clip_target, box_min, box_max));
   const float history_weight_cap = min(ResamplingBlurCap(motion, TAA_RESAMPLING_BLUR_TOLERANCE), max_history_weight);
   const float history_weight = (has_previous_state ? (min(history_sample.a, history_weight_cap) * depth_clip) : 0.0);
   const float accumulated_weight = rcp(2.0 - history_weight);

#if TAA_REINHARD_BLEND
   // Sign preserving, so scRGB colors outside of BT.709 (negative channels) survive the round trip
   const float reinhard_peak = 1.0 / TAA_TONEMAP_SCALE;
   const float3 blended = lerp(Reinhard::ReinhardSimple(current, reinhard_peak), Reinhard::ReinhardSimple(history, reinhard_peak),
                               history_weight);
   const float3 resolved = Reinhard::InverseReinhardSimple(blended, reinhard_peak);
#else
   const float3 resolved = lerp(current, history, history_weight);
#endif

   OutputHistory[pixel] = float4(resolved, accumulated_weight);
#if TAA_LOCK
   OutputLock[pixel] = ((history_weight > 0.0) ? lock_frames : 0);
#endif
   // Written separately so the output can be the game's own texture (any format), while the history stays RGBA16F.
   OutputColor[pixel] = float4(resolved, 1.0);
#endif
}
