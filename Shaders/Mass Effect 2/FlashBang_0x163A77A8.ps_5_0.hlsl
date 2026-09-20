// Mass Effect 2 (2010) - Kasumi flashbang feedback material (FlashBang_FB_Mat). Additive: vanilla's UNORM target clamped the output. See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(ME2_MAIN_SIGNATURE)
{
   const float3 flashA = FX_Fetch(Tex1, Sampler1, FXC(4).x * v5.xy + FXC(5).x, 1).xyz;
   const float3 flashB = FX_Fetch(Tex1, Sampler1, FXC(6).x * v5.xy + FXC(7).x, 1).xyz;
   const float3 flashC = FX_Fetch(Tex1, Sampler1, FXC(2).x * v5.xy + FXC(3).x, 1).xyz;
   const float3 flash = FX_Desaturate(lerp(flashC, lerp(flashA, flashB, FXC(8).x), FXC(9).x), FXC(10).x);
   const float3 lit = saturate((flash + FX_Fetch(Tex2, Sampler2, v5.xy, 2).xyz) * float3(2.0, 1.8, 1.5));
   const float w = saturate((FX_Fetch(Tex3, Sampler3, v5.xy * 0.5, 3).x + 0.02) * FXC(11).x * 50.0);
   const float3 scene = FX_SceneHDR(FX_PixelUV(v10));
   o0 = float4(saturate((lerp(scene, lit, w) + FXC(0).xyz) * v9.w * FXC(12).x), 0.0);
}
