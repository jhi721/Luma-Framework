// Mass Effect (2007) - UE3 DOFAndBloomBlend, the standalone DoF/bloom composite of chains without an uber
// (Space_PostProcess_DOF2, the default UI/thumbnail chains). Vanilla mix via ME1SceneMix plus the Luma glow, which only
// the uber composited otherwise: the replaced gather drops the native glow. Leaves LINEAR light and depth in alpha for
// the gamma pass, which runs the HDR block itself on uber-less frames.
#include "Luma_ME1_Tonemap.hlsl"

// 13 interpolators, all declared in order - linkage is by REGISTER (see Luma_ME1_Tonemap.hlsl).
void main(DGV_MAIN_SIGNATURE)
{
   float sceneDepth;
   const float4 mix = ME1SceneMix(v5.xy, v6.xy, sceneDepth);
   o0 = float4(mix.xyz * rcp(max(mix.w, 0.001)), sceneDepth);
}
