#ifndef LUMA_MELE_TONEMAP_EXPERIMENT_CONFIG
#define LUMA_MELE_TONEMAP_EXPERIMENT_CONFIG

// Compile-time selectors for the experimental stage-1 HDR reconstruction, one per colour family.
// Every one defaults to 0, which is the shipped reconstruction: with all five at 0 the preprocessor
// removes every line this experiment adds, so all 42 fxc listings stay byte-identical to the
// pre-change capture. That identity is the gate, not an expectation; see _tools/mele_bridge.
//
// HOW THIS DIFFERS FROM THE SHIPPED PATH. The shipped reconstruction lets the native per-channel value
// reach the grade untouched and only expands the graded output by a max-channel scalar. An enabled family
// here does the opposite: it feeds the grade a compressed proxy and divides the scale back out. That is a
// real change of contract, not a refactor, and it is why every selector defaults to off.
//
// Before enabling anything, read the prior in Shaders/Borderlands 2 and The Pre-Sequel/Luma_BL2TPS_Tonemap.hlsl:
// the same wrap was tried and rejected there, on the same kind of asymptotic curve that ME1LE and ME2LE have.
// Flipping a selector on for a release is a separate change from adding it and needs runtime A/B frames.

#ifndef MELE_HDR_EXP_LUT
#define MELE_HDR_EXP_LUT 0 // ME1LE/ME2LE exponential curve + colour LUT, 8 permutations.
#endif
#ifndef MELE_HDR_EXP_ANALYTIC
#define MELE_HDR_EXP_ANALYTIC 0 // ME1LE/ME2LE exponential curve + analytic grade, 2 permutations.
#endif
#ifndef MELE_HDR_ME2_FILMIC
#define MELE_HDR_ME2_FILMIC 0 // ME2LE pre-curve + 1D filmic LUT + colour LUT, 4 permutations.
#endif
#ifndef MELE_HDR_ME3_FILMIC
#define MELE_HDR_ME3_FILMIC 0 // ME3LE 1D filmic LUT from linear scene+bloom + colour LUT, 4 permutations.
#endif
#ifndef MELE_HDR_ME3_HARDCLIP
#define MELE_HDR_ME3_HARDCLIP 0 // ME3LE analytic hard-clip grade, 0x225A8330 only.
#endif

// Bench values, not calibration: exercised against the captured LUTs and cbuffers, never against finished
// game frames, so none of them may be presented as tuned.
#define MELE_HDR_BRIDGE_SHOULDER 0.75 // k, the max-channel proxy shoulder, in the adapted linear domain.
#define MELE_HDR_PIVOT           0.18 // p, scene mid-gray, where the tone-curve continuation starts.
#define MELE_HDR_PROBE_LO        0.16 // Sampled-fit probes for the filmic families, in scene-x.
#define MELE_HDR_PROBE_HI        0.20

// Roundoff tolerance on the bridge's bounded-proxy assertion. The shoulder is asymptotic to 1, so a
// compressed max channel can only exceed it by arithmetic error. It is not a clamp: widening it to make a
// failing case pass would hide the condition the check exists to report.
#define MELE_BRIDGE_PROXY_EPS 1e-4

// Minimum accepted slope for each filmic family's fit. The two are in DIFFERENT domains and the
// numbers are not interchangeable: family 04 fits in scene-x, family 03 in the LUT's own input z, and
// the two are related by F' ~= 0.95 at the pivot and by the probe window widths.
//
// For family 03 strictly positive is the requirement. For scale, one R16_UNORM step (1/65535) across
// the z window F(0.20) - F(0.16) ~= 0.0381 is a slope of 4.0e-4, so any accepted slope below that is
// quantization-limited; the bench reports that separately instead of rejecting it.
#define MELE_FILMIC_MIN_SLOPE_Z 0.0
#define MELE_FILMIC_MIN_SLOPE_X 1e-5

// One colour path per family, not selectable. Families 01-04 take their RGB ratios from the real graded SDR
// and only their luminance from the new branch; family 05 hands over its working HDR as it is.
//
// The guarantee ends at graded_hdr, before the shared output tail. The vignette, DICE, the user
// saturation/contrast controls and the late SDR clamp all run after it and are judged separately.

// Swizzle adapters for the ME1LE/ME2LE LUT body, whose grade chain is transcribed BRG-in / RGB-out.
#define MELE_RGB_TO_BRG(v) ((v).zxy)
#define MELE_BRG_TO_RGB(v) ((v).yzx)

#endif // LUMA_MELE_TONEMAP_EXPERIMENT_CONFIG
