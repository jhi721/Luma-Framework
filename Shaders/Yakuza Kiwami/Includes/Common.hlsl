#ifndef SRC_YKR_COMMON_HLSL
#define SRC_YKR_COMMON_HLSL

// Always include this instead of the global "Common.hlsl" in this game's shaders

// Define the game custom cbuffer structs
#include "GameCBuffers.hlsl"
// Global common
#include "../../Includes/Common.hlsl"
// Game specific settings
#include "Settings.hlsl"

// The materials tonemap inline (see NOTES.md, "Color pipeline"): on y = c_tone_control.x * colour (exposure-scaled linear
// light) they apply V(y) = saturate((Hable(2 y) - E/F) / W), Uncharted 2 constants, W = 11.2, then sqrt (gamma 2.0 encoding,
// before fog). V reaches 1 at y = 5.6 and its unclamped asymptote is only 1.287, so the curve itself caps the highlights.
// The vanilla display decoded that sqrt with gamma 2.2, so the scene-to-display curve was D(y) = V(y)^1.1. HDR continues D with
// its tangent above D's inflection (vanilla up to it, linear and unbounded above; D'' = 0 there, so the join is C2), as RenoDX's
// Hable and Hejl-Dawson extensions do on the composite curve; the post chain owns the display mapping.
static const float YKR_MATERIAL_CURVE_PIVOT = 0.0892318; // D''(y) = 0: D = 0.0508, D' = 0.5998

float YKR_VanillaMaterialCurve(float y)
{
   const float x = 2.0 * y;
   return ((x * (0.15 * x + 0.05) + 0.004) / (x * (0.15 * x + 0.5) + 0.06) - (0.02 / 0.3)) * 1.3790643215179443;
}

// dV/dy, by the quotient rule on Hable's numerator x (A x + C B) + D E and denominator x (A x + B) + D F
float YKR_VanillaMaterialCurveSlope(float y)
{
   const float x = 2.0 * y;
   const float numerator = x * (0.15 * x + 0.05) + 0.004;
   const float denominator = x * (0.15 * x + 0.5) + 0.06;
   return 2.0 * ((0.3 * x + 0.05) * denominator - numerator * (0.3 * x + 0.5)) / (denominator * denominator) * 1.3790643215179443;
}

// D(pivot) and D'(pivot), D = V^1.1 (V > 0 at the pivot)
float YKR_PivotDisplayCurve()
{
   return pow(YKR_VanillaMaterialCurve(YKR_MATERIAL_CURVE_PIVOT), 1.1);
}

float YKR_PivotDisplayCurveSlope()
{
   return 1.1 * pow(YKR_VanillaMaterialCurve(YKR_MATERIAL_CURVE_PIVOT), 0.1) * YKR_VanillaMaterialCurveSlope(YKR_MATERIAL_CURVE_PIVOT);
}

// "vanilla_curve" is the material's own (unclamped) V(y), whose "saturate" this replaces. SDR display modes keep that saturate,
// so they stay vanilla. HDR returns the V whose display value is D's tangent above the pivot (the shader's sqrt follows). The
// lower clamp (and NaN -> 0) stays in HDR too.
float YKR_MaterialCurve(float vanilla_curve, float y)
{
   if (LumaSettings.DisplayMode != 1)
   {
      return saturate(vanilla_curve);
   }
   const float extended_curve = pow(YKR_PivotDisplayCurve() + YKR_PivotDisplayCurveSlope() * (y - YKR_MATERIAL_CURVE_PIVOT), 1.0 / 1.1);
   return max(y > YKR_MATERIAL_CURVE_PIVOT ? extended_curve : vanilla_curve, 0.0);
}

// The light pre-pass buffer (t14) is upgraded to fp16, which keeps the point light accumulation above the UNORM cap (wanted),
// but also negative values and NaNs that the UNORM target stored as 0. This restores that lower side (max returns 0 for a NaN).
float4 YKR_LightBuffer(float4 light)
{
   return max(light, 0.0);
}

// A scene texel (gamma 2.0 encoded, i.e. sqrt of the material curve's output) as the vanilla 8-bit scene stored it. HDR display
// mode: the materials' display tangent is inverted to y and run through the vanilla curve (exact for the materials; anything
// else above the pivot, unlit or additive content and fog, is treated as a material too, so it comes out darker than vanilla
// stored it). SDR display modes render the vanilla curve already: only the UNORM clamp is emulated. Per channel, like the curve.
float4 YKR_VanillaScene(float4 scene)
{
   if (LumaSettings.DisplayMode != 1)
   {
      return saturate(scene);
   }
   const float pivot_display = YKR_PivotDisplayCurve();
   const float pivot_slope = YKR_PivotDisplayCurveSlope();
   float4 vanilla;
   [unroll] for (uint i = 0; i < 3; i++)
   {
      const float curve = sqr(max(scene[i], 0.0));
      const float display = pow(curve, 1.1);
      vanilla[i] = display > pivot_display ? sqrt(saturate(YKR_VanillaMaterialCurve(YKR_MATERIAL_CURVE_PIVOT + (display - pivot_display) / pivot_slope))) : sqrt(curve);
   }
   vanilla.a = saturate(scene.a);
   return vanilla;
}

// A bilinear sample of the scene as vanilla stored it (8-bit UNORM through the vanilla curve, every texel mapped before the filter,
// see "YKR_VanillaScene"), on the draws main.cpp flags with LumaData.CustomData1 (an fp16 scene read into a target that stayed
// 8-bit: the glow and auto exposure sources). Other draws of the same shader sample plainly. Mip 0, linear filter assumed.
float4 YKR_SampleVanillaScene(Texture2D<float4> tex, SamplerState s, float2 uv)
{
   if (LumaData.CustomData1 == 0u)
   {
      return tex.Sample(s, uv);
   }
   float2 size;
   tex.GetDimensions(size.x, size.y);
   const float2 f = frac(uv * size - 0.5);
   // Gather returns the texels (-,+), (+,+), (+,-), (-,-)
   const float4 r = tex.GatherRed(s, uv), g = tex.GatherGreen(s, uv), b = tex.GatherBlue(s, uv), a = tex.GatherAlpha(s, uv);
   const float4 weights = float4((1.0 - f.x) * f.y, f.x * f.y, f.x * (1.0 - f.y), (1.0 - f.x) * (1.0 - f.y));
   float4 result = 0.0;
   [unroll] for (uint i = 0; i < 4; i++)
   {
      result += YKR_VanillaScene(float4(r[i], g[i], b[i], a[i])) * weights[i];
   }
   return result;
}

// Linear (1.0 = paper white) -> the gamma-space post-process output of the grade and the videos, with the
// "GetUIPaperWhitePreScale" pre-scale (the HUD is drawn on top in gamma SDR).
float3 YKR_EncodeOutput(float3 color)
{
   color *= GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, false);
   return linear_to_gamma(color);
}

#endif // SRC_YKR_COMMON_HLSL
