#ifndef LUMA_MELE_TONEMAP_HUE_REFERENCE
#define LUMA_MELE_TONEMAP_HUE_REFERENCE

#include "../../Includes/ACES.hlsl"     // BT709 <-> AP1 matrices for the gamut clamp.
#include "../../Includes/Oklab.hlsl"    // Same include root as this folder's Common.hlsl, not the body's.
#include "Tonemap_MELE_HDRBridge.hlsli" // MELE_IsFinite; needs ../Includes/Reinhard.hlsl before it.

// Hue-only transfer for family 05, adapted from renodx::color::correct::HueOKLab
// (renodx/src/shaders/colorcorrect.hlsl) with RenoDX's AP1-positive clamp
// (renodx/src/shaders/color/clamp.hlsl) rebuilt on Luma's own ACES matrices.
//
// WHAT MOVES AND WHAT DOES NOT. Only the direction of the OKLab (a, b) vector is allowed to travel
// toward the reference. The target keeps its perceptual lightness L and the magnitude of its own
// chroma, so the reference sets no brightness, no saturation and no range - it is a hue donor and
// nothing else. Describing this as restoring the vanilla SDR colour would be wrong.
//
// The rescale back to the target's chroma is what makes that true, and it also bounds the operation:
// the blended vector is always renormalized to a length the target already had, so this cannot
// amplify anything. The one degenerate case is a blended vector of exactly zero - a fully neutral
// donor at strength 1, or exactly anti-parallel ab - where safeDivision returns 1 and the result
// comes out achromatic. That is left as it is deliberately; a donor-chroma threshold would be a new
// artistic rule, and the shipped strength is below 1 precisely so a neutral donor still leaves a
// direction to keep.
//
// GAMUT. RenoDX clamps to positive AP1 and converts back, which is wider than BT.709: a negative
// BT.709 component coming out of this is ordinary wide-gamut colour and is NOT a reason to abandon
// the correction. The whole-triple rejection an earlier family 05 used is exactly what produced a
// visible switching boundary, and it is not coming back. The only fallback here is for arithmetic
// that actually broke.
//
// DICE, further down the shared output tail, stays the one and only display mapper.
//
// The OKLab locals below stay snake_case where the rest of this game's HDR code is camelCase: they are a
// line-by-line port and keeping the source spelling keeps the two diffable, exactly as Shaders/Includes/
// ACES.hlsl and Reinhard.hlsl do.
float3 MELE_HueReferenceOKLab(float3 target, float3 reference, float strength)
{
   if (strength == 0.0)
   {
      return target;
   }

   float3 target_lab = Oklab::linear_srgb_to_oklab(target);
   const float3 reference_lab = Oklab::linear_srgb_to_oklab(reference);

   const float2 target_ab = target_lab.yz;
   const float chroma_before = length(target_ab);
   float2 blended_ab = lerp(target_ab, reference_lab.yz, strength);
   blended_ab *= safeDivision(chroma_before, length(blended_ab), 1);

   target_lab.yz = blended_ab;
   const float3 corrected = Oklab::oklab_to_linear_srgb(target_lab);

   const float3 clamped = mul(ACES::AP1_TO_BT709_MAT, max(0.0, mul(ACES::BT709_TO_AP1_MAT, corrected)));
   return MELE_IsFinite(clamped) ? clamped : target;
}

#endif // LUMA_MELE_TONEMAP_HUE_REFERENCE
