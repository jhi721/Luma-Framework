#ifndef LUMA_MELE_TONEMAP_FILMIC_EXTENDED
#define LUMA_MELE_TONEMAP_FILMIC_EXTENDED

#include "Tonemap_MELE_ExpExtended.hlsli" // MELE_ExpExtended; needs MELE_NativeToneCurve from Includes/Common.hlsl.
#include "Tonemap_MELE_ExperimentConfig.hlsli"
#include "Tonemap_MELE_HDRBridge.hlsli" // MELE_IsFiniteNonNegative; needs ../Includes/Reinhard.hlsl before it.

// Sampled continuation of the game's own 1D filmic LUT, for the two filmic families. Include AFTER the
// permutation declares smpFilmicLUT and its sampler, and after MELE_FILMIC_PRECURVE is defined. Both
// includes above carry guards, so a body that already pulled them in pays nothing; they are named here so
// this file states its own prerequisites instead of relying on the include order of one caller.
//
// The shipped MELE_FilmicMaxChannelExpand stays exactly as it is and remains the flag-0 path: it produces one
// max-channel scalar and leaves the native per-channel value untouched. This file is the experimental branch,
// which instead builds a per-pixel continuation of the composite curve and hands the result to the grade bridge.
// The duplication between the two is deliberate and must not be resolved by rewriting the shipped helper.
//
// Nothing here invents a curve. Every anchor is a real read of the bound LUT, so a game that ships a different
// table gets a different continuation; the constants below are probe positions, not curve coefficients.

// The LUT in its own input domain z: the strip is addressed as SC * z, and SC is the native input scale that
// covers scene-linear to about 16.2. This is the raw read, with no pre-curve of any kind applied.
float MELE_FilmicLookupZ(float z)
{
   const float SC = 0.0616082214;
   return smpFilmicLUT.SampleLevel(smpFilmicLUTSampler_s, float2(SC * z, 0.5), 0).x;
}

// One probe of the composite response T(x) = L(SC * precurve(x)), for the family whose fit lives in scene-x.
// The caller's MELE_FILMIC_PRECURVE decides the domain, which is identity for ME3LE and the exponential curve
// for ME2LE. SampleLevel with an explicit LOD matches the shipped helper's convention; the native per-pixel
// Sample calls that produce sdr_gamma are left alone.
float MELE_FilmicProbe(float scene_x)
{
   return MELE_FilmicLookupZ(MELE_FILMIC_PRECURVE(scene_x));
}

struct MELE_FilmicFit
{
   float pivot_value; // T(p), the last native value before the continuation takes over.
   float slope;       // dT/dx across the probe window, in SCENE-x, matching the shipped helper's /0.04.
   bool valid;        // False routes the caller's whole RGB triple back to its legacy value.
};

// A secant across the probe window, not an analytic derivative: the table is discrete, so this cannot be
// claimed to match the local slope of the neighbouring texels. The seam is therefore checked, not assumed.
//
// The validity test is LOCAL by construction. A table that is globally non-monotone but rising across the window
// still yields a usable fit and is accepted; only a window that is flat, falling or non-finite is rejected. That
// matters for R16_UNORM, where quantization can flatten a short run: the answer there is to report the rejection,
// never to substitute a slope of 1 or a slope borrowed from another game's curve. A positive local fit is not a
// statement about the whole table, and the two statuses must not be reported as one.
//
// This builder belongs to family 04 and its threshold is in scene-x. Family 03 has its own builder below in the
// LUT's z domain; the numbers are not interchangeable between the two.
MELE_FilmicFit MELE_BuildFilmicFit()
{
   MELE_FilmicFit fit;
   const float probe_lo = MELE_FilmicProbe(MELE_HDR_PROBE_LO);
   fit.pivot_value = MELE_FilmicProbe(MELE_HDR_PIVOT);
   const float probe_hi = MELE_FilmicProbe(MELE_HDR_PROBE_HI);
   fit.slope = (probe_hi - probe_lo) / (MELE_HDR_PROBE_HI - MELE_HDR_PROBE_LO);
   fit.valid = MELE_IsFiniteNonNegative(float3(probe_lo, fit.pivot_value, probe_hi)) && probe_lo <= fit.pivot_value && fit.pivot_value <= probe_hi && MELE_IsFiniteNonNegative(fit.slope) && fit.slope > 1e-5;
   return fit;
}

// ME3LE, family 04. The precurve is identity here, so the LUT is addressed with scene-linear scene+bloom
// directly and one scalar fit serves all three channels of the pixel; three separate fits would be the same
// numbers computed three times. Below the pivot the caller's already-sampled native value is reused rather than
// re-probed, so that region is the native result and not an approximation of it.
//
// The tonal model is unchanged. Only the validity contract moved: the inputs are now checked before the fit
// rather than alongside it, and the result is checked before it is handed out.
bool MELE_EvaluateME3FilmicExtended(float3 scene_with_bloom, float3 native_filmic_rgb, out float3 extended_filmic_rgb)
{
   extended_filmic_rgb = native_filmic_rgb;
   if (!MELE_IsFiniteNonNegative(scene_with_bloom) || !MELE_IsFiniteNonNegative(native_filmic_rgb))
   {
      return false;
   }
   const MELE_FilmicFit fit = MELE_BuildFilmicFit();
   if (!fit.valid)
   {
      return false;
   }
   const float3 continued = fit.pivot_value + fit.slope * (scene_with_bloom - MELE_HDR_PIVOT);
   const float3 result = float3(scene_with_bloom.x <= MELE_HDR_PIVOT ? native_filmic_rgb.x : continued.x,
                                scene_with_bloom.y <= MELE_HDR_PIVOT ? native_filmic_rgb.y : continued.y,
                                scene_with_bloom.z <= MELE_HDR_PIVOT ? native_filmic_rgb.z : continued.z);
   if (!MELE_IsFiniteNonNegative(result))
   {
      return false;
   }
   extended_filmic_rgb = result;
   return true;
}

// ME2LE, family 03. The game evaluates two tone stages in series and adds the bloom BETWEEN them:
//
//   native:  ell(F(C) + B)      where F(x) = 1 - exp2(-1.7x), ell(z) = L(SC * z)
//
// so the experimental branch continues the two stages separately and keeps the bloom where it belongs:
//
//   W_tone = E_L(E_F(C) + B)
//
// E_F is MELE_ExpExtended, the tangent continuation of F past scene mid-gray. E_L is the tangent continuation
// of the LUT past p_L = F(p_F), fitted in the LUT's own domain z.
//
// WHY THE NODES ARE FIXED. An earlier version fitted the composite T_i(x; B_i) = L(SC*(F(x)+B_i)) with B_i held
// fixed, so the anchors moved with the bloom. Because the shipped table flattens as its coordinate rises, a
// brighter bloom pushed the probe window into the flatter region and LOWERED the fitted slope, which made the
// working tone response FALL as the bloom rose. On the captured ME2LE table with C = (1,1,1) the native output
// rose 0.6193 -> 0.6814 as B went 0 -> 0.2 while that model fell 1.2667 -> 1.0294. Anchors that depend on the
// signal they are meant to extend are the defect; do not reintroduce them for any reason.
//
// Two consequences of fixing them. One pair of bounds and ONE set of three LUT probes now serves all three
// channels, instead of up to nine samples per pixel - a tinted bloom no longer means three different fits. And
// the probes still may not be cached across frames: the table's contents can be rewritten in place, so a cache
// keyed on the SRV pointer would be wrong.
//
// The slope denominator is the difference of z, not 0.04 and not a difference of UV. SC is already inside the
// addressing of the sampled values, so multiplying the finished slope by it again would count it twice.
struct MELE_ME2StagedFit
{
   float pivot_z;     // p_L = F(p_F), where the LUT continuation starts, in z.
   float pivot_value; // ell(p_L).
   float slope;       // s_L = d(ell)/dz across the probe window, per unit of z.
   bool valid;
};

MELE_ME2StagedFit MELE_BuildME2StagedFit()
{
   MELE_ME2StagedFit fit;
   // Nodes come from F alone. Nothing here reads C or B, which is the whole point.
   fit.pivot_z = MELE_NativeToneCurve(MELE_HDR_PIVOT);
   const float z_lo = MELE_NativeToneCurve(MELE_HDR_PROBE_LO);
   const float z_hi = MELE_NativeToneCurve(MELE_HDR_PROBE_HI);
   const float probe_lo = MELE_FilmicLookupZ(z_lo);
   fit.pivot_value = MELE_FilmicLookupZ(fit.pivot_z);
   const float probe_hi = MELE_FilmicLookupZ(z_hi);
   fit.slope = (probe_hi - probe_lo) / (z_hi - z_lo);
   fit.valid = MELE_IsFiniteNonNegative(float3(z_lo, fit.pivot_z, z_hi)) && z_hi > z_lo && MELE_IsFiniteNonNegative(float3(probe_lo, fit.pivot_value, probe_hi)) && probe_lo <= fit.pivot_value && fit.pivot_value <= probe_hi && MELE_IsFiniteNonNegative(fit.slope) && fit.slope > MELE_FILMIC_MIN_SLOPE_Z;
   return fit;
}

// Per channel: continue when the SECOND stage has passed its pivot, which a bright bloom alone can cause even
// where the scene never left the native region. The previous contract - C <= p_F therefore always native - was
// wrong for exactly that case and is gone. The native sample is reused only where BOTH stages stayed native,
// where it is the native result rather than an approximation of it; the third arm is unreachable for a
// non-negative bloom, since E_F(C) > p_L already whenever C > p_F, and exists so the branch is total.
bool MELE_EvaluateME2FilmicExtended(float3 scene_before_precurve, float3 native_bloom_contribution, float3 native_filmic_rgb, out float3 extended_filmic_rgb)
{
   extended_filmic_rgb = native_filmic_rgb;
   if (!MELE_IsFiniteNonNegative(scene_before_precurve) || !MELE_IsFiniteNonNegative(native_bloom_contribution) || !MELE_IsFiniteNonNegative(native_filmic_rgb))
   {
      return false;
   }
   const MELE_ME2StagedFit fit = MELE_BuildME2StagedFit();
   if (!fit.valid)
   {
      return false;
   }
   const float3 z = MELE_ExpExtended(scene_before_precurve, MELE_HDR_PIVOT) + native_bloom_contribution;
   if (!MELE_IsFiniteNonNegative(z))
   {
      return false;
   }
   float3 tone;
   [unroll] for (uint i = 0; i < 3; ++i)
   {
      tone[i] = (z[i] > fit.pivot_z) ? (fit.pivot_value + fit.slope * (z[i] - fit.pivot_z)) : ((scene_before_precurve[i] <= MELE_HDR_PIVOT) ? native_filmic_rgb[i] : MELE_FilmicLookupZ(z[i]));
   }
   if (!MELE_IsFiniteNonNegative(tone))
   {
      return false;
   }
   extended_filmic_rgb = tone;
   return true;
}

#endif // LUMA_MELE_TONEMAP_FILMIC_EXTENDED
