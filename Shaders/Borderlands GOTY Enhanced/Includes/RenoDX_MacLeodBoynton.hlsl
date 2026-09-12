// Borderlands GOTY Enhanced — RenoDX MacLeod–Boynton hue/purity emulation, the production HDR colour stage.
//
// Line-by-line port of the RenoDX Borderlands GOTY Enhanced colour stage at clshortfuse/renodx
// cd32113a98608e63027d40910cfe296a14dfe228:
//   src/games/borderlandsgotyenhanced/macleod_boynton.hlsli  constants, MB transforms, ApplyInternal, ApplyBT2020
//   src/games/borderlandsgotyenhanced/common.hlsli            ApplyHueAndPurityGrading, its hue-emulation block
//   src/shaders/math.hlsl                                     Invert3x3, DivideSafe
//   src/shaders/deprecated.hlsl                               SafeDivision
//   src/shaders/color/rgb.hlsl                                BT.2020/XYZ matrices, WHITE_POINT_D65, xyY -> XYZ
// Kept verbatim on purpose, unused purity modes included: _tools/bl1_bridge/mb_equiv compiles this file and the
// RenoDX source side by side under fxc and compares the listings, and nothing is simplified until that passes.
// Only the hue block of ApplyHueAndPurityGrading is carried; its saturation / dechroma / highlight-saturation
// tail is the identity at the RenoDX BL1 defaults (1 / 0 / 0) and is omitted. Nothing here depends on Luma includes.
//
// The model: RGB -> XYZ -> LMS (CIE 2006), hue as MacLeod–Boynton ratios r = L/(L+M), b = S/(L+M) around the D65
// white, intensity anchored on T = L + M, and purity as the fraction of the distance from white to the RGB gamut
// boundary along that ray. The hue emulation takes the reference's ray direction and re-applies the target's
// own purity on it, so with chrominance_emulation = 0 only the hue direction moves.

#ifndef LUMA_BL1_RENODX_MACLEOD_BOYNTON_HLSL
#define LUMA_BL1_RENODX_MACLEOD_BOYNTON_HLSL

namespace BL1_RenoDX
{
// src/shaders/math.hlsl
float DivideSafe(float dividend, float divisor, float fallback)
{
   return (divisor == 0.f) ? fallback : (dividend / divisor);
}

float3x3 Invert3x3(float3x3 m)
{
   float a = m[0][0], b = m[0][1], c = m[0][2];
   float d = m[1][0], e = m[1][1], f = m[1][2];
   float g = m[2][0], h = m[2][1], i = m[2][2];

   float A = (e * i - f * h);
   float B = -(d * i - f * g);
   float C = (d * h - e * g);
   float D = -(b * i - c * h);
   float E = (a * i - c * g);
   float F = -(a * h - b * g);
   float G = (b * f - c * e);
   float H = -(a * f - c * d);
   float I = (a * e - b * d);

   float det = a * A + b * B + c * C;
   float invDet = DivideSafe(1.0, det, 0.0);

   return float3x3(
              A, D, G,
              B, E, H,
              C, F, I) *
          invDet;
}

// src/shaders/deprecated.hlsl
float SafeDivision(float quotient, float dividend, float fallback)
{
   return (dividend == 0.f)
              ? fallback
              : (quotient / dividend);
}

// src/shaders/color/rgb.hlsl
static const float3x3 BT2020_TO_XYZ_MAT = float3x3(
    0.6369580483f, 0.1446169036f, 0.1688809752f,
    0.2627002120f, 0.6779980715f, 0.0593017165f,
    0.0000000000f, 0.0280726930f, 1.0609850577f);

static const float3x3 XYZ_TO_BT2020_MAT = float3x3(
    1.7166511880f, -0.3556707838f, -0.2533662814f,
    -0.6666843518f, 1.6164812366f, 0.0157685458f,
    0.0176398574f, -0.0427706133f, 0.9421031212f);

static const float2 WHITE_POINT_D65 = float2(0.31272, 0.32903);

// renodx::color::xyz::from::xyY
float3 XYZ_From_xyY(float3 xyY)
{
   float3 XYZ;

   XYZ.xz = float2(xyY.x, (1.f - xyY.xy.x - xyY.xy.y)) / xyY.y * xyY[2];

   XYZ.y = xyY[2];

   return XYZ;
}

// src/games/borderlandsgotyenhanced/macleod_boynton.hlsli
static const float3x3 XYZ_TO_LMS_2006 = float3x3(
    0.185082982238733f, 0.584081279463687f, -0.0240722415044404f,
    -0.134433056469973f, 0.405752392775348f, 0.0358252602217631f,
    0.000789456671966863f, -0.000912281325916184f, 0.0198490812339463f);

static const float3x3 LMS_TO_XYZ_2006 = Invert3x3(XYZ_TO_LMS_2006);

static const float EPSILON = 1e-20f;
static const float INTERVAL_MAX = 1e30f;
static const float MB_NEAR_WHITE_EPSILON = 1e-14f;

static const int MB_PURITY_MODE_DISTANCE = 0;
static const int MB_PURITY_MODE_NORMALIZED = 1;
static const int MB_PURITY_MODE_SCALE_NEUTWO = 2;
static const int MB_PURITY_ADJUST_NONE = 0;
static const int MB_PURITY_ADJUST_CLIP_MAX = 1;
static const float ONE_MINUS_EPSILON = 1.f - 1e-6f;

// Neutwo (x / sqrt(x^2 + 1)) and its inverse.
// Used to shape purity scaling with a smooth shoulder near gamut edge.
float Neutwo(float x)
{
   float denominator_squared = mad(x, x, 1.f);
   return x * rsqrt(denominator_squared);
}

float InverseNeutwo(float x)
{
   float clamped = min(max(x, 0.f), ONE_MINUS_EPSILON);
   float denominator_squared = mad(-clamped, clamped, 1.f);
   return clamped * rsqrt(denominator_squared);
}

// MB coordinate from LMS.
// r = L / (L + M), b = S / (L + M)
// T = (L + M) is intensity anchor and is preserved while changing purity.
float2 MB_From_LMS(float3 lms)
{
   float t = lms.x + lms.y;

   if (t <= 0.f)
   {
      return float2(0.f, 0.f);
   }

   return float2(
       DivideSafe(lms.x, t, 0.f),
       DivideSafe(lms.z, t, 0.f));
}

// Reconstruct LMS from MB coordinates while holding T = L + M fixed.
float3 LMS_From_MB_T(float2 mb, float t)
{
   float r = mb.x;
   float b = mb.y;
   return float3(r * t, (1.f - r) * t, b * t);
}

// Default adaptation white in MB space.
float2 MB_White_D65()
{
   float3 d65_xyz = XYZ_From_xyY(float3(WHITE_POINT_D65, 1.f));
   float3 d65_lms = mul(XYZ_TO_LMS_2006, d65_xyz);
   return MB_From_LMS(d65_lms);
}

// Half-space constraint for one channel in affine form:
// rgb(t) = a * t + b, and we require rgb(t) >= 0.
// Returns interval [lo, hi] where that channel is valid.
void IntervalLower0(float a, float b, out float lo, out float hi)
{
   if (abs(a) < EPSILON)
   {
      if (b >= 0.f)
      {
         lo = -INTERVAL_MAX;
         hi = INTERVAL_MAX;
      }
      else
      {
         lo = 1.f;
         hi = 0.f;
      }
      return;
   }

   float t0 = DivideSafe(-b, a, 0.f);

   if (a > 0.f)
   {
      lo = t0;
      hi = INTERVAL_MAX;
   }
   else
   {
      lo = -INTERVAL_MAX;
      hi = t0;
   }
}

struct MBPurityDebug
{
   float3 rgbOut;
   float3 rgbT0;
   float3 rgbEdgeGamut;

   // Max reachable t before first channel becomes negative.
   float tMaxGamut;
   // Applied t after user control mapping.
   float tFinal;

   // Purity for input color in normalized 0..1 form (1 / tMax for input at t=1).
   float purityCur01;
   // Requested normalized purity (user input).
   float purity01_in;
   // Effective normalized purity after shaping curve.
   float purity01_used;
};

// Main solver.
//
// There are three control modes:
// - purity_input_mode == MB_PURITY_MODE_NORMALIZED:
//     purity_value is 0..1 "fraction of available purity headroom".
// - purity_input_mode == MB_PURITY_MODE_DISTANCE:
//     purity_value is direct MB ray distance t.
// - purity_input_mode == MB_PURITY_MODE_SCALE_NEUTWO:
//     purity_value is a scale multiplier in Neutwo inverse space (1 = unchanged).
//
// Optional adjust mode:
// - MB_PURITY_ADJUST_NONE:
//     use selected input mode as-is.
// - MB_PURITY_ADJUST_CLIP_MAX:
//     force max purity for this hue/gamut (tFinal = tMaxGamut).
//
// Important behavior:
// - Constraint is gamut-only (RGB >= 0), no arbitrary <= 1 clamp.
// - This allows scene-linear HDR values above 1.0 to pass through naturally.
MBPurityDebug ApplyInternal(float3 rgb_linear, float purity_value, int purity_input_mode,
                            float curve_gamma, float2 mb_white_override, float t_min,
                            int purity_adjust_mode, float3x3 rgb_to_xyz_mat, float3x3 xyz_to_rgb_mat)
{
   MBPurityDebug output;

   output.purity01_in = 0.f;

   // 1) RGB -> XYZ -> LMS
   float3 xyz = mul(rgb_to_xyz_mat, rgb_linear);
   float3 lms = mul(XYZ_TO_LMS_2006, xyz);

   // T = L + M is the intensity anchor for MB purity moves.
   float t = lms.x + lms.y;

   if (t <= t_min)
   {
      output.rgbOut = rgb_linear;
      output.rgbT0 = rgb_linear;
      output.rgbEdgeGamut = rgb_linear;
      output.tMaxGamut = 0.f;
      output.purityCur01 = 0.f;
      output.purity01_used = 0.f;
      return output;
   }

   float2 white =
       (mb_white_override.x >= 0.f && mb_white_override.y >= 0.f) ? mb_white_override : MB_White_D65();

   // 2) MB ray from adapted white to input hue.
   float2 mb0 = MB_From_LMS(lms);
   float2 direction = mb0 - white;

   if (dot(direction, direction) < MB_NEAR_WHITE_EPSILON)
   {
      output.rgbOut = rgb_linear;
      output.rgbT0 = rgb_linear;
      output.rgbEdgeGamut = rgb_linear;
      output.tMaxGamut = 0.f;
      output.tFinal = 0.f;
      output.purityCur01 = 0.f;
      output.purity01_used = 0.f;
      return output;
   }

   float3 lms_t0 = LMS_From_MB_T(white, t);
   float3 xyz_t0 = mul(LMS_TO_XYZ_2006, lms_t0);
   float3 rgb_t0 = mul(xyz_to_rgb_mat, xyz_t0);
   output.rgbT0 = rgb_t0;

   // 4) Affine form of RGB along the MB ray at fixed T.
   // t = 0 -> white in MB space, t = 1 -> original input.
   float3 a = rgb_linear - rgb_t0;

   // 5) Intersect channel constraints RGB(t) >= 0 across R,G,B.
   float t_lo = 0.f;
   float t_hi = INTERVAL_MAX;

   float lo;
   float hi;

   IntervalLower0(a.x, rgb_t0.x, lo, hi);
   t_lo = max(t_lo, lo);
   t_hi = min(t_hi, hi);

   IntervalLower0(a.y, rgb_t0.y, lo, hi);
   t_lo = max(t_lo, lo);
   t_hi = min(t_hi, hi);

   IntervalLower0(a.z, rgb_t0.z, lo, hi);
   t_lo = max(t_lo, lo);
   t_hi = min(t_hi, hi);

   if (t_hi < t_lo)
   {
      output.rgbOut = max(rgb_linear, 0.f);
      output.rgbEdgeGamut = rgb_t0;
      output.tMaxGamut = 0.f;
      output.tFinal = 0.f;
      output.purityCur01 = 0.f;
      output.purity01_used = 0.f;
      return output;
   }

   float t_max = max(0.f, t_hi);
   output.tMaxGamut = t_max;

   // Current normalized purity for the input (which is at t = 1).
   float p_cur = (t_max > EPSILON) ? DivideSafe(1.f, t_max, 0.f) : 0.f;
   output.purityCur01 = saturate(p_cur);

   // Useful validation output: point on the gamut edge for this MB ray.
   float3 rgb_edge = a * t_max + rgb_t0;
   output.rgbEdgeGamut = rgb_edge;

   float t_final = 0.f;

   if (purity_adjust_mode == MB_PURITY_ADJUST_CLIP_MAX)
   {
      output.purity01_used = 1.f;
      output.purity01_in = 1.f;
      t_final = t_max;
   }
   else
   {
      if (purity_input_mode == MB_PURITY_MODE_NORMALIZED)
      {
         // Normalized mode: user operates in [0,1] of available gamut headroom.
         output.purity01_in = saturate(purity_value);
         float gamma = max(curve_gamma, 1e-6f);
         float purity01 = pow(output.purity01_in, gamma);
         output.purity01_used = purity01;
         t_final = purity01 * t_max;
      }
      else if (purity_input_mode == MB_PURITY_MODE_SCALE_NEUTWO)
      {
         // Scale mode: user controls a saturation multiplier around current purity.
         // 1 = no change, >1 increases, <1 decreases, with a smooth shoulder near edge.
         float saturation_scale = max(purity_value, 0.f);
         float purity01_target;
         if (output.purityCur01 >= ONE_MINUS_EPSILON && saturation_scale >= 1.f)
         {
            purity01_target = 1.f;
         }
         else
         {
            float z = InverseNeutwo(output.purityCur01);
            float z_scaled = z * saturation_scale;
            purity01_target = saturate(Neutwo(z_scaled));
         }
         output.purity01_in = output.purityCur01;
         output.purity01_used = purity01_target;
         t_final = purity01_target * t_max;
      }
      else
      {
         // Distance mode: user value is interpreted as MB ray distance t.
         float t_user = max(purity_value, 0.f);
         t_final = min(t_user, t_max);
         output.purity01_used =
             (t_max > EPSILON) ? saturate(DivideSafe(t_final, t_max, 0.f)) : 0.f;
         output.purity01_in = output.purity01_used;
      }
   }

   output.tFinal = t_final;

   float2 mb_final = white + t_final * direction;

   float3 lms_final = LMS_From_MB_T(mb_final, t);
   float3 xyz_final = mul(LMS_TO_XYZ_2006, lms_final);
   float3 rgb_final = mul(xyz_to_rgb_mat, xyz_final);

   // Clean up float math
   output.rgbOut = max(rgb_final, 0.f);

   return output;
}

MBPurityDebug ApplyBT2020(float3 rgb2020_linear, float purity01, float curve_gamma = 1.f,
                          float2 mb_white_override = float2(-1.f, -1.f), float t_min = 1e-6f,
                          int purity_adjust_mode = MB_PURITY_ADJUST_NONE)
{
   return ApplyInternal(rgb2020_linear, purity01, MB_PURITY_MODE_NORMALIZED, curve_gamma,
                        mb_white_override, t_min, purity_adjust_mode, BT2020_TO_XYZ_MAT,
                        XYZ_TO_BT2020_MAT);
}

// src/games/borderlandsgotyenhanced/common.hlsli, ApplyHueAndPurityGrading: the hue + chrominance emulation
// block with the RenoDX BL1 defaults written in (curve_gamma 1, D65 white, t_min 1e-7; saturation 1, dechroma 0
// and highlight_saturation 0 make its purity_scale tail the identity, so that tail is not carried). `lum` is
// only read by that tail and is not a parameter here.
float3 ApplyHueEmulationBT2020(float3 ungraded_bt2020, float3 reference_bt2020, float hue_emulation, float chrominance_emulation)
{
   const float curve_gamma = 1.f;
   const float2 mb_white_override = float2(-1.f, -1.f);
   const float t_min = 1e-7f;

   float3 color_bt2020 = ungraded_bt2020;
   if (hue_emulation == 0.f && chrominance_emulation == 0.f)
   {
      return color_bt2020;
   }

   const float kNearWhiteEpsilon = MB_NEAR_WHITE_EPSILON;
   const float2 white = (mb_white_override.x >= 0.f && mb_white_override.y >= 0.f)
                            ? mb_white_override
                            : MB_White_D65();

   float color_purity01 = ApplyBT2020(
                              color_bt2020, 1.f, 1.f, mb_white_override, t_min)
                              .purityCur01;

   // MB hue + purity emulation (analog of OkLab hue/chrominance section).
   if (hue_emulation != 0.f || chrominance_emulation != 0.f)
   {
      float reference_purity01 = ApplyBT2020(
                                     reference_bt2020, 1.f, 1.f, mb_white_override, t_min)
                                     .purityCur01;

      float purity_current = color_purity01;
      float purity_ratio = 1.f;
      float3 hue_seed_bt2020 = color_bt2020;

      if (hue_emulation != 0.f)
      {
         float3 target_lms = mul(XYZ_TO_LMS_2006,
                                 mul(BT2020_TO_XYZ_MAT, color_bt2020));
         float3 reference_lms = mul(XYZ_TO_LMS_2006,
                                    mul(BT2020_TO_XYZ_MAT, reference_bt2020));

         float target_t = target_lms.x + target_lms.y;
         if (target_t > t_min)
         {
            float2 target_direction = MB_From_LMS(target_lms) - white;
            float2 reference_direction = MB_From_LMS(reference_lms) - white;

            float target_len_sq = dot(target_direction, target_direction);
            float reference_len_sq = dot(reference_direction, reference_direction);

            if (target_len_sq > kNearWhiteEpsilon || reference_len_sq > kNearWhiteEpsilon)
            {
               float2 target_unit = (target_len_sq > kNearWhiteEpsilon)
                                        ? target_direction * rsqrt(target_len_sq)
                                        : float2(0.f, 0.f);
               float2 reference_unit = (reference_len_sq > kNearWhiteEpsilon)
                                           ? reference_direction * rsqrt(reference_len_sq)
                                           : target_unit;

               if (target_len_sq <= kNearWhiteEpsilon)
               {
                  target_unit = reference_unit;
               }

               float2 blended_unit = lerp(target_unit, reference_unit, hue_emulation);
               float blended_len_sq = dot(blended_unit, blended_unit);
               if (blended_len_sq <= kNearWhiteEpsilon)
               {
                  blended_unit = (hue_emulation >= 0.5f) ? reference_unit : target_unit;
                  blended_len_sq = dot(blended_unit, blended_unit);
               }
               blended_unit *= rsqrt(max(blended_len_sq, 1e-20f));

               float seed_len = sqrt(max(target_len_sq, 0.f));
               if (seed_len <= 1e-6f)
               {
                  seed_len = sqrt(max(reference_len_sq, 0.f));
               }
               seed_len = max(seed_len, 1e-6f);

               hue_seed_bt2020 = mul(
                   XYZ_TO_BT2020_MAT,
                   mul(LMS_TO_XYZ_2006,
                       LMS_From_MB_T(white + blended_unit * seed_len, target_t)));

               float purity_post = ApplyBT2020(
                                       hue_seed_bt2020, 1.f, 1.f, mb_white_override, t_min)
                                       .purityCur01;
               purity_ratio = SafeDivision(purity_current, purity_post, 1.f);
               purity_current = purity_post;
            }
         }
      }

      if (chrominance_emulation != 0.f)
      {
         float target_purity_ratio = SafeDivision(reference_purity01, purity_current, 1.f);
         purity_ratio = lerp(purity_ratio, target_purity_ratio, chrominance_emulation);
      }

      float applied_purity01 = saturate(purity_current * max(purity_ratio, 0.f));
      color_bt2020 = ApplyBT2020(
                         hue_seed_bt2020, applied_purity01, curve_gamma, mb_white_override, t_min)
                         .rgbOut;
      color_purity01 = applied_purity01;
   }

   return color_bt2020;
}
} // namespace BL1_RenoDX

#endif // LUMA_BL1_RENODX_MACLEOD_BOYNTON_HLSL
