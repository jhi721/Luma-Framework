// Mass Effect 2 (2010) - near-death blood feedback material (mat_GameOverBlood). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(FX_MAIN_SIGNATURE)
{
   const float s = FXC(5).x; // damage amount
   const float2 a = v5.xy * 2.5 - 1.25;
   const float vigA = 1.0 - exp2(log2(max(abs(dot(a, a)), 1e-4)) * 0.05);
   const float2 flow = FX_Fetch(Tex1, Sampler1, v5.xy * 3.0 + FXC(2).xy, 1).xy;
   const float pulse = min(max(sin(frac(FXC(6).x * (0.9 * s + 0.1) + 0.5) * 6.2831855 - 3.1415927), 0.0), 0.5) * FXC(7).x;
   const float2 blood = FX_Fetch(Tex2, Sampler2, pulse * flow + v5.xy, 2).xy;
   const float2 b = (3.0 * s + 1.0) * (v5.xy - 0.5);
   const float vigB = saturate(exp2(log2(max(abs(dot(b, b)), 1e-4)) * (5.0 - 4.5 * s)));
   const float k = min(max((10.0 * s + 10.0) * blood.y, 0.0), 1.7);
   const float wet = saturate(vigA * (1.0 - saturate(blood.y * 10.0)) * vigB);

   const float3 sceneHDR = FX_SceneHDR(FX_PixelUV(v10));
   const float3 scene = saturate(sceneHDR);
   const float3 q = scene - vigB * k * scene;
   const float3 r = wet * ((99.0 * s + 1.0) * q * q - q) + q;
   const float3 desat = FX_Desaturate(r, 0.3 * s);
   const float bleed = vigB * (blood.x * 0.75 + saturate(blood.y + blood.y));
   const float3 red = (scene * FXC(3).xyz + bleed * FXC(4).xyz) * (0.15 + 1.35 * s);

   // Linear scene weights: the darkened pass-through q and the red tint's scene term.
   const float3 e = sceneHDR - scene;
   const float3 excess = (1.0 - bleed) * FX_Desaturate((1.0 - wet) * (1.0 - vigB * k) * e, 0.3 * s) + bleed * (0.15 + 1.35 * s) * FXC(3).xyz * e;
   o0 = FX_Output(lerp(desat, red, bleed) + FXC(0).xyz, excess, v10.w);
}
