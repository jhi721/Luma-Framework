// Mass Effect 2 (2010) - Firewalker security camera feedback material (Security_Cam_View_Mat). Additive: vanilla's UNORM target clamped the output. See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(FX_MAIN_SIGNATURE)
{
   const float a = FX_Fetch(Tex4, Sampler4, float2(FXC(6).x, v5.y * 5.0 + FXC(6).y), 4).x * FXC(10).x * FXC(11).x;
   const float zoom = 3.0 * FXC(13).x;
   const float2 rollUV = float2(zoom * v5.x, zoom * v5.y + (1.0 - 2.0 * FXC(12).x) * -0.5);
   const float2 roll = FX_Fetch(Tex5, Sampler5, rollUV, 5).xy * FXC(10).x + a * 10.0;
   const float2 wobble = FX_Fetch(Tex5, Sampler5, v5.xy * float2(1.0, 0.1) + FXC(7).xy, 5).xy;
   const float2 uv = zoom * (FXC(14).x * wobble + roll) + v5.xy;
   const float3 view = FX_Desaturate(FX_SceneHDR(FX_ScreenUV(uv)), FXC(15).x) * FXC(16).x;
   const float3 noise = FX_Fetch(Tex2, Sampler2, v5.xy * 5.0 + FXC(4).xy, 2).xyz * 0.3 + FX_Fetch(Tex3, Sampler3, FXC(5).xy + v5.xy, 3).xyz;
   const float3 lines = FX_Fetch(Tex1, Sampler1, v5.xy * float2(1.0, 50.0) + FXC(3).xy, 1).xyz * 3.0 + noise;
   const float3 color = (FXC(17).x * view * lines * FXC(2).xyz + FXC(0).xyz) * v9.w;
   const float edge = saturate(FX_Fetch(Tex5, Sampler5, v5.xy * float2(1.0, 0.01) + FXC(9).xy, 5).x * 15.0);
   const float frame = saturate(FX_Fetch(Tex6, Sampler6, v5.xy * 5.0 + FXC(8).xy, 6).y) + 0.5;
   o0 = float4(saturate(color * (frame * (1.0 - saturate(edge * frame)))), 0.0);
}
