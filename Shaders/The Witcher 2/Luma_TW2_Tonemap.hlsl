// clang-format off
// ORDER MATTERS — do NOT let clang-format sort these. The game-local Common.hlsl MUST come first: it defines
// LumaGameSettings (via GameCBuffers.hlsl) BEFORE the shared Settings.hlsl (pulled in by Color.hlsl below)
// declares the LumaSettings cbuffer. If sorted after Color.hlsl, GameSettings becomes the empty fallback struct
// and every LumaSettings.GameSettings.* reference fails to compile (invalid subscript).
#include "Includes/Common.hlsl" // game-local: LumaGameSettings (grade sliders) — keep FIRST
#include "../Includes/Color.hlsl"
// clang-format on

// The Witcher 2 EE — tonemap ("exposure") pass SHARED IMPLEMENTATION (REDengine, DX9 via dgVoodoo D3D9->11).
// Holds the pass body as RunTonemap(); the per-hash wrapper files (Tonemap_0x<HASH>.ps_5_0.hlsl, one per
// permutation × dgVoodoo build) declare the full dgVoodoo interpolator set + main() and forward to it. No hash
// in this filename -> not matched/replaced directly; it is #included by the wrappers.
//
// Vanilla body transcribed VERBATIM (register-level) from the dgVoodoo-translated CSOs (0x91348C0F exposure,
// 0x00E31BF9 bloom bright-pass; DX9 origins 0xC5ADBC35/0xF01A691E), constants remapped DX9 cN -> cb4[N+8].
// The game's "tone map" (CEnvToneMappingParameters) is only an adaptive exposure multiply — NO curve, NO clamp.
// The adaptation is histogram auto-levels: black and white are luminance percentiles of the frame, and the
// exposure maps [black, white] to [0, 1] as one per-pixel ratio (hue kept), capped by m_maxMultiplier, so
// everything above the histogram white overshoots 1.0 unclamped.
// The whole post chain is fp16 and LINEAR up to the final grade, whose per-channel vMidtone power is the display
// encode (measured 1/2.2 in the neutral environment); the UI then blends src-alpha onto that gamma result.
// This pass stays bit-exact vanilla apart from the user Exposure; the Luma HDR output block runs at the end
// of the FINAL GRADE replacement instead, and fp16 keeps this pass's small overshoot alive for it.
//
// The SAME shader runs two roles per frame: a MAIN grade (RT == swapchain resolution, feeds post/UI/present)
// and an AUX draw (smaller RT, DoF/flare source). Both are vanilla here; main.cpp detects the role from the
// bound RT size, for the main-post-processing flag and to scope the exposure readback to one draw per frame.

// ---- Permutation map (set by the wrappers) -----------------------------------------------------------
// TM_BRIGHT_PASS 0 -> DX9 0xC5ADBC35: exposure + post-scale only, alpha passthrough, adaptation at t1/s1.
// TM_BRIGHT_PASS 1 -> DX9 0xF01A691E, the bloom bright-pass (CEnvBloomParameters): + threshold ramp, saturation
//                     and colour; writes alpha 1, adaptation at t2/s2, renders the half-res bloom/shaft source.
// Future static perms (DX9 0xA7D76FB1/0xEC6F063B, DX11 hashes unknown) read the exposure from
// PSC_LumRanges constants instead of the adaptation texture — add TM_STATIC here when they are dumped.
#if TM_BRIGHT_PASS
#define TM_T_ADAPT        t2
#define TM_S_ADAPT        s2
#define TM_ADAPT_MASK_AND cb3[48]
#define TM_ADAPT_MASK_OR  cb3[49]
#else
#define TM_T_ADAPT        t1
#define TM_S_ADAPT        s1
#define TM_ADAPT_MASK_AND cb3[46]
#define TM_ADAPT_MASK_OR  cb3[47]
#endif

Texture2D<float4> t0 : register(t0);              // scene (fp16, linear light, unclamped)
Texture2D<float4> t_adapt : register(TM_T_ADAPT); // 1x1 fp16 adaptation (.x black level, .z gain)

SamplerState s0_s : register(s0);
SamplerState s_adapt_s : register(TM_S_ADAPT);

cbuffer cb3 : register(b3)
{
   float4 cb3[77];
}
cbuffer cb4 : register(b4)
{
   float4 cb4[236];
}

#define LumWeights        cb4[58] // c50 PSC_LumWeights — luminance dot (dp4: folds scene alpha in)
#define LumRanges2        cb4[59] // c51 PSC_LumRanges2 — .x exposure cap (m_maxMultiplier), .y post-scale
#define BrightPassWeights cb4[60] // c52 vWeights — bright-pass luminance dot (dp4 folds alpha)
#define BrightPassParams  cb4[61] // c53 vParams — .x saturation, .y threshold, .z threshold range
#define BrightPassColor   cb4[62] // c54 vColor — bright-pass colour

// dgVoodoo texture-format fixup: every game texture read goes through (value & maskAnd) | maskOr (e.g. forcing
// alpha to 1 on X8 formats). The masks are dgVoodoo-internal state in cb3 — transcribed verbatim, do not simplify.
float4 DgVoodooTexFixup(float4 color, float4 mask_and, float4 mask_or)
{
   return asfloat((asuint(color) & asuint(mask_and)) | asuint(mask_or));
}

// dgVoodoo's guarded reciprocal: rcp with a huge-constant fallback at exactly 0 (movc in the translated CSO).
// The constant is 1e37 in every dump (l(9999999933815812510711506376257961984.0)) — keep it exact, since it is
// multiplied downstream and 1e38 overflows to inf ten times sooner.
float DgVoodooRcp(float x)
{
   return (abs(x) > 0.0) ? (1.0 / x) : 1e37;
}

// The tonemap body. v5 = TEXCOORD0 (scene UV in .xy — the only interpolator the pass uses).
// Returns the exposed linear color (alpha: scene passthrough on the exposure perm, 1 on the bright-pass perm).
float4 RunTonemap(float4 v5)
{
   // --- vanilla body (verbatim transcription) ---
   float4 adaptation = t_adapt.SampleLevel(s_adapt_s, float2(0.0, 0.0), 0.0);
   adaptation = DgVoodooTexFixup(adaptation, TM_ADAPT_MASK_AND, TM_ADAPT_MASK_OR);
   float4 scene = t0.Sample(s0_s, v5.xy);
   scene = DgVoodooTexFixup(scene, cb3[44], cb3[45]);

#if TONEMAP_TYPE == 1
   // User exposure (1 = vanilla): a real scene multiplier before the adaptive exposure reads it, so the
   // bright-pass threshold keeps tracking the same relative levels as vanilla.
   scene.rgb *= LumaSettings.GameSettings.Exposure;
#endif

   // exposure = min(gain * max(lum - black, 0) / max(lum, 1e-6), cap): luminance levels [black, white] -> [0, 1].
   // The adaptation texel holds (black, white, gain = 1 / max(white - black, 0.01)).
   float lum = dot(LumWeights, scene);
   float inv_lum = DgVoodooRcp(max(1e-6, lum));
   float exposure = min(adaptation.z * max(lum - adaptation.x, 0.0) * inv_lum, LumRanges2.x);

   float3 scaled = scene.rgb * exposure * LumRanges2.y;

#if TM_BRIGHT_PASS
   // Bloom bright-pass: threshold ramp on the exposed luminance, saturation around it, colour.
   float brightRamp = saturate((dot(BrightPassWeights, float4(scaled, scene.a)) - BrightPassParams.y) * DgVoodooRcp(BrightPassParams.z));
   float lum_scaled = dot(BrightPassWeights.xyz, scaled);
   float3 saturated = (scaled - lum_scaled) * BrightPassParams.x + lum_scaled;
   float3 vanillaColor = max(saturated, 0.0) * brightRamp * BrightPassColor.rgb;
   const float vanillaAlpha = 1.0;
#else
   float3 vanillaColor = scaled;
   const float vanillaAlpha = scene.a;
#endif

   // NOTE: the Luma HDR output block (expansion + DICE + UI pre-scale + dither) does NOT live here:
   // the game runs a FINAL GRADE pass after this one (FXAA + colour balance + split toning + vignette,
   // FinalGrade_0xDE5CF9CD.ps_5_0.hlsl) whose highlight split tone would crush anything above 1.
   // This pass therefore stays bit-exact vanilla (fp16 keeps the small unclamped overshoot alive),
   // plus the user Exposure above; the HDR block runs at the end of the final grade replacement.

   return float4(vanillaColor, vanillaAlpha);
}
