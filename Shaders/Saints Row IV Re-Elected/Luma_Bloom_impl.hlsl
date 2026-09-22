// Saints Row IV - the shared fp16 pyramidal bloom, built from the rl_hdr final's own scene input (t0) and bound
// over the game's Final_bloom (t6). The prefilter replays the native rl_hdr_prep_08 brightpass
// and rl_hdr_prep_07 combine on their own constants, copied at their draws earlier in the frame (main.cpp), with the tint of
// the rl_downsample_02 that feeds the brightpass (quarter-res 16-tap box; the pyramid's own first blur stands in for
// it), so the knee, its per-area values and the chain's tints are the game's. The scene is unexposed here, as for the
// native pass. DrawBloom binds only b11; main.cpp binds the copies at b0/b4/b5/b6 around it.

#include "../Includes/Math.hlsl"

cbuffer BrightpassVC0 : register(b0)
{
   float4 Bloom_curve_values : packoffset(c22);
}

cbuffer BrightpassVC4 : register(b4)
{
   float4 Brightpass_tint : packoffset(c1);
}

cbuffer CombineVC4 : register(b5)
{
   float4 Combine_tint : packoffset(c1);
}

cbuffer SourceDownsampleVC4 : register(b6)
{
   float4 Source_downsample_tint : packoffset(c1);
}

// rl_hdr_prep_08 0x12F9FD30, transcribed: the input capped at 32 after its tint, then S = 0.3 * (r + g + b), an equal-weight
// channel sum; below K.x the weight is linear (S * K.y), above it K.z * |K.x - S| + K.w; the colour is rescaled to that
// weight over S and tripled. Then rl_hdr_prep_07's tint.
float3 sr4_bloom_threshold(float3 color)
{
   // Non-finite taps poison a whole Gaussian kernel, negative ones would blur in as a hue shift.
   color = (IsAnyNaN_Strict(color) || any(IsInfinite_Strict(color))) ? 0.0 : max(color, 0.0);
   color = min(color * Source_downsample_tint.rgb * Brightpass_tint.w, 32.0);
   const float channelSum = dot(color, 0.3);
   const float4 k = Bloom_curve_values;
   const float weight = channelSum < k.x ? channelSum * k.y : k.z * abs(k.x - channelSum) + k.w;
   return max(color * (weight / (channelSum + 0.001) * 3.0), 0.0) * Combine_tint.rgb;
}

#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) sr4_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
