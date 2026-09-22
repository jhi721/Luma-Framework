// The shared fp16 pyramidal bloom in place of the vanilla glow pyramid (glow_pass0/1), fed by the per-texel clamped
// prefilter of the glow source (Luma_YRC_Bloom.hlsl). DrawBloom runs at glow_pass0's draw, so its constants are still
// bound at b5: the bright pass is vanilla's own gain (cb5[2]) and saturate, applied to the blurred sum instead of per
// tap. DrawBloom binds only b11.

cbuffer GlowPass0 : register(b5)
{
   float4 cb5[3];
}

float3 yrc_bloom_threshold(float3 color)
{
   return saturate(max(color, 0.0) * cb5[2].rgb);
}

#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) yrc_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
