// Mass Effect (2007) - constant-colour fill with an INTERPOLATED alpha: the sibling of UI_SolidFill_0xDE418D30, which
// takes its alpha from the constant instead. Found by a signature sweep of the 724-shader dump (2026-09-02): the two
// are the only members of their family, and this one had no replacement. Never yet caught on the canvas by the DEV
// net, so it is pre-emptive - but it writes an UNCLAMPED constant to a buffer whose 8-bit write clamp the fp16 upgrade
// removed, which is exactly what produced the 10k-nit HUD, and blending by a vertex alpha makes the overshoot worse
// rather than better (SrcAlpha/InvSrcAlpha extrapolates once alpha leaves 0..1). See Includes/GFxUI.hlsl.
#include "Includes/GFxUI.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME1_Tonemap.hlsl). This shader reads only TEXCOORD5.w (v10.w), its alpha.
void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   // The original declares no dgVoodoo mask cbuffer (CB4 only), so there is no ApplyDgvMask here: it samples nothing.
   o0 = saturate(float4(GFxSolidColor.xyz, v10.w));
}
