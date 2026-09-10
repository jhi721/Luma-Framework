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
   // Source-domain pivot, in the scene light the curve takes as INPUT - not paper white, nits, or a post-grade value.
   // Not calibrated against a capture. ⚠ It must stay above mid-gray: below it the tangent's intercept
   // F(p) - p*F'(p) turns negative and the extension crosses zero.
   const float p = 0.35;
   const float m = max3(scene);

   // Returning before the divide keeps the whole lower range bit-exact vanilla and avoids a division near black.
   if (m <= p)
      return sdrRef;

   const float pivotValue = ME2_NativeToneCurve(p.xxx).x;
   const float slope = ME2_NativeToneCurveSlope(p);
   const float extended = pivotValue + slope * (m - p); // the curve continued along its own tangent

   // F is monotone, so max(F(R), F(G), F(B)) == F(max(R, G, B)): reuse the evaluation the caller already has.
   const float nativeValue = max(max3(curved), 1e-6);
   // max(1, ...) is a FLOOR against darkening, not a ceiling. No saturate here or on the result: the display map
   // owns the roll-off, and clamping to 1 would throw away the highlights this exists to recover.
   const float gain = max(1.0, extended / nativeValue);

   return sdrRef * gain;
}

#endif // TONEMAP_TYPE >= 1 && ME2_UBER_FILMIC
#endif // LUMA_ME2_FILMIC_RECOVERY
