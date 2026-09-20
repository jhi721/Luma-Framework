// UE3 FGammaCorrection, never yet seen drawing. Its input depends on what ran before it this frame; see
// RunME2GammaCorrection in Luma_ME2_Tonemap.hlsl.
#include "Luma_ME2_Tonemap.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   o0 = float4(RunME2GammaCorrection(v5.xy), 1.0); // alpha 1.0, as the original writes it
}
