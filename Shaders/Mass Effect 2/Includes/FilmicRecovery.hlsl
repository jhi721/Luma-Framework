#ifndef LUMA_ME2_FILMIC_RECOVERY
#define LUMA_ME2_FILMIC_RECOVERY

// The HDR brightness recovery for the FILMIC uber permutation. The vanilla per-channel curve and the game grade stay
// the colour reference: this only decides HOW MUCH brighter that colour gets, never which colour it is.
// Include AFTER ME2_NativeToneCurve in Luma_ME2_Tonemap.hlsl. Needs max3() (Includes/Math.hlsl, via Common.hlsl).

#if TONEMAP_TYPE >= 1 && ME2_UBER_FILMIC

// d/dx of ME2's exact native curve, F(x) = Hejl(x - 0.004)^2. The exponent is 2.0, NOT 2.2 - the original squares a
// ~gamma-2.0 encoded value, it does not apply a display exponent. Analytic, so the pivot cannot desync from the curve.
float ME2_NativeToneCurveSlope(float scene)
{
   const float u = max(scene - 0.004, 0.0);
   const float den = u * (6.2 * u + 1.7) + 0.06; // >= 0.06 for u >= 0, no floor needed
   const float t = u * (6.2 * u + 0.5) / den;
   const float dt = ((7.44 * u + 0.744) * u + 0.03) / (den * den);
   return 2.0 * t * dt;
}

// `scene` is the exposed, DoF/bloom-mixed light the curve was fed. `curved` = ME2_NativeToneCurve(scene), pre-grade.
// `sdrRef` = VanillaToLinear(GradeUE3(curved, true, 1.0)), fade excluded - the CLAMPED grade, so it carries the
// vanilla per-channel hue skew and whitening. The result is that colour times ONE scalar: RGB ratios are preserved,
// white stays neutral, black stays black, and a channel the vanilla grade zeroed gets no light back.
float3 ME2_RecoverFilmicBrightness(float3 scene, float3 curved, float3 sdrRef)
{
   // Tuned HDR-extension onset, in the scene light the curve takes as INPUT - not paper white, nits, or a post-grade
   // value. A chosen constant, not a property of the curve: its own landmarks (inflection 0.065, the tangent through
   // the origin 0.145, mid-gray 0.18) each give a brighter extension, and none of them says what brightness the game
   // meant. Two hard bounds: below 0.145 the tangent's intercept F(p) - p*F'(p) turns negative and the extension
   // crosses zero; below 0.065 the curve is still convex and the tangent would dip under it. At 0.35, SDR white
   // recovers 1.35x, scene 4 -> 3.7x, scene 16 -> 13.3x.
   const float pivot = 0.35;
   // The source EXCURSION, not luminance: the native curve is per channel, so its hottest channel is the one the
   // shoulder compresses first - and F is monotone, so max3(F(scene)) == F(max3(scene)) below.
   const float sourcePeak = max3(scene);

   // Returning here keeps the whole lower range bit-exact vanilla.
   if (sourcePeak <= pivot)
      return sdrRef;

   const float pivotValue = ME2_NativeToneCurve(pivot.xxx).x;
   const float slope = ME2_NativeToneCurveSlope(pivot);
   const float extended = pivotValue + slope * (sourcePeak - pivot); // the curve continued along its own tangent

   // No denominator floor: this branch only runs above the pivot, where F(sourcePeak) >= F(0.35) = 0.433. In real
   // arithmetic the ratio needs no floor either - F is concave from its inflection at 0.065 up (F'' < 0, checked to
   // 200), so the tangent sits above the curve and the extension can only brighten. max(1, ...) stays regardless:
   // `extended` and `max3(curved)` are evaluated independently in float32, and right at the pivot, where they
   // coincide, rounding lands the ratio a few ULP under 1 (measured 1 - 1.2e-7 at 0.350001). The floor enforces
   // the invariant the maths promises, HDR never darker than vanilla, after that rounding; it is not an artistic
   // clamp. It also turns a NaN ratio into vanilla, since max() returns its non-NaN operand. No saturate on the
   // result: the display map owns the roll-off.
   const float gain = max(1.0, extended / max3(curved));

   return sdrRef * gain;
}

#endif // TONEMAP_TYPE >= 1 && ME2_UBER_FILMIC
#endif // LUMA_ME2_FILMIC_RECOVERY
