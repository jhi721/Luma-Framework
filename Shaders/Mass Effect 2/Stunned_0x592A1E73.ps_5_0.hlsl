// Mass Effect 2 (2010) - stun feedback material (Stunned_Material). Additive: vanilla's UNORM target clamped the output. See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(FX_MAIN_SIGNATURE)
{
   const float2 offset = FX_Fetch(Tex1, Sampler1, FXC(2).xy + v5.xy, 1).xy * 0.1 + v5.xy + FXC(3).xy;
   const float3 tinted = FX_SceneHDR(FX_ScreenUV(offset)) * FX_Fetch(Tex2, Sampler2, v5.xy, 2).xyz * FXC(4).x;
   const float3 glow = tinted * FXC(5).x * float3(2.0, 4.0, 4.0) + tinted;
   const float3 scene = FX_SceneHDR(FX_PixelUV(v10));
   o0 = float4(saturate((FXC(7).x * (FXC(6).x * glow + scene) + FXC(0).xyz) * v9.w), 0.0);
}
