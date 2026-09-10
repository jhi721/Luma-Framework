#ifndef LUMA_MELE_TONEMAP_REFERENCE_COLOR
#define LUMA_MELE_TONEMAP_REFERENCE_COLOR

#include "../../Includes/Oklab.hlsl" // Same include root as this folder's Common.hlsl, not the body's.
#include "Tonemap_MELE_ExperimentConfig.hlsli"

// Soft clip emulation for family 05, the one permutation whose vanilla blowout is a real hard clip.
// Prerequisites already met by the caller: ../Includes/Color.hlsl, ../Includes/Math.hlsl, ../Includes/DICE.hlsl.
//
// This is a Luma contract, not a port. RenoDX's own colour helpers transfer ABSOLUTE chroma and can raise it,
// and its hue transfer blends colour vectors and renormalizes their length rather than rotating an angle. Here
// the blowout control moves RELATIVE chroma C/L and can only lower it, the hue control is a uniform rotation
// along the shorter arc, and photometric Y is restored explicitly at the end. Do not describe one as the other.

// The soft per-channel reference K(x; k, P): identity below k, then easing toward P without ever passing it.
//
// Built on DICE::RangeCompress, the shared exponential primitive, rather than on DICE::LuminanceCompress,
// which computes the same k + (P-k)*(1 - exp(-(x-k)/(P-k))) but cannot be called at all under /Ges /WX: with
// its default InMaxValue it constant-folds FLT_MAX - ShoulderStart and fxc rejects that as X4122, even though
// the result is dead whenever ConsiderMaxValue is false. RangeCompress has no such subtraction on its default
// path. Nothing in Shaders/Includes is edited to work around this and no misleading finite sentinel is passed.
//
// DICE.hlsl:113 also compiles out its own below-shoulder guard, so that branch lives here either way.
//
// P is the ceiling of an SDR-like artistic reference. It is not a display peak and must not be driven by one,
// and neither the scene nor the working HDR is ever limited to it.
float3 MELE_SoftClip(float3 x, float k)
{
   const float range = 1.0 - k;
   const float3 compressed = k + range * DICE::RangeCompress((x - k) / range);
   return float3(x.x <= k ? x.x : compressed.x, x.y <= k ? x.y : compressed.y, x.z <= k ? x.z : compressed.z);
}

// Shortest-arc hue interpolation. Nothing in the shared includes provides one: RestoreHueAndChrominance lerps
// the ab vector instead, which is why it degenerates against an achromatic reference. Wrapping the difference
// into (-pi, pi] is what stops a rotation from taking the long way round the +-pi seam.
float MELE_ShortArcHueLerp(float from_hue, float to_hue, float strength)
{
   float delta = to_hue - from_hue;
   delta -= 6.28318530718 * round(delta / 6.28318530718);
   return from_hue + strength * delta;
}

// Move the working colour toward the soft reference in hue and in relative chroma, then put the original
// BT.709 luminance back with one scalar. Oklab L is a perceptual lightness, not photometric Y, so preserving
// L is not preserving Y and the restore cannot be skipped.
//
// Both strengths at zero returns the working value untouched. The strengths are cbuffer sliders now, so this
// is a runtime branch rather than a compile-time one. That result is the working HDR, which is NOT the
// same thing as the legacy value the feature selector at 0 produces; the two must not be reported as one.
//
// A black reference is not a white one, so the transfer is skipped rather than treated as a white target. An
// out-of-gamut round trip falls back to the working value for the whole triple: clamping negatives afterwards
// would preserve neither hue nor C/L, so it must not be done and then called preservation.
float3 MELE_ReferenceCombine(float3 work_hdr, float3 soft_reference, float hue_strength, float blowout_strength)
{
   if (hue_strength == 0.0 && blowout_strength == 0.0)
   {
      return work_hdr;
   }
   const float target_luminance = GetLuminance(work_hdr, CS_BT709);
   const float3 work_lch = Oklab::linear_srgb_to_oklch(work_hdr);
   const float3 reference_lch = Oklab::linear_srgb_to_oklch(soft_reference);
   if (!(work_lch.x > 1e-6) || !(reference_lch.x > 1e-6))
   {
      return work_hdr;
   }
   const float work_relative_chroma = work_lch.y / work_lch.x;
   const float reference_relative_chroma = reference_lch.y / reference_lch.x;
   // Only ever downward: lerping toward min() cannot add chroma the working value did not have.
   const float relative_chroma = lerp(work_relative_chroma, min(work_relative_chroma, reference_relative_chroma), blowout_strength);
   // A near-neutral reference has no reliable hue, so suppress the rotation there. Blowout stays available,
   // because a white reference is a perfectly good blowout target even when its angle is noise.
   const float hue = (reference_lch.y > 1e-4) ? MELE_ShortArcHueLerp(work_lch.z, reference_lch.z, hue_strength) : work_lch.z;
   const float3 transferred = Oklab::oklch_to_linear_srgb(float3(work_lch.x, relative_chroma * work_lch.x, hue));
   const float transferred_luminance = GetLuminance(transferred, CS_BT709);
   if (IsAnyNaN_Strict(transferred) || any(IsInfinite_Strict(transferred)) || !(abs(transferred_luminance) > 1e-9))
   {
      return work_hdr;
   }
   return transferred * (target_luminance / transferred_luminance);
}

#endif // LUMA_MELE_TONEMAP_REFERENCE_COLOR
