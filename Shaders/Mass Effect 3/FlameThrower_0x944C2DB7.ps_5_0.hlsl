// Mass Effect 3 (2012) - flamethrower feedback material (FlameThrower_FB). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float2 offset = ApplyDgvMask(Tex1.Sample(Sampler1, PsConstants[12].xy + v5.xy), 1).xy * PsConstants[14].y;
   const float mask = ApplyDgvMask(Tex2.Sample(Sampler2, v5.xy), 2).x;
   const float2 uv = FX_ScreenUV(mask * offset + PsConstants[11].xy + v5.xy);
   const float3 sceneHDR = FX_SceneHDR(uv);
   const float3 scene = saturate(sceneHDR);
   const float3 tint = (PsConstants[14].w * (PsConstants[13].xyz - 1.0) + 1.0) * PsConstants[15].x;
   o0 = FX_Output(lerp(scene, tint * FX_Glow(scene, PsConstants[15].y), PsConstants[15].z) + PsConstants[8].xyz, (1.0 - PsConstants[15].z) * (sceneHDR - scene), FX_Alpha(v10));
}