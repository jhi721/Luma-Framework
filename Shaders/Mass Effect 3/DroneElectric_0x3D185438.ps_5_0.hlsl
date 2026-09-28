// Mass Effect 3 (2012) - drone electric screen feedback material (DLC Citadel, ElectricScreen_Mat). See
// Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float3 zoomX = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[19].x, PsConstants[18].w)));
   const float3 zoomY = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[19].y, PsConstants[18].w)));
   const float3 zoomZ = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(PsConstants[19].z, PsConstants[18].w)));
   const float3 splitHDR = lerp(zoomX, lerp(zoomY, zoomZ, PsConstants[19].w), PsConstants[20].x);
   const float3 split = lerp(saturate(zoomX), lerp(saturate(zoomY), saturate(zoomZ), PsConstants[19].w), PsConstants[20].x);

   const float2 warp = ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy), 1).xy * 2.0 + ApplyDgvMask(Tex2.Sample(Sampler2, v5.xy * 3.0 + PsConstants[13].xy), 2).xy;
   const float bolt = ApplyDgvMask(Tex3.Sample(Sampler3, PsConstants[17].y * warp + PsConstants[16].z * v5.xy * 1.5 + PsConstants[12].xy), 3).x;
   const float flicker = saturate(ApplyDgvMask(Tex2.Sample(Sampler2, 2.0 * PsConstants[14].xy * v5.xy + PsConstants[15].xy), 2).x * 10.0);
   const float3 hole = 1.0 - saturate(ApplyDgvMask(Tex4.Sample(Sampler4, v5.xy), 4).xyz * 5.0);
   const float3 lines = abs(bolt) - 1e-6 >= 0.0 ? bolt * bolt * PsConstants[11].xyz * flicker * hole * PsConstants[18].z : 0.0;

   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   const float3 glow = FX_Glow(lerp(lines + scene, split, PsConstants[20].x), PsConstants[20].y);
   const float k = PsConstants[20].w;
   const float g = k * PsConstants[20].z;
   o0 = FX_Output(lerp(scene, PsConstants[20].z * glow, k) + PsConstants[8].xyz, (1.0 - k) * (sceneHDR - scene) + g * ((1.0 - PsConstants[20].x) * (sceneHDR - scene) + PsConstants[20].x * (splitHDR - split)), FX_Alpha(v10));
}
