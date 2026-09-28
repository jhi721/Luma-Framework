// Mass Effect 2 (2010) BioSceneEffect / ImageAdjustments material, vignette only (film grain OFF). This pass, not
// the uber, is the frame's LAST colour pass: its render target feeds the closing present blit. See Luma_ME2_Tonemap.hlsl.
#include "Luma_ME2_Tonemap.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   // Vanilla writes v10.w into alpha, which the pass's 0x7 write mask then discards; kept for fidelity.
   o0 = float4(RunME2Material(v5.xy, v10.xyw), v10.w);
}
