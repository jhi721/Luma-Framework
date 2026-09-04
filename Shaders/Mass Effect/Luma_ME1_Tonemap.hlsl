// Mass Effect (2007) — Luma HDR tonemap replacement, split across the TWO colour passes that end a gameplay frame
// (devkit-measured order): UE3's UberPostProcessBlend (PS 0xAC8341E0) reads scene A (fp16, depth in alpha) plus the
// quarter-res DoF/bloom blur and writes scene B (fp16); the engine's copy pass 0x1E37D75B mirrors B into A; then
// FGammaCorrectionPixelShader (PS 0x17CE0932) reads A and writes the 8-bit canvas the HUD blends onto. Unlike MoH
// Airborne, where the two passes were alternatives, here they normally CHAIN, so the work is split:
//   - the uber replacement does the whole HDR block (MoHA / Borderlands GOTY shape: scene mix -> vanilla grade as the
//     SDR reference -> UpgradeToneMap -> DICE -> hue lock -> user sliders -> engine fade) and leaves LINEAR light
//     (1.0 = paper white) in the fp16 intermediate;
//   - the gamma-correction replacement applies that pass's own fade, the UI paper-white pre-scale and the gamma
//     encode (POST_PROCESS_SPACE_TYPE 0: the gamma-SDR HUD blends onto this canvas right after), dither and sanitize.
// The engine SKIPS the uber pass in elevators and some loading/UI scenes (measured: gamma-only frames), and the gamma
// pass then reads the RAW fp16 scene. main.cpp reports that through LumaData.GameData.UberRanThisFrame, and the gamma
// replacement runs the whole HDR block itself in that case (the MoHA "gamma correction alone" path), with its own
// ColorScale/InverseGamma as the SDR reference. Without this, hundreds of nits of raw scene reached the canvas as-is.
// The vanilla display gamma lives in the gamma-correction pass ALONE (measured cb4 rows: uber exponent 1.0, copy
// exponent 1.0, gamma-correction 0.625 = 1/DisplayGamma with the game's default DisplayGamma=1.6), so the intermediate
// is linear graded light and the look the player sees is pow(clip(grade), 0.625) on a 2.2 display. That exponent is
// needed by the grade for its SDR reference but lives in another pass: main.cpp reads it back at the gamma draw and
// hands it over through GameSettings.DisplayGammaInverse. TONEMAP_TYPE 0 reproduces vanilla in both passes.
// Two dgVoodoo transcription rules hold throughout: every texture fetch is followed by an `and`/`or` pair against b3
// (its D3D9 format emulation), and the entry points declare all 13 interpolators — VS->PS linkage is by REGISTER.

// clang-format off
// ORDER IS LOAD-BEARING — do not sort. The game-local "Includes/Common.hlsl" MUST come first: it defines
// LUMA_GAME_CB_STRUCTS (via GameCBuffers.hlsl) BEFORE any shared header pulls Settings.hlsl, so
// LumaSettings.GameSettings resolves to the real grade struct rather than the empty dummy.
#include "Includes/Common.hlsl"             // game-local: defines LumaGameSettings (grade sliders) before the LumaSettings cbuffer
#include "../Includes/Color.hlsl"
#include "../Includes/ColorGradingLUT.hlsl" // RestoreHueAndChrominance, SimpleGamutClip
#include "../Includes/DICE.hlsl"            // DICETonemap / DefaultDICESettings
#include "../Includes/Reinhard.hlsl"        // ReinhardTonemap / DefaultReinhardSettings (NeutralSDR)
#include "../Includes/Tonemap.hlsl"         // UpgradeToneMap
// clang-format on

#include "Includes/GameBindings.hlsl" // b3/b4, the dgVoodoo masks, ApplyDgvMask, PowUE3

// HDR / vanilla. 1 = recover real highlights + DICE display map (default). 0 = vanilla clamped SDR reference.
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// Run the display map in a BT.2020 working space (round-tripped back to BT.709). Gamut-correct handling of
// highly saturated highlights — NOT a display-gamut expansion.
#ifndef TONEMAP_IN_WIDER_GAMUT
#define TONEMAP_IN_WIDER_GAMUT 1
#endif

// UE3 UberPostProcess grade constants, at the register indices the 0xAC8341E0 disassembly reads them from. This UE3
// build leaves row 9 unused, so everything from the blur clamp on sits one row above MoH Airborne's map.
#define DoFParams                 PsConstants[8]  // .x focus distance, .y 1/range, .z falloff exponent
#define DoFMaxBlur                PsConstants[10] // .x max blur near, .y max blur far
#define SceneShadowsAndDesat      PsConstants[11] // .xyz shadows lift (subtracted), .w saturation weight
#define SceneInverseHighLights    PsConstants[12] // .xyz scene scale
#define SceneMidTones             PsConstants[13] // .xyz grade gamma
#define SceneLuminanceWeights     PsConstants[14] // .xyz desaturation luminance weights
#define GammaColorScaleAndInverse PsConstants[15] // .xyz output scale, .w output (inverse) gamma — measured 1.0: this pass does not encode
#define GammaOverlayColor         PsConstants[16] // .xyz tint offset

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState BlurredImageSampler_s : register(s1);
Texture2D<float4> SceneColorTexture : register(t0); // fp16 scene A; .w carries SCENE DEPTH, not alpha
Texture2D<float4> BlurredImage : register(t1);      // quarter-res DoF/bloom blur, stored pre-divided by 4 (scaled back x4 below)

// Luma HDR bloom pyramid mip 0 (Luma_Bloom_impl.hlsl, core DrawBloom): half-res LINEAR fp16, injected by main.cpp
// before the uber draw, slot must match kLumaBloomSlot. It REPLACES the game's bloom, so there is no double glow.
Texture2D<float4> LumaBloomTexture : register(t6);

// The Luma glow term, scene-referred LINEAR light, added where the vanilla glow was (before exposure, grade and the
// HDR display mapping). Gain = the engine's BloomScale (read back from the gather, 0.1 measured) x Bloom Intensity:
// the pyramid is energy-preserving, so this is exactly what makes intensity 1 vanilla strength. Half-res source, so
// one bilinear tap.
float3 LumaBloom(float2 sceneUV)
{
   if (LumaSettings.GameSettings.LumaBloomEnable <= 0.5)
      return 0.0;
   const float3 bloom = LumaBloomTexture.SampleLevel(SceneColorTextureSampler_s, sceneUV, 0.0).rgb;
   return max(0.0, bloom) * LumaSettings.GameSettings.BloomScaleLive * LumaSettings.GameSettings.BloomIntensity;
}

// Neutral SDR reference for the highlight-recovery delta.
float3 NeutralSDR(float3 color)
{
   ReinhardSettings settings = DefaultReinhardSettings();
   settings.by_luminance = true;
   return ReinhardTonemap(color, 100.f, 100.f, settings);
}

// The vanilla canvas value (the uber's own exponent is already inside GradeUE3, the gamma pass's is applied here),
// decoded to the linear light a 2.2 display shows for it. The MoHA/BL GOTY reference decode, with the exponent split
// across two passes: the game's DisplayGamma=1.6 look (pow 1.375 in linear) is part of the reference, not a mistake to
// undo.
float3 VanillaToLinear(float3 graded)
{
   // The inverse display gamma the gamma-correction pass applies, read back by main.cpp (measured 0.625 = 1/1.6, a few
   // frames of latency; main.cpp seeds the UE3 default 1/2.2). The fallback also covers an unbound b13, which reads as
   // zeros and would otherwise make this pow(x, 0) = 1 (TW2 precedent).
   const float displayGammaInverse = LumaSettings.GameSettings.DisplayGammaInverse;
   return gamma_to_linear(PowUE3(max(0.0, graded), (displayGammaInverse > 0.0 ? displayGammaInverse : 1.0 / 2.2).xxx));
}

// The game's grade, verbatim from the disassembly. `clampSDR`: true = vanilla saturate() path, false = unclamped
// (max 0), keeping the highlights' real channel ratio instead of a per-channel hue shift. `outputScale` is normally
// GammaColorScaleAndInverse.xyz; the HDR path passes 1 and re-applies the real scale afterwards.
float3 GradeUE3(float3 scene, bool clampSDR, float3 outputScale)
{
   // Head: shadows -> scale -> midtones.
   float3 c = scene - SceneShadowsAndDesat.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   c = c * SceneInverseHighLights.xyz;
   c = PowUE3(c, SceneMidTones.xyz);

   // Tail: desat + tint + output scale + (measured identity) gamma encode.
   float desat = dot(c, SceneLuminanceWeights.xyz);
   c = c * SceneShadowsAndDesat.www + GammaOverlayColor.xyz;
   c = c + desat;
   c = c * outputScale;
   c = clampSDR ? saturate(c) : max(0.0, c); // mul_sat in the original
   return PowUE3(c, GammaColorScaleAndInverse.www);
}

// Common sanitize for anything stored in the fp16 intermediate or the canvas: recovery, hue restore, encode and dither
// can each emit NaN, and a NaN in a unorm target reads back black. Bit test, not "x != x": that form gets optimized
// away. Also covers a -nan(ind) constant on a load fade, which vanilla's saturate flushes to 0.
float3 Sanitize(float3 c)
{
   c = IsNaN_Strict(c) ? 0.0 : c;
   return max(0.0, c);
}

#if TONEMAP_TYPE >= 1
// The shared HDR back half: highlight recovery -> display rolloff -> hue lock -> user sliders. Returns LINEAR light,
// 1.0 = paper white, with NO engine fade (the caller re-applies its pass's fade after the creative sliders: contrast
// pivots on mid-gray, so a fade before it would never reach black). Both references are LINEAR, decoded from the
// vanilla canvas value: `ungraded_sdr` is the clamped grade (the look), `hue_ref` the same grade run unclamped.
float3 FinishME1HDR(float3 untonemapped, float3 ungraded_sdr, float3 hue_ref)
{
   // 3. Recover the highlight luminance the SDR tonemap clipped, on top of the graded look.
   float3 neutral_sdr = NeutralSDR(untonemapped);
   float3 recovered = UpgradeToneMap(untonemapped, neutral_sdr, ungraded_sdr);

   // 4. Display rolloff to the user's peak/paper-white nits (DICE, hue-preserving by luminance). Both are floored:
   // DICE divides by them, and a NaN in a unorm target reads back black - it looks exactly like "the 3D disappeared".
   const float paperWhite = max(LumaSettings.GamePaperWhiteNits, 1.0) / sRGB_WhiteLevelNits;
   const float peakWhite = max(LumaSettings.PeakWhiteNits, paperWhite * sRGB_WhiteLevelNits) / sRGB_WhiteLevelNits;
#if TONEMAP_IN_WIDER_GAMUT
   recovered = BT709_To_BT2020(recovered);
#endif
   // Luminance in PQ (hue-preserving), then CORRECT_CHANNELS_BEYOND_PEAK_WHITE desaturates any channel still over
   // peak toward white — panels clip per channel, so an uncorrected saturated highlight clips with a hue shift.
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   float3 hdr = DICETonemap(recovered * paperWhite, peakWhite, ds) / paperWhite;
#if TONEMAP_IN_WIDER_GAMUT
   hdr = BT2020_To_BT709(SimpleGamutClip(hdr, true));
#endif

   // 5. Lock hue EXACTLY to the un-blown reference (no hue rotation with brightness). Hue 1.0 exact, chrominance 0.0
   // (composition gamut-maps); the reference is never achromatic, so full strength is safe.
   hdr = RestoreHueAndChrominance(hdr, hue_ref, 1.0, 0.0);

   // 6. Perceptual highlight dechroma: bright sources fade toward white as luminance approaches peak. Keeps
   // colored mid-highlights, whitens only the brightest.
   // [branch], not a movc: the condition is a cbuffer uniform (so the branch is perfectly coherent) and the block
   // is 10 instructions incl. div_sat + log + exp, which fxc otherwise runs at the ship default of OFF. +1 slot.
   const float highlightDechroma = LumaSettings.GameSettings.HighlightDechroma;
   [branch] if (highlightDechroma > 0.0)
   {
      // Exponent in [1, 0.05], never 0: pow(x,0) is 1 everywhere, i.e. a full-frame greyscale rather than a
      // highlight dechroma.
      float dcExp = lerp(1.0, 0.05, highlightDechroma);
      float dcWeight = saturate(pow(saturate(GetLuminance(hdr) / peakWhite), dcExp));
      hdr = Saturation(hdr, 1.0 - dcWeight);
   }

   // User saturation (shared helper: a lerp against BT.709 luminance, NOT hue-preserving). 1.0 = vanilla.
   hdr = Saturation(hdr, LumaSettings.GameSettings.Saturation);

   // User contrast: slope around 18% mid-gray (linear, 1.0 = paper white). 1.0 = vanilla. Excursions are
   // caught by the sanitize; > peak highlights are acceptable for a creative slider.
   const float midGray = 0.18;
   return (hdr - midGray) * LumaSettings.GameSettings.Contrast + midGray;
}
#endif

// ------------------------------------------------------------------------------------------------------------------
// Stage 1: UberPostProcessBlend -> fp16 intermediate (scene B). Output is LINEAR light, 1.0 = paper white, in HDR;
// the untouched vanilla value (whatever encoding its exponent gives it) in SDR.
// `sceneUV` is TEXCOORD1 (t0), `blurUV` is TEXCOORD0 (t1) — the original samples t0 with v6 and t1 with v5.
// ------------------------------------------------------------------------------------------------------------------
float3 RunME1Tonemap(float2 blurUV, float2 sceneUV, out float sceneDepth)
{
   // 1. Scene mix, exactly as vanilla: depth-driven DoF weight, bloom at x4, normalized by the weight sum.
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

   const float depth = scene.w; // UE3 packs scene depth in the fp16 alpha
   // Handed back so the entry point can write it into the target's alpha without sampling t0 a second time: the copy
   // pass carries that alpha to the gamma pass, where the SMAA predication CS reads it (see UberPost_0xAC8341E0).
   sceneDepth = depth;
   // Vanilla DoF weight. Includes/DofBloomGather.hlsl computes the same thing off the same rows, and the two are
   // deliberately NOT shared: they are independent transcriptions of two DIFFERENT vanilla shaders (there per tap,
   // here on one sample), each verified against its own disassembly. A shared helper could not take the rows with it
   // either - GameBindings.hlsl refuses to alias game-content rows because the meaning is per pass - so the only fact
   // that can actually drift, the row indices, would stay duplicated anyway.
   const float signedDistance = depth - DoFParams.x;
   const float normalizedDistance = saturate(abs(signedDistance) * DoFParams.y);
   const float maxBlur = (signedDistance >= 0.0) ? DoFMaxBlur.y : DoFMaxBlur.x;
   const float blurAmount = min(PowUE3(normalizedDistance.xxx, DoFParams.zzz).x, maxBlur);
   const float sceneWeight = saturate(1.0 - blurAmount);

   float4 blurred = ApplyDgvMask(BlurredImage.Sample(BlurredImageSampler_s, blurUV), DgvMaskT1, DgvFillT1);
   // Stored pre-divided by 4, hence the x4; vanilla's unorm view also capped it at 4.0, a cap the fp16 upgrade lifted.
   // NEVER scaled by BloomIntensity: DoF and bloom are SUMMED into this buffer.
   const float3 bloom = blurred.xyz * 4.0;
   const float weightSum = blurred.w * 4.0 + sceneWeight;

   // The Luma glow goes where the vanilla glow was: into the numerator, so the DoF weight sum divides it too (the
   // game's blurred buffer carries bloom + DoF and the uber normalises both together). In focus w = 1: plain additive.
   float3 untonemapped = scene.xyz * sceneWeight + bloom + LumaBloom(sceneUV);
   untonemapped *= (abs(weightSum) > 0.0) ? rcp(weightSum) : FLT_MAX; // rcp guard, as the original does

   // Scene exposure (multiplier), scene-referred / pre-grade; the SDR reference below derives from the same
   // `untonemapped`, so the grade tracks the exposure change.
   untonemapped *= LumaSettings.GameSettings.Exposure;

   // The grade's output scale doubles as the engine FADE (level start, cutscenes). It stays OUT of the HDR references
   // and is re-applied as a gain at the end: UpgradeToneMap's delta is ADDITIVE, so folding it in blows up the ratio.
   const float3 outputScale = GammaColorScaleAndInverse.xyz;

   // 2. The game's own grade (artistic intent), vanilla-exact, fade included. This IS the output on the TONEMAP_TYPE 0
   // path; on the HDR path it is only the DEVELOPMENT bisect's reference (fxc dead-strips it there, measured).
   const float3 sdr_vanilla = GradeUE3(untonemapped, true, outputScale);

#if TONEMAP_TYPE >= 1
   // References carry no fade (outputScale = 1); decoded to the linear light the vanilla canvas displays.
   float3 hdr = FinishME1HDR(untonemapped, VanillaToLinear(GradeUE3(untonemapped, true, 1.0)), VanillaToLinear(GradeUE3(untonemapped, false, 1.0)));
   // Re-apply the engine fade linearly, LAST, after the creative sliders. At rest it is a no-op.
   float3 outColor = hdr * outputScale; // LINEAR, 1.0 = paper white, into the fp16 intermediate
#else
   float3 outColor = sdr_vanilla; // vanilla encoding, whatever this pass's exponent is
#endif

   outColor = Sanitize(outColor);

#if DEVELOPMENT
   // Bring-up bisect (DEV only, all off = normal): DevSetting01 solid magenta (replacement is bound), DevSetting02
   // the reconstructed scene mix (t0/t1/UVs/masks read right), DevSetting03 the vanilla grade.
   if (LumaSettings.DevSetting01 > 0.5)
      return float3(1.0, 0.0, 1.0);
   if (LumaSettings.DevSetting02 > 0.5)
      return saturate(untonemapped);
   if (LumaSettings.DevSetting03 > 0.5)
      return saturate(sdr_vanilla);
#endif

   return outColor;
}

// ------------------------------------------------------------------------------------------------------------------
// Stage 2: FGammaCorrectionPixelShader (PS 0x17CE0932) -> the 8-bit canvas, the frame's LAST colour pass before the
// HUD. Reads the intermediate (scene A): stage 1's LINEAR HDR when the uber pass ran, the RAW fp16 scene when the
// engine skipped it. Its own register map.
// ------------------------------------------------------------------------------------------------------------------
#define GcColorScale   PsConstants[8]  // .xyz ColorScale
#define GcOverlayColor PsConstants[10] // .xyz OverlayColor, .w its blend weight (this pass's fade)
#define GcInverseGamma PsConstants[11] // .x inverse display gamma (measured 0.625 = 1/1.6, the game's DisplayGamma default)

// Vanilla-exact: saturate(lerp(scene * ColorScale, Overlay.rgb, Overlay.a)) then the inverse-gamma pow.
float3 GradeGCVanilla(float3 scene)
{
   float3 c = lerp(scene * GcColorScale.xyz, GcOverlayColor.xyz, GcOverlayColor.w);
   return PowUE3(saturate(c), GcInverseGamma.xxx);
}

// The pass's grade with the overlay (its fade) held OUT, as the SDR reference of the gamma-only frames. `clampSDR` as in
// GradeUE3. Output is the vanilla canvas encoding; VanillaToLinear-equivalent decode is gamma_to_linear (2.2 display).
float3 GradeGC(float3 scene, bool clampSDR)
{
   float3 c = scene * GcColorScale.xyz;
   c = clampSDR ? saturate(c) : max(0.0, c); // mad_sat in the original
   return PowUE3(c, GcInverseGamma.xxx);
}

// `sceneUV` is TEXCOORD0 (v5) — unlike UberPostProcessBlend, which reads the scene from TEXCOORD1.
float3 RunME1GammaCorrection(float2 sceneUV)
{
   float4 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorTextureSampler_s, sceneUV), DgvMaskT0, DgvFillT0);

#if TONEMAP_TYPE >= 1
   // The post-load flash (~0.35 s of a re-graded frame, measured 2026-09-04: mids x0.10, highlights x2.25, peak
   // 222 -> 953 nits) is NOT this branch mistaking a stale graded buffer for a raw one. That was tried and it did not
   // help: gating the branch on the intermediate's own alpha (the uber saturate()s its alpha into [0, 1], a raw scene
   // carries depth in Unreal units) changed nothing in game, so on those frames the buffer really does hold a raw
   // scene and this pass is right to grade it. See NOTES.md; do not re-implement the alpha test.
   float3 hdr;
   if (LumaData.GameData.UberRanThisFrame > 0.5)
   {
      // Stage 1 left LINEAR light here, its SDR reference already carrying this pass's display gamma: only this pass's
      // own scale applies, NOT the pow again.
      hdr = scene.xyz * GcColorScale.xyz;
   }
   else
   {
      // Gamma-only frame (elevators, some loading/UI scenes): the RAW fp16 scene, no DoF, no bloom, no uber grade. The
      // whole HDR block runs here with this pass's own grade as the SDR reference (MoHA's gamma-correction path). No Luma
      // bloom: the pyramid is only injected at the uber draw.
      float3 untonemapped = scene.xyz * LumaSettings.GameSettings.Exposure;
      hdr = FinishME1HDR(untonemapped, gamma_to_linear(GradeGC(untonemapped, true)), gamma_to_linear(GradeGC(untonemapped, false)));
   }
   // The fade, LAST, after the creative sliders (the same rule as the uber's output scale). Branched, not lerped:
   // the decode below is uniform (cb4 only) but fxc hoists it into the preamble of the frame's last fullscreen pass,
   // where .w is 0 on every frame with no fade running - measured 48 -> 42 executed instructions on the hot path.
   float3 outColor = hdr; // linear, 1.0 = paper white
   [branch] if (GcOverlayColor.w > 0.0)
   {
      // This pass's overlay colour is a pre-encode SDR value: a fade toward it happens in linear, decoded the way the
      // references are.
      const float3 overlay = gamma_to_linear(PowUE3(saturate(GcOverlayColor.xyz), GcInverseGamma.xxx));
      outColor = lerp(hdr, overlay, GcOverlayColor.w);
   }

   // --- Common tail: UI paper-white pre-scale + post-process-space encode ---
#if UI_DRAW_TYPE >= 2
   // Pre-scale so the gamma-SDR HUD on this same canvas lands at UIPaperWhite after composition rescales by it.
   // Guarded: an unset GamePaperWhiteNits would black the scene and leave the HUD, i.e. "the 3D disappeared".
   if (LumaSettings.GamePaperWhiteNits > 0.0)
      outColor *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   outColor = max(0.0, outColor); // negatives would turn into NaN in linear_to_gamma below
#if POST_PROCESS_SPACE_TYPE == 0
   // Store gamma so the game's gamma-space HUD blends like vanilla; composition decodes + applies paper white.
   outColor = linear_to_gamma(outColor);
   // Anti-banding dither in the stored gamma space (the core composition does not dither). Animated triangular
   // noise; sub-perceptual at bit depth 9.
   // [branch] for the same reason as the dechroma above: 17 instructions and two sincos, flattened otherwise, which
   // made turning Dithering off cost exactly as much as leaving it on.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      ApplyDithering(outColor, sceneUV, true, 1.0, DITHERING_BIT_DEPTH, LumaSettings.FrameIndex, true);
   }
#endif
#else
   // Vanilla: the intermediate holds the vanilla stage-1 value (or the raw scene); this pass's saturate and pow are the
   // original's, so both frame shapes come out vanilla-exact.
   float3 outColor = GradeGCVanilla(scene.xyz);
#endif

   return Sanitize(outColor);
}
