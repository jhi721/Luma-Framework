// Mass Effect 3 (2012) - biotic smoke feedback material (DLC Omega, Biotic_Smoke_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

float3 Mix(float3 warped, float3 zoomA, float3 zoomB, float3 zoomC)
{
   return lerp(warped, lerp(zoomA, lerp(zoomB, zoomC, PsConstants[13].y), PsConstants[13].z), PsConstants[13].z);
}

void main(DGV_MAIN_SIGNATURE)
{
   const float2 noise = ApplyDgvMask(Tex1.Sample(Sampler1, v5.xy * 0.125 + PsConstants[11].xy), 1).xy * 0.15;
   const float fade = saturate(1.0 - 2.0 * ApplyDgvMask(Tex2.Sample(Sampler2, v5.xy), 2).x);
   const float2 uv = fade * noise + v5.xy;
   const float3 warped = FX_SceneHDR(FX_ScreenUV(uv));
   const float3 zoomA = FX_SceneHDR(FX_ZoomUV(uv, FX_Zoom(PsConstants[12].z, PsConstants[12].y)));
   const float3 zoomB = FX_SceneHDR(FX_ZoomUV(uv, FX_Zoom(PsConstants[12].w, PsConstants[12].y)));
   const float3 zoomC = FX_SceneHDR(FX_ZoomUV(uv, FX_Zoom(PsConstants[13].x, PsConstants[12].y)));
   const float3 mixed = Mix(saturate(warped), saturate(zoomA), saturate(zoomB), saturate(zoomC));
   const float3 a = max(abs(mixed), 1e-6);
   const float3 glow = a * a * PsConstants[13].w * 20.0 + mixed;
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   const float k = PsConstants[14].y;
   o0 = FX_Output(lerp(scene, PsConstants[14].x * glow, k) + PsConstants[8].xyz, (1.0 - k) * (sceneHDR - scene) + k * PsConstants[14].x * (Mix(warped, zoomA, zoomB, zoomC) - mixed), FX_Alpha(v10));
}
