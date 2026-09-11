#ifndef LUMA_MELE_TONEMAP_HDR_BRIDGE
#define LUMA_MELE_TONEMAP_HDR_BRIDGE

#include "Tonemap_MELE_HDRConfig.hlsli"

// Pure math. Prerequisites, which this file deliberately does NOT include:
//   ../Includes/Color.hlsl    gamma_to_linear, GetLuminance, GCT_MIRROR
//   ../Includes/Math.hlsl     max3, IsNaN_Strict, IsInfinite_Strict  (arrive through Color.hlsl)
//   ../Includes/Reinhard.hlsl ReinhardRange
// Reinhard.hlsl has no include guard, unlike every other shared header, so including it from here
// would be a duplicate-namespace error in the bodies that already include it. Include it in the body,
// before this file.

// Two predicates, not one. Every value this reconstruction guards is either a light quantity that has no
// meaning below zero, or an artist dial whose sign is free - a shadow lift, a luminance weight, an
// overlay offset, and the signed differences those produce. Applying the non-negative form to one of
// those would reject valid game data as corrupt, so the choice is made per value.
//
// The non-negative pair repeats the two strict tests instead of calling MELE_IsFinite. That is
// deliberate: expressing it as MELE_IsFinite(x) && x >= 0 reads better but fxc does not fold the call
// away, and it cost two instructions in every permutation that uses the bridge. Measured, not assumed.
bool MELE_IsFinite(float x)
{
   return !IsNaN_Strict(x) && !IsInfinite_Strict(x);
}
bool MELE_IsFinite(float3 v)
{
   return !IsAnyNaN_Strict(v) && !any(IsInfinite_Strict(v));
}
bool MELE_IsFiniteNonNegative(float x)
{
   return !IsNaN_Strict(x) && !IsInfinite_Strict(x) && x >= 0.0;
}
bool MELE_IsFiniteNonNegative(float3 v)
{
   return !IsAnyNaN_Strict(v) && !any(IsInfinite_Strict(v)) && all(v >= 0.0);
}

// The composite transfer between the grade-input domain and the linear domain the caller works in.
// For every MELE family the tail from grade input to linear gradedHDR is MELE_NativeGammaCurve
// followed by gamma_to_linear(., GCT_MIRROR), which with an identity colour grade composes to
// (scale*c)^r for r = GammaColorScaleAndInverse.w * DefaultGamma.
//
// r is read from the frame's own cbuffer and never hardcoded to 2.2. Passing it in keeps this header
// free of the per-body $Globals.
//
// Why an adapter at all: scaling the grade INPUT by s scales the decoded linear OUTPUT by s^r.
// Compressing in the adapted domain makes the restore exact - W = A(X), P = q*W, Q = A^-1(P) scales
// the input by q^(1/r), so the output scales by exactly q and a plain divide undoes it. A raw input
// multiply with a linear divide would leave a q^(r-1) residue whenever r != 1. GCT_MIRROR keeps a
// negative working value signed rather than turning it into a NaN; whether to trust it stays the
// caller's decision.
float3 MELE_BridgeAdapt(float3 v, float r)
{
   return gamma_to_linear(v, GCT_MIRROR, r);
}
float3 MELE_BridgeUnadapt(float3 v, float r)
{
   return gamma_to_linear(v, GCT_MIRROR, 1.0 / r);
}

// Max-channel proxy. One scalar for all three channels, so the limiter cannot move an RGB ratio; the
// per-channel character stays owned by the working curve and by the native reference.
//
// ReinhardRange with In_Peak <= 0 compresses from infinity and is the shifted rational shoulder this
// needs: identity at and below k, C1 across the seam, asymptotic to 1. It is NOT ReinhardPiecewise,
// which matches value but not derivative at its seam.
//
// q BELONGS TO THE ADAPTED DOMAIN. It is not transferable between gamma, LUT codes, scene units and
// nits, and MELE_TryRestoreGradeRange must be handed the same q this produced.
//
// Every check runs BEFORE the pow, the division and the LUT read it protects. An invalid working
// value cannot be recognised afterwards from the finiteness of the output: every permutation's grade
// opens with a saturate or a min(1), which launders a bad input into a plausible number.
//
// On failure both out parameters keep the neutral values written at entry and the caller must use
// neither: false means the HDR reconstruction declined and the caller retains the exact
// native SDR reference for the whole RGB triple.
bool MELE_TryBuildGradeProxy(float3 workNative, float r, out float q, out float3 proxyNative)
{
   q = 1.0;
   proxyNative = workNative;

   const float k = MELE_HDR_BRIDGE_SHOULDER;
   if (!MELE_IsFiniteNonNegative(workNative) || !MELE_IsFiniteNonNegative(r) || r <= 0.0 || !(k > 0.0 && k < 1.0))
   {
      return false;
   }

   // pow(x, 1) is exp2(log2(x)) on this hardware, not the identity, and r was measured at exactly 1
   // in every captured frame. This keeps that common case bit-exact; it is a shortcut for one value
   // of r, never an assumption that r is 1.
   const float3 adapted = (r == 1.0) ? workNative : MELE_BridgeAdapt(workNative, r);
   if (!MELE_IsFiniteNonNegative(adapted))
   {
      return false;
   }

   const float m = max3(adapted);
   if (m <= k)
   {
      return true; // q stays 1 and the proxy stays the working value: no round trip, no roundoff.
   }

   const float scale = Reinhard::ReinhardRange(m.xxx, k).x / m;
   const float3 compressed = adapted * scale;
   // The shoulder is asymptotic to 1, so the tolerance covers roundoff in it and nothing else. A
   // compressed value genuinely above 1 means the shoulder did not do its job, which is a failure
   // rather than something to clamp quietly.
   if (!MELE_IsFiniteNonNegative(scale) || scale <= 0.0 || scale > 1.0 || !MELE_IsFiniteNonNegative(compressed) || max3(compressed) > 1.0 + MELE_BRIDGE_PROXY_EPS)
   {
      return false;
   }

   const float3 candidateProxy = (r == 1.0) ? compressed : MELE_BridgeUnadapt(compressed, r);
   if (!MELE_IsFiniteNonNegative(candidateProxy))
   {
      return false;
   }
   q = scale;
   proxyNative = candidateProxy;
   return true;
}

// Undo the compression on the decoded linear grade output. Never invert the curve by re-reading the
// changed LUT output: the LUT moved the colour, so that read cannot recover the original scale.
bool MELE_TryRestoreGradeRange(float3 gradedLinear, float q, out float3 workHDR)
{
   workHDR = gradedLinear;
   if (!MELE_IsFiniteNonNegative(gradedLinear) || !MELE_IsFiniteNonNegative(q) || q <= 0.0)
   {
      return false;
   }
   const float3 restored = gradedLinear / q;
   if (!MELE_IsFiniteNonNegative(restored))
   {
      return false;
   }
   workHDR = restored;
   return true;
}

// The NATIVE colour contract for families 01-04: RGB ratios from the exact native grade result,
// luminance from the new working value. Y is the same linear BT.709 luminance on both sides, so on a
// finite positive reference the reference's channel ratios survive exactly - the per-channel shift
// and the whitening the native chain produced are kept, not undone. A coloured reference stays
// coloured, equal channels stay equal, and a channel the grade zeroed is not refilled.
//
// Of the working value only Y is used; its own hue and chroma are deliberately discarded.
//
// The guarantee ends here, before the shared output tail. The vignette, DICE, the user controls and
// the late SDR clamp all run after it.
//
// Guard contract, deliberately not one blanket fallback:
//   the WHOLE reference triple is validated, not only its luminance: a dot product returns a finite
//     number from non-finite channels, and a positive one from a reference with a negative channel.
//   exactly black reference or exactly zero target -> black. The grade produced that black.
//   near-black but positive reference -> divided normally. The LUT permutations carry clampFloor, so
//     a vanilla black arrives here at 1e-4 in luminance, two decades above the guard. Collapsing that
//     to black would erase a real part of the native output; count the guard's trips, do not widen it.
//   a non-finite gain or product -> the reference itself for the WHOLE triple. Switching channels
//     independently would change hue, which is the failure being avoided.
// A positive target luminance is never clamped to 1, and no path takes colour from the raw scene.
//
// Declining returns the reference unscaled, which is the exact native SDR result the caller already
// holds - there is no second HDR model to fall back to and no fallback for a caller to choose. A
// bool + out-parameter form would move that decision to the call site, and it was written and
// measured: fxc costs 3 to 4 extra instructions on every permutation that uses this helper, because
// it stops folding the fallback into the select it already emits. That is the eighteen of families
// 01-04, +4 on sixteen of them and +3 on the two analytic ones; 0x225A8330 takes its colour from the
// hue donor instead, never calls this, and did not move. Measured, not assumed - see the same trade
// in MELE_IsFiniteNonNegative above. Do not re-attempt it without re-measuring.
#define MELE_NATIVE_COLOR_MIN_LUMINANCE 1e-6

float3 MELE_NativeColorAtLuminance(float3 nativeReferenceLinear, float targetLuminance)
{
   if (!MELE_IsFiniteNonNegative(nativeReferenceLinear) || !MELE_IsFiniteNonNegative(targetLuminance))
   {
      return nativeReferenceLinear;
   }
   if (targetLuminance == 0.0 || all(nativeReferenceLinear == 0.0))
   {
      return float3(0.0, 0.0, 0.0); // The grade produced that black.
   }
   const float referenceLuminance = GetLuminance(nativeReferenceLinear, CS_BT709);
   if (!MELE_IsFiniteNonNegative(referenceLuminance) || referenceLuminance < MELE_NATIVE_COLOR_MIN_LUMINANCE)
   {
      return nativeReferenceLinear;
   }
   const float gain = targetLuminance / referenceLuminance;
   const float3 result = nativeReferenceLinear * gain;
   if (!MELE_IsFiniteNonNegative(gain) || !MELE_IsFiniteNonNegative(result))
   {
      return nativeReferenceLinear;
   }
   return result;
}
#endif // LUMA_MELE_TONEMAP_HDR_BRIDGE
