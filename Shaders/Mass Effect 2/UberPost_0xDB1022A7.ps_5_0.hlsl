// Mass Effect 2 (2010) - UE3 UberPostProcessBlend, FILMIC permutation (Hejl/Uncharted-2 curve before the grade).
// The live gameplay perm. See Luma_ME2_Tonemap.hlsl.
#define ME2_UBER_FILMIC 1
#include "Luma_ME2_Tonemap.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   // Alpha stays at the vanilla 0: SMAA predication reads the game's own fp16 scene, captured at this draw's t0, so
   // nothing needs depth here - and an Unreal-unit depth in an fp16 canvas alpha is a side channel no blend expects.
   o0 = float4(RunME2Uber(v5.xy, v6.xy), 0.0);
}
