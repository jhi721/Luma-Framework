// Mass Effect 3 (2012) - Luma HDR bloom pyramid (core DrawBloom), ME1 2007's setup. Input = a Karis average of the uber's
// scene texture (PS t0): fp16, LINEAR, pre-glow.

// clang-format off
#include "Includes/Common.hlsl" // game-local: pulls GameCBuffers (LumaSettings) before the shared includes
// clang-format on

// A runtime cbuffer value. main.cpp re-binds LumaSettings before DrawBloom, which owns b11.
#define LUMA_BLOOM_THRESHOLD max(0.0, LumaSettings.GameSettings.BloomThreshold)

// Vanilla's SHAPE (gather 0x699E0C60 / downsample 0xC6215545): the WHOLE sample weighted by a one-sided knee that
// starts at the threshold and is twice its width, saturate((max(rgb) - 1) * 0.5) at threshold 1. The shared quadratic
// keeps only the excess.
float3 me3_bloom_threshold(float3 color)
{
   return color * saturate((max3(color) - LUMA_BLOOM_THRESHOLD) * rcp(max(1e-6, LUMA_BLOOM_THRESHOLD * 2.0)));
}
#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) me3_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
