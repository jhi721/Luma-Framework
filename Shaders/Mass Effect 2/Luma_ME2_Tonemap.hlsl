// Mass Effect 2 (2010) Luma HDR tonemap, split across the two colour passes that end a frame: the uber display-maps
// and encodes, so the canvas between them holds vanilla's domain (gamma) for the game's post-uber feedback materials
// (bMergePostUber: Singularity, Warp, ...) drawn in between; the material decodes, vignettes and contains the
// vignette's white point to peak. Material-less frames are finished by Luma_ME2_DisplayMap.hlsl; FGammaCorrection,
// never yet seen drawing, is handled for whichever of those ran before it.

// clang-format off
// ORDER IS LOAD-BEARING, do not sort: game-local Common.hlsl first, or GameSettings resolves to the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // Reinhard::ReinhardPiecewise (hue-shift reference)
#include "Includes/MacLeodBoynton.hlsl"     // MacLeodBoynton::HueOnlyBT2020. Byte-identical copy of the BL GOTY production model: do not edit here, sync it from "Borderlands GOTY Enhanced/Includes"
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3

// HDR / vanilla. 1 = recover real highlights + DICE display map (the default, as in main.cpp). 0 = vanilla clamped
// SDR reference, the one every HDR change gets compared against.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
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

// Closing guard for anything stored in the fp16 canvas, two different contracts in one place. NaN -> 0: the
// recovery, the encode and the dither can each emit NaN, and a NaN in the canvas reads back black (bit test, not
// "x != x", which fast math folds away). Negative -> 0 is gamut CONTAINMENT, not sanitizing: the FILMIC HDR path
// and every SDR path are non-negative by construction, so it only ever acts on the hard-clip permutation's
// MacLeod-Boynton return from BT.2020 (0 of 20000 sampled highlights, but not guaranteed), on the signed grain and
// offset terms the material adds after its encode, and on the signed dither near black.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// Paper and peak white in DICE's units. Through the Settings.hlsl accessors, not LumaSettings directly: they carry the
// HDR_TONEMAP_* overrides and the devkit white level. Floors stay - outside DEVELOPMENT there is no unset fallback,
// and DICE divides by both.
void ME2_DisplayWhites(out float paperWhite, out float peakWhite)
{
   paperWhite = max(GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   peakWhite = max(PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
}

// The display map's DICE settings, shared with ME2_ContainToPeak so both contain to the same peak the same way.
DICESettings ME2_DisplayDICESettings()
{
   // Luminance in PQ (hue-preserving), then CORRECT_CHANNELS_BEYOND_PEAK_WHITE fades over-peak channels to white.
   // Identity below the shoulder (a third of peak), so diffuse content and the upstream sliders are untouched.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Highlight dechroma handed to DICE rather than run as our own pass afterwards. Core's is better placed: it ramps
   // on the MAX CHANNEL (by luminance a bright blue never triggers), exists only between ShoulderStart * PeakWhite
   // and peak (1/3 of peak for this type, so mid-tones cannot be touched), and runs INSIDE the containment in the
   // processing primaries. 0 = off for the OUTPUT but not the cost: DICE's guard carries no [branch], so fxc
   // flattens it for every pixel above the shoulder.
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   // DICE converts InOutColorSpace -> ProcessingColorSpace on entry and back on exit. The callers convert to BT.2020
   // themselves and undo it after, so leaving the default CS_BT709 in makes it convert a SECOND time and run its
   // compression, its average()-based source luminance and its channel containment on doubly-narrowed primaries.
   ds.InOutColorSpace = CS_BT2020;
   return ds;
}

// The display map plus the user saturation. Called by the uber (so the canvas after it is display-referred, as
// vanilla's was) and by the gamma pass on a raw-scene frame. Takes/returns LINEAR light, 1.0 = paper white; reads
// only LumaSettings and NO cb4 row, so either can call it.
float3 MapME2ToDisplay(float3 sceneHDR)
{
   float paperWhite, peakWhite;
   ME2_DisplayWhites(paperWhite, peakWhite);
   // The map runs in a BT.2020 working space and is round-tripped back below: gamut-correct handling of highly
   // saturated highlights, NOT a display-gamut expansion. Validated offline.
   float3 hdr = DICETonemap(BT709_To_BT2020(sceneHDR) * paperWhite, peakWhite, ME2_DisplayDICESettings()) / paperWhite;
   // The clip is a guard, not a working step: nothing upstream can leave BT.2020 here. Every caller hands in
   // non-negative BT.709 (the uber's Sanitize, the gamma pass's max(0)), which sits strictly inside BT.2020, and every
   // DICE step either preserves chromaticity or pulls inward - the luminance scale, the desaturation toward luminance,
   // CorrectOutOfRangeColor. MEASURED: it moved 0 of 867k samples, over paper white 80/203/250, peak 400/1000/4000 and
   // HighlightDechroma 0 and 1. Kept anyway: it carries its own branch, so an in-gamut pixel pays ~5 instructions, and
   // the three sibling MacLeod-Boynton ports all call it the same way.
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));

   // User saturation LAST, after the display map: the repo's convention. Lerp against BT.709 luminance, not
   // hue-preserving; 1.0 is a no-op. Scale-linear, so it needs no view of the engine fade the uber re-applies.
   return Saturation(hdr, LumaSettings.GameSettings.Saturation);
}

// The beyond-peak step DICE ends with, run alone: for a gain applied AFTER the map (the material's vignette, whose
// white point lifts blue x1.39 in linear). Desaturates over-peak channels toward white as the map would have; no hue
// restore follows it, so nothing can undo that (the magenta rim of the old hoisted vignette). LINEAR in and out.
float3 ME2_ContainToPeak(float3 mapped)
{
   float paperWhite, peakWhite;
   ME2_DisplayWhites(paperWhite, peakWhite);
   const DICESettings ds = ME2_DisplayDICESettings();
   const float smoothing = ds.GamutMappingSmoothing >= 0.0 ? ds.GamutMappingSmoothing : 1.0 - ds.ShoulderStart; // as DICE
   return BT2020_To_BT709(CorrectOutOfRangeColor(BT709_To_BT2020(mapped) * paperWhite, ds.Mirrored, true, ds.DesaturationVsDarkeningRatio, peakWhite, smoothing, ds.ProcessingColorSpace) / paperWhite);
}

// Display-mapped linear light -> the canvas' post-process space. The pre-scale comes later, in FinishME2Canvas, so the
// material's grain and offset between the two stay relative to the scene as vanilla had them.
float3 EncodeME2Canvas(float3 mapped)
{
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes and applies paper white.
   // GCT_POSITIVE, not MIRROR: no negative light may reach the canvas the gamma HUD blends onto.
   return linear_to_gamma(mapped, GCT_POSITIVE);
#else
   return mapped;
#endif
}

// The inverse of EncodeME2Canvas. MIRROR: the feedback materials between the uber and the material may leave negatives.
float3 DecodeME2Canvas(float3 encoded)
{
#if POST_PROCESS_SPACE_TYPE == 0
   return gamma_to_linear(encoded, GCT_MIRROR);
#else
   return encoded;
#endif
}

#if UI_DRAW_TYPE >= 2
// The scene's pre-scale, so the gamma-SDR HUD on this canvas lands at UIPaperWhite after composition rescales by it,
// in the canvas' post-process space: a pure-pow encode makes that multiply equal the linear one. Accessors, not
// LumaSettings (see MapME2ToDisplay), and still guarded: a zero would black the scene and leave the HUD.
float ME2_CanvasUIPrescale()
{
   const float prescale = GamePaperWhiteNits > 0.0 ? GamePaperWhiteNits / max(UIPaperWhiteNits, 1.0) : 1.0;
#if POST_PROCESS_SPACE_TYPE == 0
   return linear_to_gamma1(prescale, GCT_POSITIVE);
#else
   return prescale;
#endif
}
#endif

// The closing steps of every canvas writer, in the canvas' post-process space: UI pre-scale, dither, guard. `dither`
// is false only for a canvas that was already dithered once.
float3 FinishME2Canvas(float3 encoded, float2 ditherUV, bool dither)
{
#if UI_DRAW_TYPE >= 2
   encoded *= ME2_CanvasUIPrescale();
#endif

#if POST_PROCESS_SPACE_TYPE == 0
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR.
   [branch] if (dither && LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(encoded, ditherUV, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(encoded, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, ditherUV, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         encoded = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
#endif

   return Sanitize(encoded);
}

// User contrast, multiplicative around mid-gray (0.18, 1.0 = paper white), the repo's form (RenoDX_Contrast). Callers
// run it BEFORE the display map so DICE contains whatever it pushes up, and before the engine fade, which would
// otherwise land at k^C. [branch] on a cbuffer uniform: at the 1.0 default this is a bit-exact no-op. The pow is spelled
// out with a floored log2 so Contrast 0 on a black pixel is 0 * log2(1e-30) = 0 rather than pow(0, 0) = NaN; black
// stays black at every setting (0^C = 0).
float3 ME2_ApplyContrast(float3 color)
{
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      color = exp2(LumaSettings.GameSettings.Contrast * log2(max(color / MidGray, 1e-30))) * MidGray;
   }
   return color;
}
#endif // TONEMAP_TYPE >= 1

// The depth-driven DoF weight of UE3's blend, transcribed once: the uber and the standalone blend are the same
// engine arithmetic reading the same two cb4 rows, so a correction to it must not have to be made twice. The rows
// are passed in because each stage names them itself - row meaning is per pass in this game.
float ME2_DoFSceneWeight(float depth, float4 dofParams, float4 maxBlurNearFar)
{
   const float signedDistance = depth - dofParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * dofParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? maxBlurNearFar.y : maxBlurNearFar.x;
   const float blurAmount = min(PowUE3(max(normalizedDistance, 1e-4).xxx, dofParams.zzz).x, maxBlur); // 1e-4 as the original
   return saturate(1.0 - blurAmount);
}

// ---------- Stage 1: UberPostProcessBlend -> fp16 canvas, display-mapped and encoded (vanilla in SDR) ----------
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

float3 RunME2Uber(float2 blurUV, float2 sceneUV)
{
   // 1. Scene mix, exactly as vanilla: depth-driven DoF weight, bloom at x4, normalized by the weight sum.
   const float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   const float depth = scene.w; // UE3 packs scene depth in the fp16 alpha; read for the DoF weight, not written back

   const float sceneWeight = ME2_DoFSceneWeight(depth, DoFParams, DoFMaxBlur);

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
#if ME2_UBER_FILMIC
   // Keep the native per-channel filmic + grade as the colour reference: it carries no fade (outputScale = 1),
   // decoded to the linear light the vanilla canvas displays, so it already holds the vanilla channel skew and
   // whitening. Brightness alone is recovered, through a scalar gain taken from the curve's own tangent above the
   // pivot.
   const float3 sdr_ref = VanillaToLinear(GradeUE3(curved, true, 1.0));
   float3 recovered = ME2_RecoverFilmicBrightness(untonemapped, curved, sdr_ref);
#else
   // Hard-clip permutation: nothing to invert, so the grade run UNCLAMPED is the rebuild — vanilla-exact below the
   // clip and its own analytic continuation above it.
   float3 recovered = VanillaToLinear(GradeUE3(curved, false, 1.0));
   // The gate sits at the donor's own shoulder, and ONE constant serves both so they cannot drift (Witcher 2's
   // shape). Below it ReinhardPiecewise returns its input exactly, so the reference equals the target and the
   // transfer is a provable no-op; the test may be taken in BT.709 because every BT.2020 channel is a convex
   // combination of the BT.709 ones (the matrix rows sum to 1), so it can never exceed their max. SIMULATED in
   // float32 over the shipped model: above the shoulder the gate changes nothing at all, and below it
   // running the stage anyway costs up to 2.8e-3 relative on saturated near-gamut colours - pure round-off from the
   // RGB->LMS->MB round trip and the 1/t_max solve, for a result that should be the input. Raising it further is NOT
   // free: past the shoulder the donor starts bending real hue. Tuned constants, not user controls; path-to-white
   // stays with DICE at the display peak.
   const float hueReferenceShoulder = 1.5;
   [branch] if (max3(recovered) > hueReferenceShoulder)
   {
      // The hue stage alone runs in BT.2020 - the working space every other MacLeod-Boynton port in this repo uses,
      // and the one the canonical wrapper is built for. Only this island moves: the reconstruction above and
      // everything below stay BT.709. A HueOnly solve re-applies the TARGET's own purity, and the target is a
      // BT.709-authored UE3 grade, so the result stays inside BT.709 in practice (measured: 0 of 20000 sampled
      // highlights left the gamut; the display map's gamut clip contains any remainder). Purity is the
      // fraction of the distance from white to the GAMUT BOUNDARY, so the working space is part of the answer.
      const float3 target2020 = BT709_To_BT2020(recovered);
      // Hue reference: a per-channel Reinhard (ceiling 5, shoulder 1.5) of the colour itself, the reference our
      // RenoDX ports ship. Primaries decide which channel turns first, hence the skew magnitude.
      const float3 reference2020 = Reinhard::ReinhardPiecewise(target2020, 5.0, hueReferenceShoulder);
      // Hue 1 / chrominance 0, the canonical HueOnly contract: the reference supplies a hue DIRECTION and nothing
      // else, while the target keeps its own purity and its own T = L + M. T is MacLeod-Boynton's intensity anchor,
      // NOT photometric luminance - BT.709 Y does move here, most of all on saturated blues.
      recovered = BT2020_To_BT709(MacLeodBoynton::HueOnlyBT2020(target2020, reference2020));
   }
#endif

   // Neither permutation runs a colour stage after its reconstruction, deliberately: the FILMIC one rebuilds
   // brightness by a SCALAR ratio, so the vanilla chromaticity survives it untouched, and the hard-clip one has just
   // set its own hue above. The display map owns path-to-white.

   // User contrast stays in THIS pass because the engine fade below is a cb4 row only the uber can read, and contrast
   // must precede it. User saturation runs inside MapME2ToDisplay, after the display map.
   recovered = ME2_ApplyContrast(recovered);

   // Re-apply the engine fade linearly, after contrast (a no-op at rest), then map and encode: the canvas carries
   // vanilla's domain to the feedback materials drawn before the BioSceneEffect material, as MELE's stage 1 does.
   float3 outColor = EncodeME2Canvas(MapME2ToDisplay(Sanitize(recovered * outputScale)));
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
   // because the HDR path holds that row out of its references and re-applies it before the display map.
   if (LumaSettings.DevSetting04 > 0.5)
      return saturate(outputScale * 0.5);
#endif

   return Sanitize(outColor);
}

// End of stage 1. The cb4 aliases above name GAME CONTENT, whose meaning is per pass, and stages 2 and 3 re-use rows
// 8, 10 and 11 for other things - so retiring them here makes a leak a compile error instead of a naming convention.
#undef DoFParams
#undef DoFMaxBlur
#undef SceneShadowsAndDesat
#undef SceneInverseHighLights
#undef SceneMidTones
#undef SceneLuminanceWeights
#undef GammaColorScaleAndInverse
#undef GammaOverlayColor

// ---------- Stage 1b: FDOFAndBloomBlend -> the canvas, on chains that have no uber ----------
// The vanilla pass is the uber's scene mix and nothing else: same DoF weight, same x4 blur, same normalisation,
// alpha carrying the scene depth through. Transcribed from the cooked SM3 (18 slots): c0 -> cb4[8], c2 -> cb4[10],
// which is the SM3 c<N> = cb4[N+8] mapping the FX materials use, and the same two rows the gather reads.
//
// On the HDR path this pass has to finish what the uber would have: the Luma glow goes in where the vanilla one
// was, then the display map and the canvas encode, so a frame from one of these chains reaches the HUD and the
// present blit in the SAME domain as an uber frame. There is no grade here and no engine fade row - that row
// belongs to the uber - so neither is applied; the scene simply is not graded on these chains, as in vanilla.
#define BlendDoFParams PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define BlendMaxBlur   PsConstants[10] // .x max blur near, .y max blur far

float4 RunME2DofBloomBlend(float2 blurUV, float2 sceneUV)
{
   const float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);
   const float depth = scene.w; // UE3 packs scene depth in the fp16 alpha, and this pass passes it through

   const float sceneWeight = ME2_DoFSceneWeight(depth, BlendDoFParams, BlendMaxBlur);

   const float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   const float3 bloom = blurred.xyz * 4.0;
   const float3 vanillaMix = scene.xyz * sceneWeight + bloom;
   const float rcpWeightSum = rcp(max(blurred.w * 4.0 + sceneWeight, 1e-3)); // the original's own floor, not the uber's abs() guard

#if TONEMAP_TYPE >= 1
   // WHOSE FRAME IS THIS. The two in-game chains that carry this pass are attached as a VFX template's
   // `oFrameBufferEffect` (BioVFXTemplate, bPlayerOnly, intensity curve): BioVFX_DesignerCamera.DrunkCamera in
   // the bars and the Normandy medbay, and BioVFX_Crt_FlameThrower.VFX.Flame_Thrower_FB_VFX in SFXGame (the
   // third pair of chains is UnrealEd's UI/thumbnail ones). Whether BioWare's engine REPLACES the level's chain
   // with that one or MERGES it in is engine code, not package data, so this pass does not assume: LumaData says
   // what already ran this frame, exactly as the material asks.
   [branch] if (LumaData.GameData.UberRanThisFrame > 0.5)
   {
      // Merged after the uber: t0 is its display-mapped, encoded canvas. Vanilla's arithmetic is the whole pass
      // here - no second display map, and no second Luma glow, which the uber's composite already added.
      return float4(Sanitize(vanillaMix * rcpWeightSum), depth);
   }

   // Uber-less chain: this IS the frame's colour pass. The Luma glow joins the numerator where the vanilla one
   // sits, since the replaced gather has already stopped writing that one, and the display map and the canvas
   // encode happen here because nothing downstream will do them. The sample stays inside this branch: on a merged
   // frame the pyramid is not this pass's to add.
   const float3 mixed = ME2_ApplyContrast(Sanitize((vanillaMix + LumaBloom(sceneUV)) * rcpWeightSum) * LumaSettings.GameSettings.Exposure);
   return float4(EncodeME2Canvas(MapME2ToDisplay(mixed)), depth);
#else
   return float4(vanillaMix * rcpWeightSum, depth);
#endif
}

#undef BlendDoFParams
#undef BlendMaxBlur

// ---------- Stage 2: BioSceneEffect material -> the canvas the present blit takes, the frame's LAST pass ----------
// Its own register map: PsConstants[8] means something different here than on the uber.
#define MatOffset      PsConstants[8]  // .xyz additive offset, applied last in the encoded domain
#define MatScreenUV    PsConstants[9]  // .xy ScreenPosition scale, .wz its bias (note the swizzle order)
#define MatGrainOffset PsConstants[10] // .xy grain UV offset, added before the 0.5 scale
#define MatGrainBias   PsConstants[11] // .xy grain UV bias, added after it

SamplerState MatSceneSampler_s : register(s0);
SamplerState MatGrainSampler_s : register(s1);
Texture2D<float4> MatSceneTexture : register(t0); // the canvas the uber (and any feedback material after it) wrote
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
   // The vignette in vanilla's place either way: the encode is a pure pow, so multiplying by gamma_to_linear(vig) in
   // linear IS the vanilla multiply in the encoded domain. What t0 holds is NOT a given, so it is asked rather than
   // assumed - core refreshes LumaData on every replaced draw, not just the gamma pass's.
   const float3 vignette = gamma_to_linear(ME2_Vignette(grainUV), GCT_POSITIVE); // vig >= 0
   [branch] if (LumaData.GameData.UberRanThisFrame > 0.5)
   {
      // The uber left the display-mapped, encoded scene here. Its vignette white point lifts blue past peak
      // (vanilla's 8-bit canvas clipped it), so the map's own beyond-peak step contains it.
      outColor = EncodeME2Canvas(ME2_ContainToPeak(DecodeME2Canvas(scene) * vignette));
   }
   else
   {
      // Neither the uber nor the standalone DOFAndBloom blend ran: t0 is RAW linear scene, so decoding it as gamma
      // would crush the frame. Map it here instead - with the map LAST, DICE contains the vignette's white point
      // itself, which is what ME2_ContainToPeak redoes on the other branch. No Luma bloom: the pyramid is built at
      // the uber draw, which by definition did not happen. Exposure as the gamma pass applies it on a raw scene.
      // Only these two producers are covered, and none of the 384 cooked chains pairs the blend with this material:
      // the blend leaves an ENCODED canvas, which would take this branch, and the flag it would have to set cannot
      // be set there without suppressing a later uber draw on a merged chain.
      outColor = EncodeME2Canvas(MapME2ToDisplay(Sanitize(scene * LumaSettings.GameSettings.Exposure) * vignette));
   }
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

#if TONEMAP_TYPE >= 1
   return FinishME2Canvas(outColor, sceneUV, true);
#else
   // Vanilla wrote this into an 8-bit UNORM canvas, which clamped grain and offset along with the vignette's white
   // point; the fp16 canvas needs the clamp spelled out.
   return Sanitize(saturate(outColor));
#endif
}

// End of stage 2: the gamma pass reads rows 8, 10 and 11 as something else again.
#undef MatOffset
#undef MatScreenUV
#undef MatGrainOffset
#undef MatGrainBias

// ---------- Stage 3: UE3 FGammaCorrection -> a canvas, on whatever frame reaches it ----------
// Never yet seen drawing. What it reads depends on what ran before it this frame, which main.cpp reports through
// LumaData: nothing (t0 is the RAW fp16 scene and this pass is the whole grade), the uber (t0 holds its display-mapped,
// encoded output), or a finished canvas (the material, the material-less display map or an earlier gamma draw wrote
// it, pre-scaled). Same t0/s0 as the material.
#define GcColorScale   PsConstants[8]  // .xyz ColorScale
#define GcOverlayColor PsConstants[10] // .xyz OverlayColor, .w its blend weight (this pass's fade)
#define GcInverseGamma PsConstants[11] // .x inverse display gamma

float3 RunME2GammaCorrection(float2 sceneUV)
{
   const float3 scene = ApplyDgvMask(MatSceneTexture.Sample(MatSceneSampler_s, sceneUV), DgvMaskT0, DgvFillT0).rgb;

#if TONEMAP_TYPE >= 1
   const bool canvasFinished = LumaData.GameData.CanvasFinishedThisFrame > 0.5;
   const bool rawScene = !canvasFinished && LumaData.GameData.UberRanThisFrame <= 0.5;

   // This pass's input in vanilla's own domain. On a raw frame that is the linear scene, exposed as the uber would.
   // Otherwise the input already carries the uber's display encode and vanilla encodes it a SECOND time: reproduced,
   // not corrected, so the picture stays what the game shows. Only the clamps go.
   float3 input;
   if (rawScene)
   {
      input = scene * LumaSettings.GameSettings.Exposure; // no Luma bloom: the pyramid is injected at the uber draw
   }
   else
   {
      float3 canvas = scene;
#if UI_DRAW_TYPE >= 2
      if (canvasFinished)
         canvas /= ME2_CanvasUIPrescale();
#endif
#if POST_PROCESS_SPACE_TYPE == 0
      input = max(canvas, 0.0);
#else
      input = linear_to_gamma(canvas, GCT_POSITIVE);
#endif
   }

   // Vanilla's grade with the saturate() as a lower-only max(0), so highlights keep their real channel ratio, and the
   // overlay (this pass's fade) held OUT until after the creative sliders.
   float3 outColor = VanillaToLinear(PowUE3(max(input * GcColorScale.xyz, 0.0), GcInverseGamma.xxx));
   if (rawScene)
      outColor = ME2_ApplyContrast(outColor); // the uber already applied it on every other input

   // Vanilla lerps toward the overlay BEFORE its inverse-gamma pow and the display decode, so the fade is taken in that
   // domain: re-encoded through the inverse of both exponents, blended, decoded again. Below 1.0 this is the vanilla
   // fade exactly; above it, its continuation. Branched: .w is 0 whenever no fade runs.
   [branch] if (GcOverlayColor.w > 0.0)
   {
      const float invGamma = max(GcInverseGamma.x, 1e-4);
      const float3 encoded = PowUE3(linear_to_gamma(outColor, GCT_POSITIVE), (1.0 / invGamma).xxx);
      outColor = VanillaToLinear(PowUE3(lerp(encoded, GcOverlayColor.xyz, GcOverlayColor.w), invGamma.xxx));
   }

   // The uber's output and a finished canvas are already display-mapped, and the exponents above only pull them inward
   // (below peak), so they are re-encoded as is; only a finished canvas was dithered already. A raw scene is mapped here.
   if (rawScene)
      outColor = MapME2ToDisplay(outColor);
   return FinishME2Canvas(EncodeME2Canvas(outColor), sceneUV, !canvasFinished);
#else
   // Vanilla: pow(saturate(lerp(c * ColorScale, Overlay.rgb, Overlay.a)), InverseGamma).
   const float3 c = lerp(scene * GcColorScale.xyz, GcOverlayColor.xyz, GcOverlayColor.w);
   return PowUE3(max(saturate(c), 1e-4), GcInverseGamma.xxx);
#endif
}
