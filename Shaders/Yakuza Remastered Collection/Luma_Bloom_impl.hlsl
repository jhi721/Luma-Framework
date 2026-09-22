// The shared fp16 pyramidal bloom in place of the vanilla glow pyramid (glow_pass0/1), fed by glow_pass0's output
// (Luma_YRC_GlowGain.hlsl: rgb + alpha, each already saturated per texel, so up to 2). It blurs it as it is.
#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) (color)

#include "../Includes/Bloom.hlsl"
