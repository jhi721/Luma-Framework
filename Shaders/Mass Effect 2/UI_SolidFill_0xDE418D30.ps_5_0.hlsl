// Mass Effect 2 (2010) constant-colour fill (UE3 FOneColorPixelShader): screen fades, letterbox bars, menu
// backdrops. Vanilla is a bare `mov o0, cb4[8]` with no clamp; the saturate is the 8-bit canvas contract, as in the
// sibling ME1 2007 port. No texture, so no dgVoodoo mask.
#include "Includes/CanvasUI.hlsl"
// This shader reads nothing - the colour is the constant PsConstants[8].
void main(ME2_MAIN_SIGNATURE)
{
   o0 = saturate(CanvasMul); // `mov o0.xyzw, cb4[8].xyzw` in the original
}
