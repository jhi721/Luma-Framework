// Mass Effect 3 (2012) - FB_MotionBlur feedback material (MotionBlur_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float3 s1 = FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[12].x, PsConstants[11].y)));
   const float3 s0 = FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[11].w, PsConstants[11].y)));
   const float3 blurred = lerp(FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[11].z, PsConstants[11].y))), lerp(s0, s1, PsConstants[12].y), PsConstants[12].z);
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   const float3 glow = FX_Glow(lerp(scene, blurred, PsConstants[12].z), PsConstants[12].w);
   o0 = FX_Output(lerp(scene, PsConstants[13].x * glow, PsConstants[13].y) + PsConstants[8].xyz, (1.0 - PsConstants[13].y) * (sceneHDR - scene), FX_Alpha(v10));
}