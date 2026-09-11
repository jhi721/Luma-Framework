#ifndef LUMA_MELE_TONEMAP_HDR_CONFIG
#define LUMA_MELE_TONEMAP_HDR_CONFIG

// Constants of the stage-1 HDR reconstruction, one model per colour family:
//
//   01  ME1LE/ME2LE exponential curve + colour LUT          8 permutations
//   02  ME1LE/ME2LE exponential curve + analytic grade      2 permutations
//   03  ME2LE pre-curve + 1D filmic LUT + colour LUT        4 permutations
//   04  ME3LE 1D filmic LUT from linear scene+bloom + LUT   4 permutations
//   05  ME3LE analytic hard-clip grade                      0x225A8330 only
//
// There is no selector here and no second reconstruction to select. An in-game A/B across the three
// games on 2026-09-11 chose these five, they shipped enabled, and the max-channel path they replaced
// was deleted afterwards. Git history is how that path is read now.
//
// WHAT THESE MODELS DO. They prepare the grade INPUT - a compressed proxy, or a tone curve continued
// past mid-gray - and divide the scale back out afterwards, so the grade sees range the vanilla clip
// had already flattened. Where a model declines, the caller keeps the exact native SDR result; it
// never falls back to a different reconstruction, because there is no longer one to fall back to.
//
// Porting a model to ANOTHER game is a separate decision with its own evidence. The prior in
// Shaders/Borderlands 2 and The Pre-Sequel/Luma_BL2TPS_Tonemap.hlsl stands: the same wrap was tried and
// rejected there, on the same kind of asymptotic curve ME1LE and ME2LE have. MELE's frames do not
// transfer to BL2's.

// Bench values: exercised against the captured LUTs and cbuffers, and carried unchanged through the
// 2026-09-11 A/B that chose these families. That A/B judged the families as a whole, so it validates
// this set as a working combination and not any one number individually - none of them may be presented
// as individually tuned, and changing one still needs its own frames.
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

// Strength of family 05's hue-only transfer toward the native hard-clipped SDR. A calibration value, not
// a property of the algorithm. The family was promoted by the 2026-09-11 A/B; this number was set back
// to 0.75 afterwards, so it is the one part of family 05 that A/B did not run at its shipped value.
//
// Why not 1.0: at exactly 1.0 the blend IS the donor's ab, so the target keeps none of its own. A hard
// clip drives the brightest colours to white, and a white donor's OKLab ab is not zero but matrix
// round-off, about 3.7e-8; the renormalization then scales that round-off back up to the target's full
// chroma, and the hue of a fully blown highlight becomes arbitrary rather than preserved - measured at
// +41.3 deg to +89.9 deg on one stimulus. At 0.75 the target keeps 0.25 of its own ab, which is far
// above that round-off, so the direction survives and the degenerate case cannot be reached from a
// neutral donor. No donor-chroma threshold is added to paper over it: that would be a new artistic
// rule, and lowering this number is the honest control.
#ifndef MELE_HARDCLIP_HUE_STRENGTH
#define MELE_HARDCLIP_HUE_STRENGTH 0.75
#endif

// One colour path per family, not selectable. Families 01-04 take their RGB ratios from the real graded SDR
// and only their luminance from the new branch; family 05 keeps its working HDR and moves only its hue.
//
// The guarantee ends at gradedHDR, before the shared output tail. The vignette, DICE, the user
// saturation/contrast controls and the late SDR clamp all run after it and are judged separately.

// Swizzle adapters for the ME1LE/ME2LE LUT body, whose grade chain is transcribed BRG-in / RGB-out.
#define MELE_RGB_TO_BRG(v) ((v).zxy)
#define MELE_BRG_TO_RGB(v) ((v).yzx)

#endif // LUMA_MELE_TONEMAP_HDR_CONFIG
