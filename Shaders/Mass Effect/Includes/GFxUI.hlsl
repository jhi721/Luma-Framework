#ifndef LUMA_ME1_GFX_UI
#define LUMA_ME1_GFX_UI

// Mass Effect (2007) - Scaleform GFx HUD/menu pixel shaders. Bodies transcribed from the dgVoodoo->ps_5_0 disasm; the
// ONLY change is the final saturate, which restores the write clamp of the vanilla 8-bit canvas.
//
// Why: the HUD draws onto the canvas AFTER FGammaCorrection (0x17CE0932), so it lands on the fp16 mirror Luma
// substitutes for the r8g8b8a8 canvas. D3D11 clamps a UNORM render target's source colour AND alpha to [0, 1] before
// blending; a float target never does. Every GFx shader ends in the Flash colour transform, colour * mul + add, with
// multipliers above 1 being ordinary Flash "brightness" effects, and an alpha above 1 turns the SrcAlpha/InvSrcAlpha
// blend into an extrapolation: measured in the docking-bay elevator as HUD elements at ~10k nits over a correct scene.
// The saturate is the vanilla contract, not a tuning choice; it applies before UI_DRAW_TYPE 2 scales the UI to its
// paper white in the display composition.
//
// Register map (Scaleform's D3D9 renderer, c0/c2/c3 -> dgVoodoo's b4 rows 8 + 0/2/3): the solid/text colour (cxform
// already folded in on the CPU) = PsConstants[8], mul = PsConstants[10], add = [11].

#include "GameBindings.hlsl"

#define GFxSolidColor PsConstants[8]
#define GFxCxformMul  PsConstants[10]
#define GFxCxformAdd  PsConstants[11]

// The Flash colour transform, applied to the whole float4 (alpha included) exactly as the originals do.
float4 GFxCxform(float4 color)
{
   return color * GFxCxformMul + GFxCxformAdd;
}

#endif // LUMA_ME1_GFX_UI
