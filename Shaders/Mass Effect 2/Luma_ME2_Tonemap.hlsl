// Mass Effect 2 (2010) Luma HDR tonemap, split across the two colour passes that end a frame: the uber leaves
// LINEAR UNMAPPED light, the material vignettes and THEN display-maps. Order, perms and evidence: NOTES.md.

// clang-format off
// ORDER IS LOAD-BEARING, do not sort: game-local Common.hlsl first, or GameSettings resolves to the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip, and Oklab through it
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // Reinhard::ReinhardPiecewise (hue-shift reference)
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3

// HDR / vanilla. 1 = recover real highlights + DICE display map. 0 = vanilla clamped SDR reference (default while
// the port is being brought up: it is the reference every HDR change gets compared against).
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 0
#endif

// Run the display map in a BT.2020 working space (round-tripped back to BT.709). Gamut-correct handling of
// highly saturated highlights — NOT a display-gamut expansion.
#ifndef TONEMAP_IN_WIDER_GAMUT
#define TONEMAP_IN_WIDER_GAMUT 1
#endif

// Set by the 0xDB1022A7 entry point only: that permutation runs the native filmic curve before the grade.
#ifndef ME2_UBER_FILMIC
#define ME2_UBER_FILMIC 0
#endif

// Set by the 0xCF0CB35A entry point only: that permutation adds native film grain.
#ifndef ME2_MATERIAL_GRAIN
#define ME2_MATERIAL_GRAIN 0
#endif

// ---------------------------------------- Shared ----------------------------------------

// Common sanitize for anything stored in the fp16 canvas: the recovery, the encode and the dither can each emit
// NaN, and a NaN in a unorm target reads back black. Bit test, not "x != x": that form gets optimized away.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// The display map, and the LAST colour operation of the scene. Called from the MATERIAL pass, after the vignette.
// Takes/returns LINEAR light, 1.0 = paper white; reads only LumaSettings and NO cb4 row, so it can live in either pass.
float3 MapME2ToDisplay(float3 sceneHDR)
{
   // Through the Settings.hlsl accessors, not LumaSettings directly: they carry the HDR_TONEMAP_* overrides and the
   // devkit white level. Floors stay - outside DEVELOPMENT there is no unset fallback, and DICE divides by both.
   const float paperWhite = max(GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
#if TONEMAP_IN_WIDER_GAMUT
   sceneHDR = BT709_To_BT2020(sceneHDR);
#endif
   // Luminance in PQ (hue-preserving), then CORRECT_CHANNELS_BEYOND_PEAK_WHITE fades over-peak channels to white.
   // Identity below the shoulder (a third of peak), so diffuse content and the upstream sliders are untouched.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
#if TONEMAP_IN_WIDER_GAMUT
   // DICE converts InOutColorSpace -> ProcessingColorSpace on entry and back on exit. We already converted above
   // and undo it below, so leaving the default CS_BT709 in makes it convert a SECOND time and run its compression,
   // its average()-based source luminance and its channel containment on doubly-narrowed primaries.
   ds.InOutColorSpace = CS_BT2020;
#endif
   float3 hdr = DICETonemap(sceneHDR * paperWhite, peakWhite, ds) / paperWhite;
#if TONEMAP_IN_WIDER_GAMUT
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));
#endif

   // Perceptual highlight dechroma (optional, 0 = off). Display-referred by construction (it normalizes by peak),
   // so it cannot move upstream: on unmapped input the weight saturates and the highlights go greyscale.
   // `hdr` is paper-white-relative while `peakWhite` is 80-nit-relative, hence the paperWhite factor — without it
   // the effect silently weakened as the user raised Game Paper White. Linear in both the slider and luminance,
   // so it is continuous at 0 (an exponent-only mapping jumped to a large desaturation on the first slider tick).
   const float dcWeight = LumaSettings.GameSettings.HighlightDechroma * saturate(GetLuminance(hdr) * paperWhite / peakWhite);
   return Saturation(hdr, 1.0 - dcWeight);
}
#endif // TONEMAP_TYPE >= 1

// ---------- Stage 1: UberPostProcessBlend -> fp16 canvas, LINEAR unmapped in HDR / vanilla in SDR ----------
// `sceneUV` is TEXCOORD1 (t0), `blurUV` is TEXCOORD0 (t1) - the original samples t0 with v6 and t1 with v5.
#define DoFParams                 PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur                PsConstants[10] // .x max blur near, .y max blur far
#define SceneShadowsAndDesat      PsConstants[11] // .xyz shadows lift (subtracted), .w saturation weight
#define SceneInverseHighLights    PsConstants[12] // .xyz scene scale
#define SceneMidTones             PsConstants[13] // .xyz grade gamma
#define SceneLuminanceWeights     PsConstants[14] // .xyz desaturation luminance weights
#define GammaColorScaleAndInverse PsConstants[15] // .xyz output scale (doubles as the engine fade), .w display encode
#define GammaOverlayColor         PsConstants[16] // .xyz tint offset

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState BlurredImageSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene; .w carries SCENE DEPTH, not alpha
Texture2D<float4> BlurredImage : register(t1);      // quarter-res DoF+bloom blur, stored pre-divided by 4

// Luma HDR bloom pyramid mip 0 (Luma_Bloom_impl.hlsl): half-res LINEAR fp16, injected before the uber draw, slot
// must match kLumaBloomSlot. It REPLACES the game's bloom - the gather replacement stops writing that one.
Texture2D<float4> LumaBloomTexture : register(t6);

// The Luma glow, scene-referred LINEAR, added where the vanilla glow was. Gain = the engine's BloomScale x Bloom
// Intensity; the pyramid is energy-preserving, so that product is what makes intensity 1 vanilla strength.
float3 LumaBloom(float2 sceneUV)
{
   if (LumaSettings.GameSettings.LumaBloomEnable <= 0.5)
      return 0.0;
   const float3 bloom = LumaBloomTexture.SampleLevel(SceneColorTextureSampler_s, sceneUV, 0.0).rgb;
   return max(0.0, bloom) * LumaSettings.GameSettings.BloomScaleLive * LumaSettings.GameSettings.BloomIntensity;
}

// The native Hejl / Uncharted-2-family filmic curve, transcribed from 0xDB1022A7. It outputs a ~gamma-encoded
// value which the original immediately squares, i.e. converts back to ~linear at gamma 2.0, before the grade.
float3 ME2_NativeToneCurve(float3 scene)
{
   const float3 x = max(scene - 0.004, 0.0);
   const float3 num = x * (6.2 * x + 0.5);
   const float3 den = x * (6.2 * x + 1.7) + 0.06;
   const float3 t = num / den; // den >= 0.06 for x >= 0, so it needs no floor
   return t * t;
}

// Deliberately here and not with the includes at the top: it evaluates the curve above and its analytic slope.
#include "Includes/FilmicRecovery.hlsl"

// Highlight hue emulation for the unclamped-grade recovery (hard-clip perm only). The unclamped
// grade keeps a blown source's real channel ratio; no SDR pipeline ever showed that ratio — a per-channel limiter turned
// the hue toward white (R saturates first, then G) and dropped chroma. `hdr` = unclamped grade, `reference` = the colour
// after the limiter being emulated, the RenoDX synthetic one built at the call site, both LINEAR with 1.0 = SDR white. Oklch: rotate the hue
// along the shortest arc toward the reference's, keep the HDR lightness, and optionally lower saturation toward the
// reference's (its whitening). Saturation is compared as C/L, so a dimmer reference does not read as extra whitening,
// and the whole transfer is scale-invariant: Game Paper White cannot move it. Powerless guard: hue is undefined near the
// achromatic axis, so a near-white reference (a fully clipped source) or target fades the transfer out instead of
// running away — the reason the shared JzAzBz helper had to be capped at 0.8 against a clipped reference.
float3 EmulateHighlightHue(float3 hdr, float3 reference, float hueStrength, float whitening)
{
   const float3 t = Oklab::linear_srgb_to_oklab(hdr);
   const float Lt = max(t.x, 1e-4);
   const float satT = length(t.yz) / Lt;
   // Saturation (Oklab C/L) below which hue is numerical noise: a clipped (1,1,1) reads ~4e-8, real colour 0.05+.
   // Hard zero below half the threshold (smoothstep's lower edge). An achromatic TARGET has nothing to rotate and
   // nothing to whiten: return it bit-exact.
   const float kPowerless = 0.02;
   const float confT = smoothstep(0.5 * kPowerless, kPowerless, satT);
   if (confT <= 0.0)
      return hdr;
   const float3 s = Oklab::linear_srgb_to_oklab(reference);
   const float Ls = max(s.x, 1e-4);
   const float satS = length(s.yz) / Ls;
   // The two axes read the reference differently. HUE needs a chromatic reference: a white one carries no hue, so its
   // direction is noise and confS fades the rotation out. WHITENING must NOT be gated on the reference: a white
   // reference IS the whitening signal (the limiter blew that pixel to white), so it uses satS directly and a fully
   // clipped core goes white at HDR lightness, continuously with the rim.
   const float confS = smoothstep(0.5 * kPowerless, kPowerless, satS);
   const float hueT = atan2(t.z, t.y);
   const float hueS = atan2(s.z, s.y);
   const float delta = atan2(sin(hueS - hueT), cos(hueS - hueT)); // shortest arc, (-pi, pi]
   const float hue = hueT + hueStrength * confS * confT * delta;
   const float sat = lerp(satT, min(satT, satS), whitening * confT); // whitening only ever lowers saturation
   const float chroma = sat * Lt;
   return max(0.0, Oklab::oklab_to_linear_srgb(float3(t.x, chroma * cos(hue), chroma * sin(hue))));
}

// The vanilla canvas value decoded to the linear light a 2.2 display shows for it. Unlike ME1 2007 the uber's grade
// already applied the display exponent (GammaColorScaleAndInverse.w), so this is a plain decode.
float3 VanillaToLinear(float3 graded)
{
   return gamma_to_linear(max(0.0, graded), GCT_MIRROR);
}

// The game's grade, verbatim from both ubers' disassembly. `clampSDR` false = unclamped (max 0), keeping the real
// channel ratio. `outputScale` is 1 on the HDR path because that row doubles as the engine fade, re-applied later.
float3 GradeUE3(float3 curved, bool clampSDR, float3 outputScale)
{
   // Head: shadows -> scale -> midtones.
   float3 c = curved - SceneShadowsAndDesat.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   c = c * SceneInverseHighLights.xyz;
   c = PowUE3(c, SceneMidTones.xyz);

   // Tail: desat + tint + output scale + display encode.
   const float desat = dot(c, SceneLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesat.www + GammaOverlayColor.xyz;
   c = c + desat;
   c = c * outputScale;
   c = clampSDR ? saturate(c) : max(0.0, c); // mul_sat in the original
   return PowUE3(c, GammaColorScaleAndInverse.www);
}

#if TONEMAP_TYPE >= 1 && ME2_UBER_FILMIC
// Bit test rather than a comparison: `x != x` is legal for the compiler to fold away under fast math.
bool ME2_FilmicColorIsFinite(float3 value)
{
   return all((asuint(value) & 0x7F800000u) != 0x7F800000u);
}

// FILMIC-only artistic control, applied AFTER the brightness recovery and before the creative sliders. The recovery
// is scalar and keeps the vanilla channel ratios; this optionally moves highlights further, toward what the SAME
// vanilla curve and grade produce for a MORE EXPOSED version of the same scene. That second evaluation is a colour
// reference only: hue and relative chroma are taken from it, brightness is not, and the main image, its bloom and its
// recovery gain never see the extra exposure. `scene` is `untonemapped` (post-Exposure, pre-fade), `recovered` the
// recovery's output.
float3 ME2_ApplyFilmicHighlightColor(float3 scene, float3 recovered)
{
   // Tuned constants, not user controls. `referenceEV` picks WHICH vanilla colour the transfer aims at - the one the
   // same curve and grade give the scene a stop brighter. The two strengths say how far a pixel travels toward it,
   // and the mask below plus the helper's near-achromatic guard already keep that off everything but bright colour.
   const float referenceEV = 1.0;
   const float hueStrength = 1.0;
   const float blowout = 1.0;

   if (!ME2_FilmicColorIsFinite(scene) || !ME2_FilmicColorIsFinite(recovered))
      return recovered;
   // The recovery derives from the CLAMPED grade, so it is non-negative. This wrapper is not a signed-RGB pipeline.
   if (any(recovered < 0.0))
      return recovered;

   // Highlight mask on the SOURCE scene, before fade: off at or below 1.0, full at 4.0, two stops of input exposure
   // in between. Deliberately independent of paper white, display peak and the reference exposure above - that
   // exposure picks the reference colour, it must not slide the mask.
   const float maskStart = 1.0;
   const float maskEnd = 4.0;
   const float m = max3(scene);
   if (m <= maskStart)
      return recovered;
   const float weight = smoothstep(0.0, 1.0, log2(m / maskStart) / log2(maskEnd / maskStart));
   if (weight <= 0.0)
      return recovered;

   // Denominator guards, not the start of an artistic effect.
   const float epsilonY = 1e-6;
   const float originalY = GetLuminance(recovered, CS_BT709);
   if (!(originalY > epsilonY))
      return recovered;

   // The ONLY place the extra exposure applies.
   const float3 referenceScene = scene * exp2(referenceEV);
   if (!ME2_FilmicColorIsFinite(referenceScene))
      return recovered;
   const float3 reference = VanillaToLinear(GradeUE3(ME2_NativeToneCurve(referenceScene), true, 1.0));
   if (!ME2_FilmicColorIsFinite(reference))
      return recovered;
   // A BLACK reference is not a whitening signal, unlike a white one - it carries neither hue nor blowout.
   if (!(GetLuminance(reference, CS_BT709) > epsilonY))
      return recovered;

   const float3 transferred = EmulateHighlightHue(recovered, reference, hueStrength * weight, blowout * weight);
   if (!ME2_FilmicColorIsFinite(transferred) || all(transferred == recovered))
      return recovered;
   const float transferredY = GetLuminance(transferred, CS_BT709);
   if (!(transferredY > epsilonY))
      return recovered;

   // EmulateHighlightHue holds Oklab L, which is not photometric. Restore the linear BT.709 Y the recovery had, with
   // the same metric on both sides. No saturate: the recovery's output is unmapped HDR and the display map owns the peak.
   const float3 result = transferred * (originalY / transferredY);
   return ME2_FilmicColorIsFinite(result) ? result : recovered;
}
#endif // TONEMAP_TYPE >= 1 && ME2_UBER_FILMIC

float3 RunME2Uber(float2 blurUV, float2 sceneUV)
{
   // 1. Scene mix, exactly as vanilla: depth-driven DoF weight, bloom at x4, normalized by the weight sum.
   const float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   const float depth = scene.w; // UE3 packs scene depth in the fp16 alpha; read for the DoF weight, not written back

   const float signedDistance = depth - DoFParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * DoFParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? DoFMaxBlur.y : DoFMaxBlur.x;
   const float blurAmount = min(PowUE3(max(normalizedDistance, 1e-4).xxx, DoFParams.zzz).x, maxBlur); // 1e-4 as the original
   const float sceneWeight = saturate(1.0 - blurAmount);

   const float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   // Stored pre-divided by 4, hence the x4. NEVER scaled by a bloom slider: DoF and bloom are SUMMED into this
   // buffer, so the vanilla glow can only be touched inside the gather replacement.
   const float3 bloom = blurred.xyz * 4.0;
   const float weightSum = blurred.w * 4.0 + sceneWeight;

   // Into the numerator, where the vanilla glow was, so the DoF weight sum divides it too. In focus w = 1, plain
   // additive. The FILMIC perm then compresses the glow through its curve, as vanilla did - so the halo differs per perm.
   float3 untonemapped = scene.xyz * sceneWeight + bloom + LumaBloom(sceneUV);
   untonemapped *= (abs(weightSum) > 0.0) ? rcp(weightSum) : FLT_MAX; // rcp guard, as the original does

   // Scene exposure (multiplier), scene-referred / pre-grade; the SDR reference below derives from the same
   // `untonemapped`, so the grade tracks the exposure change.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // 2. The native curve, per channel, feeding the grade UNTOUCHED so the vanilla white blowout survives into HDR.
   // The hard-clip permutation has no curve; its only compression is the grade's own saturate.
#if ME2_UBER_FILMIC
   const float3 curved = ME2_NativeToneCurve(untonemapped);
#else
   const float3 curved = untonemapped;
#endif

   // The grade's output scale doubles as the engine FADE (level start, cutscenes). It stays OUT of the HDR
   // references and is re-applied as a gain at the end.
   const float3 outputScale = GammaColorScaleAndInverse.xyz;

   // 3. The game's own grade, vanilla-exact, fade included. This IS the output on the TONEMAP_TYPE 0 path.
   const float3 sdr_vanilla = GradeUE3(curved, true, outputScale);

#if TONEMAP_TYPE >= 1
   // The reference carries no fade (outputScale = 1), decoded to the linear light the vanilla canvas displays.
   const float3 sdr_ref = VanillaToLinear(GradeUE3(curved, true, 1.0));

#if ME2_UBER_FILMIC
   // Keep the native per-channel filmic + grade as the colour reference: `sdr_ref` already carries the vanilla
   // channel skew and whitening. Brightness alone is recovered, through a scalar gain taken from the curve's own
   // tangent above the pivot.
   const float3 recovered = ME2_RecoverFilmicBrightness(untonemapped, curved, sdr_ref);
#else
   // Hard-clip permutation: nothing to invert, so the grade run UNCLAMPED is the rebuild — vanilla-exact below the
   // clip and its own analytic continuation above it.
   float3 recovered = VanillaToLinear(GradeUE3(curved, false, 1.0));
   // Hue shift toward a per-channel Reinhard (ceiling 5, shoulder 1.5) of the colour itself, built in BT.2020 — the
   // reference our RenoDX ports ship. Primaries decide which channel turns first, hence the skew magnitude
   // (fire +31.4 deg in BT.2020 vs +41.8 in 709).
   const float3 hueEmuRef = BT2020_To_BT709(Reinhard::ReinhardPiecewise(BT709_To_BT2020(recovered), 5.0, 1.5));
   // Exact gate: below 1.0 the reference equals `recovered` (the Reinhard shoulder sits at 1.5 and the BT.2020
   // channels of a BT.709 colour never exceed its max), so the transfer is a provable no-op and the Oklab round trips
   // run only on real highlights.
   // Tuned constants, not user controls: full hue shift, and no whitening - the powerless guard inside the helper
   // makes 1.0 safe on the hue axis, while path-to-white is left to DICE at the display peak.
   [branch] if (max3(recovered) > 1.0)
       recovered = EmulateHighlightHue(recovered, hueEmuRef, 1.0, 0.0);
#endif

   // NEITHER recovery restores hue, deliberately: the FILMIC one rebuilds by a SCALAR ratio and the hard-clip one
   // carries the unclamped grade's own channel ratio. Evidence in NOTES.md; the display map owns path-to-white.
   // What follows on the filmic perm is a separate, opt-in artistic pass that leaves the recovered brightness alone
   // rather than correcting it: at its default strengths of 0 this line is the whole HDR colour.
   float3 hdr = recovered;

#if ME2_UBER_FILMIC
   hdr = ME2_ApplyFilmicHighlightColor(untonemapped, hdr);
#endif

   // Creative sliders, scene-referred. They stay in THIS pass because the fade below is a cb4 row only it can read,
   // and contrast must precede the fade: fade-first makes (0 - 0.18) * C + 0.18 land on grey instead of black.
   hdr = Saturation(hdr, LumaSettings.GameSettings.Saturation);
   // User contrast: slope around mid-gray (linear, 1.0 = paper white). 1.0 = vanilla.
   hdr = (hdr - MidGray) * LumaSettings.GameSettings.Contrast + MidGray;

   // Re-apply the engine fade linearly, LAST, after the creative sliders. At rest it is a no-op.
   // ⚠ This pass leaves UNMAPPED linear HDR (values can reach hundreds); the material maps it. See NOTES.md.
   float3 outColor = hdr * outputScale;
#else
   float3 outColor = sdr_vanilla; // vanilla display-encoded value
#endif

#if DEVELOPMENT
   // Bring-up bisect (DEV only, all off = normal): DevSetting01 solid magenta (replacement is bound), DevSetting02
   // the reconstructed scene mix (t0/t1/UVs/masks read right), DevSetting03 the vanilla grade.
   if (LumaSettings.DevSetting01 > 0.5)
      return float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      return saturate(untonemapped);
   if (LumaSettings.DevSetting03 > 0.5)
      return saturate(sdr_vanilla);
   // DevSetting04 paints the engine fade as a flat colour: mid-grey = 1.0 at rest, darker = fading down. Needed
   // because the HDR path holds that row out of its references and re-applies it at the end.
   if (LumaSettings.DevSetting04 > 0.5)
      return saturate(outputScale * 0.5);
#endif

   return Sanitize(outColor);
}

// End of stage 1. The cb4 aliases above name GAME CONTENT, whose meaning is per pass, and stage 2 re-uses rows 10
// and 11 for other things - so retiring them here makes a leak a compile error instead of a naming convention.
#undef DoFParams
#undef DoFMaxBlur
#undef SceneShadowsAndDesat
#undef SceneInverseHighLights
#undef SceneMidTones
#undef SceneLuminanceWeights
#undef GammaColorScaleAndInverse
#undef GammaOverlayColor

// ---------- Stage 2: BioSceneEffect material -> the canvas the present blit takes, the frame's LAST pass ----------
// Its own register map: PsConstants[8] means something different here than on the uber.
#define MatOffset      PsConstants[8]  // .xyz additive offset, applied last in the encoded domain
#define MatScreenUV    PsConstants[9]  // .xy ScreenPosition scale, .wz its bias (note the swizzle order)
#define MatGrainOffset PsConstants[10] // .xy grain UV offset, added before the 0.5 scale
#define MatGrainBias   PsConstants[11] // .xy grain UV bias, added after it

SamplerState MatSceneSampler_s : register(s0);
SamplerState MatGrainSampler_s : register(s1);
Texture2D<float4> MatSceneTexture : register(t0); // the canvas the uber wrote
Texture2D<float4> MatGrainTexture : register(t1); // grain permutation only

// The native BioSceneEffect radial vignette, from 0x277DA7AE / 0xCF0CB35A. Its centre value is a blue-tinted WHITE
// POINT (1.010363, 1.000006, 1.163092), so the slider scales only the darkening around it. Vanilla place, vanilla UV.
float3 ME2_Vignette(float2 uv)
{
   float2 vc = (uv - 0.5) * float2(0.832050323, 0.554700196);
   // The 1e-4 floors are the ORIGINAL's, at the ops the disassembly puts them on (`max r1.x, r0.x, l(0.0001)`
   // before each log): PowUE3's own 1e-30 is only a log2(0) guard, a different fact, so it is not a substitute.
   const float d = sqrt(dot(vc, vc));
   float falloff = 1.0 - PowUE3(max(d, 1e-4).xxx, 6.5.xxx).x;
   falloff = PowUE3(max(falloff, 1e-4).xxx, 200.0.xxx).x;

   const float3 tint = float3(0.010363, 0.000006, 0.163092);
   // Scale only the darkening: at intensity 0 the frame keeps the native white point instead of turning flat grey.
   // Vanilla is falloff = 1 at the centre, so the white point is tint + 1 and intensity 1 is the vanilla value.
   return lerp(tint + 1.0, tint + falloff, LumaSettings.GameSettings.VignetteIntensity);
}

// The native film grain, in whatever domain the caller is in - vanilla applies it AFTER the display encode, so both
// paths pass an encoded value. Amplitude tracks the sqrt of the vignetted colour, i.e. it is signal dependent.
#if ME2_MATERIAL_GRAIN
float3 ME2_Grain(float3 color, float2 grainUV, float intensity)
{
   const float2 nuv = (grainUV * 5.0 + MatGrainOffset.xy) * 0.5 + MatGrainBias.xy;
   const float3 grain = ApplyDgvMask(MatGrainTexture.Sample(MatGrainSampler_s, nuv), DgvMaskT1, DgvFillT1).xyz - 0.6;
   const float3 amplitude = sqrt(max(color, 0.0)) * float3(0.1, 0.15, 0.15);
   return color + amplitude * grain * intensity;
}
#endif

float3 RunME2Material(float2 grainUV, float3 screenPosition)
{
   // ScreenPosition fetch, exactly as vanilla: perspective divide, then the pass's own scale/bias.
   const float invW = (abs(screenPosition.z) > 0.0) ? rcp(screenPosition.z) : FLT_MAX;
   const float2 sceneUV = (screenPosition.xy * invW) * MatScreenUV.xy + MatScreenUV.wz;
   const float3 scene = ApplyDgvMask(MatSceneTexture.Sample(MatSceneSampler_s, sceneUV), DgvMaskT0, DgvFillT0).xyz;

   float3 outColor;
#if TONEMAP_TYPE >= 1
   // The uber left UNMAPPED linear light here. Vignette first, in vanilla's place: linear_to_gamma is a signed pure
   // pow, so multiplying by gamma_to_linear(vig) in linear IS the vanilla multiply in the encoded domain.
   outColor = scene * gamma_to_linear(ME2_Vignette(grainUV), GCT_POSITIVE); // vig = lerp(tint+1, tint+exp2(..)) >= 0

   // THEN the display map, last colour operation of the scene, so peak containment covers the vignette's white point
   // too. renodx's ordering; the two alternatives that failed (+39% blue, magenta rim) are recorded in NOTES.md.
   outColor = MapME2ToDisplay(outColor);

   // The additive terms below still stay in gamma, where vanilla put them: an add is not domain-invariant, and
   // grain needs constant perceptual amplitude rather than amplitude that collapses in shadows.

#if UI_DRAW_TYPE >= 2
   // Pre-scale so the gamma-SDR HUD on this canvas lands at UIPaperWhite after composition rescales by it. Accessors,
   // not LumaSettings (see MapME2ToDisplay), and still guarded: a zero would black the scene and leave the HUD.
   if (GamePaperWhiteNits > 0.0)
      outColor *= GamePaperWhiteNits / max(UIPaperWhiteNits, 1.0);
#endif

#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes and applies paper white.
   // GCT_POSITIVE, not MIRROR: no negative light may reach the canvas the gamma HUD blends onto.
   outColor = linear_to_gamma(outColor, GCT_POSITIVE);
#endif
#else
   // Vanilla: the canvas holds the vanilla display-encoded value, so every operation here is the original's -
   // including the vignette, which stays in this pass and in the encoded domain exactly as the game had it.
   outColor = scene * ME2_Vignette(grainUV);
#endif

   // Grain and the pass's additive offset are shared by both paths: vanilla applied them after its display encode,
   // which is where both branches above leave off.
#if ME2_MATERIAL_GRAIN
   // TONEMAP_TYPE is a preprocessor constant, so the ternary folds; the vanilla path stays bit-exact because the
   // slider is an HDR-path control.
   outColor = ME2_Grain(outColor, grainUV, TONEMAP_TYPE >= 1 ? LumaSettings.GameSettings.FilmGrainIntensity : 1.0);
#endif
   outColor += MatOffset.xyz;

#if TONEMAP_TYPE >= 1 && POST_PROCESS_SPACE_TYPE == 0
   // Anti-banding dither in the stored gamma space (the core composition does not dither). Animated triangular
   // noise; sub-perceptual at bit depth 9. [branch] because the block is ~17 instructions and two sincos.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyDithering(outColor, sceneUV, true, 1.0, DITHERING_BIT_DEPTH, LumaSettings.FrameIndex, true);
   }
#endif

   return Sanitize(outColor);
}
