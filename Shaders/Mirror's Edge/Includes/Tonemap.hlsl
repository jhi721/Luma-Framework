// The two grades that write the gamma-encoded canvas (TdToneMapping, and GammaCorrection when the TdTonemapping system setting is
// off): dgVoodoo's D3D9 constants, the original's pow, the HDR display map and the canvas encode they share.
#ifndef LUMA_MIRRORS_EDGE_TONEMAP
#define LUMA_MIRRORS_EDGE_TONEMAP

// clang-format off
#include "../../Includes/Color.hlsl"
#include "../../Includes/CInfinityRolloff.hlsl" // Tonemap::CInfinityRollOff::MaxChannel (the display map)
#include "../../Includes/Reinhard.hlsl" // Reinhard::ReinhardPiecewise (hue reference)
#include "../../Includes/MacLeodBoynton.hlsl" // MacLeodBoynton::HueAndPurityEmulationBT2020
#include "../../Includes/DgVoodoo.hlsl"
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

// The original's pow: |x| floored at 1e-4, dgVoodoo's guarded log.
float3 PowTd(float3 base, float3 exponent)
{
   const float3 b = max(abs(base), 0.0001);
   return exp2(exponent * float3(DgVoodooLog2(b.x), DgVoodooLog2(b.y), DgVoodooLog2(b.z)));
}

// The display map's rolloff strength (MGR:R's and Vanquish's: the paper white stays within 1%)
static const float kDisplayMapStrength = 1.0;

// The extended (unclipped) grade, linear BT.709 (1 = paper white), to the display: a max-channel C-infinity rolloff to the peak (MGR:R,
// Vanquish), which keeps the unclipped hue. The original clipped per channel, turning bright highlights toward the primaries (fire to
// yellow): that hue turn comes back from a per-channel Reinhard (ceiling 5, shoulder 1.5: identity below it) in full, its whitening at
// "Highlights Desaturation" (MacLeod-Boynton). Skipped below the shoulder, where the reference is the color itself.
float3 DisplayMapTd(float3 linearBT709)
{
   const float3 color2020 = BT709_To_BT2020(linearBT709);
   [branch] if (max3(color2020) > 1.5)
   {
      linearBT709 = BT2020_To_BT709(MacLeodBoynton::HueAndPurityEmulationBT2020(color2020, Reinhard::ReinhardPiecewise(color2020, 5.0, 1.5),
                                                                                1.0, LumaSettings.GameSettings.HighlightsDesaturation));
   }
   // Contrast (a power of relative luminance around mid gray) and a chrominance scale around relative luminance
   // ("Saturation()": lerp(Y, rgb, s)): the rolloff doesn't take them
   const float contrast = LumaSettings.GameSettings.Contrast;
   [branch] if (contrast != 1.0)
   {
      linearBT709 *= pow(max(GetLuminance(linearBT709, CS_BT709), FLT_MIN) / MidGray, contrast - 1.0);
   }
   const float saturationScale = LumaSettings.GameSettings.Saturation;
   [branch] if (saturationScale != 1.0)
   {
      linearBT709 = Saturation(linearBT709, saturationScale, CS_BT709);
   }
   // The peak can't go below paper white (the rolloff needs it above its 0.18 anchor)
   const float paperWhite = max(GamePaperWhiteNits, 1.0);
   return Tonemap::CInfinityRollOff::MaxChannel(linearBT709, max(PeakWhiteNits, paperWhite) / paperWhite, 0.18, kDisplayMapStrength);
}

// A linear frame as the canvas holds it: gamma space (POST_PROCESS_SPACE_TYPE 0), so the HUD blends onto it like vanilla.
float3 EncodeTdCanvas(float3 linearColor, float2 uv)
{
   float3 outColor = linear_to_gamma(max(0.0, linearColor * GetUIPaperWhitePreScale(LumaSettings.GamePaperWhiteNits, LumaSettings.UIPaperWhiteNits, true)));
   // One step of the output quantizer: the 8 bit code in SDR, the 10 bit BT.2020 PQ code in HDR
   if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyOutputDithering(outColor, uv, true, LumaSettings.DisplayMode != 0, LumaSettings.UIPaperWhiteNits);
   }
   // Last: the dither can step below 0 and the grade can emit NaN (a NaN in the canvas reads back black)
   return IsNaN_Strict(outColor) ? 0.0 : max(0.0, outColor);
}

#endif // LUMA_MIRRORS_EDGE_TONEMAP
