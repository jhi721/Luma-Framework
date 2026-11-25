// Hashes registered from C++ ("redirected_shader_hashes"): originally 0xCC629DA4.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space (2008): the uber post PS_AABloomlutColor1 (lutcolor.fx, mask 0x4e: EdgeAA, bloom without blackcolor, 1 LUT; blob
// 055fa7980d80, dgVoodoo 2.87.6 0xCC629DA4), the opening cutscene's. Transcribed from the translation for a DEVELOPMENT A/B of the
// LUT tail above 1 (DevSetting01): < 1/3 "WrapUberLut" as shipped (UberLutPatch.h: vanilla ratios, the scaled-back grade's
// brightness), < 2/3 the plain max-channel wrap (LUT(x / m) * m, RenoDX's lut default), else a suggested route: a max-channel
// Reinhard to the display peak (shoulder at 1, strength 2..16 by DevSetting02), then the LUTs on that compressed again from mid gray
// with the luminance normalization (DevSetting03 > 0.5 turns it off) and a uniform brightness restore, output for the display (the
// display map passes it through, Includes/DisplayMap.hlsl). Every route keeps the vanilla grade where max3(x) <= 1 except the last.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: the texture masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: gLUT_params1 c0 (x amount, y weight), bloom c1..c2, offsets c3..c5
}

Texture3D<float4> t32 : register(t32); // gLUTTexture1 (32^3)
Texture2D<float4> t1 : register(t1);   // The scene copy
Texture2D<float4> t2 : register(t2);   // Bloom (biased by 0.5)
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

static const float3 BT601_LUMA = float3(0.299, 0.587, 0.114); // On encoded R'G'B' (luma), as the EdgeAA's dp3

float4 SceneTap(float2 uv)
{
   return ApplyDgvMask(t1.Sample(s1_s, uv), cb3[46], cb3[47]);
}

// The vanilla LUT application: x + amount * (weight * LUT(x) - x), the LUT coordinate scaled to the texel centres
float4 Grade(float4 x, float amount)
{
   const float4 lut = ApplyDgvMask(t32.Sample(s0_s, x.xyz * 0.96875 + 0.015625), cb3[44], cb3[45]);
   return x + amount * (lut * cb4[8].y - x);
}

float LuminanceGamma22(float3 color)
{
   return dot(pow(max(color, 0.0), 2.2), Rec709_Luminance) + 1e-10;
}

// The suggested "ReinhardAdvanced" (shoulder start, peak in -> peak out, shoulder strength), max channel scalar form
float ReinhardAdvancedScalar(float color, float shoulder_start, float peak_in, float peak_out, float shoulder_strength, bool asymptotic)
{
   const float range_out = peak_out - shoulder_start;
   const float below = min(color, shoulder_start);
   const float normalized_beyond = (color - below) / range_out;
   const float inv_white_ratio = asymptotic ? 0.0 : (range_out / (peak_in - shoulder_start)); // range out / FLT_MAX = 0
   const float t = saturate(rcp(rcp(normalized_beyond) + 1.0 - inv_white_ratio));
   const float mid_point = 1.0 - pow(1.0 + rcp(shoulder_strength), -shoulder_strength);
   const float k = 4.0 * (0.5 / mid_point - 1.0);
   return below + t / (1.0 + k * t * (1.0 - t)) * range_out;
}

// The suggested "SampleGradingLUT" on gamma 2.2 values (its tetrahedral read is the hardware trilinear here)
float3 SampleGradingSuggested(float3 encoded, float amount, bool normalize)
{
   float3 graded = Grade(float4(encoded, 1.0), amount).rgb;
   if (normalize)
   {
      // Luma (BT.709 weights on the gamma encoded R'G'B'), not relative luminance
      const float old_luma = dot(encoded, Rec709_Luminance);
      const float new_luma = dot(graded, Rec709_Luminance);
      const float weight = sqr(max3(encoded));
      graded = lerp(graded, graded * (old_luma / max(new_luma, 1e-10)), weight);
      graded *= lerp(1.0, max(new_luma / max(old_luma, 1e-10), 1.0), weight);
   }
   return graded;
}

void main(DGV_MAIN_SIGNATURE)
{
   // EdgeAA: 4 diagonal taps, the edge from their BT.601 weighted differences (on encoded values), smoothstep toward their average
   const float2 offset = cb4[13].xy;
   const float edge_range = 1.5 - cb4[13].w;
   const float inv_edge_range = DgVoodooRcp(edge_range);
   const float2 uv = cb4[11].zw + v5.xy;
   const float4 tap0 = SceneTap(uv - offset);
   const float4 tap1 = SceneTap(uv + offset * float2(1.0, -1.0));
   const float4 tap2 = SceneTap(uv + offset);
   const float4 tap3 = SceneTap(uv + offset * float2(-1.0, 1.0));
   const float4 center = SceneTap(uv);
   const float center_luma = dot(center.rgb, BT601_LUMA);
   const float4 lumas = float4(dot(tap0.rgb, BT601_LUMA), dot(tap1.rgb, BT601_LUMA), dot(tap2.rgb, BT601_LUMA), dot(tap3.rgb, BT601_LUMA));
   float edge = saturate(inv_edge_range * (dot(abs(lumas - center_luma), cb4[13].zzzz) - cb4[13].w));
   edge = edge * edge * (3.0 - 2.0 * edge);
   const float4 color = center + edge * ((tap0 + tap1 + tap2 + tap3) * 0.25 - center);

   // Bloom without blackcolor: max(c, c - w * saturate(c - threshold) + (bloom - 0.5) * tint)
   const float4 bloom = ApplyDgvMask(t2.Sample(s2_s, cb4[12].zw + v5.xy), cb3[48], cb3[49]) - 0.5;
   const float4 x = max(color, saturate(color - cb4[9]) * -cb4[9].w + color + bloom * cb4[10]);

   // The LUT tail ("Color Grading Intensity" scales the amount, as the patched shaders)
   const float amount = cb4[8].x * LumaSettings.GameSettings.ColorGradingIntensity;
   const float4 vanilla = x + amount * (ApplyDgvMask(t32.Sample(s0_s, x.xyz * 0.96875 + 0.015625), cb3[44], cb3[45]) * cb4[8].y - x);
   o0.a = vanilla.a;
   const float3 v = Grade(float4(saturate(x.rgb), x.a), amount).rgb;
   o0.rgb = v;
   if (LumaSettings.DisplayMode != 1)
      return;

   float route = 0.0;
   float strength = 2.0;
   bool normalize = true;
#if DEVELOPMENT
   route = LumaSettings.DevSetting01;
   strength = exp2(lerp(1.0, 4.0, LumaSettings.DevSetting02));
   normalize = LumaSettings.DevSetting03 <= 0.5;
#endif
   const float m = max(1.0, max3(x.rgb));
   if (route < 1.0 / 3.0)
   {
      const float3 g = Grade(float4(x.rgb / m, 1.0), amount).rgb * m;
      o0.rgb = v * pow(LuminanceGamma22(g) / LuminanceGamma22(v), 1.0 / 2.2);
   }
   else if (route < 2.0 / 3.0)
   {
      o0.rgb = Grade(float4(x.rgb / m, 1.0), amount).rgb * m;
   }
   else
   {
      const float paper_white = max(GamePaperWhiteNits, 1.0);
      const float peak = max(PeakWhiteNits, paper_white) / paper_white; // As GammaCanvasDisplayMap's
      // The exposure before the Reinhard (the display map skips it on this route), so it can't push past the peak
      float3 linear_color = pow(max(x.rgb, 0.0), 2.2) * LumaSettings.GameSettings.Exposure;
      const float max_channel = max3(linear_color);
      if (max_channel > 1.0)
      {
         linear_color *= ReinhardAdvancedScalar(max_channel, 1.0, 0.0 /* FLT_MAX: asymptotic */, peak, strength, true) / max_channel;
      }
      // "ApplySDRLUTsLuma": compressed from mid gray to the peak for the LUT, the brightness restored uniformly
      const float maximum = max(max3(linear_color), MidGray);
      const float ratio = ReinhardAdvancedScalar(maximum, MidGray, peak, 1.0, 2.0, false) / maximum;
      const float3 source = saturate(linear_color * ratio); // its CorrectOutOfRangeColor is a no-op on positive in-range BT.709
      float3 graded = pow(max(SampleGradingSuggested(pow(source, 1.0 / 2.2), amount, normalize), 0.0), 2.2);
      const float3 black = SampleGradingSuggested(0.0, amount, false);
      const float3 grey = SampleGradingSuggested(0.5, amount, false);
      const float3 white = SampleGradingSuggested(1.0, amount, false);
      if (!(any(white <= grey) || any(grey <= black)))
      {
         graded /= ratio;
      }
      o0.rgb = pow(graded, 1.0 / 2.2);
   }
}
