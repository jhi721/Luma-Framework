// Mass Effect (2007) - UE3 UberPostProcessBlend final color pass. HDR replacement, and the only permutation of this
// pass in the menu + gameplay shader dump (constant-fingerprint audit, dgVoodoo 2.87.3). See Luma_ME1_Tonemap.hlsl.
#include "Luma_ME1_Tonemap.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl).
void main(DGV_MAIN_SIGNATURE)
{
   // Alpha: the original writes -0.0 and nothing of the game's reads it. Luma does - the copy pass 0x1E37D75B carries
   // it into scene A for the SMAA predication CS, and with -0.0 the mask measured all-zero.
   float sceneDepth;
   const float3 graded = RunME1Tonemap(v5.xy, v6.xy, sceneDepth);
   o0 = float4(graded, sceneDepth);
}
