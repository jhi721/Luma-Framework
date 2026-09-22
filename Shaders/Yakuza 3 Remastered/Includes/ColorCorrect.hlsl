// Yakuza 3 Remastered — Luma HDR replacement for the engine's "color correct" family (CScreenEffectColorCorrect).
//
// The engine has no tone curve: every material writes `color * exposure` (gamma space, 2.2) straight into the 8-bit
// b8g8r8a8_unorm scene RT, whose UNORM format hard-clips it at 1.0. The first full-screen pass that reads the finished
// scene is this grade, so it is where Luma rebuilds SDR and HDR from the (now fp16) scene.
//
// One implementation serves all 34 pixel shaders of the family, selected by defines:
//   CCR_HLS  HLS hue/lightness/saturation stage (hue -> RGB through the gradient texture t1)
//   CCR_SC   saturation/contrast stage (inside HLS when CCR_HLS, else in BT.601 YCbCr)
//   CCR_GM   per-zone gamma (pow in the 2.2 domain, min 1)
//   CCR_GI   per-zone gain (mul_sat)
//   CCR_OF   per-zone offset (add_sat)
//   CCR_MASK fx_ccr_*_mask: alpha test + lerp to the graded color by the mask texture t2
//   CCR_COLLECTION ps_color_collection: 5-tap cross blur-sharpen before the grade
// The zone weights (shadow/mid/highlight) always come from the mean of the stage input color. Every stage is
// transcribed operand-for-operand from the disassembly (ps_ccr_* in data/shader/sh_ogre3_w64.par).
//
// SDR path: the exact vanilla grade on the saturated scene (the UNORM RT clamp, emulated).
// HDR path (canon for hard-clip games, BL GOTY precedent): the same grade as an extended function (upper clamps
// become max(0); HLS runs on the color normalized by its max channel since HLS is undefined above 1; zone weights
// from the saturated mean) -> linear -> soft ReinhardPiecewise hue reference in BT.2020 -> MacLeod-Boynton hue
// emulation -> DICE to peak. Output stays in gamma space (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) so the HUD,
// CMAA2, CAS and render_fb downstream behave as in vanilla.

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

// Shadow / midtone / highlight weights from the mean of the stage input.
float3 ZoneWeights(float3 c, bool clampSDR)
{
   float m = dot(c, 1.0 / 3.0);
   // Above 1 (HDR only) the vanilla polynomials turn around ((1-m)^2 grows again): hold them at their m = 1 values.
   m = clampSDR ? m : saturate(m);
   float im = 1.0 - m;
   return saturate(float3(im * im, 1.0 - im * im - m * m, m * m));
}

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
   float lightness = sum * 0.5 + cb5[0].y;
#if CCR_SC
   float satAdjust = dot(cb5[4].xyz, w) + cb5[0].z;
   float lightAdjust = dot(cb5[8].xyz, w) + cb5[0].w;
   lightness = lightness * (1.0 / (lightAdjust * -2.0 + 1.0)) - lightAdjust;
   float saturation = (lightness < 0.5) ? satLow : satHigh;
   saturation = saturation * satAdjust + saturation;
#else
   float saturation = (lightness < 0.5) ? satLow : satHigh;
#endif
   lightness = saturate(lightness);
   saturation = saturate(saturation);

   float3 hueColor = t1.Sample(s1_s, float2(hue * (1.0 / 6.0), saturation)).rgb;
   float3 high = (1.0 - hueColor) * ((lightness - 0.5) * 2.0) + hueColor;
   float3 low = -hueColor * ((0.5 - lightness) * 2.0) + hueColor;
   return saturate((0.5 >= lightness) ? low : high);
}
#endif

// The whole vanilla grade. `clampSDR` true = verbatim; false = the extended HDR function (see header).
float3 Grade(float3 c, bool clampSDR)
{
   const float3 w = ZoneWeights(c, clampSDR);

#if CCR_HLS
   // HLS is only defined inside [0,1]: grade the color normalized by its max channel and restore the scale
   // (scale = 1, i.e. exact vanilla, whenever the color is in range).
   const float scale = clampSDR ? 1.0 : max(1.0, max3(c));
   c = HLSStage(c / scale, w) * scale;
#elif CCR_SC
   float satAdjust = dot(cb5[4].xyz, w) + cb5[0].z;
   float lightAdjust = dot(cb5[8].xyz, w) + cb5[0].w;
   float y = dot(float3(0.299, 0.587, 0.114), c) + cb5[0].y;
   y = y * (1.0 / (lightAdjust * -2.0 + 1.0)) - lightAdjust;
   float cb = dot(float3(-0.16874, -0.33126, 0.5), c) * (satAdjust + 1.0);
   float cr = dot(float3(0.5, -0.41869, -0.08131), c) * (satAdjust + 1.0);
   c = float3(y + 1.402 * cr, y - 0.34414 * cb - 0.71414 * cr, y + 1.772 * cb);
   c = clampSDR ? saturate(c) : max(0.0, c);
#endif

#if CCR_GM
   float3 gamma = float3(dot(cb5[5].xyz, w), dot(cb5[9].xyz, w), dot(cb5[13].xyz, w)) + cb5[1].xyz;
   c = exp2(log2(c) * (gamma * 0.454545));
   c = clampSDR ? min(c, 1.0) : c;
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
   // Alpha test (engine-wide material convention: cb11[0].z = reference in 1/255 units, 0 = off).
   if (cb11[0].z > 0u && (colorAlpha - float(cb11[0].z) * 0.00392156886) < 0.0)
      discard;
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
   const float3 sceneSDR = saturate(scene);
   float3 gradedSDR = Grade(sceneSDR, true);
#if CCR_MASK
   gradedSDR = (gradedSDR - sceneSDR) * (t2.Sample(s2_s, uv.zw).r * cb4[0].x) + sceneSDR;
#endif

#if TONEMAP_TYPE >= 1
   const float3 sceneHDR = max(0.0, scene);
   float3 gradedHDR = Grade(sceneHDR, false);
#if CCR_MASK
   gradedHDR = (gradedHDR - sceneHDR) * (t2.Sample(s2_s, uv.zw).r * cb4[0].x) + sceneHDR;
#endif
   float3 extendedBT2020 = BT709_To_BT2020(gamma_to_linear(gradedHDR, GCT_POSITIVE));

   // Soft hue reference: per-channel ReinhardPiecewise(5, 1.5) in BT.2020 bends saturated highlights the way the
   // vanilla per-channel clip did, without its whitening; MacLeod-Boynton keeps only its hue direction.
   float3 hueReferenceBT2020 = Reinhard::ReinhardPiecewise(extendedBT2020, 5.0, 1.5);
   float3 diceInBT2020 = MacLeodBoynton::HueOnlyBT2020(extendedBT2020, hueReferenceBT2020);

   const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
   DICESettings ds = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   ds.InOutColorSpace = CS_BT2020; // already BT.2020: the default BT.709 would convert a second time
   float3 color = DICETonemap(diceInBT2020 * paperWhite, peakWhite, ds) / paperWhite;
   color = BT2020_To_BT709(SimpleGamutClip(color, true));
#else
   float3 color = gamma_to_linear(gradedSDR);
#endif

#if UI_DRAW_TYPE >= 2
   // The HUD is drawn on top in gamma SDR; pre-scale so it lands at UI paper white after composition.
   color *= LumaSettings.GamePaperWhiteNits / max(LumaSettings.UIPaperWhiteNits, 1.0);
#endif
   color = (color == color) ? color : 0.0; // NaN -> 0
   color = max(0.0, color);
   return float4(linear_to_gamma(color), colorAlpha);
}
