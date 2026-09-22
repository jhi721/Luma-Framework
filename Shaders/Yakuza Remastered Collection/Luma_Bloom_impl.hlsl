// The shared fp16 pyramidal bloom in place of the vanilla glow pyramid (glow_pass0/1), fed by the per-texel clamped
// prefilter of the glow source (Luma_YRC_Bloom.hlsl). It blurs the glow as it is: glow_pass0's per-scene gains (its
// cb5[2]) weight color and luma differently, so the composite applies them after the blur (bloom_composite_ps).

float3 yrc_bloom_threshold(float3 color)
{
   return saturate(color);
}

#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) yrc_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
