#ifndef LUMA_ME2_CANVAS_UI
#define LUMA_ME2_CANVAS_UI

// Mass Effect 2 (2010) UE3 Canvas HUD/menu shaders, from the dgVoodoo->ps_5_0 disasm. The only change is a final
// saturate in each CALLER, restoring the vanilla 8-bit canvas write clamp: a float target clamps neither colour nor
// alpha, and an alpha above 1 turns the SrcAlpha/InvSrcAlpha blend into an extrapolation.

#include "GameBindings.hlsl"

#define CanvasMul   PsConstants[8]
#define CanvasAdd   PsConstants[9]
#define CanvasGamma PsConstants[10] // .x only

// The Canvas colour transform, exactly as both originals compute it: the mad runs on all four channels, alpha is
// written straight out, and only RGB takes the gamma pow.
float4 ME2_CanvasTransform(float4 color)
{
   const float4 c = color * CanvasMul + CanvasAdd;
   return float4(PowUE3(c.rgb, CanvasGamma.xxx), c.w);
}

#endif // LUMA_ME2_CANVAS_UI
