// Mass Effect 2 (2010) - heat shimmer feedback material (Distortion_HeatShimmer_FB). Additive: vanilla's UNORM target clamped the output. See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   const float2 n = FX_Fetch(Tex1, Sampler1, v5.xy * float2(5.0, 4.0) + FXC(2).xy, 1).xy;
   const float3 tapA = FX_SceneHDR(FX_ScreenUV(n * 0.07 + v5.xy - 0.008));
   const float3 tapB = FX_SceneHDR(FX_ScreenUV(n * 0.035 + v5.xy - 0.003));
   const float3 scene = FX_SceneHDR(FX_PixelUV(v10));
   const float w = FX_Fetch(Tex2, Sampler2, v5.xy, 2).x * FXC(3).x;
   o0 = float4(saturate((lerp(scene, lerp(tapA, tapB, 0.5), w) + FXC(0).xyz) * v9.w), 0.0);
}
