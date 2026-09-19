// Mass Effect 3 (2012) - security camera vision feedback material (DLC Citadel, Security_Vision_Mat). See
// Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float frame = floor(16.0 * PsConstants[13].z);
   const float2 cell = float2(fmod(frame * 0.25, 1.0), floor(frame * 0.25) * 0.25);
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);

   const float2 jitter = ApplyDgvMask(Tex1.Sample(Sampler1, (v5.xy * 40.0 + scene.xy) * 0.25 + cell), 1).xy;
   const float3 noise = ApplyDgvMask(Tex2.Sample(Sampler2, scene.xy + jitter + PsConstants[11].xy + v5.xy), 2).xyz * 2.0;
   const float3 gain = pow(max(abs(noise), 1e-6), 2.5) + 1.0; // linear scene factor
   const float3 a = max(abs(scene), 1e-6);
   const float3 band = FX_DarkBand(scene, 80.0, 10.0, 50.0, 100.0) * float3(0.1, 0.2, 0.4) * ApplyDgvMask(Tex3.Sample(Sampler3, PsConstants[12].xy + v5.xy), 3).xyz;
   const float3 lit = (scene * gain + a * a * PsConstants[13].w * 2.0 + band) * PsConstants[14].z;
   const float3 excess = (sceneHDR - scene) * gain * PsConstants[14].z;
   const float3 lum = float3(0.3, 0.59, 0.11);
   o0 = FX_Output(lerp(scene, lerp(lit, dot(lit, lum), 0.25), PsConstants[14].w) + PsConstants[8].xyz, (1.0 - PsConstants[14].w) * (sceneHDR - scene) + PsConstants[14].w * lerp(excess, dot(excess, lum), 0.25), FX_Alpha(v10));
}
