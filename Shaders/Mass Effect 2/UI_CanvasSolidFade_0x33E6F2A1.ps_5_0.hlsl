// Mass Effect 2 (2010) UE3 Canvas solid-colour quad with a fade: as 0x1B3AC3BE, plus alpha scaled by COLOR1.w.
// See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"

// This shader reads COLOR0 (v2) and COLOR1.w (v3.w).
void main(ME2_MAIN_SIGNATURE)
{
   const float4 c = ME2_CanvasTransform(v2);
   o0 = saturate(float4(c.rgb, c.w * v3.w)); // `mul o0.w, r0.wwww, v3.wwww` in the original
}
