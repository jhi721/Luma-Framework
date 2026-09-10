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
// The prior worth knowing before enabling anything: the same wrap was tried and rejected in a sibling game
// running this exact scheme. See Shaders/Borderlands 2 and The Pre-Sequel/Luma_BL2TPS_Tonemap.hlsl:19-24,
// where it cost up to 26 degrees of hue on a curve that never clipped. ME1LE and ME2LE share that
// asymptotic-curve property, so families 01 and 02 are the ones that prior most directly predicts.
// Measured against the live tables (see _tools/mele_bridge/MEASUREMENTS.md) the branch is markedly more
// conservative on strongly coloured highlights in all three games, by 0.28x to 0.46x.
//
// PROMOTION RULE. Flipping a selector on for a release is a separate change from adding it, and it needs
// runtime A/B frames the offline bench cannot produce. Nothing here ships enabled.

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

// Bench values, not calibration. The bench that pins them is _tools/mele_bridge/bench.py, which
// exercises them against the LUTs and cbuffers captured from all three games as well as against
// synthetic C/B. They are NOT calibrated against finished game frames, so none may be presented as
// tuned; what has been checked is that the model behaves as specified on real tables.
#define MELE_HDR_BRIDGE_SHOULDER 0.75 // k, the max-channel proxy shoulder, in the adapted linear domain.
#define MELE_HDR_PIVOT           0.18 // p, scene mid-gray, where the tone-curve continuation starts.
#define MELE_HDR_PROBE_LO        0.16 // Sampled-fit probes for the filmic families, in scene-x.
#define MELE_HDR_PROBE_HI        0.20

// Roundoff tolerance on the bridge's bounded-proxy assertion. The rational shoulder is asymptotic to
// 1, so a compressed max channel can only exceed 1 by arithmetic error; this covers that error and
// nothing else. It is not a clamp, and widening it to make a failing case pass would hide exactly
// the condition the check exists to report.
#define MELE_BRIDGE_PROXY_EPS 1e-4

// Minimum accepted slope of the ME2LE two-stage continuation, in the LUT's own input domain z. It is
// deliberately NOT the 1e-5 that family 04 uses: that threshold lives in scene-x, and the two domains
// are related by F' ~= 0.95 at the pivot and by the probe window widths, so carrying the number
// across would be meaningless. Strictly positive is the requirement. For scale, one R16_UNORM step
// (1/65535) across the z window F(0.20) - F(0.16) ~= 0.0381 is a slope of 4.0e-4, so any accepted
// slope below that is quantization-limited; the bench reports that separately instead of rejecting it.
#define MELE_FILMIC_MIN_SLOPE_Z 0.0

// There is one colour path per family and it is not selectable. Families 01-04 take their RGB ratios from
// the real graded SDR and only their luminance from the new branch; family 05 takes the soft-clip reference.
// The diagnostic mode that returned the working HDR unchanged is gone: for family 05 the same result is
// reached by leaving both transfer sliders at zero, and for 01-04 its colour was never a shippable answer.
//
// The guarantee ends at graded_hdr, before the shared output tail. The vignette, DICE, the user
// saturation/contrast controls and the late SDR clamp all run after it and are judged separately.

// Family 05 transfer strengths live in the game cbuffer, not here: they are continuous, they change per
// frame without recompiling, and a shader define can only carry a single digit. Both at zero returns the
// working HDR without entering Oklab, which is NOT the legacy value the feature selector at 0 gives.

// Start of the soft per-channel reference. It is the ceiling of an SDR-like artistic reference, not a
// display peak, and nothing else in the frame is limited to it.
#define MELE_HARDCLIP_REFERENCE_START 0.75

// Documented swizzle adapters for the ME1LE/ME2LE LUT body, whose grade chain is transcribed
// BRG-in / RGB-out. Round trip is exact and is pinned by the bench; a grey stimulus cannot catch a
// rotation error, so the colour-impulse case is the one that matters.
#define MELE_RGB_TO_BRG(v) ((v).zxy)
#define MELE_BRG_TO_RGB(v) ((v).yzx)

#endif // LUMA_MELE_TONEMAP_EXPERIMENT_CONFIG
