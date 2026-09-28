// Yakuza Remastered Collection — Luma HDR replacement for the engine's "color correct" family (CScreenEffectColorCorrect).
//
// The engine has no tone curve: every material writes `color * exposure` (gamma space, 2.2) straight into the 8-bit
// b8g8r8a8_unorm scene RT, whose UNORM format hard-clips it at 1.0. The first full-screen pass that reads the finished
// scene is this grade, so it is where Luma rebuilds SDR and HDR from the (now fp16) scene.
//
// One implementation serves every pixel shader of the family (Y3R/Y4R 34, Y5R 35), selected by defines:
//   CCR_HLS  HLS hue/lightness/saturation stage (hue -> RGB through the gradient texture t1)
//   CCR_SC   saturation/contrast stage (inside HLS when CCR_HLS, else in BT.601 Y'CbCr)
//   CCR_GM   per-zone gamma (pow in the 2.2 domain, min 1)
//   CCR_GI   per-zone gain (mul_sat)
//   CCR_OF   per-zone offset (add_sat)
//   CCR_MASK fx_ccr_*_mask: alpha test + lerp to the graded color by the mask texture t2
//   CCR_COLLECTION ps_color_collection: 5-tap cross blur-sharpen before the grade
//   CCR_BRIGHTNESS_AFTER_CONTRAST (Y5R builds) the brightness offset cb5[0].y is added after the contrast stage, not before
//   CCR_NO_ZONES   fx_ccr_*_mask_n (Y5R): no shadow/midtone/highlight terms, only the global cb5[0..3] controls
//   CCR_MATERIAL_CURVE (Y5R builds) the scene comes from the extended Y5R material tone curve: the vanilla SDR scene is
//                  rebuilt through YRC_Y5VanillaMaterialCurve instead of the UNORM clamp
// The zone weights (shadow/mid/highlight) always come from the mean of the stage input color. Every stage is
// transcribed operand-for-operand from the disassembly (ps_ccr_* in the games' sh_* shader archives).
//
// SDR path: the exact vanilla grade on the saturated scene (the UNORM RT clamp, emulated).
// HDR path (canon for hard-clip games, BL GOTY precedent): the same grade as an extended function (upper clamps
// become max(0); HLS runs on the color normalized by its max channel since HLS is undefined above 1; zone weights
// from the saturated mean) -> linear -> soft ReinhardPiecewise hue reference in BT.2020 (Y5R: the gated vanilla SDR) -> MacLeod-Boynton hue
// emulation -> DICE to peak. The user grade (Exposure, Contrast before DICE, Highlights Desaturation inside it,
// Saturation after it) is HDR only. Output stays in gamma space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) so the HUD,
// the AA (CMAA2/FXAA or Luma's SMAA), CAS and render_fb downstream behave as in vanilla.

// clang-format off
#include "Common.hlsl"
#include "../../Includes/ColorGradingLUT.hlsl" // SimpleGamutClip
#include "../../Includes/DICE.hlsl"
#include "../../Includes/Reinhard.hlsl"
#include "MacLeodBoynton.hlsl"                 // Byte-identical copy of the BL GOTY production model: do not edit here, sync it from "Borderlands GOTY Enhanced/Includes"
// clang-format on

#ifndef CCR_HLS
#define CCR_HLS 0
#endif
#ifndef CCR_SC
#define CCR_SC 0
#endif
#ifndef CCR_GM
#define CCR_GM 0
#endif
#ifndef CCR_GI
#define CCR_GI 0
#endif
#ifndef CCR_OF
#define CCR_OF 0
#endif
#ifndef CCR_MASK
#define CCR_MASK 0
#endif
#ifndef CCR_COLLECTION
#define CCR_COLLECTION 0
#endif
#ifndef CCR_BRIGHTNESS_AFTER_CONTRAST
#define CCR_BRIGHTNESS_AFTER_CONTRAST 0
#endif
#ifndef CCR_NO_ZONES
#define CCR_NO_ZONES 0
#endif
#ifndef CCR_MATERIAL_CURVE
#define CCR_MATERIAL_CURVE 0
#endif

cbuffer cb5 : register(b5)
{
   float4 cb5[16];
}
#if CCR_MASK
cbuffer cb4 : register(b4)
{
   float4 cb4[1];
}
cbuffer cb11 : register(b11)
{
   uint4 cb11[1];
}
#endif

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0); // scene
#if CCR_HLS
SamplerState s1_s : register(s1);
Texture2D<float4> t1 : register(t1); // hue (x) / saturation (y) gradient
#endif
#if CCR_MASK
SamplerState s2_s : register(s2);
Texture2D<float4> t2 : register(t2); // mask (r)
#endif

// Shadow / midtone / highlight weights from the mean of the vanilla (SDR) stage input. The HDR grade takes them from it
// too: above 1 the mean of an over-range saturated color would pick highlight weights vanilla never applied (black under
// inverted cutscene contrast), and the polynomials turn around.
float3 ZoneWeights(float3 sceneSDR)
{
#if CCR_NO_ZONES
   return 0.0; // Folds every zone term away, leaving the global controls
#else
   float m = saturate(dot(sceneSDR, 1.0 / 3.0));
   float im = 1.0 - m;
   return saturate(float3(im * im, 1.0 - im * im - m * m, m * m));
#endif
}

#if CCR_SC
// Contrast around 0.5 (lightAdjust) and the brightness offset cb5[0].y, applied to the HLS lightness coordinate (hls perms) or to luma.
float ContrastBrightness(float l, float lightAdjust)
{
   // Cutscene grades reach the pole at lightAdjust = 0.5: vanilla's inf saturated to white, but the extended HDR grade
   // would carry it into a NaN (black), so the divisor stops just short of 0.
   float d = lightAdjust * -2.0 + 1.0;
   d = abs(d) < 1e-4 ? (d < 0.0 ? -1e-4 : 1e-4) : d;
#if CCR_BRIGHTNESS_AFTER_CONTRAST
   return l * (1.0 / d) - lightAdjust + cb5[0].y;
#else
   return (l + cb5[0].y) * (1.0 / d) - lightAdjust;
#endif
}
#endif

#if CCR_HLS
// RGB -> HLS, optional saturation/contrast, HLS -> RGB through t1. Input in [0,1].
float3 HLSStage(float3 c, float3 w)
{
   float2 rg = (c.g < c.r) ? float2(c.r, c.g) : float2(c.g, c.r);
   float mn = min(c.b, rg.y);
   float mx = max(c.b, rg.x);
   float d = mx - mn;
   float sum = mn + mx;

   float3 hueTerms = float3(c.g - c.b, c.b - c.r, c.r - c.g) / d + cb5[0].x + float3(0.0, 2.0, 4.0);
   float selR = (c.r == mx) ? 1.0 : 0.0;
   float selG = ((c.g == mx) ? 1.0 : 0.0) * ((c.r != c.g) ? 1.0 : 0.0);
   float selB0 = ((c.b == mx) ? 1.0 : 0.0) * ((c.r != c.b) ? 1.0 : 0.0);
   float selB = ((c.g != c.b) ? 1.0 : 0.0) * selB0;
   float hue = hueTerms.y * selG + hueTerms.z * selB + hueTerms.x * selR;

   float satHigh = d / (2.0 - sum);
   float satLow = d / sum;
#if CCR_SC
   float satAdjust = dot(cb5[4].xyz, w) + cb5[0].z;
   float lightAdjust = dot(cb5[8].xyz, w) + cb5[0].w;
   float hlsLightness = ContrastBrightness(sum * 0.5, lightAdjust);
   float hlsSaturation = (hlsLightness < 0.5) ? satLow : satHigh;
   hlsSaturation = hlsSaturation * satAdjust + hlsSaturation;
#else
   float hlsLightness = sum * 0.5 + cb5[0].y;
   float hlsSaturation = (hlsLightness < 0.5) ? satLow : satHigh;
#endif
   hlsLightness = saturate(hlsLightness);
   hlsSaturation = saturate(hlsSaturation);

   float3 hueColor = t1.Sample(s1_s, float2(hue * (1.0 / 6.0), hlsSaturation)).rgb;
   float3 high = (1.0 - hueColor) * ((hlsLightness - 0.5) * 2.0) + hueColor;
   float3 low = -hueColor * ((0.5 - hlsLightness) * 2.0) + hueColor;
   return saturate((0.5 >= hlsLightness) ? low : high);
}
#endif

// The whole vanilla grade. `clampSDR` true = verbatim; false = the extended HDR function (see header). `w` = ZoneWeights.
float3 Grade(float3 c, float3 w, bool clampSDR)
{

#if CCR_HLS
   // HLS is only defined inside [0,1]: grade the color normalized by its max channel and restore the scale
   // (scale = 1, i.e. exact vanilla, whenever the color is in range).
   const float scale = clampSDR ? 1.0 : max(1.0, max3(c));
   c = HLSStage(c / scale, w) * scale;
#elif CCR_SC
   float satAdjust = dot(cb5[4].xyz, w) + cb5[0].z;
   float lightAdjust = dot(cb5[8].xyz, w) + cb5[0].w;
   float y = ContrastBrightness(dot(float3(0.299, 0.587, 0.114), c), lightAdjust);
   float cb = dot(float3(-0.16874, -0.33126, 0.5), c) * (satAdjust + 1.0);
   float cr = dot(float3(0.5, -0.41869, -0.08131), c) * (satAdjust + 1.0);
   c = float3(y + 1.402 * cr, y - 0.34414 * cb - 0.71414 * cr, y + 1.772 * cb);
   c = clampSDR ? saturate(c) : max(0.0, c);
#endif

#if CCR_GM
   float3 gamma = float3(dot(cb5[5].xyz, w), dot(cb5[9].xyz, w), dot(cb5[13].xyz, w)) + cb5[1].xyz;
   const float3 powered = exp2(log2(c) * (gamma * 0.454545));
   // Values up to 1 keep the vanilla clip. Above 1 (HDR only) a cutscene flash's non-positive exponent would pull them
   // back down (or 0 up to inf): they are held at vanilla's white instead.
   c = (clampSDR || c <= 1.0) ? min(powered, 1.0) : max(powered, 1.0);
#endif

#if CCR_GI
   float3 gain = (1.0 - cb5[6].xyz * w.x) * cb5[2].xyz;
   gain *= 1.0 - cb5[10].xyz * w.y;
   gain *= 1.0 - cb5[14].xyz * w.z;
   c = clampSDR ? saturate(c * gain) : max(0.0, c * gain);
#endif

#if CCR_OF
   float3 offset = cb5[11].xyz * w.y;
   offset = cb5[7].xyz * w.x + offset;
   offset = cb5[15].xyz * w.z + offset;
   offset += cb5[3].xyz;
   c = clampSDR ? saturate(c + offset) : max(0.0, c + offset);
#endif

   return c;
}

float4 ColorCorrect(float colorAlpha, float4 uv)
{
#if CCR_MASK
   YRC_AlphaTest(colorAlpha, cb11[0].z);
#endif

   float3 scene = t0.Sample(s0_s, uv.xy).rgb;
#if CCR_COLLECTION
   // Anti-flicker cross blur, then sharpened back where the neighborhood differs (verbatim).
   float3 cross = t0.Sample(s0_s, uv.xy + float2(-0.000672, -0.000681)).rgb;
   cross += t0.Sample(s0_s, uv.xy + float2(0.000672, 0.000681)).rgb;
   cross += t0.Sample(s0_s, uv.xy + float2(0.000383, -0.001194)).rgb;
   cross += t0.Sample(s0_s, uv.xy + float2(-0.000383, 0.001194)).rgb;
   float3 delta = cross * 0.25 - scene;
   scene += min(dot(delta, delta) * 16.0, 1.0) * 0.8 * delta;
#endif

   // Vanilla: the scene RT was UNORM, so the grade only ever saw [0,1].
#if CCR_MATERIAL_CURVE
   const float3 sceneSDR = YRC_Y5VanillaMaterialCurve(float4(scene, 0.0)).rgb;
#else
   const float3 sceneSDR = saturate(scene);
#endif
   const float3 w = ZoneWeights(sceneSDR);
   float3 gradedSDR = Grade(sceneSDR, w, true);
#if CCR_MASK
   const float maskWeight = t2.Sample(s2_s, uv.zw).r * cb4[0].x;
   gradedSDR = (gradedSDR - sceneSDR) * maskWeight + sceneSDR;
#endif

#if TONEMAP_TYPE >= 1
   const float3 sceneHDR = max(0.0, scene);
   float3 gradedHDR = Grade(sceneHDR, w, false);
#if CCR_MASK
   gradedHDR = (gradedHDR - sceneHDR) * maskWeight + sceneHDR;
#endif
   // D3D min returns the non-NaN operand: a leftover NaN or inf lands at peak like vanilla's white store, not at black.
   gradedHDR = min(gradedHDR, 1e4);
   float3 extendedLinear = gamma_to_linear(gradedHDR, GCT_POSITIVE) * LumaSettings.GameSettings.Exposure;
   // User contrast before the display map so DICE contains what it pushes up (BL GOTY form). [branch] keeps 1.0 a
   // bit-exact no-op; the floored log2 keeps Contrast 0 on black at 0 instead of pow(0, 0) = NaN.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      extendedLinear = exp2(LumaSettings.GameSettings.Contrast * log2(max(extendedLinear / MidGray, 1e-30))) * MidGray;
   }
   float3 extendedBT2020 = BT709_To_BT2020(extendedLinear);

#if CCR_MATERIAL_CURVE
   // Y5R's highlights never clipped: its materials bend each channel through the soft vanilla curve, so the vanilla graded
   // SDR is the exact hue reference. Where it whitens its hue is unstable, so it fades to this color's own hue by its HSV
   // saturation coordinate, (max - min) / max of the gamma-encoded color (0.05..0.3, a BotW-style gate); both are
   // max-normalized so the lerp blends hues, not magnitudes.
   const float3 vanillaSDR = max(0.0, gradedSDR);
   const float vanillaMax = max3(vanillaSDR);
   const float hueGate = smoothstep(0.05, 0.3, vanillaMax > 0.0 ? (vanillaMax - min3(vanillaSDR)) / vanillaMax : 0.0);
   const float3 vanillaBT2020 = BT709_To_BT2020(gamma_to_linear(vanillaSDR, GCT_POSITIVE));
   float3 hueReferenceBT2020 = lerp(extendedBT2020 / max(max3(extendedBT2020), 1e-6), vanillaBT2020 / max(max3(vanillaBT2020), 1e-6), hueGate);
#else
   // Soft hue reference: per-channel ReinhardPiecewise(5, 1.5) in BT.2020 bends saturated highlights the way the
   // vanilla per-channel clip did, without its whitening; MacLeod-Boynton keeps only its hue direction.
   float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5);
#endif
   float3 diceInBT2020 = MacLeodBoynton::HueOnlyBT2020(extendedBT2020, hueReferenceBT2020);

   const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   ds.InOutColorSpace = CS_BT2020; // already BT.2020: the default BT.709 would convert a second time
   ds.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   float3 color = DICETonemap(diceInBT2020 * paperWhite, peakWhite, ds) / paperWhite;
   color = Saturation(BT2020_To_BT709(SimpleGamutClip(color, true)), LumaSettings.GameSettings.Saturation);
#else
   float3 color = gamma_to_linear(gradedSDR);
#endif

   // max also turns NaN into 0 (D3D10+ min/max return the non-NaN operand).
   return float4(YRC_EncodeOutput(max(0.0, color)), colorAlpha);
}
