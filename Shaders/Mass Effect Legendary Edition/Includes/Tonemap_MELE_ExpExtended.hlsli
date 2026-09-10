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

#endif // LUMA_MELE_TONEMAP_EXP_EXTENDED
