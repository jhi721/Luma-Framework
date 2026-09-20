// Mass Effect 2 (2010) - flamethrower feedback material (FlameThrower_FB). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   const float2 noise = FX_Fetch(Tex1, Sampler1, v5.xy * 0.125 + FXC(3).xy, 1).xy;
   const float3 mask = saturate(FX_Fetch(Tex2, Sampler2, FXC(4).x * noise + FXC(2).xy + v5.xy, 2).xyz * 10.0);
   const float2 d = v5.xy * 2.0 - 1.0;
   const float fade = saturate(1.0 - PowUE3(max(abs(dot(d, d)), 1e-4), 0.2));
   const float3 sceneHDR = FX_SceneHDR(FX_PixelUV(v10));
   const float3 scene = saturate(sceneHDR);
   // floor(scene) was 0 below vanilla's 8-bit white; unclamped it adds 500 per unit of HDR.
   const float3 heat = (fade * mask + 0.05) * scene * float3(75.0, 10.0, 1.0) + floor(scene) * float3(500.0, 200.0, 100.0);
   o0 = FX_Output(FXC(5).x * heat + scene + FXC(0).xyz, sceneHDR - scene, v10.w);
}
