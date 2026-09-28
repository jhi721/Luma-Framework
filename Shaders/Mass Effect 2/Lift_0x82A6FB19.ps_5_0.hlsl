// Mass Effect 2 (2010) - biotic Lift feedback material (Lift_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   const float2 c = v5.xy - 0.5;
   const float len = length(c);
   const float ring = len * FX_Fetch(Tex1, Sampler1, v5.xy, 1).x * 5.0 - FXC(2).x;
   const float2 n = v5.xy * 2.0 - 1.0;
   const float k = FXC(3).x;
   const float w = saturate(rcp(rsqrt(max(abs(dot(n, n)), 1e-4)))) * k;
   const float3 warpedHDR = FX_SceneHDR(FX_ScreenUV(v5.xy - w * (c * rcp(len)) * ring * ring));
   // Partial inversion x + c4 (1 - 2x): a negative slope on anything above vanilla's 8-bit white.
   const float3 warped = FX_Desaturate(saturate(warpedHDR), 0.5);
   const float3 inverted = FXC(4).x * (1.0 - 2.0 * warped) + warped;
   const float3 sceneHDR = FX_SceneHDR(FX_PixelUV(v10));
   const float3 scene = saturate(sceneHDR);
   const float3 excess = k * (sceneHDR - scene) + (1.0 - k) * max(1.0 - 2.0 * FXC(4).x, 0.0) * (FX_Desaturate(warpedHDR, 0.5) - warped);
   o0 = FX_Output(lerp(inverted, scene, k) + FXC(0).xyz, excess, v10.w);
}
