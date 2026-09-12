// Mass Effect 2 (2010) Luma HDR tonemap, split across the two colour passes that end a frame: the uber leaves
// LINEAR UNMAPPED light, the material vignettes and THEN display-maps. Order, perms and evidence: NOTES.md.

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

// HDR / vanilla. 1 = recover real highlights + DICE display map. 0 = vanilla clamped SDR reference (default while
// the port is being brought up: it is the reference every HDR change gets compared against).
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 0
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
// and both SDR paths are non-negative by construction, so in the uber it only ever acts on the hard-clip
// permutation's MacLeod-Boynton return from BT.2020 (0 of 20000 sampled highlights, but not guaranteed), and in
// the material on the signed grain and offset terms vanilla added after its own encode.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// The display map plus the user saturation: the LAST colour operations of the scene. Called from the MATERIAL pass,
// after the vignette. Takes/returns LINEAR light, 1.0 = paper white; reads only LumaSettings and NO cb4 row, so it
// can live in either pass.
float3 MapME2ToDisplay(float3 sceneHDR)
{
   // Through the Settings.hlsl accessors, not LumaSettings directly: they carry the HDR_TONEMAP_* overrides and the
   // devkit white level. Floors stay - outside DEVELOPMENT there is no unset fallback, and DICE divides by both.
   const float paperWhite = max(GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
   // The map runs in a BT.2020 working space and is round-tripped back below: gamut-correct handling of highly
   // saturated highlights, NOT a display-gamut expansion. The BT.709 alternative was a diagnostic, never a shipped
   // configuration, and is gone; this path is validated in _tools/me2_bridge/me2_hdr_final_check.py.
   sceneHDR = BT709_To_BT2020(sceneHDR);
   // Luminance in PQ (hue-preserving), then CORRECT_CHANNELS_BEYOND_PEAK_WHITE fades over-peak channels to white.
   // Identity below the shoulder (a third of peak), so diffuse content and the upstream sliders are untouched.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Highlight dechroma handed to DICE rather than run as our own pass afterwards. Core's is better placed: it ramps
   // on the MAX CHANNEL (by luminance a bright blue never triggers), exists only between ShoulderStart * PeakWhite
   // and peak (1/3 of peak for this type, so mid-tones cannot be touched), and runs INSIDE the containment in the
   // processing primaries. 0 = off for the OUTPUT but not the cost: DICE's guard carries no [branch], so fxc
   // flattens it for every pixel above the shoulder.
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   // DICE converts InOutColorSpace -> ProcessingColorSpace on entry and back on exit. We already converted above
   // and undo it below, so leaving the default CS_BT709 in makes it convert a SECOND time and run its compression,
   // its average()-based source luminance and its channel containment on doubly-narrowed primaries.
   ds.InOutColorSpace = CS_BT2020;
   float3 hdr = DICETonemap(sceneHDR * paperWhite, peakWhite, ds) / paperWhite;
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));

   // User saturation LAST, after the display map: the repo's convention. Lerp against BT.709 luminance, not
   // hue-preserving; 1.0 is a no-op. Scale-linear, so it needs no view of the engine fade the uber re-applies.
   return Saturation(hdr, LumaSettings.GameSettings.Saturation);
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
   // Exact gate: below 1.0 the reference equals the target (the Reinhard shoulder sits at 1.5 and the BT.2020
   // channels of a BT.709 colour never exceed its max), so the transfer is a provable no-op. It also pays for
   // itself - the model runs purity solves, so a real branch is worth taking on everything that is not a
   // highlight. Tuned constants, not user controls; path-to-white stays with DICE at the display peak.
   [branch] if (max3(recovered) > 1.0)
   {
      // The hue stage alone runs in BT.2020 - the working space every other MacLeod-Boynton port in this repo uses,
      // and the one the canonical wrapper is built for. Only this island moves: the reconstruction above and
      // everything below stay BT.709. A HueOnly solve re-applies the TARGET's own purity, and the target is a
      // BT.709-authored UE3 grade, so the result stays inside BT.709 in practice (measured: 0 of 20000 sampled
      // highlights left the gamut; the closing Sanitize is the containment for any remainder). Purity is the
      // fraction of the distance from white to the GAMUT BOUNDARY, so the working space is part of the answer.
      const float3 target2020 = BT709_To_BT2020(recovered);
      // Hue reference: a per-channel Reinhard (ceiling 5, shoulder 1.5) of the colour itself, the reference our
      // RenoDX ports ship. Primaries decide which channel turns first, hence the skew magnitude.
      const float3 reference2020 = Reinhard::ReinhardPiecewise(target2020, 5.0, 1.5);
      // Hue 1 / chrominance 0, the canonical HueOnly contract: the reference supplies a hue DIRECTION and nothing
      // else, while the target keeps its own purity and its own T = L + M. T is MacLeod-Boynton's intensity anchor,
      // NOT photometric luminance - BT.709 Y does move here, most of all on saturated blues.
      recovered = BT2020_To_BT709(MacLeodBoynton::HueOnlyBT2020(target2020, reference2020));
   }
#endif

   // Neither permutation runs a colour stage after its reconstruction, deliberately: the FILMIC one rebuilds
   // brightness by a SCALAR ratio, so the vanilla chromaticity survives it untouched, and the hard-clip one has just
   // set its own hue above. Evidence in NOTES.md; the display map owns path-to-white.
   float3 hdr = recovered;

   // User contrast BEFORE the display map so DICE contains whatever it pushes up: after the rolloff the slider
   // would escape the Scene Peak it just established, and nothing downstream re-contains it. Multiplicative around
   // mid-gray (0.18, 1.0 = paper white), the repo's form (RenoDX_Contrast). It stays in THIS pass because the engine
   // fade below is a cb4 row only the uber can read, and contrast must precede the fade: after it, a fade k would
   // land at k^C, a non-linear fade at any C != 1. [branch] on a cbuffer uniform: at the 1.0 default this is a
   // bit-exact no-op. The pow is spelled out with a floored log2 so Contrast 0 on a black pixel is
   // 0 * log2(1e-30) = 0 rather than pow(0, 0) = NaN; black stays black at every setting (0^C = 0), where the old
   // additive pivot lifted it to 0.18 * (1 - C). User saturation runs in the material, after the display map.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      hdr = exp2(LumaSettings.GameSettings.Contrast * log2(max(hdr / MidGray, 1e-30))) * MidGray;
   }

   // Re-apply the engine fade linearly, LAST, after contrast. At rest it is a no-op.
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
