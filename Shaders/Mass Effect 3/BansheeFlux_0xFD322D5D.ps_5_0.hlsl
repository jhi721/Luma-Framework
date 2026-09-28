// Mass Effect 3 (2012) - Banshee flux feedback material (Flux_Post_FB_mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float3 split = lerp(FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[13].x, PsConstants[12].z))), FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[13].y, PsConstants[12].z))), PsConstants[13].z);
   const float3 blurred = lerp(FX_Scene(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[12].w, PsConstants[12].z))), split, PsConstants[13].w);
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   const float3 c = saturate(lerp(scene, blurred, PsConstants[13].w));
   const float3 flux = max(abs((exp2(log2(max(1.0 - c, 1e-6)) * 100.0) * 40.0 + c) * PsConstants[11].xyz), 1e-6);
   o0 = FX_Output(PsConstants[14].y * (flux * flux - c) + c + PsConstants[8].xyz, (1.0 - PsConstants[14].y) * (sceneHDR - scene), FX_Alpha(v10));
}