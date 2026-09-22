// fx_ccr_hls_sc_gm_gi_of_mask (sh_ogre3_w64.par). See Includes/ColorCorrect.hlsl.
#define CCR_HLS        1
#define CCR_SC         1
#define CCR_GM         1
#define CCR_GI         1
#define CCR_OF         1
#define CCR_MASK       1
#define CCR_COLLECTION 0
#include "Includes/ColorCorrect.hlsl"

void main(float4 v0 : COLOR0, float4 v1 : SV_Position, float4 v2 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = ColorCorrect(v0.w, v2);
}
