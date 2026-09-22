// ps_ccr_____sc____gi___ (Y5R sh_devil_w64.par): UV in v0, alpha 1. See Includes/ColorCorrect.hlsl.
#define CCR_HLS                       0
#define CCR_SC                        1
#define CCR_GM                        0
#define CCR_GI                        1
#define CCR_OF                        0
#define CCR_MASK                      0
#define CCR_COLLECTION                0
#define CCR_BRIGHTNESS_AFTER_CONTRAST 1
#include "Includes/ColorCorrect.hlsl"

void main(float2 v0 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = ColorCorrect(1.0, float4(v0, 0.0, 0.0));
}
