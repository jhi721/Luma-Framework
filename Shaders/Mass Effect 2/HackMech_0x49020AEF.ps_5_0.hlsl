// Mass Effect 2 (2010) - Arrival mech hacking feedback material (Hack_Mech_Fb). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(FX_MAIN_SIGNATURE)
{
   // Blocky scanline jitter: noise-driven offsets snapped to whole units, scaled by 0.004.
   const float2 a = FX_Fetch(Tex1, Sampler1, v5.xy * float2(0.025, 0.05) + FXC(2).xy + v5.xy * float2(0.1, 0.5) + FXC(3).xy, 1).xy * float2(2.0, 1.0) - float2(1.0, 0.0);
   const float2 b = FX_Fetch(Tex1, Sampler1, v5.xy * float2(0.1, 0.2) + FXC(4).xy + v5.xy * float2(0.01, 0.025) + FXC(5).xy, 1).xy;
   const float jx = 2.0 * b.x + a.x;
   const float jy = b.y + a.y;
   const float2 uv = float2(jx - 1.0 - frac(jx), jy - frac(jy)) * 0.004 + v5.xy;

   // RGB split: red at uv, green at a 0.98 zoom, blue at a 0.96 zoom.
   const float3 tapR = FX_SceneHDR(FX_ScreenUV(uv));
   const float3 tapG = FX_SceneHDR(FX_ScreenUV(uv * 0.98 + 0.00999999));
   const float3 tapB = FX_SceneHDR(FX_ScreenUV(uv * 0.96 + 0.02));
   const float3 splitHDR = float3(tapR.x, tapG.y, tapB.z);
   const float3 split = saturate(splitHDR);
   const float n = FX_Fetch(Tex1, Sampler1, v5.xy * 10.0 + FXC(6).xy, 1).z * FX_Fetch(Tex1, Sampler1, v5.xy * 8.0 + FXC(7).xy, 1).z;
   const float3 lines = (n * 0.2 + split * 0.5) * FXC(8).xyz;
   const float3 screen = lerp(lines, FXC(8).xyz * FX_Fetch(Tex2, Sampler2, FXC(9).xy + v5.xy, 2).xyz, FXC(10).x);
   const float3 base = lerp(saturate(tapR), screen, 0.5);

   // The pixel's colour pushes a second tap, whose pow 1.5 glows over the lines.
   const float3 pixel = FX_Scene(FX_PixelUV(v10));
   const float3 glowTap = FX_Scene(FX_ScreenUV(pixel.xy * 0.5 + uv));
   const float3 glow = n * exp2(log2(max(abs(glowTap), 1e-4)) * 1.5) * FXC(8).xyz * 20.0 + base;
   const float2 c = v5.xy - 0.5;
   const float cut = saturate(exp2(log2(max(abs(dot(c, c)), 1e-4)) * FXC(12).x));
   const float keep = 1.0 - (cut + frac(-cut)); // 0 outside the window
   const float3 excess = keep * FXC(11).x * (0.5 * (tapR - saturate(tapR)) + 0.5 * (1.0 - FXC(10).x) * 0.5 * FXC(8).xyz * (splitHDR - split));
   o0 = FX_Output(keep * glow * FXC(11).x + FXC(0).xyz, excess, v10.w);
}
