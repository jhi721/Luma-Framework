#ifndef SRC_YKR_COLOR_CORRECT_HLSL
#define SRC_YKR_COLOR_CORRECT_HLSL

// The "color correct" family (CDrawEffectScreenColorCorrection, ps_ccr_* and fx_ccr_*_mask): the first full-screen pass that
// reads the finished scene, then FXAA, the HUD and render_fb. Every replacement is the decompiled original verbatim (the family
// differs from the Remastered Collection's: its contrast divides the whole Y'CbCr->R'G'B' result, chroma included), renamed to
// "YKR_ColorCorrectGrade", with each scene sample wrapped in "YKR_ColorCorrectScene"; a generated "main" runs it once per
// grade below and hands both results to "YKR_ColorCorrectOutput".
//
// SDR: the vanilla grade on the scene as vanilla stored it ("YKR_VanillaScene"), exact.
// HDR (RenoDX's "Vanilla+"): the grade runs a second time on the extended scene compressed into its [0,1] range (Neutwo on the
// display-linear max channel, peak 1), then the result is expanded by the same scale; hue and most of the chrominance are taken
// from the vanilla grade (OKLab), then Exposure and a Neutwo max channel display map in BT.2020. Output: gamma space, 1.0 = UI
// paper white, values above 1 allowed (POST_PROCESS_SPACE_TYPE 0, UI_DRAW_TYPE 2).

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. "Common.hlsl" first, it pulls the Luma settings.
#include "Common.hlsl"
#include "../../Includes/ColorGradingLUT.hlsl" // RestoreHueAndChrominance
// clang-format on

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Set by "main" before the second (HDR) grade
static bool ykr_color_correct_compressed = false;
// The smallest compression scale among this pixel's scene samples (the "_ca" permutations take three)
static float ykr_color_correct_scale = 1.0;

// Neutwo, from RenoDX's "tonemap/neutwo.hlsl" (Carlos Lopez, MIT): x / sqrt(x^2 / peak^2 + 1), slope 1 at 0, asymptote "peak"
float YKR_Neutwo(float x, float peak)
{
   return peak * x * rsqrt(mad(x, x, peak * peak));
}

bool YKR_ColorCorrectHDR()
{
#if TONEMAP_TYPE >= 1
   return LumaSettings.DisplayMode == 1;
#else
   return false;
#endif
}

float4 YKR_ColorCorrectScene(float4 scene)
{
   if (!ykr_color_correct_compressed)
   {
      return YKR_VanillaScene(scene);
   }
   // The vanilla display decoded the encoded scene with its 2.2 gamma
   const float3 encoded = max(scene.rgb, 0.0);
   const float display_max = pow(max3(encoded), 2.2);
   const float scale = display_max > 0.0 ? YKR_Neutwo(display_max, 1.0) / display_max : 1.0;
   // ponytail: "_ca" expands its three taps by the most compressed one's scale; exact for every other permutation (one tap)
   ykr_color_correct_scale = min(ykr_color_correct_scale, scale);
   return float4(encoded * pow(scale, 1.0 / 2.2), saturate(scene.a));
}

// Both grades are gamma 2.2 encoded and end in a saturate
float4 YKR_ColorCorrectOutput(float4 vanilla_graded, float4 hdr_graded)
{
   const float3 vanilla_linear = gamma_to_linear(saturate(vanilla_graded.rgb));
   float3 color = vanilla_linear;
   [branch] if (ykr_color_correct_compressed)
   {
      color = gamma_to_linear(saturate(hdr_graded.rgb)) / ykr_color_correct_scale;
      // The vanilla grade is the clipped reference: hue below 0.9 (an achromatic reference has no hue to lock to), and 0.7 of its
      // chrominance, Splatoon's dose: most of vanilla's whitening of the brightest colors, which the display map no longer does.
      // Oklab chrominance grows with brightness, so the reference is first scaled to the color's luminance: what transfers is
      // vanilla's chrominance relative to its lightness, not its absolute value (a red that vanilla clipped at 1 would whiten).
      // Only the whitening transfers: the HDR grade already ran the vanilla saturation, and raising chrominance toward the scaled
      // reference pushes saturated colors out of BT.709.
      const float vanilla_luminance = GetLuminance(vanilla_linear);
      const float3 vanilla_reference = vanilla_luminance > 0.0 ? vanilla_linear * (GetLuminance(color) / vanilla_luminance) : vanilla_linear;
      color = RestoreHueAndChrominance(color, vanilla_reference, 0.8, 0.7, 0.0, 1.0);
      color *= LumaSettings.GameSettings.Exposure;
      // A scalar in BT.709 is the same scalar in BT.2020, whose max channel the PQ output holds at the peak
      const float display_max = max3(BT709_To_BT2020(color));
      const float paper_white = max(LumaSettings.GamePaperWhiteNits, 1.0);
      const float peak = max(LumaSettings.PeakWhiteNits, paper_white) / paper_white;
      color *= display_max > 0.0 ? YKR_Neutwo(display_max, peak) / display_max : 1.0;
   }
   // max also turns NaN into 0
   return float4(YKR_EncodeOutput(max(0.0, color)), vanilla_graded.a);
}

#endif // SRC_YKR_COLOR_CORRECT_HLSL
