// fx_ccr_hls_sc_gm_gi_of_mask (Y5R sh_devil_w64.par). See Includes/ColorCorrect.hlsl.
#define CCR_HLS                       1
#define CCR_SC                        1
#define CCR_GM                        1
#define CCR_GI                        1
#define CCR_OF                        1
#define CCR_MASK                      1
#define CCR_COLLECTION                0
#define CCR_BRIGHTNESS_AFTER_CONTRAST 1
#include "Includes/ColorCorrect.hlsl"

void main(float4 v0 : COLOR0, float4 v1 : TEXCOORD0, float4 v2 : SV_Position, out float4 o0 : SV_Target0)
{
   o0 = ColorCorrect(v0.w, v1);
}
