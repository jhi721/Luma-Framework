#ifndef SRC_GAMMA_CANVAS_DISPLAY_MAP_HLSL
#define SRC_GAMMA_CANVAS_DISPLAY_MAP_HLSL

// The HDR display map of games whose scene stays a gamma encoded SDR canvas (POST_PROCESS_SPACE_TYPE 0, 1 = paper white) that the UI
// then blends onto as vanilla (Mirror's Edge, Dead Space 2). Include this after the game's "Common.hlsl".
// clang-format off
#include "Color.hlsl"
#include "CInfinityRolloff.hlsl" // Tonemap::CInfinityRollOff::MaxChannel (the display map)
#include "Reinhard.hlsl" // Reinhard::ReinhardPiecewise (hue reference)
#include "MacLeodBoynton.hlsl" // MacLeodBoynton::HueAndPurityEmulationBT2020
// clang-format on

// The display map's rolloff strength (MGR:R's and Vanquish's: the paper white stays within 1%)
static const float kGammaCanvasDisplayMapStrength = 1.0;

// The extended (unclipped) scene, linear BT.709 (1 = paper white), to the display: a max-channel C-infinity rolloff to the peak (MGR:R,
// Vanquish), which keeps the unclipped hue. The vanilla 8 bit targets clipped per channel, turning bright highlights toward the
// primaries (fire to yellow): that hue turn comes back from a per-channel Reinhard (ceiling 5, shoulder 1.5: identity below it) in
// full, its whitening by "highlightsDesaturation" (MacLeod-Boynton). Skipped below the shoulder, where the reference is the color
// itself. "hueEmulation" < 1 where the frame already carries that hue turn (Dead Space 2's LUT tail keeps the clipped vanilla grade's
// RGB ratios). "contrast" is a power of relative luminance around mid gray and "saturation" a scale of the RGB residual from
// relative luminance ("Saturation()": lerp(Y, rgb, s)), both 1 = neutral: the rolloff doesn't take them.
float3 GammaCanvasDisplayMap(float3 linearBT709, float contrast, float saturation, float highlightsDesaturation, float hueEmulation = 1.0)
{
   const float3 color2020 = BT709_To_BT2020(linearBT709);
   [branch] if ((hueEmulation > 0.0 || highlightsDesaturation > 0.0) && max3(color2020) > 1.5)
   {
      linearBT709 = BT2020_To_BT709(MacLeodBoynton::HueAndPurityEmulationBT2020(color2020, Reinhard::ReinhardPiecewise(color2020, 5.0, 1.5), hueEmulation, highlightsDesaturation));
   }
   [branch] if (contrast != 1.0)
   {
      linearBT709 *= pow(max(GetLuminance(linearBT709, CS_BT709), FLT_MIN) / MidGray, contrast - 1.0);
   }
   [branch] if (saturation != 1.0)
   {
      linearBT709 = Saturation(linearBT709, saturation, CS_BT709);
   }
   // The peak can't go below paper white (the rolloff needs it above its 0.18 anchor)
   const float paperWhite = max(GamePaperWhiteNits, 1.0);
   return Tonemap::CInfinityRollOff::MaxChannel(linearBT709, max(PeakWhiteNits, paperWhite) / paperWhite, 0.18, kGammaCanvasDisplayMapStrength);
}

// A linear frame back to the canvas: gamma space, pre-scaled for UI_DRAW_TYPE 2 (the UI then blends onto it as vanilla), then one step of
// the output quantizer's dither (the 8 bit code in SDR, the 10 bit BT.2020 PQ code in HDR).
float3 EncodeGammaCanvas(float3 linearColor, float2 uv, bool dithering)
{
   float3 outColor = linear_to_gamma(max(0.0, linearColor * GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true)));
   if (dithering)
   {
      ApplyOutputDithering(outColor, uv, true, LumaSettings.DisplayMode != 0, LumaSettings.UIPaperWhiteNits);
   }
   // Last: the dither can step below 0 and the grade can emit NaN (a NaN in the canvas reads back black)
   return IsNaN_Strict(outColor) ? 0.0 : max(0.0, outColor);
}

#endif // SRC_GAMMA_CANVAS_DISPLAY_MAP_HLSL
