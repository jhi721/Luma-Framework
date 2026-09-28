// Mass Effect 3 (2012) - Marksman feedback material (Marksmen_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float3 sampleR = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(0.24, PsConstants[11].y)));
   const float3 sampleG = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(0.23, PsConstants[11].y)));
   const float3 sampleB = FX_SceneHDR(FX_ZoomUV(v5.xy, FX_Zoom(0.22, PsConstants[11].y)));
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   const float3 baseHDR = lerp(sceneHDR, lerp(sampleR, lerp(sampleG, sampleB, 0.6), 0.8), 0.8);
   const float3 base = lerp(scene, lerp(saturate(sampleR), lerp(saturate(sampleG), saturate(sampleB), 0.6), 0.8), 0.8);

   float3 m = max(abs(ApplyDgvMask(Tex2.Sample(Sampler2, PsConstants[11].z * (1.0 - 20.0 * scene.xy) + v5.xy), 2).xyz * 4.0), 1e-6);
   m = m * m * m * m;
   const float3 boosted = exp2(log2(max(abs(base * 3.0), 1e-6)) * 2.5) * 5.0;
   const float3 desat = boosted + 0.2 * (dot(boosted, float3(0.3, 0.59, 0.11)) - boosted);
   const float streak = ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy), 1).y;
   const float streakPow = abs(streak) - 1e-6 >= 0.0 ? exp2(log2(abs(streak)) * 1.5) : 0.0;
   const float3 amount = (m * streak * 0.4 + streakPow) * PsConstants[11].w;
   // base is linear in every sample: each one's HDR excess passes through with its exact weight.
   o0 = FX_Output(PsConstants[12].x * (amount * desat + base) + PsConstants[8].xyz, PsConstants[12].x * (baseHDR - base), FX_Alpha(v10));
}