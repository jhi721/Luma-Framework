// Mirror's Edge - TdToneMappingPixelShader, the only grade of the post chain (fp16 scene -> gamma-encoded canvas that
// TdMotionBlur and the HUD then use). The original clips twice (mul_sat after the exposure and after the output scale),
// then encodes with the brightness slider's gamma (c13.w, 0.5 at default) and applies per-channel curves: 16 point
// sampled (slope, offset) segments, R/G in t1, B in t2, picked at saturate(x * 15/16). Above 1 the saturate keeps the
// last segment, so the curve continues linearly there. HDR: the same grade with both upper clips as max(0) is the
// signal; the hue follows a soft ReinhardPiecewise reference (MacLeod-Boynton), DICE maps to the display (the
// Medal of Honor Airborne scheme, same UE3 grade family).
#include "Includes/Common.hlsl"
// clang-format off
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../Includes/DICE.hlsl"
#include "../Includes/Reinhard.hlsl"
#include "../Includes/MacLeodBoynton.hlsl"
#include "../Includes/DgVoodoo.hlsl"
// clang-format on

// 1 = HDR, 0 = vanilla clamped SDR.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// b3/b4 are dgVoodoo's state and the D3D9 pixel constants (c0 at row 8).
cbuffer DgVoodooState : register(b3)
{
   float4 DgvConstants[77] : packoffset(c0);
}
cbuffer PixelShaderConstants : register(b4)
{
   float4 PsConstants[236] : packoffset(c0);
}

#define SceneShadowsAndDesaturation PsConstants[8]  // .xyz shadows (subtracted), .w desaturation weight
#define SceneInverseHighLights      PsConstants[10] // .xyz scale
#define SceneMidTones               PsConstants[11] // .xyz pow
#define SceneScaledLuminanceWeights PsConstants[12] // .xyz desaturation weights (the engine's name), applied after the midtones pow: a weighted sum of a nonlinear signal, not luminance
#define GammaColorScaleAndInverse   PsConstants[13] // .xyz output scale (fades), .w inverse gamma (brightness slider)
#define GammaOverlayColor           PsConstants[14] // .xyz tint (damage)

Texture2D<float4> SceneColorTexture : register(t0); // .w = linear depth
Texture2D<float4> ColorCurvesKTexture : register(t1);
Texture2D<float4> ColorCurvesMTexture : register(t2);
Texture2D<float4> ExposureTexture : register(t3); // 1x1 unorm, exposure / 64
SamplerState SceneColorSampler : register(s0);
SamplerState ColorCurvesKSampler : register(s1);
SamplerState ColorCurvesMSampler : register(s2);
SamplerState ExposureSampler : register(s3);

// The original's pow: |x| floored at 1e-4, dgVoodoo's guarded log.
float3 PowTd(float3 base, float3 exponent)
{
   const float3 b = max(abs(base), 0.0001);
   return exp2(exponent * float3(DgVoodooLog2(b.x), DgVoodooLog2(b.y), DgVoodooLog2(b.z)));
}

// The game's grade, operand for operand. `clampSDR` false turns both upper clips into max(0).
float3 GradeTd(float3 scene, float exposure, bool clampSDR)
{
   float3 c = exposure * scene;
   c = clampSDR ? saturate(c) : max(0.0, c);
   c = PowTd(c * SceneInverseHighLights.xyz - SceneShadowsAndDesaturation.xyz, SceneMidTones.xyz);
   c = dot(c, SceneScaledLuminanceWeights.xyz) + (c * SceneShadowsAndDesaturation.w + GammaOverlayColor.xyz);
   c *= GammaColorScaleAndInverse.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c);
   const float3 g = PowTd(c, GammaColorScaleAndInverse.www);

   const float3 u = saturate(g * 0.9375);
   const float4 curveR = ApplyDgvMask(ColorCurvesKTexture.Sample(ColorCurvesKSampler, float2(u.x, 0.0)), DgvConstants[46], DgvConstants[47]);
   const float4 curveG = ApplyDgvMask(ColorCurvesKTexture.Sample(ColorCurvesKSampler, float2(u.y, 0.0)), DgvConstants[46], DgvConstants[47]);
   const float4 curveB = ApplyDgvMask(ColorCurvesMTexture.Sample(ColorCurvesMSampler, float2(u.z, 0.0)), DgvConstants[48], DgvConstants[49]);
   const float3 curved = float3(g.x * curveR.x + curveR.y, g.y * curveG.z + curveG.w, g.z * curveB.x + curveB.y);
   return lerp(g, curved, LumaSettings.GameSettings.ColorGradingIntensity);
}

// dgVoodoo's fixed interpolator layout: every entry is declared, linkage is by register.
void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   const float exposure = ApplyDgvMask(ExposureTexture.Sample(ExposureSampler, 0.5), DgvConstants[50], DgvConstants[51]).x * 64.0;
   const float3 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorSampler, v5.xy), DgvConstants[44], DgvConstants[45]).xyz * LumaSettings.GameSettings.Exposure;

#if TONEMAP_TYPE >= 1
   float3 extendedLinear = gamma_to_linear(GradeTd(scene, exposure, false));

   // Before the display map, so DICE contains it. Bit exact no-op at 1.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      extendedLinear = PowTd(max(0.0, extendedLinear / MidGray), LumaSettings.GameSettings.Contrast.xxx) * MidGray;
   }

   const float paperWhite = max(LumaSettings.GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(LumaSettings.PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;

   // The per-channel curves lean saturated highlights; the soft per-channel reference keeps that lean without the
   // clip's whitening, MacLeod-Boynton puts its hue on the extended grade's own purity.
   const float3 extendedBT2020 = BT709_To_BT2020(extendedLinear);
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightsDesaturation;
   ds.InOutColorSpace = CS_BT2020; // already BT.2020: the default would convert a second time
   float3 outColor = DICETonemap(MacLeodBoynton::HueOnlyBT2020(extendedBT2020, Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5)) * paperWhite, peakWhite, ds) / paperWhite;
   outColor = BT2020_To_BT709(SimpleGamutClip(outColor, true));
   outColor = Saturation(outColor, LumaSettings.GameSettings.Saturation);
#else
   float3 outColor = gamma_to_linear(GradeTd(scene, exposure, true));
#endif

   // Gamma space canvas (POST_PROCESS_SPACE_TYPE 0): the HUD blends onto it like vanilla.
   outColor = linear_to_gamma(max(0.0, PreScaleForUIPaperWhite(outColor)));
   // One step of the output quantizer: the 8 bit code in SDR, the 10 bit BT.2020 PQ code in HDR
   if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
      {
         ApplyDithering(outColor, v5.xy, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      }
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(outColor, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, v5.xy, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         outColor = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
   // Last: the dither can step below 0 and the grade can emit NaN (a NaN in the canvas reads back black)
   outColor = IsNaN_Strict(outColor) ? 0.0 : max(0.0, outColor);

#if DEVELOPMENT
   // Bring-up: DevSetting01 magenta (replacement bound), DevSetting02 the vanilla clamped grade.
   if (LumaSettings.DevSetting01 > 0.5)
      outColor = float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      outColor = GradeTd(scene, exposure, true);
#endif

   o0 = float4(outColor, 1.0);
}
