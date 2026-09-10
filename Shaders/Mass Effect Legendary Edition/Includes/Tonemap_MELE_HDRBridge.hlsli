#ifndef LUMA_MELE_TONEMAP_HDR_BRIDGE
#define LUMA_MELE_TONEMAP_HDR_BRIDGE

#include "Tonemap_MELE_ExperimentConfig.hlsli"

// Pure math. Prerequisites, which this file deliberately does NOT include:
//   ../Includes/Color.hlsl    gamma_to_linear, GetLuminance, GCT_MIRROR
//   ../Includes/Math.hlsl     max3, IsNaN_Strict, IsInfinite_Strict  (arrive through Color.hlsl)
//   ../Includes/Reinhard.hlsl ReinhardRange
// Reinhard.hlsl has no include guard, unlike every other shared header, so including it from here
// would be a duplicate-namespace error in the two LUT bodies that already include it. Include it in
// the body, before this file.

// The validity predicate every experimental family shares. Finite AND non-negative, checked with the
// strict helpers rather than with x != x, which a fast-math build is free to fold away. Non-negative
// is part of it because every value this experiment guards - a scene sample, a bloom contribution, a
// tone response, a decoded grade output - is a light quantity with no meaning below zero, and because
// the nonlinear steps downstream (log2, pow, a LUT coordinate) turn a negative into a NaN several
// operations after the point where it could still have been reported.
bool MELE_IsFiniteNonNegative(float x)
{
   return !IsNaN_Strict(x) && !IsInfinite_Strict(x) && x >= 0.0;
}
bool MELE_IsFiniteNonNegative(float3 v)
{
   return !IsAnyNaN_Strict(v) && !any(IsInfinite_Strict(v)) && all(v >= 0.0);
}

// The signed sibling, for values that have every right to be negative: an artist's shadow lift, a
// colour-grading weight, an overlay offset, and the intermediate differences those produce. Applying
// the non-negative predicate to one of those would reject valid game data as corrupt, so the two are
// deliberately separate and the choice between them is made per value, never by habit.
bool MELE_IsFinite(float x)
{
   return !IsNaN_Strict(x) && !IsInfinite_Strict(x);
}
bool MELE_IsFinite(float3 v)
{
   return !IsAnyNaN_Strict(v) && !any(IsInfinite_Strict(v));
}

// The neutral composite transfer between the grade-input domain and the linear domain the caller
// finally works in. For every MELE family the tail from grade input to linear graded_hdr is
// MELE_NativeGammaCurve, pow(saturate(scale*c), invGamma), followed by gamma_to_linear(., GCT_MIRROR),
// pow(|x|, DefaultGamma). With an identity colour grade that composes to (scale*c)^r for
//
//   r = GammaColorScaleAndInverse.w * DefaultGamma
//
// read from the game cbuffer, never hardcoded to 2.2: the exponent is a property of the frame, and
// at r == 1 the adapter is the identity. Passing r in keeps this header free of the per-body $Globals.
//
// Why the adapter is needed at all: scaling the grade INPUT by s scales the decoded linear OUTPUT by
// s^r. Working in the adapted domain makes the restore exact - W = A(X), P = q*W, Q = A^-1(P) scales
// the input by q^(1/r), so the decoded output scales by exactly q and a plain divide undoes it. Doing
// the same with a raw input multiply and a linear divide leaves a q^(r-1) residue whenever r != 1.
// gamma_to_linear(c, ., G) is pow(c, G) (../Includes/Color.hlsl:370), so A is the G = r call and
// A^-1 the G = 1/r one. GCT_MIRROR keeps a negative working value signed rather than turning it
// into a NaN; whether to trust that value at all stays the caller's decision.
float3 MELE_BridgeAdapt(float3 v, float r)
{
   return gamma_to_linear(v, GCT_MIRROR, r);
}
float3 MELE_BridgeUnadapt(float3 v, float r)
{
   return gamma_to_linear(v, GCT_MIRROR, 1.0 / r);
}

// State carried from the compression to the restore. Keep the scale in the domain it was defined
// in; it is not transferable between gamma, LUT codes, scene units and nits.
struct MELE_BridgeState
{
   float q; // Max-channel compression applied to the adapted working value; 1 below the shoulder.
   float r; // The adapter exponent the scale was defined against.
};

// Max-channel proxy. One scalar for all three channels, so the limiter itself cannot move an RGB
// ratio; the per-channel character stays owned by the working curve and by the native reference.
//
// ReinhardRange with In_Peak <= 0 compresses from infinity and is exactly the shifted rational
// shoulder this needs, k + (1-k)(m-k)/((m-k)+(1-k)): identity at and below k, C1 across the seam,
// asymptotic to 1. It is NOT ReinhardPiecewise, which matches value but not derivative at its seam;
// do not substitute one for the other on the strength of the shared name.
//
// EVERY check runs before the data-dependent pow, division and LUT sampling it protects. An invalid
// working value has to be reported here and not carried into the grade to be recognised afterwards by
// the finiteness of whatever came out: the grade opens with a saturate or a min(1) in every
// permutation, so it launders a bad input into a plausible number the caller can no longer question.
//
// On failure both out parameters keep the neutral values written at entry and the caller must use
// neither: false means take the family's own legacy result for the whole RGB triple.
bool MELE_TryBuildGradeProxy(float3 work_native, float r, out MELE_BridgeState state, out float3 proxy_native)
{
   state.q = 1.0;
   state.r = r;
   proxy_native = work_native;

   const float k = MELE_HDR_BRIDGE_SHOULDER;
   if (!MELE_IsFiniteNonNegative(work_native) || !MELE_IsFiniteNonNegative(r) || r <= 0.0 || !(k > 0.0 && k < 1.0))
   {
      return false;
   }

   // pow(x, 1) is exp2(log2(x)) on this hardware, not the identity, and r was measured at exactly 1
   // in every captured frame. This keeps that overwhelmingly common case bit-exact; it is a shortcut
   // for one value of r, never an assumption that r is 1.
   const float3 adapted = (r == 1.0) ? work_native : MELE_BridgeAdapt(work_native, r);
   if (!MELE_IsFiniteNonNegative(adapted))
   {
      return false;
   }

   const float m = max3(adapted);
   if (m <= k)
   {
      return true; // q stays 1 and the proxy stays the working value: no round trip, no roundoff.
   }

   const float q = Reinhard::ReinhardRange(m.xxx, k).x / m;
   const float3 compressed = adapted * q;
   // The shoulder is asymptotic to 1, so the compressed value belongs to a bounded domain. The
   // tolerance covers roundoff in that shoulder only; a compressed value genuinely above 1 means the
   // shoulder did not do its job, which is a failure rather than something to clamp quietly.
   if (!MELE_IsFiniteNonNegative(q) || q <= 0.0 || q > 1.0 || !MELE_IsFiniteNonNegative(compressed) || max3(compressed) > 1.0 + MELE_BRIDGE_PROXY_EPS)
   {
      return false;
   }

   const float3 candidate_proxy = (r == 1.0) ? compressed : MELE_BridgeUnadapt(compressed, r);
   if (!MELE_IsFiniteNonNegative(candidate_proxy))
   {
      return false;
   }
   state.q = q;
   proxy_native = candidate_proxy;
   return true;
}

// Undo the compression on the decoded linear grade output. Never invert the curve by re-reading the
// changed LUT output: the LUT moved the colour, so that read cannot recover the original scale.
bool MELE_TryRestoreGradeRange(float3 graded_linear, MELE_BridgeState state, out float3 work_hdr)
{
   work_hdr = graded_linear;
   if (!MELE_IsFiniteNonNegative(graded_linear) || !MELE_IsFiniteNonNegative(state.q) || state.q <= 0.0)
   {
      return false;
   }
   const float3 restored = graded_linear / state.q;
   if (!MELE_IsFiniteNonNegative(restored))
   {
      return false;
   }
   work_hdr = restored;
   return true;
}

// The NATIVE colour mode: RGB ratios from the exact native grade result, luminance from the new
// working value. Y is the same linear BT.709 luminance on both sides, so on a finite positive
// reference the reference's channel ratios survive exactly. That is the whole point - it keeps the
// per-channel shift and the whitening the native chain produced rather than undoing them. A coloured
// reference stays coloured, equal channels stay equal, and a channel the grade zeroed is not refilled.
//
// Of the working value only Y is used. Its own hue and chroma are deliberately discarded, so this
// must not be described as also preserving the working branch's colour advantages. The mode is
// chosen at the call site; this function is the NATIVE implementation, not a switch.
//
// The guarantee ends here, before the shared output tail. Whatever the vignette, DICE, the user
// controls and the late SDR clamp then do is measured separately and is not promised by this.
//
// Guard contract, which is deliberately not one blanket fallback:
//   the WHOLE reference triple is validated, not only its luminance. A dot product can return a
//     finite number from channels that are not finite, and a positive one from a reference that has
//     a negative channel, so checking Y alone accepts colours that cannot be scaled.
//   exactly black reference or exactly zero target -> black. The grade produced that black and this
//     is not the place to put light back into it.
//   near-black but positive reference -> divided normally. The LUT permutations carry clampFloor,
//     so a vanilla black floors at 1e-4 linear and, the encode and the decode being inverse powers,
//     arrives back at 1e-4 in luminance, two decades above the guard below. Collapsing that to black
//     would erase a real part of the native output. Count the guard's trips rather than widening it.
//   a non-finite gain or product -> the caller's legacy value for the WHOLE triple. Switching
//     channels independently would change hue, which is the failure being avoided.
// A positive target luminance is never clamped to 1, and no path takes colour from the raw scene.
// On every input the previous implementation accepted - a finite non-negative reference with a
// positive luminance - this returns exactly what it returned. The added checks reject inputs; they
// do not reshape an accepted result.
#define MELE_NATIVE_COLOR_MIN_LUMINANCE 1e-6

float3 MELE_NativeColorAtLuminance(float3 native_reference_linear, float target_luminance, float3 legacy_family_hdr)
{
   if (!MELE_IsFiniteNonNegative(native_reference_linear) || !MELE_IsFiniteNonNegative(target_luminance))
   {
      return legacy_family_hdr;
   }
   if (target_luminance == 0.0 || all(native_reference_linear == 0.0))
   {
      return float3(0.0, 0.0, 0.0);
   }
   const float reference_luminance = GetLuminance(native_reference_linear, CS_BT709);
   if (!MELE_IsFiniteNonNegative(reference_luminance) || reference_luminance < MELE_NATIVE_COLOR_MIN_LUMINANCE)
   {
      return legacy_family_hdr;
   }
   const float gain = target_luminance / reference_luminance;
   const float3 result = native_reference_linear * gain;
   if (!MELE_IsFiniteNonNegative(gain) || !MELE_IsFiniteNonNegative(result))
   {
      return legacy_family_hdr;
   }
   return result;
}
#endif // LUMA_MELE_TONEMAP_HDR_BRIDGE
