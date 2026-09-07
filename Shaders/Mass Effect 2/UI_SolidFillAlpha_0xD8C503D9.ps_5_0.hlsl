// Mass Effect 2 (2010) constant-colour fill with an interpolated alpha (UE3 FOneColorPixelShader, alpha from the
// vertex): the faded variant of 0xDE418D30. Vanilla writes both unclamped; the saturate is the 8-bit canvas
// contract, as in the sibling ME1 2007 port. No texture, so no dgVoodoo mask.
#include "Includes/CanvasUI.hlsl"
// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). This shader reads TEXCOORD5.w (v10.w) for the alpha; the colour is the constant PsConstants[8].
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
   o0 = saturate(float4(CanvasMul.rgb, v10.w)); // `mov o0.xyz, cb4[8]` + `mov o0.w, v10.wwww`
}
