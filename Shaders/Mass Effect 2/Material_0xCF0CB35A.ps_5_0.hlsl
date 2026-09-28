// Mass Effect 2 (2010) BioSceneEffect material, vignette + FILM GRAIN, selected by [SystemSettings] FilmGrain=True.
// The frame's LAST colour pass: its render target feeds the closing present blit. See Luma_ME2_Tonemap.hlsl.
#define ME2_MATERIAL_GRAIN 1
#include "Luma_ME2_Tonemap.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   // Vanilla writes v10.w into alpha, which the pass's 0x7 write mask then discards; kept for fidelity.
   o0 = float4(RunME2Material(v5.xy, v10.xyw), v10.w);
}
