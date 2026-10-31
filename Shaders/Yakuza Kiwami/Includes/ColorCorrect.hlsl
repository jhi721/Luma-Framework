#ifndef SRC_YKR_COLOR_CORRECT_HLSL
#define SRC_YKR_COLOR_CORRECT_HLSL

// The "color correct" family (CDrawEffectScreenColorCorrection, ps_ccr_* and fx_ccr_*_mask): the first full-screen pass that
// reads the finished scene, then FXAA, the HUD and render_fb. Every replacement is the decompiled original verbatim (the family
// differs from the Remastered Collection's: its contrast divides the whole Y'CbCr->R'G'B' result, chroma included), renamed to
// "YKR_ColorCorrectGrade", with each scene sample wrapped in "YKR_ColorCorrectScene"; a generated "main" runs it once per
// grade below and hands both results to "YKR_ColorCorrectOutput".
//
// SDR: the vanilla grade on the scene as vanilla stored it ("YKR_VanillaScene"), exact.
// HDR: the grade runs a second time on the extended scene compressed into its [0,1] range (Neutwo on the display-linear max
// channel, peak 1), then the result is expanded by the same scale and takes the vanilla grade's hue. Exposure, contrast and
// saturation follow, then DICE (by channel, in PQ) maps it to the peak. Output: gamma space, 1.0 = UI paper white, values above 1
// allowed (POST_PROCESS_SPACE_TYPE 0, UI_DRAW_TYPE 2).

// clang-format off
// ORDER IS LOAD-BEARING - do not sort. "Common.hlsl" first, it pulls the Luma settings.
#include "Common.hlsl"
#include "../../Includes/DICE.hlsl"
#include "../../Includes/MacLeodBoynton.hlsl" // Neutwo
// clang-format on

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Set by "main" before the second (HDR) grade
static bool ykr_color_correct_compressed = false;
// The smallest compression scale among this pixel's scene samples (the "_ca" permutations take three)
static float ykr_color_correct_scale = 1.0;

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
   const float scale = display_max > 0.0 ? MacLeodBoynton::Neutwo(display_max) / display_max : 1.0;
   // ponytail: "_ca" expands its three taps by the most compressed one's scale; exact for every other permutation (one tap)
   ykr_color_correct_scale = min(ykr_color_correct_scale, scale);
   return float4(encoded * pow(scale, 1.0 / 2.2), saturate(scene.a));
}

// Both grades are gamma 2.2 encoded and end in a saturate
float4 YKR_ColorCorrectOutput(float4 vanilla_graded, float4 hdr_graded)
{
   const float3 vanilla_encoded = saturate(vanilla_graded.rgb);
   float3 color = gamma_to_linear(vanilla_encoded);
   [branch] if (ykr_color_correct_compressed)
   {
      float3 extended_linear = gamma_to_linear(saturate(hdr_graded.rgb)) / ykr_color_correct_scale * LumaSettings.GameSettings.Exposure;
      // The materials bend each channel through the soft vanilla curve, so the vanilla grade is the hue reference. Where it whitens
      // (the curve's clip) its hue is unstable, so the restoration fades out by its HSV saturation coordinate,
      // (max - min) / max of the gamma-encoded grade (0.05..0.3, Y5R's gate).
      const float vanilla_max = max3(vanilla_encoded);
      const float hue_gate = smoothstep(0.05, 0.3, vanilla_max > 0.0 ? (vanilla_max - min3(vanilla_encoded)) / vanilla_max : 0.0);
      extended_linear = RestoreHueAndChrominance(extended_linear, color, hue_gate, 0.0, 0.0, FLT_MAX, 0.0, CS_BT709);
      // Contrast (a power around mid gray) and saturation go before DICE
      const float contrast = LumaSettings.GameSettings.Contrast;
      [branch] if (contrast != 1.0)
      {
         extended_linear *= pow(max(GetLuminance(extended_linear, CS_BT709), FLT_MIN) / MidGray, contrast - 1.0);
      }
      const float saturation = LumaSettings.GameSettings.Saturation;
      [branch] if (saturation != 1.0)
      {
         extended_linear = Saturation(extended_linear, saturation, CS_BT709);
      }
      const float paper_white = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
      const float peak_white = max(LumaSettings.PeakWhiteNits, LumaSettings.GamePaperWhiteNits) / sRGB_WhiteLevelNits;
      color = DICETonemap(extended_linear * paper_white, peak_white, DefaultDICESettings(DICE_TYPE_BY_CHANNEL_PQ)) / paper_white;
   }
   // max also turns NaN into 0
   return float4(YKR_EncodeOutput(max(0.0, color)), vanilla_graded.a);
}

#endif // SRC_YKR_COLOR_CORRECT_HLSL
