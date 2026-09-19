// Mass Effect 2 (2010) - Adrenaline Rush feedback material (Adrenaline_FB_Mat). See Includes/FXMaterial.hlsl.
#include "Includes/FXMaterial.hlsl"

void main(FX_MAIN_SIGNATURE)
{
   // Chromatic zoom split: three zooms around the centre, mixed by c10 then c11.
   const float3 zoomA = FX_SceneHDR(FX_ScreenUV(lerp(0.5, v5.xy, (FXC(6).x * (FXC(7).x - 0.245) + 0.245) * 4.0)));
   const float3 zoomB = FX_SceneHDR(FX_ScreenUV(lerp(0.5, v5.xy, (FXC(8).x * (FXC(9).x - 0.24) + 0.24) * 4.0)));
   const float3 zoomC = FX_SceneHDR(FX_ScreenUV(lerp(0.5, v5.xy, (FXC(4).x * (FXC(5).x - 0.249) + 0.249) * 4.0)));
   const float3 split = lerp(saturate(zoomC), lerp(saturate(zoomA), saturate(zoomB), FXC(10).x), FXC(11).x);
   const float2 d = v5.xy * 3.0 - 1.5;
   const float d2 = max(abs(dot(d, d)), 1e-4);
   const float blend = saturate(d2 * d2);
   const float3 sceneHDR = FX_SceneHDR(FX_PixelUV(v10));
   const float3 scene = saturate(sceneHDR);
   const float3 mixed = lerp(scene, split, blend);

   // Rotated noisy mask tint, fading out toward the centre.
   const float2 c = v5.xy - 0.5;
   float2 maskUV = float2(dot(float2(0.0707372, -0.997495), c), dot(float2(0.997495, 0.0707372), c)) + 0.5;
   maskUV += FXC(12).x * FX_Fetch(Tex1, Sampler1, v5.xy * 3.0 + FXC(3).xy, 1).xy;
   const float2 mask = FX_Fetch(Tex2, Sampler2, maskUV, 2).xy;
   const float2 n = v5.xy * 2.0 - 1.0;
   const float r = log2(max(abs(dot(n, n)), 1e-4));
   const float inner = saturate(exp2(r * 0.1));
   const float edge = saturate(exp2(r * 0.2));
   const float3 tint = mask.x * (1.0 - inner) * saturate(mask.y * 5.0) * FXC(2).xyz;

   // Whiteout 5 (m + 0.015)^2 toward the edges.
   const float3 m = FXC(13).x * tint + mixed + 0.015;
   const float weight = FXC(14).x * edge;
   o0 = FX_Output(weight * (m * m * 5.0 - scene) + scene + FXC(0).xyz, (1.0 - weight) * (sceneHDR - scene), v10.w);
}
