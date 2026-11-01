#ifndef SRC_PLATINUM_TONE_CURVE_HLSL
#define SRC_PLATINUM_TONE_CURVE_HLSL

#include "DgVoodoo.hlsl"

// Platinum Games' ColorTableTexture (Bayonetta, Metal Gear Rising Revengeance; the same CPU code): a 256x1 R8G8B8A8 table the CPU
// writes every frame from a level-authored spline clamped to [0,1], one grey curve in R, G and B, read through dgVoodoo's masks with
// a gamma encoded value as U (point sampled). The spline can reach 1 before the domain's end (MGR:R: texel 194 = 0.76 in a 2026-11-01 capture;
// Bayonetta: texel 255) and is flat above it, so HDR continues it by its tangent above a pivot, vanilla exact below the pivot.
namespace PlatinumToneCurve
{
// The pivot: on the curves' straight part (encoded slope ~1.4 in MGR:R, ~1.0 in Bayonetta in the 2026-11-01 captures) and above
// vanilla's white point at their gains (scene ~0.41 and ~0.47), so the SDR range stays exact. The secant's half window: 16 texels,
// as RenoDX alienisolation.
static const float kPivot = 0.5;
static const float kWindow = 16.0 / 255.0;

struct Tangent
{
   float pivot;
   float value;
   float slope;
};

float Texel(Texture2D<float4> table, float4 mask, float4 fill, int index)
{
   return ApplyDgvMask(table.Load(int3(index, 0, 0)), mask, fill).x;
}

// A texel smoothed [1, 2, 1] / 4 against the 8-bit rounding of the spline (the step between texels alternates 1 and 2 codes on a
// slope of 1.5), kept within its neighbours so it can't overshoot (RenoDX dyinglight's CUSTOM_LUT_SMOOTHING); the end texels stay
float Smoothed(float previous, float center, float next)
{
   return clamp((previous + 2.0 * center + next) * 0.25, min(previous, min(center, next)), max(previous, max(center, next)));
}

// The table interpolated in fp32 between the smoothed texels (the game reads it with a point sampler: 256 levels again on the fp16
// scene; the hardware's linear filter would weigh with 8 bits). Position x * 255, so 8-bit code v lands on texel v as in vanilla.
float Sample(Texture2D<float4> table, float4 mask, float4 fill, float x)
{
   const float position = saturate(x) * 255.0;
   const int first = min(int(position), 254);
   const float t0 = Texel(table, mask, fill, max(first - 1, 0));
   const float t1 = Texel(table, mask, fill, first);
   const float t2 = Texel(table, mask, fill, first + 1);
   const float t3 = Texel(table, mask, fill, min(first + 2, 255));
   const float s1 = (first == 0) ? t1 : Smoothed(t0, t1, t2);
   const float s2 = (first == 254) ? t2 : Smoothed(t1, t2, t3);
   return lerp(s1, s2, position - first);
}

// From the table alone (never the pixel): the same tangent for every channel and pixel of a frame
Tangent FitTangent(Texture2D<float4> table, float4 mask, float4 fill)
{
   // The first texel at 1 (the curve is monotonic; 255 if none), the pivot kept a window below it so the secant never reads the
   // flat part
   uint firstWhite = 0;
   [unroll] for (uint step = 128; step > 0; step /= 2)
   {
      if (Texel(table, mask, fill, firstWhite + step - 1) < 1.0)
      {
         firstWhite += step;
      }
   }
   Tangent tangent;
   tangent.pivot = clamp(firstWhite / 255.0 - kWindow, kWindow, kPivot);
   tangent.value = Sample(table, mask, fill, tangent.pivot);
   tangent.slope = (Sample(table, mask, fill, tangent.pivot + kWindow) - Sample(table, mask, fill, tangent.pivot - kWindow)) / (2.0 * kWindow);
   return tangent;
}

// x unclipped and non-negative; curved = the table's value at x
float Apply(Tangent tangent, float x, float curved)
{
   return x > tangent.pivot ? (tangent.value + tangent.slope * (x - tangent.pivot)) : curved;
}

float3 Apply(Tangent tangent, float3 x, float3 curved)
{
   return x > tangent.pivot ? (tangent.value + tangent.slope * (x - tangent.pivot)) : curved;
}
} // namespace PlatinumToneCurve

#endif // SRC_PLATINUM_TONE_CURVE_HLSL
