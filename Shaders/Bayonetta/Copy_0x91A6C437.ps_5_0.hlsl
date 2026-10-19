// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "../Includes/PsychoV31.hlsl" // PsychoVDisplayMap
#include "../Includes/DgVoodoo.hlsl" // DGV_MAIN_SIGNATURE, ApplyDgvMask
// clang-format on

// Bayonetta "filtershadercopytex" (D3D9 ps_3_0, dgVoodoo 2.87.5 -> ps_5_0 0x91A6C437; "espshadermultiparticle*" translate to the
// same code): t0 * c192. The game copies with it everywhere (bloom levels and composite, the scene to the back buffer).
// main.cpp flags one draw a frame with LumaData.CustomData1: "ResetRender", the scene copied into dgVoodoo's D3D9 back buffer (the
// canvas) right before the UI and the movies draw on it. The scene is gamma encoded (1 = paper white, gamma 2.2 display) with the
// grade's and the bloom's values above 1, so that copy carries the HDR display map: linearize, RenoDX's PsychoV31 to the peak (with
// the user's contrast, saturation and highlights desaturation, neutral at their defaults), the
// UI paper white pre-scale (UI_DRAW_TYPE 2), encode back for the Core composition. Elsewhere the copy is vanilla, unclamped on the
// fp16 targets (the bloom levels are 8-bit and clamp on their own), but for the bloom composite's alpha (CustomData1 2 and 3).

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants: c192 colour multiplier
}

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   o0 = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]) * cb4[192];
   // Additive blends read the source alpha: the 8-bit targets clamped it
   o0.a = saturate(o0.a);
   // The bloom composite: "Bloom Intensity" through its blend's source alpha, at most 1 when the blend lerps (3)
   if (LumaData.CustomData1 >= 2)
   {
      o0.a *= (LumaData.CustomData1 == 2) ? LumaSettings.GameSettings.BloomIntensity : min(LumaSettings.GameSettings.BloomIntensity, 1.0);
      return;
   }
   if (LumaData.CustomData1 == 0)
   {
      return;
   }
   // SDR: the 8-bit canvas clipped the scene before the UI blended on it
   if (LumaSettings.DisplayMode != 1)
   {
      o0.rgb = saturate(o0.rgb);
      return;
   }
#if TONEMAP_TYPE >= 1
   float3 color = IsNaN_Strict(o0.rgb) ? 0.0 : max(o0.rgb, 0.0);
   color = PsychoVDisplayMap(gamma_to_linear(color, GCT_MIRROR), LumaSettings.GameSettings.Contrast, LumaSettings.GameSettings.Saturation, LumaSettings.GameSettings.HighlightsDesaturation);
#else
   // The vanilla clip, on the HDR canvas: only the UI paper white pre-scale the composition expects
   float3 color = gamma_to_linear(saturate(o0.rgb), GCT_MIRROR);
#endif
   color *= GetUIPaperWhitePreScale(GamePaperWhiteNits, UIPaperWhiteNits, true);
   o0.rgb = linear_to_gamma(color, GCT_MIRROR);
}
