// Mass Effect (2007) - UE3 FGammaCorrectionPixelShader, the frame's LAST colour pass, always: reads the intermediate,
// writes the canvas the HUD blends onto. UberRanThisFrame selects chained vs alone (elevators, loading). See
// Luma_ME1_Tonemap.hlsl.
#include "Luma_ME1_Tonemap.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl). This pass reads only
// TEXCOORD0, the scene UV in v5.
void main(DGV_MAIN_SIGNATURE)
{
   // Alpha is written as 1.0 by the original (UberPostProcessBlend writes -0.0 instead). This canvas' alpha is never
   // read.
   o0 = float4(RunME1GammaCorrection(v5.xy), 1.0);
}
