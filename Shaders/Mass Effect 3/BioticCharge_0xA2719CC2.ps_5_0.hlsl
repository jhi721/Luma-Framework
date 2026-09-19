// Mass Effect 3 (2012) - Biotic Charge feedback material (mat_BioticsMode). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float2 noise = ApplyDgvMask(Tex2.Sample(Sampler2, PsConstants[11].xy + v5.xy), 2).xy;
   const float fade = 1.0 - ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy), 1).x;
   const float3 warped = FX_Scene(FX_ScreenUV(fade * noise * 0.1 + v5.xy));
   const float3 tinted = lerp(warped, dot(warped, float3(0.3, 0.59, 0.11)), PsConstants[13].y) * PsConstants[12].xyz;
   const float2 pixelUV = FX_PixelUV(v10);
   const float3 sceneHDR = FX_SceneHDR(pixelUV);
   const float3 scene = saturate(sceneHDR);
   const float glow = ApplyDgvMask(Tex2.Sample(Sampler2, noise * 0.1 + pixelUV), 2).x * 7.0;
   const float weight = fade * PsConstants[13].z;
   o0 = FX_Output(lerp(scene, glow * warped + tinted, weight) + PsConstants[8].xyz, (1.0 - weight) * (sceneHDR - scene), FX_Alpha(v10));
}