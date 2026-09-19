// Mass Effect 3 (2012) - Rannoch chase motion-blur material (MotionBlur_Mat_Gth002). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

// One tap: the uv pushed by a noise read at a constant position, then zoomed.
float3 Tap(float2 uv, float2 noiseUV, float zoomRow)
{
   const float2 pushed = PsConstants[15].x * ApplyDgvMask(Tex1.Sample(Sampler1, noiseUV), 1).xy + uv;
   return FX_Scene(FX_ZoomUV(pushed, FX_Zoom(zoomRow, PsConstants[16].z)));
}

void main(DGV_MAIN_SIGNATURE)
{
   float3 blurred = lerp(Tap(v5.xy, PsConstants[13].xy, PsConstants[17].w), Tap(v5.xy, PsConstants[14].xy, PsConstants[18].w), PsConstants[19].x);
   blurred = lerp(Tap(v5.xy, PsConstants[12].xy, PsConstants[16].w), blurred, PsConstants[19].y);
   const float2 pushed = ApplyDgvMask(Tex1.Sample(Sampler1, PsConstants[11].xy), 1).xy * PsConstants[15].x * 0.2 + v5.xy;
   blurred = lerp(FX_Scene(FX_ScreenUV(pushed)), blurred, PsConstants[19].y);
   const float3 sceneHDR = FX_PixelSceneHDR(v10);
   const float3 scene = saturate(sceneHDR);
   o0 = FX_Output(lerp(scene, PsConstants[19].w * FX_Glow(blurred, PsConstants[19].z), PsConstants[20].x) + PsConstants[8].xyz, (1.0 - PsConstants[20].x) * (sceneHDR - scene), FX_Alpha(v10));
}