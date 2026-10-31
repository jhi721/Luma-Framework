// DLAA passes for Yakuza Kiwami, drawn by main.cpp with "DrawFullscreenPassD3D12()" (b0: the target's width and height, then each
// pass' own constants). The scene (S) DLAA antialiases is the fp16 one the color correct (ccr) reads: the material curve's gamma
// encoding, extended above 1 in HDR. DLSS gets it in a linear-like domain (2.2, undone exactly after it), with the motion of the
// characters where their patched draws wrote it and camera motion elsewhere.

#include "Includes/MotionVectorFill.hlsl"

// For "gamma_to_linear" and "linear_to_gamma" only: Luma's cbuffers aren't in these passes' root signature, nothing here reads them
#include "Includes/Common.hlsl"

Texture2D<float4> source : register(t0);

// The scene depth (D32S8, standard Z, with the hair) into an R32 copy at the low-res depth draw: the game clears it again before ccr, where DLSS reads it
float4 copy_ps(float4 position : SV_Position) : SV_Target
{
   return source.Load(int3(position.xy, 0));
}

Texture2D<float2> object_motion : register(t1);

cbuffer MotionVectorConstants : register(b0)
{
   float2 size;
   // 1 if "object_motion" counts this frame, else t1 is a stand-in
   float use_object_motion;
   float padding;
   // Current clip space to the previous frame's, column vectors, both unjittered
   row_major float4x4 reprojection;
   float2 jitter_ndc;
}

// "source": the depth copy, "object_motion": what the patched prepass draws of the characters wrote (UV deltas, previous minus current, like the
// camera motion; MOTION_VECTOR_MARKER where nothing was written: the target is cleared to it). Everything else gets the camera motion (see
// "CameraMotion()"). A point the previous camera can't project (w = 0) gets none: DLSS spreads a non finite motion vector through its history
float2 motion_vectors_ps(float4 position : SV_Position) : SV_Target
{
   const uint2 pixel = uint2(position.xy);
   const float2 object = object_motion.Load(int3(pixel, 0));
   // Ordered compare: a NaN one gets the camera motion
   const bool is_object = use_object_motion != 0.0 && object.x < MOTION_VECTOR_MARKER;
   const float2 motion = is_object ? object : CameraMotion(pixel, uint2(size), reprojection, jitter_ndc, source.Load(int3(pixel, 0)).x);
   return all(isfinite(motion)) ? motion : 0.0;
}

// S into DLSS' input, alpha kept for nothing (DLSS ignores it). A non finite pixel of the fp16 scene would spread through DLSS'
// history: NaN goes to 0 (max), infinity and its overflow from the power to fp16's largest value
float4 decode_ps(float4 position : SV_Position) : SV_Target
{
   const float4 color = source.Load(int3(position.xy, 0));
   return float4(min(gamma_to_linear(color.rgb, GCT_POSITIVE), 65504.0), color.a);
}

// DLSS' output back into S, drawn with an RGB write mask: some ccr permutations read S' alpha
float4 encode_ps(float4 position : SV_Position) : SV_Target
{
   return float4(linear_to_gamma(source.Load(int3(position.xy, 0)).rgb, GCT_POSITIVE), 1.0);
}
