// The shared fp16 pyramidal bloom in place of the vanilla glow pyramid (glow_pass1), fed by glow_pass0's level with its glow_pass2 weights folded in
// (Luma_YKR_Bloom.hlsl "fold_ps"): it blurs it as it is.
#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) (color)

#include "../Includes/Bloom.hlsl"
