#ifndef LUMA_MELE_TONEMAP_HDR_BRIDGE
#define LUMA_MELE_TONEMAP_HDR_BRIDGE

#include "Tonemap_MELE_ExperimentConfig.hlsli"

// Pure math. Prerequisites, which this file deliberately does NOT include:
//   ../Includes/Color.hlsl    gamma_to_linear, GetLuminance, GCT_MIRROR
//   ../Includes/Math.hlsl     max3  (arrives through Color.hlsl)
//   ../Includes/Reinhard.hlsl ReinhardRange
// Reinhard.hlsl has no include guard, unlike every other shared header, so including it from here
// would be a duplicate-namespace error in the two LUT bodies that already include it. Include it in
// the body, before this file.

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
MELE_BridgeState MELE_BuildGradeProxy(float3 work_native, float r, out float3 proxy_native)
{
   MELE_BridgeState state;
   state.r = r;
   const float3 adapted = MELE_BridgeAdapt(work_native, r);
   const float m = max3(adapted);
   state.q = (m <= MELE_HDR_BRIDGE_SHOULDER) ? 1.0 : (Reinhard::ReinhardRange(m.xxx, MELE_HDR_BRIDGE_SHOULDER).x / m);
   proxy_native = MELE_BridgeUnadapt(adapted * state.q, r);
   return state;
}

// Undo the compression on the decoded linear grade output. Never invert the curve by re-reading the
// changed LUT output: the LUT moved the colour, so that read cannot recover the original scale.
float3 MELE_RestoreGradeRange(float3 graded_linear, MELE_BridgeState state)
{
   return graded_linear / state.q;
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
//   exactly black reference or exactly zero target -> black. The grade produced that black and this
//     is not the place to put light back into it.
//   near-black but positive reference -> divided normally. The LUT permutations carry clampFloor,
//     so a vanilla black floors at 1e-4 linear and, the encode and the decode being inverse powers,
//     arrives back at 1e-4 in luminance, two decades above the guard below. Collapsing that to black
//     would erase a real part of the native output. Count the guard's trips
//     rather than widening it.
//   non-finite or negative luminance on either side -> the caller's legacy value for the WHOLE
//     triple. Switching channels independently would change hue, which is the failure being avoided.
// A positive target luminance is never clamped to 1, and no path takes colour from the raw scene.
#define MELE_NATIVE_COLOR_MIN_LUMINANCE 1e-6

float3 MELE_NativeColorAtLuminance(float3 native_reference_linear, float target_luminance, float3 legacy_family_hdr)
{
   const float reference_luminance = GetLuminance(native_reference_linear, CS_BT709);
   if (IsNaN_Strict(reference_luminance) || IsInfinite_Strict(reference_luminance) || IsNaN_Strict(target_luminance) || IsInfinite_Strict(target_luminance) || reference_luminance < 0.0 || target_luminance < 0.0)
   {
      return legacy_family_hdr;
   }
   if (target_luminance == 0.0 || all(native_reference_linear == 0.0))
   {
      return float3(0.0, 0.0, 0.0);
   }
   if (reference_luminance < MELE_NATIVE_COLOR_MIN_LUMINANCE)
   {
      return legacy_family_hdr;
   }
   return native_reference_linear * (target_luminance / reference_luminance);
}
#endif // LUMA_MELE_TONEMAP_HDR_BRIDGE
