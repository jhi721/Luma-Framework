// Katana engine DOF merge: upsamples the half resolution DOF color (CoC blend weight in alpha) into the full resolution scene
// (alpha blended). A family: 5 taps (center counted twice) / 6, 3x3 tent / 16 (P5S_DOF_MERGE_TENT), or 4 corner taps faded in by
// alpha (< 1, the reduction variant, P5S_DOF_MERGE_REDUCTION); the other two include this file.
// Luma: the tap offset is the DOF texture's texel from its size instead of "g_vTexelSize", as under DLSS/FSR upscaling the DOF
// textures are output resolution (identical without upscaling).
// clang-format off
#include "Includes/Common.hlsl"
#include "Includes/cbDof.hlsl"
// clang-format on

#ifndef P5S_DOF_MERGE_TENT
#define P5S_DOF_MERGE_TENT 0
#endif
#ifndef P5S_DOF_MERGE_REDUCTION
#define P5S_DOF_MERGE_REDUCTION 0
#endif

SamplerState sampleLinear_s : register(s7);
Texture2D<float4> g_t4MainMap : register(t0);

float4 SampleDof(float2 uv)
{
   return g_t4MainMap.SampleLevel(sampleLinear_s, uv, 0);
}

void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, float2 w1 : TEXCOORD1, out float4 o0 : SV_Target0)
{
   float2 size;
   g_t4MainMap.GetDimensions(size.x, size.y);
   const float2 texel = 1.0 / size;
   const float4 corners = SampleDof(w1 - texel) + SampleDof(w1 + float2(texel.x, -texel.y)) + SampleDof(w1 + float2(-texel.x, texel.y)) + SampleDof(w1 + texel);
#if P5S_DOF_MERGE_TENT
   const float4 edges = SampleDof(w1 + float2(texel.x, 0.0)) + SampleDof(w1 - float2(texel.x, 0.0)) + SampleDof(w1 + float2(0.0, texel.y)) + SampleDof(w1 - float2(0.0, texel.y));
   o0 = (SampleDof(w1) * 4.0 + edges * 2.0 + corners) / 16.0;
#elif P5S_DOF_MERGE_REDUCTION
   const float4 center = SampleDof(w1);
   const float cornerWeight = min(10.0 - center.a * 10.0, 1.0);
   o0 = center.a < 1.0 ? (corners * cornerWeight + center * 2.0) / (cornerWeight * 4.0 + 2.0) : center;
#else
   o0 = (SampleDof(w1) * 2.0 + corners) / 6.0;
#endif
}
