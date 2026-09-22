// ps_ccr_____sc_gm______ (sh_ogre3_w64.par). See Includes/ColorCorrect.hlsl.
#define CCR_HLS        0
#define CCR_SC         1
#define CCR_GM         1
#define CCR_GI         0
#define CCR_OF         0
#define CCR_MASK       0
#define CCR_COLLECTION 0
#include "Includes/ColorCorrect.hlsl"

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
   o0 = ColorCorrect(v0.w, float4(v1, 0.0, 0.0));
}
