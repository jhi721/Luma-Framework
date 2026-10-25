// DLAA passes for Yakuza Kiwami, drawn by main.cpp with "DrawFullscreenPassD3D12()" (b0: the target's width and height, then each
// pass' own constants). The scene (S) DLAA antialiases is the fp16 one the color correct (ccr) reads: the material curve's gamma
// encoding, extended above 1 in HDR. DLSS gets it in a linear-like domain (2.2, undone exactly after it) and camera motion only.

#include "../Includes/MotionVectorFill.hlsl"

Texture2D<float4> source : register(t0);

// The scene depth (D32S8, standard Z) into an R32 copy at the SSAO draw: the game clears it again before ccr, where DLSS reads it
float4 copy_ps(float4 position : SV_Position) : SV_Target
{
   return source.Load(int3(position.xy, 0));
}

cbuffer MotionVectorConstants : register(b0)
{
   float2 size;
   float2 padding;
   // Current clip space to the previous frame's, column vectors, both unjittered
   row_major float4x4 reprojection;
   float2 jitter_ndc;
}

// "source": the depth copy. UV deltas, previous minus current (see "CameraMotion()"). A point the previous camera can't project
// (w = 0) gets none: DLSS spreads a non finite motion vector through its history
float2 motion_vectors_ps(float4 position : SV_Position) : SV_Target
{
   const uint2 pixel = uint2(position.xy);
   const float2 motion = CameraMotion(pixel, uint2(size), reprojection, jitter_ndc, source.Load(int3(pixel, 0)).x);
   return all(isfinite(motion)) ? motion : 0.0;
}

// S into DLSS' input, alpha kept for nothing (DLSS ignores it). A non finite pixel of the fp16 scene would spread through DLSS'
// history: NaN goes to 0 (max), infinity and its overflow from the power to fp16's largest value
float4 decode_ps(float4 position : SV_Position) : SV_Target
{
   const float4 color = source.Load(int3(position.xy, 0));
   return float4(min(pow(max(color.rgb, 0.0), 2.2), 65504.0), color.a);
}

// DLSS' output back into S, drawn with an RGB write mask: some ccr permutations read S' alpha
float4 encode_ps(float4 position : SV_Position) : SV_Target
{
   return float4(pow(max(source.Load(int3(position.xy, 0)).rgb, 0.0), 1.0 / 2.2), 1.0);
}
