// ps_3_0 port of Shaders/Includes/PlatinumToneCurve.hlsl (same math; not a replacement of it). Differences forced by SM3: the table
// is a point-sampled sampler2D read with tex2Dlod at the texel centre instead of Texture2D::Load, no dgVoodoo masks (native D3D9
// returns the real R8G8B8A8 table), float arithmetic instead of int/uint. Include after declaring the sampler.
#ifndef PLATINUM_TONE_CURVE_SM3_HLSLI
#define PLATINUM_TONE_CURVE_SM3_HLSLI

namespace PlatinumToneCurve
{
static const float kPivot = 0.5;
static const float kWindow = 16.0 / 255.0;

struct Tangent
{
   float pivot;
   float value;
   float slope;
};

float Texel(sampler2D table, float index)
{
   return tex2Dlod(table, float4((index + 0.5) / 256.0, 0.0, 0.0, 0.0)).x;
}

float Smoothed(float previous, float center, float next)
{
   return clamp((previous + 2.0 * center + next) * 0.25, min(previous, min(center, next)), max(previous, max(center, next)));
}

float Sample(sampler2D table, float x)
{
   const float position = saturate(x) * 255.0;
   const float first = min(floor(position), 254.0);
   const float t0 = Texel(table, max(first - 1.0, 0.0));
   const float t1 = Texel(table, first);
   const float t2 = Texel(table, first + 1.0);
   const float t3 = Texel(table, min(first + 2.0, 255.0));
   const float s1 = (first == 0.0) ? t1 : Smoothed(t0, t1, t2);
   const float s2 = (first == 254.0) ? t2 : Smoothed(t1, t2, t3);
   return lerp(s1, s2, position - first);
}

Tangent FitTangent(sampler2D table)
{
   float first_white = 0.0;
   [unroll] for (float step = 128.0; step >= 1.0; step *= 0.5)
   {
      if (Texel(table, first_white + step - 1.0) < 1.0)
      {
         first_white += step;
      }
   }
   Tangent tangent;
   tangent.pivot = clamp(first_white / 255.0 - kWindow, kWindow, kPivot);
   tangent.value = Sample(table, tangent.pivot);
   tangent.slope = (Sample(table, tangent.pivot + kWindow) - Sample(table, tangent.pivot - kWindow)) / (2.0 * kWindow);
   return tangent;
}

float Apply(Tangent tangent, float x, float curved)
{
   return x > tangent.pivot ? (tangent.value + tangent.slope * (x - tangent.pivot)) : curved;
}

float3 Apply(Tangent tangent, float3 x, float3 curved)
{
   return x > tangent.pivot ? (tangent.value + tangent.slope * (x - tangent.pivot)) : curved;
}
} // namespace PlatinumToneCurve

#endif
