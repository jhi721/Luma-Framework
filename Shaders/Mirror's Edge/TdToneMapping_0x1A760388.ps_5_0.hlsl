// Mirror's Edge - TdToneMappingPixelShader, the only grade of the post chain (fp16 scene -> gamma-encoded canvas that
// TdMotionBlur and the HUD then use). The original clips twice (mul_sat after the exposure and after the output scale),
// then encodes with the brightness slider's gamma (c13.w, 0.5 at default) and applies per-channel curves: 16 point
// sampled (slope, offset) segments, R/G in t1, B in t2, picked at saturate(x * 15/16). Above 1 the saturate keeps the
// last segment, so the curve continues linearly there. HDR: the same grade with both upper clips as max(0) is the
// signal, RenoDX's PsychoV31 maps it to the display and the hue emulation then takes the hue of a soft ReinhardPiecewise
// reference (MacLeod-Boynton, see "DisplayMapTd"). SDR displays keep the vanilla grade.
#include "Includes/Common.hlsl"
#include "Includes/Tonemap.hlsl"

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
void main(DGV_MAIN_SIGNATURE)
{
   const float exposure = ApplyDgvMask(ExposureTexture.Sample(ExposureSampler, 0.5), DgvConstants[50], DgvConstants[51]).x * 64.0;
   const float3 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorSampler, v5.xy), DgvConstants[44], DgvConstants[45]).xyz * LumaSettings.GameSettings.Exposure;

   // SDR displays keep the vanilla grade (see "PsychoVDisplayMap")
   const bool hdr = TONEMAP_TYPE >= 1 && LumaSettings.DisplayMode == 1;
   float3 displayLinear = gamma_to_linear(GradeTd(scene, exposure, !hdr));
   [branch] if (hdr)
   {
      displayLinear = DisplayMapTd(displayLinear);
   }
   float3 outColor = EncodeTdCanvas(displayLinear, v5.xy);

#if DEVELOPMENT
   // Bring-up: DevSetting01 magenta (replacement bound), DevSetting02 the vanilla clamped grade.
   if (LumaSettings.DevSetting01 > 0.5)
      outColor = float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      outColor = GradeTd(scene, exposure, true);
#endif

   o0 = float4(outColor, 1.0);
}
