// Medal of Honor: Airborne - UE3 FGammaCorrectionPixelShader (VS 0xA2F269CA). HDR replacement: this pass, not
// UberPostProcessBlend, writes the final LDR canvas when "bAllowDepthOfField = False". See Luma_MOHA_Tonemap.hlsl.
#include "Luma_MOHA_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_MOHA_Tonemap.hlsl). This pass reads only TEXCOORD0, the scene UV in v5.
void main(DGV_MAIN_SIGNATURE)
{
   // Alpha is written as 1.0 by the original (UberPostProcessBlend writes -0.0 instead). This canvas' alpha is never
   // read: the HUD blends srcAlpha=zero/destAlpha=one and the final blit into the scRGB swapchain ignores it.
   o0 = float4(RunMOHAGammaCorrection(v5.xy), 1.0);
}
