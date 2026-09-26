// Katana engine PostEffect3 composite: scene exposure, chromatic aberration, sun/lens flare, vignette ("limb darkening"),
// then the HDR 3D LUT (32^3 BGRA8 asset, tonemap + grade baked offline) through an ARRI LogC EI1000 (no cut) shaper, an optional LDR LUT, an output power curve (g_vGammaCorrection) and fade.
// Writes linear colors to the swapchain (through an sRGB view), UI and FXAA follow.
// Luma: in HDR, the vanilla LUT output is multiplied by one scalar of the pre-LUT relative luminance Y: E(Y) / G(Y), where G is the LUT's
// gray tone curve and E is G continued by its tangent past the pivot where G's output reaches mid gray. Only luminance crosses into the
// output (as in BL2/TPS), a scalar can't rotate hue, so the grade and its path to white stay vanilla, and below the pivot the output is
// exactly vanilla. DICE then maps it to the peak. The LUT is a per area asset, so the tangent is found per pixel.
// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/DICE.hlsl"
#include "../Includes/ColorGradingLUT.hlsl"
#include "Includes/cbComposite.hlsl"
// clang-format on

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_tSceneMap : register(t0);
Texture2D<float4> g_tLensFlareMap : register(t1);
Texture2D<float4> g_tExposureScaleInfo : register(t2);
Texture3D<float4> g_tHdrLut : register(t3);
Texture3D<float4> g_tLdrLut : register(t4);

// 3Dmigoto declarations
#define cmp -

static const float LUTSize = 32.0;

// The vanilla log shaper. Its output is directly the LUT UV (no half texel remapping).
float3 EncodeLUTInput(float3 color)
{
   return saturate(log2(color * 5.55555582 + 0.0479959995) * 0.0734997839 + 0.386036009);
}
float3 DecodeLUTInput(float3 encodedColor)
{
   return (exp2((encodedColor - 0.386036009) / 0.0734997839) - 0.0479959995) / 5.55555582;
}

// The LUT's gray tone curve: the average of its gray diagonal (unaffected by "Color Grading Intensity"), linear output over linear scene input
float LUTGrayCurve(float x)
{
   return average(g_tHdrLut.SampleLevel(sampleLinear_s, EncodeLUTInput(x), 0).rgb);
}

// Luma: "Color Grading Intensity" fades the LUT's color grading out towards its gray tone curve alone (0), applied by luminance
float3 SampleGradedLUT(float3 color)
{
   float3 graded = g_tHdrLut.SampleLevel(sampleLinear_s, EncodeLUTInput(color), 0).rgb;
   [branch] if (LumaSettings.GameSettings.ColorGradingIntensity != 1.0)
   {
      const float luminance = GetLuminance(color);
      const float3 neutral = luminance > 0.0 ? color * (LUTGrayCurve(luminance) / luminance) : 0.0;
      graded = lerp(neutral, graded, LumaSettings.GameSettings.ColorGradingIntensity);
   }
   return graded;
}

// The gray tone curve's tangent where it crosses "targetValue": the first gray diagonal texel center at or above it, and the secant from the
// previous texel. If the curve never gets there, the pivot stays out of reach (no extension, the other outputs are unused). The LUT output
// is linear (the composite writes through an sRGB view). All the texels are loaded unconditionally so the loads don't serialize.
void FindLUTTangent(float targetValue, out float pivot, out float pivotOutput, out float slope)
{
   pivot = FLT_MAX;
   pivotOutput = 0.0;
   slope = 0.0;
   float prevOutput = average(g_tHdrLut.Load(int4(0, 0, 0, 0)).rgb);
   [unroll] for (int i = 1; i < (int)LUTSize; ++i)
   {
      float output = average(g_tHdrLut.Load(int4(i, i, i, 0)).rgb);
      if (pivot == FLT_MAX && output >= targetValue)
      {
         // Texel centers, the log shaper is strictly monotonic so the step is never 0
         pivot = DecodeLUTInput((i + 0.5) / LUTSize).x;
         pivotOutput = output;
         slope = (output - prevOutput) / (pivot - DecodeLUTInput((i - 0.5) / LUTSize).x);
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
   r0.z *= LumaSettings.GameSettings.Exposure; // Luma: exposure slider (also scales the sun flare, as the vanilla exposure does)
   r1.xz = v1.xy * g_vCompositeLastViewport.zw + g_vCompositeLastViewport.xy;
   // Luma: a 3D layer drawn at the output resolution (the pause screen's; LumaData.CustomData4 = target / vanilla viewport width): its scene is
   // the whole target, not the render resolution corner. Only the scene samples: the flare, vignette and aberration keep the vanilla UV.
   const float sceneUVScale = LumaData.CustomData4 > 0.0 ? LumaData.CustomData4 : 1.0;
   r2.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r1.xz * sceneUVScale, 0).xyz;
   // Luma: the scene is upgraded from R11G11B10_FLOAT, which could not hold negatives (65024, the vanilla cap, is its max)
   r2.xyz = clamp(r2.xyz, 0.0, 65024.0);
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
         r10.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, r9.xy * sceneUVScale, 0).xyz;
         r10.xyz = clamp(r10.xyz, 0.0, 65024.0);
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
      r3.xyz = g_tSceneMap.SampleLevel(sampleLinear_s, g_vSun2dInfo.xy * sceneUVScale, 0).xyz;
      r3.xyz = clamp(r3.xyz, 0.0, 65024.0);
      r0.xzw = r3.xyz * r0.zzz;
      r3.xyz = g_tLensFlareMap.SampleLevel(sampleLinear_s, r1.xz, 0).xyz;
      r3.xyz = clamp(r3.xyz, 0.0, 65024.0); // Luma: the flare target is upgraded from R11G11B10_FLOAT too
      r1.w = dot(r0.xzw, float3(0.222014993, 0.706655025, 0.0713300034));
      r1.w = cmp(g_vEtcEffect.w < r1.w);
      r1.w = r1.w ? g_vEtcEffect.z : 0;
      r0.xzw = r3.xyz * r0.xzw;
      r2.xyz = r0.xzw * r1.www * LumaSettings.GameSettings.LensFlareIntensity + r2.xyz; // Luma: flare intensity slider
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
      // Luma: vignette intensity slider scales the vignette's blend weight
      r0.w = g_vLimbDarkenningInfo.w * LumaSettings.GameSettings.VignetteIntensity;
      r0.x = r0.x * r0.w + (1.0 - r0.w);
      r2.xyz = r2.xyz * r0.xxx;
   }
   const float3 untonemappedColor = r2.xyz;
   r0.xyz = SampleGradedLUT(r2.xyz);

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
   if (!P5S_HDR_SCENE) // Luma: the output power curve only applies to SDR, Luma has its own
   {
      r0.w = cmp(g_vGammaCorrection.x != 1.000000);
      r1.xyz = log2(abs(r0.xyz));
      r1.xyz = g_vGammaCorrection.xxx * r1.xyz;
      r1.xyz = exp2(r1.xyz);
      r0.xyz = r0.www ? r1.xyz : r0.xyz;
   }
   else
   {
      const float luminance = GetLuminance(untonemappedColor);
      const float grayOutput = LUTGrayCurve(luminance);
      // The curve is monotonic, so pixels it maps below mid gray are under the pivot and skip the 32 texel search
      [branch] if (grayOutput >= MidGray)
      {
         float pivot, pivotOutput, slope;
         FindLUTTangent(MidGray, pivot, pivotOutput, slope);
         // Never below vanilla, in case the curve is still convex past the pivot
         if (luminance > pivot)
            r0.xyz *= max(1.0, (pivotOutput + slope * (luminance - pivot)) / grayOutput);
      }

      const float paperWhite = LumaSettings.GamePaperWhiteNits / sRGB_WhiteLevelNits;
      const float peakWhite = LumaSettings.PeakWhiteNits / sRGB_WhiteLevelNits;
      DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
      settings.DesaturationVsDarkeningRatio = 0.5;
      settings.HighlightsDesaturation = LumaSettings.GameSettings.HighlightsDesaturation;
      r0.xyz = DICETonemap(r0.xyz * paperWhite, peakWhite, settings) / paperWhite;
      // User saturation last, after the display map (repository convention)
      r0.xyz = Saturation(r0.xyz, LumaSettings.GameSettings.Saturation);
   }

   o0.xyz = g_vRadialBlurCenter.zzz * r0.xyz;
   o0.w = 1;

#if UI_DRAW_TYPE == 2 // Scale by the inverse of the relative UI brightness so we can draw the UI at brightness 1x and then multiply it back to its intended range
   ColorGradingLUTTransferFunctionInOutCorrected(o0.rgb, VANILLA_ENCODING_TYPE, GAMMA_CORRECTION_TYPE, true);
   o0.rgb *= LumaSettings.GamePaperWhiteNits / LumaSettings.UIPaperWhiteNits;
   ColorGradingLUTTransferFunctionInOutCorrected(o0.rgb, GAMMA_CORRECTION_TYPE, VANILLA_ENCODING_TYPE, true);
#endif

   // Luma: when SMAA follows (it sets LumaData.CustomData3), the dither runs at its end instead (SMAA neighborhood blending, or Luma_P5S_SMAAFinalize.hlsl with RCAS).
   // The UI draws after this, undithered.
   if (LumaData.CustomData3 == 0.0)
      P5S_DitherOutput(o0.rgb, v1.xy);
   return;
}