// Mass Effect 3 (2012) - ending green motion-blur material (MotionBlur_Green). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   // Chromatic split: one zoom per channel.
   const float3 split = float3(FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[11].z, PsConstants[11].y))).r, FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[11].w, PsConstants[11].y))).g, FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[12].x, PsConstants[11].y))).b);
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   float3 c = float3(split.r * (1.0 - PsConstants[12].z), PsConstants[12].z * split.g * (1.0 - PsConstants[12].y), PsConstants[12].z * PsConstants[12].y * split.b);
   c = lerp(scene * float3(0.8, 1.5, 1.0), c, PsConstants[12].z);
   o0 = FX_Output(lerp(scene, PsConstants[13].x * FX_Glow(c, PsConstants[12].w), PsConstants[13].y) + PsConstants[8].xyz, (1.0 - PsConstants[13].y) * (sceneHDR - scene), FX_Alpha(v10));
}