// Mass Effect (2007) - constant-colour fill with an INTERPOLATED alpha, sibling of UI_SolidFill_0xDE418D30 (constant
// alpha); the dump sweep found only those two. Pre-emptive: an unclamped constant plus SrcAlpha extrapolation is the
// 10k-nit HUD. See Includes/GFxUI.hlsl: vanilla body plus the 8-bit canvas clamp.
#include "Includes/GFxUI.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This shader reads only
// TEXCOORD5.w (v10.w), its alpha.
void main(DGV_MAIN_SIGNATURE)
{
   // The original declares no dgVoodoo mask cbuffer (CB4 only), so there is no ApplyDgvMask here: it samples nothing.
   o0 = saturate(float4(GFxSolidColor.xyz, v10.w));
}
