#ifndef LUMA_MELE_TONEMAP_FILMIC_EXTENDED
#define LUMA_MELE_TONEMAP_FILMIC_EXTENDED

#include "Tonemap_MELE_ExperimentConfig.hlsli"

// Sampled continuation of the game's own 1D filmic LUT, for the two filmic families. Include AFTER the
// permutation declares smpFilmicLUT and its sampler, and after MELE_FILMIC_PRECURVE is defined.
//
// The shipped MELE_FilmicMaxChannelExpand stays exactly as it is and remains the flag-0 path: it produces one
// max-channel scalar and leaves the native per-channel value untouched. This file is the experimental branch,
// which instead builds a per-pixel continuation of the composite curve and hands the result to the grade bridge.
// The duplication between the two is deliberate and must not be resolved by rewriting the shipped helper.
//
// Nothing here invents a curve. Every anchor is a real read of the bound LUT, so a game that ships a different
// table gets a different continuation; the constants below are probe positions, not curve coefficients.

struct MELE_FilmicFit
{
   float pivot_value; // T(p), the last native value before the continuation takes over.
   float slope;       // dT/dx across the probe window, in SCENE-x, matching the shipped helper's /0.04.
   bool valid;        // False routes the caller's whole RGB triple back to its legacy value.
};

// One probe of the composite response T(x) = L(SC * precurve(x)). SC is the native input scale; the caller's
// MELE_FILMIC_PRECURVE decides the domain, which is identity for ME3LE and the exponential curve for ME2LE.
// SampleLevel with an explicit LOD matches the shipped helper's convention; the native per-pixel Sample calls
// that produce sdr_gamma are left alone.
float MELE_FilmicProbe(float scene_x)
{
   const float SC = 0.0616082214;
   return smpFilmicLUT.SampleLevel(smpFilmicLUTSampler_s, float2(SC * MELE_FILMIC_PRECURVE(scene_x), 0.5), 0).x;
}

// A secant across the probe window, not an analytic derivative: the table is discrete, so this cannot be
// claimed to match the local slope of the neighbouring texels. The seam is therefore checked, not assumed.
//
// The validity test is LOCAL by construction. A table that is globally non-monotone but rising across the window
// still yields a usable fit and is accepted; only a window that is flat, falling or non-finite is rejected. That
// matters for R16_UNORM, where quantization can flatten a short run: the answer there is to report the rejection,
// never to substitute a slope of 1 or a slope borrowed from another game's curve.
MELE_FilmicFit MELE_BuildFilmicFit()
{
   MELE_FilmicFit fit;
   const float probe_lo = MELE_FilmicProbe(MELE_HDR_PROBE_LO);
   fit.pivot_value = MELE_FilmicProbe(MELE_HDR_PIVOT);
   const float probe_hi = MELE_FilmicProbe(MELE_HDR_PROBE_HI);
   fit.slope = (probe_hi - probe_lo) / (MELE_HDR_PROBE_HI - MELE_HDR_PROBE_LO);
   fit.valid = !IsAnyNaN_Strict(float3(probe_lo, fit.pivot_value, probe_hi)) && !any(IsInfinite_Strict(float3(probe_lo, fit.pivot_value, probe_hi))) && probe_lo <= fit.pivot_value && fit.pivot_value <= probe_hi && fit.slope > 1e-5;
   return fit;
}

// ME3LE, family 04. The precurve is identity here, so the LUT is addressed with scene-linear scene+bloom
// directly and one scalar fit serves all three channels of the pixel; three separate fits would be the same
// numbers computed three times. Below the pivot the caller's already-sampled native value is reused rather than
// re-probed, so that region is the native result and not an approximation of it.
bool MELE_EvaluateME3FilmicExtended(float3 scene_with_bloom, float3 native_filmic_rgb, out float3 extended_filmic_rgb)
{
   const MELE_FilmicFit fit = MELE_BuildFilmicFit();
   const float3 continued = fit.pivot_value + fit.slope * (scene_with_bloom - MELE_HDR_PIVOT);
   extended_filmic_rgb = float3(scene_with_bloom.x <= MELE_HDR_PIVOT ? native_filmic_rgb.x : continued.x,
                                scene_with_bloom.y <= MELE_HDR_PIVOT ? native_filmic_rgb.y : continued.y,
                                scene_with_bloom.z <= MELE_HDR_PIVOT ? native_filmic_rgb.z : continued.z);
   return fit.valid && !any(scene_with_bloom < 0.0);
}

// ME2LE, family 03. The LUT is addressed through the exponential pre-curve AND the bloom is added inside that
// coordinate, so the response the game actually evaluates is the composite
//
//   T_i(x; B_i) = L(SC * (F(x) + B_i))
//
// with B_i held fixed. That is the whole difference from family 04, and from the shipped scalar helper: the
// latter is handed untonemapped = C + B and pre-curves the sum, modelling L(F(C+B)). Those two forms diverge
// as soon as the bloom is non-zero, which is the defect this family exists to address - rebuilding L(F(C+B))
// here would reproduce it.
//
// Because B differs per channel whenever the bloom is tinted, the three composite curves are genuinely
// different and each needs its own anchors: up to nine extra samples per pixel on top of the native three.
// That cost is real and is measured before it is optimized; a cache keyed on the SRV pointer alone would be
// wrong, because the table's contents can be rewritten in place.
//
// The slope denominator is the difference of SCENE-x, not of LUT coordinates. The pre-curve difference is
// already inside the sampled values, so dividing by a coordinate delta would count it twice.
float MELE_FilmicProbeME2(float scene_x, float bloom)
{
   const float SC = 0.0616082214;
   return smpFilmicLUT.SampleLevel(smpFilmicLUTSampler_s, float2(SC * (MELE_FILMIC_PRECURVE(scene_x) + bloom), 0.5), 0).x;
}

bool MELE_EvaluateME2FilmicExtended(float3 scene_before_precurve, float3 native_bloom_contribution, float3 native_filmic_rgb, out float3 extended_filmic_rgb)
{
   bool valid = true;
   extended_filmic_rgb = native_filmic_rgb;
   [unroll] for (uint i = 0; i < 3; ++i)
   {
      const float bloom = native_bloom_contribution[i];
      const float probe_lo = MELE_FilmicProbeME2(MELE_HDR_PROBE_LO, bloom);
      const float probe_p = MELE_FilmicProbeME2(MELE_HDR_PIVOT, bloom);
      const float probe_hi = MELE_FilmicProbeME2(MELE_HDR_PROBE_HI, bloom);
      const float slope = (probe_hi - probe_lo) / (MELE_HDR_PROBE_HI - MELE_HDR_PROBE_LO);
      valid = valid && !IsAnyNaN_Strict(float3(probe_lo, probe_p, probe_hi)) && !any(IsInfinite_Strict(float3(probe_lo, probe_p, probe_hi))) && probe_lo <= probe_p && probe_p <= probe_hi && slope > 1e-5;
      // Below the pivot the caller's own native sample is kept, so that region is the native result itself.
      if (scene_before_precurve[i] > MELE_HDR_PIVOT)
      {
         extended_filmic_rgb[i] = probe_p + slope * (scene_before_precurve[i] - MELE_HDR_PIVOT);
      }
   }
   return valid && !any(scene_before_precurve < 0.0) && !any(native_bloom_contribution < 0.0);
}

#endif // LUMA_MELE_TONEMAP_FILMIC_EXTENDED
