// Medal of Honor (2010) — Luma HDR bloom pyramid (core DrawBloom, four auto-registered passes).
//
// Input = the tonemap pass's own scene texture (PS t0): fp16, LINEAR and pre-glow, because this engine builds its
// halo further downstream, out of the 8-bit canvas the tonemap writes. That is what makes the port viable and is
// exactly the property TW2 lacks (its only reachable source is gamma-space and already contains the halo).
// Being linear and scene referred, this source needs no gamma decode.
//
// Units warning: 1.0 here is NOT paper white. The vanilla chain reaches SDR white at `untonemapped * 0.588235 ==
// 1`, i.e. at 1.7 / SceneExposure.z in these units, and SceneExposure.z (cb4[8].z) is a per-area engine constant
// an injected pass cannot see. So the threshold is a user slider, calibrated in game.

// clang-format off
#include "Includes/DgVoodoo.hlsl" // game-local: b3/b4, so the prefilter can read the game's own SceneExposure
#include "Includes/Common.hlsl"   // game-local: pulls GameCBuffers (LumaSettings) before the shared includes
// clang-format on

#define SceneExposure PsConstants[8] // .z only, and only at the TONEMAP pass -- see the warning below

// The threshold function reads these as macros, so runtime cbuffer values work (BioShock Infinite precedent:
// it reads the game's native _Globals straight from its prefilter). DrawBloom backs up, binds and restores only
// b11, and its internal draws bypass the core per-draw injection path, so everything else the caller left bound
// reaches this pass. main.cpp re-binds b13 AND the game's b4 immediately before the call rather than relying on
// inheritance.
//
// The threshold is a FRACTION OF SDR WHITE, not an absolute scene value. Section 15 of NOTES.md recorded that
// the anchor "an injected pass cannot read"; it can -- the pyramid is built from the tonemap draw's own
// post-draw hook, where the game's cb4 is still bound, so the exposure is available live and per-area with no
// read-back and no latency.
//
// WARNING: cb4[8] means three different things in this game. It is SceneExposure only at the tonemap pass; at
// the vanilla bright pass it is the bloom threshold and at the composite it is the bloom amount. This pass is
// dispatched from the tonemap draw, which is the only reason the name below is correct.
float moh_sdr_white_scene()
{
   // The tonemap clips at saturate(color * SceneExposure.z * Exposure * 0.588235), and 0.588235 = 1/1.7, so the
   // scene value that becomes SDR white is 1.7 / (SceneExposure.z * Exposure). The Luma bloom is added to `color`
   // BEFORE that multiply, which is why the threshold belongs in raw scene units.
   const float e = SceneExposure.z * max(LumaSettings.GameSettings.Exposure, 1e-4);
   // Implausible means the bind missed: fall back to a fixed scene threshold rather than to 0 (whole frame
   // glows) or to infinity (no glow at all).
   return (e > 1e-4 && e < 1e4) ? (1.7 * rcp(e)) : 1.7;
}

#define LUMA_BLOOM_THRESHOLD (max(0.0, LumaSettings.GameSettings.BloomThreshold) * moh_sdr_white_scene())
#define LUMA_BLOOM_SOFT_KNEE (LUMA_BLOOM_THRESHOLD * 0.5)

// Vanilla's SHAPE, not the shared quadratic one: the game's bright pass keeps the WHOLE sample once it passes the
// threshold, while quadratic_threshold keeps only the EXCESS, which Airborne measured 6x too dim at brightness
// 1.2 and 2x at 2.0 — the band where nearly every glowing pixel lives.
// The knee is applied to the WEIGHT, not the colour: the game's own test is a hard subtract against a constant,
// affordable for a dithered quarter-res gather and not affordable here, with no TAA to hide the flicker on a
// surface drifting across the threshold. The ramp is ONE-SIDED, starting AT the threshold, so nothing below it
// contributes and no glow is invented across the band underneath.
float3 moh_bloom_threshold(float3 color)
{
   const float br = max(color.r, max(color.g, color.b));
   const float k = max(1e-6, LUMA_BLOOM_SOFT_KNEE);
   const float t = LUMA_BLOOM_THRESHOLD;
   return color * saturate((br - t) * rcp(k));
}
#define LUMA_BLOOM_THRESHOLD_FUNCTION(color) moh_bloom_threshold(color)

#include "../Includes/Bloom.hlsl"
