// Shared stage-1 output tail, included inside each main() after it defines gradedHDR, sdrGamma, v0, v1, o0, o1.
// The bodies already decode sdrGamma for gradedHDR (MELE_NativeColorGradedHDR, or 0x225A8330's hue donor), but this
// tail decodes it again on purpose: consuming that decode reschedules the vanilla curve on 0x2754F750 and 0x69F03340
// in Publishing, turning r0*w + (1 - e) into (r0*w - e) + 1 at the same instruction count. Float32 addition is not
// associative and that line is native transcription, so the duplicate decode stays (measured). fxc still picks either
// form per permutation from unrelated code (the exposure metering moved some ME1LE/ME2LE ones): at most
// 1 ULP, and only a `precise` on the curve would pin it, which spreads to the whole expression.
// Includer macros, all defaulting to off: TM_VIGNETTE_TYPE (0 none, 1 radial power, 3 ME3LE smoothstep), TM_HAS_GRAIN,
// TM_ALPHA_LUMA (native ME3LE output luma to alpha).
#ifndef TM_VIGNETTE_TYPE
#define TM_VIGNETTE_TYPE 0
#endif
#ifndef TM_HAS_GRAIN
#define TM_HAS_GRAIN 0
#endif
#ifndef TM_ALPHA_LUMA
#define TM_ALPHA_LUMA 0
#endif

float3 sdrLinear = gamma_to_linear(sdrGamma, GCT_MIRROR);

// Native vignette, hoisted ahead of the tonemap and expressed in linear so PsychoV31 absorbs its blue-tinted white
// point instead of the frame leaving stage 1 above Scene Peak. linear_to_gamma is a signed pure pow, so this is
// exactly the vanilla multiply in the encoded domain. Grain and dither stay in gamma below, where hoisting would
// amplify shadow noise.
float3 vigLinear = 1.0;
float3 vigGamma = 1.0; // The native gamma-domain multiplier, for the exposure metering below.
#if TM_VIGNETTE_TYPE == 1
// The slider scales only radial darkening, so zero intensity does not alter the native white point tint.
{
   float2 vc = float2(-0.5, -0.5) + v0.zw;
   vc = float2(0.832050323, 0.554700196) * vc;
   float vd = max(9.99999975e-05, dot(vc, vc));
   vd = exp2(3.25 * log2(vd));
   vd = 1.0 - vd;
   vd = exp2(TM_VIG_POW * log2(max(vd, 9.99999975e-05)));
   const float3 whitePoint = TM_VIG_FLOOR + 1.0; // Floor plus center value.
   float3 vig = TM_VIG_FLOOR + vd;
   vig = whitePoint * lerp(1.0, vig / whitePoint, LumaSettings.GameSettings.VignetteIntensity);
   vigGamma = vig;
   vigLinear = gamma_to_linear(vig, GCT_MIRROR);
}
#elif TM_VIGNETTE_TYPE == 3
// Native ME3LE smoothstep vignette; scale only radial darkening, not its blue-tinted white point.
{
   float2 vc = v0.zw * ScreenUVScaleBias.xy + ScreenUVScaleBias.zw;
   vc = float2(-0.5, -0.5) + vc;
   vc = float2(0.832050323, 0.554700196) * vc;
   float vd = dot(vc, vc);
   vd = saturate(4.0 * (vd - 0.0500000007));
   float vs = (3.0 - 2.0 * vd) * vd * vd; // smoothstep(0, 1, vd).
   const float3 whitePoint = float3(1.01036298, 1.00000572, 1.16309249);
   float3 vig = whitePoint - vs * LumaSettings.GameSettings.VignetteIntensity;
   vigGamma = vig;
   vigLinear = gamma_to_linear(vig, GCT_MIRROR);
}
#endif

float3 postProcessedColor;
if (LumaSettings.DisplayMode == 1) // HDR
{
   // gradedHDR arrives as Game-Paper-White-relative linear HDR. See "PsychoVDisplayMap". Its peak is Scene Peak over
   // Game Paper White, a unit conversion, not an application of Game Paper White: the two passes that apply that scale
   // to an outgoing frame are Output_0x0765601C and Video_0x7B5C59DF. Grain, dither, and RCAS can still push the result
   // ~2% past Scene Peak; accepted.
   postProcessedColor = PsychoVDisplayMap(gradedHDR * vigLinear,
                                          LumaSettings.GameSettings.Contrast,
                                          LumaSettings.GameSettings.Saturation,
                                          LumaSettings.GameSettings.HighlightDechroma); // Game-Paper-White-relative.
}
else // SDR still uses the scRGB swapchain; sdrLinear is the exact native grade.
{
   // No tonemap sits between here and the encode, so the linear vignette equals the vanilla gamma multiply.
   postProcessedColor = sdrLinear * vigLinear;
}

postProcessedColor = IsNaN_Strict(postProcessedColor) ? 0.0 : postProcessedColor;
// Stage 1 owns the non-negative floor: no negative RGB may reach the gamma HUD or the swapchain, so wide-gamut
// (negative BT.709) components the hue transfer kept end here.
postProcessedColor = max(0.0, postProcessedColor);

// Encode gamma(scene / R), R = UI Paper White / Game Paper White. The native gamma HUD blends before stage 2
// (Output_0x0765601C), which restores R and applies Game Paper White once to the combined frame.
const float uiPaperWhiteRelativeToGame = MELE_GetUIPaperWhiteRelativeToGame();
o0.xyz = linear_to_gamma(postProcessedColor / max(uiPaperWhiteRelativeToGame, 1e-4), GCT_MIRROR);

float grain = 0.0;
#if TM_HAS_GRAIN
// Native film grain stays in gamma after encoding, as vanilla did, scaled by the user control.
{
   float2 nuv = v0.zw * NoiseTextureOffset.xy + NoiseTextureOffset.zw;
   float n = NoiseTexture.Sample(NoiseTextureSampler_s, nuv).x;
   grain = (n - 0.5) * FilmGrain_Scale * LumaSettings.GameSettings.FilmGrainIntensity;
   o0.xyz = o0.xyz + grain;
}
#endif

o0.xyz = max(0.0, o0.xyz); // Grain may make shadows negative; retain HDR headroom above 1.

// linear_to_gamma is a pure pow, so dividing by R in linear equals dividing by gamma(R) in the encoded domain.
// Used by the native SDR clamp at the end of this tail.
const float rEncoded = linear_to_gamma1(uiPaperWhiteRelativeToGame, GCT_MIRROR);

// Native eye adaptation meters the vanilla final gamma scene: the native SDR grade times the gamma vignette, plus
// grain, clamped as vanilla's mov_sat did, HUD not yet present. Built from the native result, not from o0, so the HDR
// reconstruction, the display map and both Paper White controls stay out of exposure in every Display Mode.
{
   float3 metered = saturate(sdrGamma * vigGamma + grain);
   float adaptLuma = dot(metered, float3(0.212670997, 0.715160012, 0.0721689984));
   o1 = 0.25 * log2(adaptLuma * 15 + 1);
}

// Anti-banding dither, one step of the output quantizer: 8-bit in SDR, 10-bit BT.2020 PQ in HDR and SDR on HDR.
if (LumaSettings.GameSettings.Dithering > 0.5)
{
   if (LumaSettings.DisplayMode == 0)
      ApplyDithering(o0.xyz, v1.xy, true, 1.0, 8u, LumaSettings.FrameIndex, true);
   else
   {
      const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
      float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(o0.xyz, GCT_MIRROR) * pqScale), GCT_MIRROR);
      ApplyDithering(pq, v1.xy, true, 1.0, 10u, LumaSettings.FrameIndex, true);
      o0.xyz = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
   }
}

#if TM_ALPHA_LUMA
o0.w = dot(o0.xyz, float3(0.298999995, 0.587000012, 0.114)); // Preserve native ME3LE luma alpha, unclamped as native.
#else
o0.w = 0;
#endif

// Vanilla ended stage 1 with mov_sat into an 8-bit UNORM target, so the gamma HUD always composited against a
// value capped at white; the Luma intermediate is fp16, so the cap must be explicit. Applied last, after the
// alpha dot product, to keep vanilla's clamp position, and in the transport-normalized domain so neither Paper
// White control moves it. HDR keeps its headroom.
if (LumaSettings.DisplayMode != 1)
{
   o0.xyz = min(o0.xyz, 1.0 / max(rEncoded, 1e-4));
}
