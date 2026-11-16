#ifndef SRC_BAYONETTA_DISPLAY_MAP_HLSL
#define SRC_BAYONETTA_DISPLAY_MAP_HLSL

// clang-format off
#include "../../Includes/Reinhard.hlsl" // Reinhard::ReinhardPiecewise (hue reference)
#include "../../Includes/MacLeodBoynton.hlsl" // MacLeodBoynton::HueAndPurityEmulationBT2020
// clang-format on

// ResetRender's HDR path, on the scene copied into the canvas right before the UI and the movies draw on it: Copy_0x91A6C437 on
// dgVoodoo, a D3D12 pass over the canvas on native D3D9 (_tools/d9_native_spike). "color" is the gamma encoded scene (1 = paper white,
// gamma 2.2 display) with the grade's and the bloom's values above 1; the result is encoded the same way for the Core composition.
// Linearize, the vanilla clip's hue turn (MacLeod-Boynton), the user's contrast and saturation, the display map (a max-channel
// C-infinity rolloff to the peak), the UI paper white pre-scale (UI_DRAW_TYPE 2). HDR only (DisplayMode 1): in SDR the callers clip
// the scene as the 8-bit canvas did, early (a return in here gets flattened into the HDR path).
float3 ResetRenderDisplayMap(float3 color)
{
#if TONEMAP_TYPE >= 1
   color = IsNaN_Strict(color) ? 0.0 : max(color, 0.0);
   // Vanilla clipped per channel in 8-bit (the scene before the grade, the canvas here), turning bright highlights toward the
   // primaries; the grades' tangent continuation and the max-channel map keep the unclipped hue. Restore only that hue turn, from a
   // per-channel Reinhard (ceiling 5, shoulder 1.5: identity below it, so the scene under 1.5 stays exact), the reference our RenoDX ports ship, at hue
   // strength 1. "Highlights Desaturation" is the chrominance strength, the share of the reference's whitening (0 = none, 1 = vanilla's).
   // Before the map on purpose: it is hue neutral and contains any channel the turn lifts. Below the shoulder the reference is the
   // colour itself, so the solve (some 300 instructions) is skipped there.
   color = gamma_to_linear(color, GCT_MIRROR);
   const float3 color2020 = BT709_To_BT2020(color);
   [branch] if (max3(color2020) > 1.5)
   {
      color = BT2020_To_BT709(MacLeodBoynton::HueAndPurityEmulationBT2020(color2020, Reinhard::ReinhardPiecewise(color2020, 5.0, 1.5), 1.0,
                                                                          LumaSettings.GameSettings.HighlightsDesaturation));
   }
   // Contrast (a power around mid gray) and saturation, as Sunset Overdrive's DICE path: the map below doesn't take them
   const float contrast = LumaSettings.GameSettings.Contrast;
   [branch] if (contrast != 1.0)
   {
      color *= pow(max(GetLuminance(color, CS_BT709), FLT_MIN) / MidGray, contrast - 1.0);
   }
   const float saturationScale = LumaSettings.GameSettings.Saturation;
   [branch] if (saturationScale != 1.0)
   {
      color = Saturation(color, saturationScale, CS_BT709);
   }
   // Linear BT.709, 1 = paper white; the peak can't go below paper white (the rolloff needs it above its 0.18 anchor)
   const float paperWhite = max(GamePaperWhiteNits, 1.0);
   color = Tonemap::CInfinityRollOff::MaxChannel(color, max(PeakWhiteNits, paperWhite) / paperWhite, 0.18, kDisplayMapStrength);
#else
   // The vanilla clip, on the HDR canvas: only the UI paper white pre-scale the composition expects
   color = gamma_to_linear(saturate(color), GCT_MIRROR);
#endif
   color *= GetUIPaperWhitePreScale(GamePaperWhiteNits, UIPaperWhiteNits, true);
   return linear_to_gamma(color, GCT_MIRROR);
}

#endif // SRC_BAYONETTA_DISPLAY_MAP_HLSL
