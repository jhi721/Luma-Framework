// Mass Effect 2 (2010) constant-colour fill (UE3 FOneColorPixelShader): screen fades, letterbox bars, menu
// backdrops. Vanilla is a bare `mov o0, cb4[8]` with no clamp; the saturate is the 8-bit canvas contract, as in the
// sibling ME1 2007 port. No texture, so no dgVoodoo mask.
#include "Includes/CanvasUI.hlsl"
// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). This shader reads nothing - the colour is the constant PsConstants[8].
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
   o0 = saturate(CanvasMul); // `mov o0.xyzw, cb4[8].xyzw` in the original
}
