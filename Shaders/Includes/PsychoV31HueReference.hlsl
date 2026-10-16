#ifndef LUMA_PSYCHOV31_HUE_REFERENCE_HLSL
#define LUMA_PSYCHOV31_HUE_REFERENCE_HLSL

// Kept apart from "PsychoV31.hlsl" so its other consumers don't pull in the MacLeod-Boynton model (its static initializers alone
// perturb fxc's code generation). Include this after the game's "Common.hlsl".
#include "MacLeodBoynton.hlsl"
#include "PsychoV31.hlsl"
#include "Reinhard.hlsl"

// "PsychoVDisplayMap()" after the colour stage of RenoDX's BL1 port, for grades whose vanilla clipped per channel: a per channel
// ReinhardPiecewise(x, 5, "hueReferenceShoulder") of the color in BT.2020 is a soft hue reference (linear below the shoulder and rolling
// toward 5 above, it bends a saturated highlight's hue the way the clip did, without its whitening), whose hue direction MacLeod-Boynton
// rebuilds on the color's own purity and T = L + M anchor (hue strength 1, chrominance 0). The hue transfer can leave BT.709, so the
// display map's source boundary is BT.2020. "color" is linear BT.709, 1 = paper white.
// "skipBelowShoulder": skip the hue stage (some 300 instructions of purity solves) for colors with no channel above the shoulder, where
// it is a no-op up to float rounding (the reference equals the color there, as BT.2020 channels never exceed the BT.709 max).
float3 PsychoVDisplayMapWithHueReference(float3 color, float contrast, float saturation, float highlightsDesaturation,
                                         float hueReferenceShoulder = 1.5, bool skipBelowShoulder = false)
{
   [branch] if (!skipBelowShoulder || max3(color) > hueReferenceShoulder)
   {
      const float3 colorBT2020 = BT709_To_BT2020(color);
      const float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(colorBT2020, 5.0, hueReferenceShoulder);
      color = BT2020_To_BT709(MacLeodBoynton::HueOnlyBT2020(colorBT2020, hueReferenceBT2020));
   }
   return PsychoVDisplayMap(color, contrast, saturation, highlightsDesaturation, renodx::tonemap::psychov::PSYCHO30_SOURCE_BOUNDARY_BT2020);
}

#endif // LUMA_PSYCHOV31_HUE_REFERENCE_HLSL
