// Mass Effect 2 (2010) Luma HDR bloom pyramid (core DrawBloom). Input = the uber's scene texture (PS t0): fp16,
// LINEAR, pre-glow. Threshold 1.0 = the game's own bright-pass, knee = half. Same file as the ME1 2007 port.

// clang-format off
#include "Includes/Common.hlsl" // game-local: pulls GameCBuffers (LumaSettings) before the shared includes
// clang-format on

// The threshold function reads these two as macros, so a runtime cbuffer value works (BioShock precedent).
// b13 (LumaSettings) is re-bound by main.cpp right before DrawBloom, which owns b11 for its own constants.
#define LUMA_BLOOM_THRESHOLD max(0.0, LumaSettings.GameSettings.BloomThreshold)
#define LUMA_BLOOM_SOFT_KNEE (LUMA_BLOOM_THRESHOLD * 0.5)

// Vanilla's SHAPE, not quadratic_threshold: the gather keeps the WHOLE sample once a channel passes, and keeping
// only the excess measured 6x too dim at 1.2. Knee on the WEIGHT and one-sided, from the threshold up (NOTES.md).
float3 me2_bloom_threshold(float3 color)
{
   const float br = max(color.r, max(color.g, color.b));
   const float k = max(1e-6, LUMA_BLOOM_SOFT_KNEE);
   const float t = LUMA_BLOOM_THRESHOLD;
   return color * saturate((br - t) * rcp(k));
}
#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) me2_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
