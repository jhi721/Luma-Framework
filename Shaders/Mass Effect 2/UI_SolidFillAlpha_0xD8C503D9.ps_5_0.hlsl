// Mass Effect 2 (2010) constant-colour fill with an interpolated alpha (UE3 FOneColorPixelShader, alpha from the
// vertex): the faded variant of 0xDE418D30. Vanilla writes both unclamped; the saturate is the 8-bit canvas
// contract, as in the sibling ME1 2007 port. No texture, so no dgVoodoo mask.
#include "Includes/CanvasUI.hlsl"
// This shader reads TEXCOORD5.w (v10.w) for the alpha; the colour is the constant PsConstants[8].
void main(ME2_MAIN_SIGNATURE)
{
   o0 = saturate(float4(CanvasMul.rgb, v10.w)); // `mov o0.xyz, cb4[8]` + `mov o0.w, v10.wwww`
}
