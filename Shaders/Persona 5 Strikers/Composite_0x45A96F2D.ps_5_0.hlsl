// Katana engine PostEffect3 composite: scene exposure, radial blur, sun/lens flare, vignette ("limb darkening"),
// then the HDR 3D LUT (32^3 BGRA8 asset, tonemap + grade baked offline) through an ARRI LogC EI1000 (no cut) shaper, an optional LDR LUT, brightness and fade.
// Writes linear colors to the swapchain (through an sRGB view), UI and FXAA follow.
// Luma: in HDR, the LUT output is extended above mid gray by matching the untonemapped scene color to the LUT's mid gray slope
// (same method as "Unreal Engine/Luma_UpgradeTonemapLUT.hlsl", run per pixel as the LUT is a static asset).
// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/DICE.hlsl"
#include "../Includes/ColorGradingLUT.hlsl"
// clang-format on

cbuffer cbComposite : register(b2)
{
   float4 g_vSceneTexSize : packoffset(c0);
   float4 g_vCompositeInfo : packoffset(c1);
   float4 g_vSun2dInfo : packoffset(c2);
   float4 g_vEtcEffect : packoffset(c3);
   float4 g_vBloomInfo : packoffset(c4);
   float4 g_vLimbDarkenningInfo : packoffset(c5);
   float4 g_vFxaaParams : packoffset(c6);
   float4 g_vGammaCorrection : packoffset(c7);
   float4 g_vRadialBlurCenter : packoffset(c8);
   float4 g_vRadialBlurInfo : packoffset(c9);
   float4 g_vFxaaQualityParams : packoffset(c10);
   float4 g_vCompositeLastViewport : packoffset(c11);
   float4 g_vMaxUV : packoffset(c12);
}

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_tSceneMap : register(t0);
Texture2D<float4> g_tLensFlareMap : register(t1);
Texture2D<float4> g_tExposureScaleInfo : register(t2);
Texture3D<float4> g_tHdrLut : register(t3);
Texture3D<float4> g_tLdrLut : register(t4);

// 3Dmigoto declarations
#define cmp -

static const float LUTSize = 32.0;
// Fixed values from the Unreal Engine LUT upgrade defaults
static const float HighlightsHuePreservation = 0.667;
static const float HighlightsChrominancePreservation = 0.333;

// The vanilla log shaper. Its output is directly the LUT UV (no half texel remapping).
float3 EncodeLUTInput(float3 color)
{
   return saturate(log2(color * 5.55555582 + 0.0479959995) * 0.0734997839 + 0.386036009);
}
float3 DecodeLUTInput(float3 encodedColor)
{
   return (exp2((encodedColor - 0.386036009) / 0.0734997839) - 0.0479959995) / 5.55555582;
}

// The LUT's slope (in linear scene input) on its grey diagonal where it crosses "targetValue", as a line through the previous texel.
// The LUT output is linear (the composite writes through an sRGB view). All the texels are loaded unconditionally so the loads don't serialize.
void FindLUTSlope(float targetValue, out float3 slope, out float3 offset)
{
   slope = 1.0;
   offset = 0.0;
   bool found = false;
   float3 prevOutput = g_tHdrLut.Load(int4(0, 0, 0, 0)).rgb;
   [unroll] for (int i = 1; i < (int)LUTSize; ++i)
   {
      float3 output = g_tHdrLut.Load(int4(i, i, i, 0)).rgb;
      if (!found && average(output) >= targetValue)
      {
         found = true;
         // Texel centers, the log shaper is strictly monotonic so the step is never 0
         float x1 = DecodeLUTInput((i - 0.5) / LUTSize).x;
         float x2 = DecodeLUTInput((i + 0.5) / LUTSize).x;
         slope = (output - prevOutput) / (x2 - x1);
         offset = prevOutput - slope * x1;
      }
      prevOutput = output;
   }
}

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0)
{
   const float4 icb[] = {{0, 0, 1.000000, 0},
                         {0, 1.000000, 0, 0},
                         {1.000000, 0, 0, 0},
                         {1.000000, 0, 0, 0},
                         {0, 1.000000, 0, 0},
                         {0, 0, 1.000000, 0},
                         {0, 1.000000, 0, 0},
                         {1.000000, 0, 0, 0},
                         {1.000000, 0, 1.000000, 0},
                         {1.000000, 0, 1.000000, 0},
                         {1.000000, 0, 0, 0},
                         {0, 1.000000, 0, 0}};
   float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11;
   uint4 bitmask, uiDest;
   float4 fDest;

   r0.x = cmp(0 < g_vSun2dInfo.z);
   r0.y = cmp(0 < g_vLimbDarkenningInfo.w);
   r0.z = cmp(g_vCompositeInfo.z < 0);
   r0.w = g_tExposureScaleInfo.Load(int3(0, 0, 0)).x;
   r1.xy = cmp(float2(0, 0) < g_vCompositeInfo.zy);
   r1.x = r1.x ? g_vCompositeInfo.z : 1;
   r0.z = r0.z ? r0.w : r1.x;
   r1.xz = v1.xy * g_vCompositeLastViewport.zw + g_vCompositeLastViewport.xy;
   r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.xz, 0).xyz;
   r2.xyz = min(float3(65024, 65024, 65024), r2.xyz);
   r0.w = cmp(0 < g_vEtcEffect.x);
   if (r0.w != 0)
   {
      r0.w = (uint)g_vEtcEffect.y;
      r3.xy = r1.xz * float2(2, 2) + float2(-1, -1);
      r1.w = dot(r3.xy, r3.xy);
      r3.xy = r3.xy * r1.ww;
      r3.xy = g_vEtcEffect.xx * r3.xy;
      r3.zw = g_vSceneTexSize.xy * -r3.xy;
      r3.zw = float2(0.5, 0.5) * r3.zw;
      r1.w = dot(r3.zw, r3.zw);
      r1.w = sqrt(r1.w);
      r1.w = (int)r1.w;
      r1.w = max(3, (int)r1.w);
      r1.w = min(16, (int)r1.w);
      r2.w = (int)r1.w;
      r3.xy = -r3.xy / r2.ww;
      r4.xyz = icb[r0.w + 0].xyz * r2.xyz;
      r3.zw = (int2)r0.ww + int2(1, 2);
      r5.xyz = icb[r3.z + 0].xyz + -icb[r0.w + 0].xyz;
      r6.xyz = icb[r3.w + 0].xyz + -icb[r3.z + 0].xyz;
      r7.xyz = r4.xyz;
      r8.xyz = icb[r0.w + 0].xyz;
      r9.xy = r1.xz;
      r3.w = 1;
      while (true)
      {
         r4.w = cmp((int)r3.w >= (int)r1.w);
         if (r4.w != 0)
            break;
         r9.xy = r9.xy + r3.xy;
         r10.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r9.xy, 0).xyz;
         r10.xyz = min(float3(65024, 65024, 65024), r10.xyz);
         r4.w = (int)r3.w;
         r4.w = r4.w / r2.w;
         r5.w = cmp(r4.w < 0.5);
         if (r5.w != 0)
         {
            r5.w = r4.w + r4.w;
            r11.xyz = r5.www * r5.xyz + icb[r0.w + 0].xyz;
         }
         else
         {
            r4.w = r4.w * 2 + -1;
            r11.xyz = r4.www * r6.xyz + icb[r3.z + 0].xyz;
         }
         r7.xyz = r10.xyz * r11.xyz + r7.xyz;
         r8.xyz = r11.xyz + r8.xyz;
         r3.w = (int)r3.w + 1;
      }
      r2.xyz = r7.xyz / r8.xyz;
   }
   r2.xyz = r2.xyz * r0.zzz;
   if (r0.x != 0)
   {
      r3.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, g_vSun2dInfo.xy, 0).xyz;
      r3.xyz = min(float3(65024, 65024, 65024), r3.xyz);
      r0.xzw = r3.xyz * r0.zzz;
      r3.xyz = g_tLensFlareMap.SampleLevel(sampleLinear_s, r1.xz, 0).xyz;
      r3.xyz = min(float3(65024, 65024, 65024), r3.xyz);
      r1.w = dot(r0.xzw, float3(0.222014993, 0.706655025, 0.0713300034));
      r1.w = cmp(g_vEtcEffect.w < r1.w);
      r1.w = r1.w ? g_vEtcEffect.z : 0;
      r0.xzw = r3.xyz * r0.xzw;
      r2.xyz = r0.xzw * r1.www + r2.xyz;
   }
   if (r0.y != 0)
   {
      r0.yz = float2(-0.5, -0.5) + r1.xz;
      r0.x = g_vCompositeInfo.x * r0.y;
      r0.x = dot(r0.xz, r0.xz);
      r0.y = sqrt(r0.x);
      r0.y = -g_vLimbDarkenningInfo.y + r0.y;
      r0.z = cmp(0 < r0.y);
      r0.y = saturate(-r0.y * g_vLimbDarkenningInfo.z + 1);
      r0.y = r0.z ? r0.y : 1;
      r0.z = cmp(0 < r0.y);
      r0.x = g_vLimbDarkenningInfo.x + r0.x;
      r0.x = g_vLimbDarkenningInfo.x / r0.x;
      r0.x = r0.x * r0.x;
      r0.x = r0.z ? r0.x : 1;
      r0.x = r0.x * r0.y;
      r0.y = 1 + -g_vLimbDarkenningInfo.w;
      r0.x = r0.x * g_vLimbDarkenningInfo.w + r0.y;
      r2.xyz = r2.xyz * r0.xxx;
   }
   const float3 untonemappedColor = r2.xyz;
   r0.xyz = EncodeLUTInput(r2.xyz);
   r0.xyz = g_tHdrLut.SampleLevel(sampleLinear_s, r0.xyz, 0).xyz;

   if (r1.y != 0)
   {
      r1.xyz = saturate(r0.xyz);
      r1.xyz = log2(r1.xyz);
      r1.xyz = float3(0.454545468, 0.454545468, 0.454545468) * r1.xyz;
      r1.xyz = exp2(r1.xyz);
      r1.xyz = g_tLdrLut.SampleLevel(sampleLinear_s, r1.xyz, 0).xyz;
      r1.xyz = r1.xyz + -r0.xyz;
      r0.xyz = g_vCompositeInfo.yyy * r1.xyz + r0.xyz;
   }
   if (LumaSettings.DisplayMode != 1) // Luma: the brightness curve only applies to SDR, Luma has its own
   {
      r0.w = cmp(g_vGammaCorrection.x != 1.000000);
      r1.xyz = log2(abs(r0.xyz));
      r1.xyz = g_vGammaCorrection.xxx * r1.xyz;
      r1.xyz = exp2(r1.xyz);
      r0.xyz = r0.www ? r1.xyz : r0.xyz;
   }
   else
   {
      const float3 tonemappedColor = r0.xyz;
      float3 midGraySlope;
      float3 midGrayOffset;
      FindLUTSlope(MidGray, midGraySlope, midGrayOffset);

      float3 midGrayProgress = saturate(tonemappedColor / MidGray);
      float maxMidGrayProgress = max3(midGrayProgress);
      float highlightsProgress = sqrt(maxMidGrayProgress);
      float maxMidGrayToWhiteProgress = max3(saturate((tonemappedColor - MidGray) / (1.0 - MidGray)));

      // Match the untonemapped color's slope with the LUT's on mid gray, which also carries the grade around it
      float3 remappedHDRColor = (untonemappedColor * midGraySlope) + midGrayOffset;
      // Keep the vanilla shadows, per channel
      remappedHDRColor = lerp(tonemappedColor, remappedHDRColor, sqrt(midGrayProgress));

      // Above mid gray, restore the hue of a LUT sample at a lower exposure, which isn't yet desaturated by the LUT's shoulder
      float tonemapHalveScale = lerp(1.0, 0.667, highlightsProgress); // Exposure of the hue reference (unrelated to the hue preservation amount)
      float3 hueSourceTonemappedColor = g_tHdrLut.SampleLevel(sampleLinear_s, EncodeLUTInput(untonemappedColor * tonemapHalveScale), 0).rgb / tonemapHalveScale;
      remappedHDRColor = RestoreHueAndChrominance(remappedHDRColor, hueSourceTonemappedColor, HighlightsHuePreservation, 1.0 - highlightsProgress);
      // Towards white, restore part of the vanilla chrominance
      remappedHDRColor = RestoreHueAndChrominance(remappedHDRColor, tonemappedColor, 0.0, sqrt(maxMidGrayToWhiteProgress) * HighlightsChrominancePreservation);

      r0.xyz = lerp(tonemappedColor, remappedHDRColor, sqr(maxMidGrayProgress));

      const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
      const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
      DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
      settings.DesaturationVsDarkeningRatio = 0.5;
      r0.xyz = DICETonemap(r0.xyz * paperWhite, peakWhite, settings) / paperWhite;
   }

   o0.xyz = g_vRadialBlurCenter.zzz * r0.xyz;
   o0.w = 1;

#if UI_DRAW_TYPE == 2 // Scale by the inverse of the relative UI brightness so we can draw the UI at brightness 1x and then multiply it back to its intended range
   ColorGradingLUTTransferFunctionInOutCorrected(o0.rgb, VANILLA_ENCODING_TYPE, GAMMA_CORRECTION_TYPE, true);
   o0.rgb *= LumaSettings.GamePaperWhiteNits / LumaSettings.UIPaperWhiteNits;
   ColorGradingLUTTransferFunctionInOutCorrected(o0.rgb, GAMMA_CORRECTION_TYPE, VANILLA_ENCODING_TYPE, true);
#endif

   // Luma: anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR and SDR on HDR.
   // The color is linear here, with 1 = UI paper white (UI_DRAW_TYPE 2). The UI draws after this, undithered.
   if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
      {
         ApplyDithering(o0.rgb, v1.xy, false, 1.0, 8u, LumaSettings.FrameIndex, true);
      }
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(o0.rgb * pqScale), GCT_MIRROR);
         ApplyDithering(pq, v1.xy, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         o0.rgb = BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale;
      }
   }
   return;
}