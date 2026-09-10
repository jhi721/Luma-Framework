#ifndef LUMA_MELE_TONEMAP_EXP_EXTENDED
#define LUMA_MELE_TONEMAP_EXP_EXTENDED

// Pure math. Prerequisites: Includes/Common.hlsl (for MELE_NativeToneCurve) and the shared
// Color.hlsl, both already included by every body that uses this file.

// Tangent continuation of the native per-channel curve past scene mid-gray, for the two
// exponential families. Below the pivot this IS MELE_NativeToneCurve, bit for bit; above it the
// curve is replaced by its own tangent so the working value keeps rising instead of saturating.
//
//   F(x)  = 1 - exp2(-a*x),  a = 1.70000005          (Includes/Common.hlsl:44, unchanged)
//   F'(p) = a * ln2 * exp2(-a*p)
//   E(x)  = F(x)                     for x <= p
//         = F(p) + F'(p) * (x - p)   for x >  p
//
// There is no positive inflection in this exponential, so there is no Hable-style shoulder to find
// here and no foreign coefficient to import. MELE_NativeToneCurve itself is never modified: the SDR
// reference still runs through it, and only this branch sees the continuation.
float3 MELE_ExpExtended(float3 x, float pivot)
{
   const float slope = 1.70000005 * 0.693147181 * exp2(-1.70000005 * pivot);
   const float3 tangent = MELE_NativeToneCurve(pivot) + slope * (x - pivot);
   return float3(x.x <= pivot ? MELE_NativeToneCurve(x.x) : tangent.x,
                 x.y <= pivot ? MELE_NativeToneCurve(x.y) : tangent.y,
                 x.z <= pivot ? MELE_NativeToneCurve(x.z) : tangent.z);
}

// Unbounded twin of MELE_NativeGammaCurve (Includes/Common.hlsl:30) for the analytic families:
// same scale and same exponent, without the leading saturate that caps the encoded result at 1.
// Includes/Common.hlsl is shared and is NOT edited; this is a separate function so the SDR
// reference keeps calling the faithful one. The black floor is deliberately absent because the two
// analytic ME1LE/ME2LE permutations pass clampFloor = false, and that asymmetry is transcribed
// from bytecode (Includes/Common.hlsl:25-29), not a rounding of one form to the other.
//
// GCT_MIRROR is the odd extension pow(|x|) * sign(x), so a negative working value survives the
// encode instead of turning into a NaN. The caller still owns the decision to fall back.
float3 MELE_NativeGammaCurveHDR(float3 c, float3 scale, float invGamma)
{
   return linear_to_gamma(scale * c, GCT_MIRROR, 1.0 / invGamma);
}

#endif // LUMA_MELE_TONEMAP_EXP_EXTENDED
