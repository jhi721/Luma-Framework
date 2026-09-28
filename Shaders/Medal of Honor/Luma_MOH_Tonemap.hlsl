// Medal of Honor (2010) — Luma HDR tonemap replacement, shared by all FOUR permutations of the game's single
// tonemap pass. The engine compiles one shader per (EdgeAA x motion-blur/DoF) combination, and the grade tail
// moves register with the combination, so the wrappers select both the feature blocks and the register map:
//
//   PS hash      EdgeAA  MB/DoF  grade reg   file
//   0x3EDE377E     -       -      cb4[12]    Tonemap_0x3EDE377E.ps_5_0.hlsl
//   0x8B4ED0E3     x       -      cb4[13]    Tonemap_0x8B4ED0E3.ps_5_0.hlsl
//   0x77653D15     -       x      cb4[14]    Tonemap_0x77653D15.ps_5_0.hlsl
//   0x02B86437     x       x      cb4[14]    Tonemap_0x02B86437.ps_5_0.hlsl
//
// The pass reads the fp16 SceneColor (t0) and writes the 8-bit canvas the bloom pyramid, DoF and the HUD then use,
// so it is the ONLY place the real HDR signal is still available.
//
// Strategy (analytic in-shader TM around an untouched vanilla chain):
//   1. Reproduce the scene mix exactly as the game does (EdgeAA and/or the MB/DoF composite).  -> untonemapped
//   2. Run the vanilla chain UNCHANGED on it: saturate -> filmic -> pow(x, 0.4) -> 3D LUT.     -> graded SDR (the look)
//   3. Uncompress by the MAX CHANNEL: one scalar from max3(untonemapped), multiplied into the graded SDR. The
//      vanilla chain is the compressor, so the clip's own hue skew and white blowout survive by construction.
//   4. DICE display rolloff to the user's peak/paper-white nits.
//
// A DEVELOPMENT-only A/B against the additive recovery this replaced lives behind DevSetting04; it and everything
// it reaches (NeutralSDR, UpgradeToneMap, ENABLE_HUE_RESTORATION) are temporary and marked for deletion.
//
// Why the vanilla chain is run untouched rather than "unclamped": the filmic curve is a rational fit that is only
// valid on [0,1]. It peaks at c ~ 0.9, turns over, goes NEGATIVE past c ~ 1.35 and its denominator crosses zero at
// c ~ 1.62. Feeding it (or the [0,1] LUT after it) an HDR value produces garbage, so the vanilla saturate stays.
//
// Output is GAMMA space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white): the HUD blends src-alpha onto this same
// canvas right after, and a linear buffer washes it out. UI_DRAW_TYPE 2 pre-scales by GamePaperWhite/UIPaperWhite.
//
// Every texture fetch in a dgVoodoo-translated shader is followed by an `and`/`or` pair against b3 — the wrapper's
// D3D9 format emulation. Reproduced bit-exactly below; dropping it shifts colour. Same for the 13-interpolator
// input signature in the wrappers: VS->PS linkage is by REGISTER, so omitting an unused one shifts the rest.

// clang-format off
// ORDER IS LOAD-BEARING — do not sort. The game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS before any shared header pulls Settings.hlsl, so LumaSettings.GameSettings resolves to the
// real grade struct rather than the empty dummy.
#include "Includes/DgVoodoo.hlsl"           // game-local: b3/b4 + ApplyDgvMask, the wrapper contract
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders)
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // RestoreHueAndChrominance, SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // ReinhardPiecewise (the max-channel wrap), ReinhardTonemap (legacy A/B)
#if DEVELOPMENT
#include "../Includes/Tonemap.hlsl" // UpgradeToneMap - legacy A/B path only, see FinishMOH
#endif
// clang-format on

// HDR / vanilla. 1 = recover real highlights + DICE display map (default). 0 = vanilla clamped SDR reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Run the display map in a BT.2020 working space (round-tripped back to BT.709). Gamut-correct handling of
// highly saturated highlights — NOT a display-gamut expansion.
#ifndef TONEMAP_IN_WIDER_GAMUT
#define TONEMAP_IN_WIDER_GAMUT 1
#endif

#if DEVELOPMENT
// LEGACY A/B ONLY (DevSetting04). The shipped max-channel path needs no hue restoration: a scalar cannot change
// the ratio between channels, so the vanilla clip's skew survives on its own. Strength and chroma come from
// LumaSettings.GameSettings.HighlightsHue*, which are therefore DEV sliders that no shipped path reads.
#ifndef ENABLE_HUE_RESTORATION
#define ENABLE_HUE_RESTORATION 1
#endif
#endif

// Permutation switches, set by the per-hash wrapper files.
#ifndef MOH_EDGE_AA
#define MOH_EDGE_AA 0
#endif
#ifndef MOH_MOTION_BLUR
#define MOH_MOTION_BLUR 0
#endif
#ifndef MOH_GRADE_REG
#define MOH_GRADE_REG 12
#endif

// --- Game bindings (must match the original shader exactly) ---
// b3/b4 themselves come from Includes/DgVoodoo.hlsl; only the per-permutation rows are named here.
// Grade tail, at the register index this permutation reads it from: .x output scale (the engine FADE),
// .z/.w a scale/offset applied to the LUT result.
#define GradeParams PsConstants[MOH_GRADE_REG]
// Scene exposure, .z only. Same register in every permutation.
#define SceneExposure PsConstants[8]
#if MOH_EDGE_AA || MOH_MOTION_BLUR
// .xy pixel size for the EdgeAA taps; the MB block reuses the same row as its offset scale (.xy) and bias (.zw).
#define PixelOffsets PsConstants[12]
#endif
#if MOH_MOTION_BLUR
#define DepthParams PsConstants[10] // .z/.w linearise the depth the scene alpha carries
#define NoiseParams PsConstants[13] // .xy screen-space frequency of the per-pixel offset jitter
#endif

SamplerState SceneColorSampler_s : register(s0);
SamplerState ColorGradingLUTSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene colour; .w carries SCENE DEPTH, not alpha
Texture3D<float4> ColorGradingLUT : register(t33);  // 32x32x32 r8g8b8a8, indexed by the gamma-encoded colour

// --- Luma HDR bloom pyramid mip 0 (Luma_Bloom_impl.hlsl, built by core's DrawBloom) ---
// Half-res, LINEAR fp16, injected by main.cpp right before this draw; the slot must match kLumaBloomSlot there.
// t6 is free: the four perms declare only t0, t2, t3 and t33, and dgVoodoo's 1x1 placeholder in that slot is
// overwritten by our own bind. It REPLACES the game's bloom rather than adding to it — with LumaBloomEnable on,
// the replaced bright pass writes 0 and the replaced composite passes its base through, so there is no double
// glow to reconcile.
Texture2D<float4> LumaBloomTexture : register(t6);
#if MOH_MOTION_BLUR
SamplerState BlurredImageSampler_s : register(s2);
SamplerState DepthSampler_s : register(s3);
Texture2D<float4> BlurredImage : register(t2); // blurred colour, coverage in .w
Texture2D<float4> DepthTexture : register(t3); // downsampled max-depth (already linear), .x
#endif

// Added in scene-referred LINEAR light, before exposure and the whole vanilla chain, so the glow is tonemapped
// WITH the scene instead of being pasted on after the display map. That is the entire point of the port: the
// engine's own glow is built from the gamma-encoded, already-display-mapped canvas, where a five-level sum turns
// into a ~50x multiplication in linear and no amount of reshaping the blend can fix it.
// Half-res source, so one bilinear tap.
float3 ApplyLumaBloom(float3 color, float2 sceneUV)
{
#if TONEMAP_TYPE >= 1
   if (LumaSettings.GameSettings.LumaBloomEnable > 0.5)
   {
      const float3 bloom = LumaBloomTexture.SampleLevel(SceneColorSampler_s, sceneUV, 0.0).rgb;
      color += max(0.0, bloom) * LumaSettings.GameSettings.BloomIntensity;
   }
#endif
   return color;
}

// The original's guarded reciprocal: dgVoodoo emits a zero test that yields a huge sentinel instead of inf.
float SafeRcp(float value)
{
   return abs(value) > 0.0 ? rcp(value) : FLT_MAX;
}

// pow() as the original computes it: max(|x|, 1e-4) then log2/mul/exp2. The floor replaces the compiler's log2(0)
// guard and is what keeps the LUT coordinate finite at black.
float3 PowMOH(float3 base, float exponent)
{
   return exp2(exponent * log2(max(abs(base), 1e-4)));
}

float4 SampleScene(float2 uv)
{
   return ApplyDgvMask(SceneColorTexture.Sample(SceneColorSampler_s, uv), DgvMaskScene, DgvFillScene);
}

#if DEVELOPMENT
// Neutral SDR reference for the legacy additive recovery (DevSetting04 A/B only).
float3 NeutralSDR(float3 color)
{
   ReinhardSettings settings = DefaultReinhardSettings();
   settings.by_luminance = true;
   return ReinhardTonemap(color, 100.f, 100.f, settings);
}
#endif // DEVELOPMENT

#if MOH_EDGE_AA
// The engine's own "EdgeAA": a luma-gradient directional blur, transcribed operand for operand. Horizontal and
// vertical high-pass of the 3-tap mean give a gradient vector; two taps along it are averaged with the centre, and
// only where the gradient is strong enough (the `ge` test below) does the blurred value win.
float3 RunEdgeAA(float2 uv, float3 center)
{
   const float2 pixel = PixelOffsets.xy;

   float3 left = SampleScene(uv + pixel * float2(-1.0, 0.0)).rgb;
   float3 right = SampleScene(uv + pixel * float2(1.0, 0.0)).rgb;
   float3 hHighPass = center - (left + right + center) * 0.333333;
   float horizontal = dot(hHighPass, 0.333);

   float3 down = SampleScene(uv + pixel * float2(0.0, -1.0)).rgb;
   float3 up = SampleScene(uv + pixel * float2(0.0, 1.0)).rgb;
   float3 vHighPass = center - (up + down + center) * 0.333333;
   float vertical = dot(vHighPass, 0.333);

   // Gradient vector (vertical, -horizontal): the blur runs ALONG the edge, not across it.
   float2 gradient = float2(vertical, -horizontal);
   float lengthSquared = dot(gradient, gradient);
   float gradientLength = sqrt(lengthSquared);
   // The original reaches the length as rcp(rsqrt(|g|^2)) behind dgVoodoo's zero guards; sqrt is the same value
   // without the huge sentinel, which the compiler refuses to constant-fold against 0.1 below.
   float2 direction = (lengthSquared > 0.0) ? gradient * rsqrt(lengthSquared) : 0.0;
   // The second tap steps by (0.1 - |g|) pixels: a strong edge makes this negative and selects the blurred value,
   // a flat area leaves it at ~0.1 and keeps the centre untouched.
   float backStep = 0.1 - gradientLength;

   float3 tapA = SampleScene(uv + direction * pixel).rgb;
   float3 tapB = SampleScene(uv - backStep * pixel).rgb;
   float3 blurred = (center + tapA + tapB) * 0.333333;

   // `ge` against 0 per component in the original: the centre survives wherever the step turned non-negative.
   return (backStep >= 0.0) ? center : blurred;
}
#endif // MOH_EDGE_AA

#if MOH_MOTION_BLUR
// Camera motion blur / DoF composite. Four taps of the downsampled max-depth buffer around the pixel are compared
// against the centre's linear depth; the differences steer where the blurred colour buffer is sampled, so the blur
// does not bleed across a depth discontinuity. Transcribed from the disassembly.
float3 RunMotionBlurDoF(float2 uv, float3 color, float sceneDepthRaw)
{
   // Per-pixel jitter, so the two colour taps below do not band.
   float3 jitter = frac(NoiseParams.xyy * uv.xyy);
   jitter = jitter * PixelOffsets.xyy - PixelOffsets.zww;

   // Centre depth comes from the SCENE ALPHA (UE3 packs depth there), linearised and reciprocated.
   float centerDepth = SafeRcp(sceneDepthRaw * DepthParams.z - DepthParams.w);

   // Four neighbour depths, each clamped exactly as the original does.
   float4 depths;
   depths.x = min(max(ApplyDgvMask(DepthTexture.Sample(DepthSampler_s, uv), DgvMaskDepth, DgvFillDepth).x, 1.0), 65504.0);
   depths.z = min(max(ApplyDgvMask(DepthTexture.Sample(DepthSampler_s, uv + jitter.xy * 4.0), DgvMaskDepth, DgvFillDepth).x, 1.0), 65504.0);

   float3 scaled = jitter.xyz * float3(4.0, 4.0, 6.8);
   float2 uvA = uv + float2(scaled.x, 0.0);
   float2 uvB = uv + float2(0.0, scaled.y);
   depths.y = min(max(ApplyDgvMask(DepthTexture.Sample(DepthSampler_s, uvA), DgvMaskDepth, DgvFillDepth).x, 1.0), 65504.0);
   depths.w = min(max(ApplyDgvMask(DepthTexture.Sample(DepthSampler_s, uvB), DgvMaskDepth, DgvFillDepth).x, 1.0), 65504.0);

   depths -= centerDepth;

   float3 weights;
   weights.x = -abs(depths.x) + abs(depths.y);
   weights.y = -abs(depths.w) + abs(depths.z);
   weights.z = dot(abs(depths), float4(-1.0, -1.0, 1.0, 1.0));
   weights = saturate(weights * -0.001 + 0.25);

   float2 offset = scaled.xx * weights.xy;
   float2 base = uv - jitter.xz * 2.0;

   // The first tap also carries the third jitter component (x6.8) on Y — the original keeps it live in the same
   // register the doubled offsets land in, so the two taps are NOT symmetric.
   float4 tapA = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, base + float2(offset.y * 2.0, scaled.z)), DgvMaskBlur, DgvFillBlur);
   float4 tapB = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, base + float2(offset.x * 2.0, 0.0)), DgvMaskBlur, DgvFillBlur);
   float4 blurred = lerp(tapB, tapA, weights.z);

   // The blurred buffer is premultiplied: its alpha is the coverage the scene has to give up.
   return color * (1.0 - blurred.w) + blurred.rgb;
}
#endif // MOH_MOTION_BLUR

// Transcribed coefficients of the vanilla filmic curve, shared by the vector and scalar forms below so the two
// cannot disagree if they are ever re-derived from the disassembly.
#define MOH_FILMIC_NUM(c, x2, x3) (0.359186 * (c) + 8.834813 * (x2) - 3.594901 * (x3))
#define MOH_FILMIC_DEN(c, x2, x3) (1.0 + 5.367284 * (c) + 3.800965 * (x2) - 1.844681 * (x3))

// The vanilla filmic curve. Only valid on [0,1] — see the header note; the caller must clamp first.
float3 FilmicMOH(float3 c)
{
   float3 x = c * 1.7;
   float3 x2 = x * x;
   float3 x3 = x * x2;
   float3 numerator = MOH_FILMIC_NUM(c, x2, x3);
   float3 denominator = MOH_FILMIC_DEN(c, x2, x3);
   return numerator * float3(SafeRcp(denominator.x), SafeRcp(denominator.y), SafeRcp(denominator.z));
}

// Scalar form of the same rational. Three of the expansion's four evaluations are on constants and fxc folds
// them away; this exists for the fourth, on the max channel, where the float3 form would leave two dead lanes of
// a divide. Coefficients come from the shared defines above so a re-derivation cannot land in one copy only.
float FilmicMOH1(float c)
{
   float x = c * 1.7;
   float x2 = x * x;
   float x3 = x * x2;
   float numerator = MOH_FILMIC_NUM(c, x2, x3);
   float denominator = MOH_FILMIC_DEN(c, x2, x3);
   return numerator * SafeRcp(denominator);
}

// The curve as the pass actually applies it: scene-referred in, display-referred out, clipped where vanilla clips.
float FilmicMOHScene(float u)
{
   return FilmicMOH1(saturate(u * 0.588235)); // 0.588235 = 1/1.7
}

#if TONEMAP_TYPE >= 1
// How much brighter than its SDR rendition the pixel should be, as ONE SCALAR taken from the max channel.
//
// This is the repository's bounded-LUT recipe (NOTES section 4) in the form MELE ships it
// (MELE_FilmicMaxChannelExpand, Tonemap_ME12_LUT_Body.hlsl): the LUT input is never touched, the vanilla
// per-channel chain runs exactly as the game wrote it, and the uncompression is a single multiply applied to the
// graded result. The vanilla white blowout therefore survives into HDR because the value being scaled already
// carries it, and the per-channel clip's hue skew survives too, by construction — a scalar cannot change the
// ratio between channels. That is what makes an explicit hue-restoration pass unnecessary here.
//
// Simpler than MELE's, which needs a central difference across three texture probes because its curve IS a
// 4096x1 LUT. Ours is analytic, so every anchor is a compile-time constant. They are derived from the curve
// rather than written as literals so the two cannot drift apart; fxc folds all three.
float MOH_FilmicMaxChannelExpand(float3 untonemapped)
{
   const float mch = max(max(untonemapped.r, max(untonemapped.g, untonemapped.b)), 1e-6);

   // Mid-gray is in `untonemapped` units, i.e. AFTER exposure. The curve happens to be near-identity there
   // (0.18 -> 0.1805), which is its designed neutral point, so the blend below needs no per-game tuning.
   const float y_mid = FilmicMOHScene(0.18);                                 // 0.1805
   const float slope = (FilmicMOHScene(0.20) - FilmicMOHScene(0.16)) / 0.04; // 1.3414, per SCENE unit
   const float g_top = FilmicMOH1(1.0);                                      // 0.9929, the value AT the clip

   const float g_mch = FilmicMOHScene(mch);
   const float tm_tan = y_mid + slope * (mch - 0.18); // C1 tangent at mid-gray

   // Zero at and below mid-gray, one at the ceiling. Squared so the expansion stays out of the upper mid-tones
   // instead of ramping in as soon as the curve starts bending.
   const float progress = saturate((g_mch - y_mid) / max(g_top - y_mid, 1e-4));
   const float tm = lerp(g_mch, tm_tan, progress * progress);

   // Past the clip (scene 1.7) g_mch sits on the ceiling and progress pins at 1, so tm follows the tangent and
   // keeps growing linearly. That is the extrapolation past the LUT's terminal cell, for free.
   const float fit = Reinhard::ReinhardPiecewise(tm, 1.0, 0.9); // identity below 0.9, shoulder to [0,1]

   // EXACTLY 1.0 below the seam, which tm only crosses around scene ~1.1: everything in SDR range comes out
   // bit-unchanged, so this is opt-in per pixel rather than a frame-wide gain.
   return (tm > 0.9) ? (tm / fit) : 1.0;
}
#endif // TONEMAP_TYPE >= 1

// The vanilla grade: gamma encode then the 32^3 LUT, sampled exactly as the original does (LOD 0, no scale/bias on
// the coordinate — the sampler's clamp mode owns the edges).
float3 GradeMOH(float3 tonemapped)
{
   float3 encoded = PowMOH(tonemapped, 0.4);
   float4 graded = ColorGradingLUT.SampleLevel(ColorGradingLUTSampler_s, encoded, 0.0);
   return ApplyDgvMask(graded, DgvMaskLUT, DgvFillLUT).rgb;
}

// Shared HDR back half: highlight recovery -> vanilla hue emulation -> display rolloff -> user sliders -> engine
// fade -> paper white -> encode -> sanitize.
//
// `graded_sdr` is the vanilla LUT output WITHOUT the fade tail (the look reference the recovery builds on), and
// `vanilla_sdr` is the pass's exact output, fade included (the TONEMAP_TYPE 0 result). The fade is held out of the
// reference and re-applied here as a plain gain: UpgradeToneMap builds an ADDITIVE delta from `untonemapped`, which
// carries no fade, so folding the fade into the reference makes the ratio blow up as the screen darkens.
float3 FinishMOH(float3 untonemapped, float3 graded_sdr, float3 vanilla_sdr, float2 sceneUV)
{
#if TONEMAP_TYPE >= 1
   // Linearized with the SHARED transfer function, not with the inverse of the game's own pow(x, 0.4).
   // The engine encodes at gamma 2.5 but was authored on ~2.2 displays, so the light the player actually saw is
   // pow(sdr, 2.2) — that, not the engine's nominal intent, is the reference the recovery has to build on, and it
   // is what makes the TONEMAP_TYPE 0 path round-trip to a vanilla-identical image (Settings.hlsl: a game that
   // fuses tonemap and encoding into one formula selects Gamma 2.2).
   float3 ungraded_sdr = gamma_to_linear(graded_sdr); // linear SDR reference (1.0 = white)

   // Uncompress by the max channel: one scalar, applied to the graded SDR. See MOH_FilmicMaxChannelExpand.
   float3 recovered = ungraded_sdr * MOH_FilmicMaxChannelExpand(untonemapped);

#if DEVELOPMENT
   // A/B against the path this replaced, for the calibration pass only. DevSetting04 = 1 selects the legacy
   // additive recovery plus its explicit hue restoration; 0 (the default, and the only thing Publishing compiles)
   // selects the max-channel wrap above. DELETE both this branch and everything it reaches once the comparison is
   // settled -- this game's sources are untracked, so there is no commit to fall back to in the meantime.
   [branch] if (LumaSettings.DevSetting04 > 0.5)
   {
      // Recover the highlight luminance the SDR chain clipped, on top of the graded look.
      float3 neutral_sdr = NeutralSDR(untonemapped);
      recovered = UpgradeToneMap(untonemapped, neutral_sdr, ungraded_sdr);

#if ENABLE_HUE_RESTORATION
      // --- vanilla highlight-hue emulation ---
      // The vanilla ceiling here is a per-channel `saturate` feeding a [0,1] LUT: on a bright saturated source R
      // reaches 1 first, then G, so the hue skews toward yellow-white approaching the clip, and the recovery above
      // removes that skew along with the clip. `ungraded_sdr` IS that clipped grade, so it is the faithful reference
      // and needs no synthetic stand-in (BL2 builds a ReinhardPiecewise one only because its wrap makes the grade
      // invertible; Airborne and BL GOTY re-run their analytic grades UNCLAMPED and lock hue at strength 1.0 AFTER
      // the display map instead — neither is constructible here: the filmic curve is only valid on [0,1] and the LUT
      // is a [0,1] lookup, so no un-blown grade exists).
      //
      // Placement is therefore the clipped-reference one: BEFORE the display map, so DICE rolls off a colour that
      // already carries the vanilla skew, rather than being pulled back afterwards.
      //
      // The gate is exact, not conservative: below 1.0 nothing clipped, `recovered` is `ungraded_sdr` and the restore
      // provably does nothing, sparing two Oklab round trips on the overwhelming majority of pixels.
      //
      // HAZARD: hue strength must stay BELOW 1.0. Once every channel clips the reference is (1,1,1), whose
      // chrominance is 1.7e-4 rather than 0, so the helper misses its safe-division fallback and its renormalization
      // goes near-singular — the hue runs away while chroma stays put, and a white-hot pixel comes out CYAN. The
      // measured margins are in The Witcher 2's FinalGrade_0xDE5CF9CD.ps_5_0.hlsl, which restores against the same
      // kind of reference: +0.87 deg at 0.80, +4.49 at 0.95, +143.9 at 1.00. 0.90 is the hard ceiling.
      [branch] if (max(recovered.r, max(recovered.g, recovered.b)) > 1.0)
      {
         // Only the CHROMA argument can whiten: the helper transfers hue, then restores the target's chrominance.
         recovered = RestoreHueAndChrominance(recovered, ungraded_sdr, saturate(LumaSettings.GameSettings.HighlightsHueStrength), saturate(LumaSettings.GameSettings.HighlightsHueChroma));
      }
#endif // ENABLE_HUE_RESTORATION
   }
#endif // DEVELOPMENT

   // Display rolloff to the user's peak/paper-white nits (DICE, hue-preserving by luminance). Both are floored:
   // DICE divides by them, so an unset (zero) value would emit NaN over the whole frame, and a NaN in a unorm
   // target reads back black — a failure mode that looks exactly like "the 3D disappeared".
   const float paperWhite = max(LumaSettings.GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(LumaSettings.PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
#if TONEMAP_IN_WIDER_GAMUT
   recovered = BT709_To_BT2020(recovered);
#endif
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   float3 hdr = DICETonemap(recovered * paperWhite, peakWhite, ds) / paperWhite;
#if TONEMAP_IN_WIDER_GAMUT
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));
#endif

   // Perceptual highlight dechroma: bright sources fade toward white as luminance approaches peak.
   const float highlightDechroma = LumaSettings.GameSettings.HighlightDechroma;
   // Uniform, and OFF by default: without [branch] fxc flattens this to a movc and pays the div/log/exp on every
   // pixel of every frame for a result it then discards (verified on the disassembly).
   [branch] if (highlightDechroma > 0.0)
   {
      // Exponent in [1, 0.05], never 0: pow(x,0) is 1 everywhere, i.e. a full-frame greyscale rather than a
      // highlight dechroma.
      float dcExp = lerp(1.0, 0.05, highlightDechroma);
      float dcWeight = saturate(pow(saturate(GetLuminance(hdr) / peakWhite), dcExp));
      hdr = Saturation(hdr, 1.0 - dcWeight);
   }

   // User saturation (shared helper: a lerp against BT.709 luminance). 1.0 = vanilla.
   hdr = Saturation(hdr, LumaSettings.GameSettings.Saturation);

   // User contrast: slope around 18% mid-gray (linear, 1.0 = paper white). 1.0 = vanilla.
   const float midGray = 0.18;
   hdr = (hdr - midGray) * LumaSettings.GameSettings.Contrast + midGray;

   // Engine fade (level start, cutscene transitions), re-applied LAST and in the GAME'S OWN encoding, because that
   // is where the original applies it: `out = (lut * .z + .w) * .x` runs on the pow(x, 0.4) result, so doing it in
   // linear would bend the fade curve and mistint the frame whenever the offset is non-zero. It also has to come
   // after the creative sliders: the contrast slider pivots around mid-gray, so a fade folded in earlier would
   // resolve to (0 - 0.18) * C + 0.18 at full fade and never reach black.
   // Uniform branch — at rest (.z = 1, .w = 0, .x = 1) this is a no-op and the two pows are skipped entirely.
   [branch] if (GradeParams.z != 1.0 || GradeParams.w != 0.0 || GradeParams.x != 1.0)
   {
      float3 gameEncoded = PowMOH(hdr, 0.4);
      gameEncoded = gameEncoded * GradeParams.z + GradeParams.w;
      gameEncoded *= GradeParams.x;
      hdr = PowMOH(max(0.0, gameEncoded), 2.5); // exact inverse of the pow(x, 0.4) above, so the round trip is lossless
   }

   float3 outColor = hdr; // linear, 1.0 = paper white
#else
   // Vanilla reference: linearize the exact SDR output.
   // Decoded and re-encoded with the same shared transfer function, so the canvas ends up holding the vanilla
   // bytes and the composition emits the vanilla image.
   float3 outColor = gamma_to_linear(saturate(vanilla_sdr));
#endif

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
   // Pre-scale so the gamma-SDR HUD on this same canvas lands at UIPaperWhite after composition rescales by it.
   // Through the shared helper (game-local Includes/Common.hlsl), which is also what the two bloom replacements
   // derive their paper-white anchor from — inlining it here is how those anchors drift apart.
   outColor *= UiPreScale();
   outColor = max(0.0, outColor); // negatives would turn into NaN in linear_to_gamma below
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
#if TONEMAP_TYPE >= 1
   // Uniform. Flattened without this, and the body carries two sincos.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
       ApplyDithering(outColor, sceneUV, true, 1.0, DITHERING_BIT_DEPTH, LumaSettings.FrameIndex, true);
#endif
#endif
   // Sanitize LAST: the recovery, hue restore, gamma encode and dither can each emit NaN, and a NaN written to a
   // unorm render target reads back as an undefined (in practice black) pixel.
   outColor = IsNaN_Strict(outColor) ? 0.0 : outColor; // bit test, not "x != x": that form gets optimized away
   outColor = max(0.0, outColor);

#if DEVELOPMENT
   // Bring-up bisect (DEV only, driven by the Luma dev sliders; all off = normal output):
   //   DevSetting01 - solid magenta: proves the replacement is bound and reaching the canvas at all.
   //   DevSetting02 - the reconstructed scene mix, gamma encoded: proves t0/t2/t3/UVs/masks are read correctly.
   //   DevSetting03 - the vanilla output: proves the chain reconstruction matches the original.
   if (LumaSettings.DevSetting01 > 0.5)
      return float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      return linear_to_gamma(saturate(untonemapped));
   if (LumaSettings.DevSetting03 > 0.5)
      return saturate(vanilla_sdr);
#endif

   return outColor;
}

// Core entry, shared by the four wrappers. `sceneUV` is TEXCOORD0 (v5 in the original).
// Returns the canvas value plus the alpha the bloom bright pass depends on.
float4 RunMOHTonemap(float2 sceneUV)
{
   float4 scene = SampleScene(sceneUV);

   float3 color = scene.rgb;
#if MOH_EDGE_AA
   // Stood down while SMAA owns antialiasing: EdgeAA is a luma-gradient directional BLUR, not a resolve, so
   // running both just smears. With the gate on, this permutation collapses onto its non-EdgeAA twin.
   if (LumaSettings.GameSettings.SMAAEnable <= 0.5)
      color = RunEdgeAA(sceneUV, color);
#endif
#if MOH_MOTION_BLUR
   // Depth always comes from the CENTRE sample's alpha, even when EdgeAA replaced the colour.
   color = RunMotionBlurDoF(sceneUV, color, scene.w);
#endif

   // Vanilla writes max3 of the scene mix, taken BEFORE exposure, scaled by 1/8 and floored at 1/8. The bloom
   // bright pass multiplies its taps by (alpha * 8) to recover overbright from this 8-bit canvas.
   const float rawMax = max(max(color.r, color.g), color.b);

   // AFTER rawMax: that value is the vanilla alpha contract the bright pass reads, and it has to stay vanilla.
   // BEFORE exposure: the glow is scene-referred like everything else, so it tracks the exposure change.
   color = ApplyLumaBloom(color, sceneUV);

   // Scene exposure (multiplier), scene-referred; the SDR references below derive from the same value, so the
   // grade tracks the exposure change.
   float3 untonemapped = color * SceneExposure.z * LumaSettings.GameSettings.Exposure;

   // The vanilla chain, unchanged: the saturate is the LUT's domain guard, not an artefact.
   float3 tonemapped = FilmicMOH(saturate(untonemapped * 0.588235));
   float3 graded_sdr = GradeMOH(tonemapped);                        // no fade
   float3 vanilla_sdr = graded_sdr * GradeParams.z + GradeParams.w; // fade tail, exactly as the original
   vanilla_sdr *= GradeParams.x;

   float3 outColor = FinishMOH(untonemapped, graded_sdr, vanilla_sdr, sceneUV);

   // The vanilla contract, in BOTH paths. This used to write a flat 0.125 in HDR, on the reasoning that the fp16
   // canvas already carries the overbright so the bright pass must not scale it again. That reasoning cost the
   // bloom chain the very thing that made it selective: vanilla's weight is `max(rawMax, 1)`, which is exactly 1
   // for every pixel below scene white, so the vanilla glow ignored the whole mid-tone range and only overbright
   // sources could pass the 0.9 threshold. With a flat weight and an HDR canvas instead, everything the display map
   // lifted past 1.0 -- a wide band right below diffuse white -- started clearing the threshold, the pyramid
   // integrated it over five levels, and the fallback turned into a full-frame veil.
   // The bright pass clips the canvas back to the paper-white anchor before applying this weight, which is what the
   // 8-bit target did for free, so the two together reproduce vanilla exactly. See BloomBrightPass_0x5FE75452.
   const float outAlpha = max(rawMax * 0.125, 0.125);

   return float4(outColor, outAlpha);
}
