// UE3's cel outline (EdgeDetectionPostProcessBlendPixelShader.usf): a Sobel filter on the linearized scene depth, 9 taps (the VS
// puts the 8 neighbors' UVs in TEXCOORD0-3 and the center's in TEXCOORD4), multiplied into the scene. Transcribed from the
// disassembly of 0x08DC66D1 (world) and 0xC3C26F77 (first person weapon, WEAPON 1: depth x 10, no nearer-only clamp, saturated edge).
// Below ScreenPercentage 100 the scene is a top-left sub-rect of the depth copy: the taps are clamped to its last texel, or the
// border taps read stale depth outside it and draw a black line along the bottom and right edges. At 100% the clamp is the
// sampler's own (the texture edge).

#include "Common.hlsl" // game-local: LumaData.GameData

cbuffer Globals : register(b0)
{
   float4 HFilterDiagCoeff;
   float4 HFilterAxisCoeff;
   float4 VFilterDiagCoeff;
   float4 VFilterAxisCoeff;
   float4 MiscParameters; // z: edge exponent
};
cbuffer PSOffsetConstants : register(b2)
{
   float4 ScreenPositionScaleBias;
   float4 MinZ_MaxZRatio; // zw: device z to 1 / view z
};

SamplerState SceneDepthTextureSampler : register(s0);
Texture2D<float4> SceneDepthTexture : register(t0);

float OutlineViewDepth(float2 uv, float2 max_uv)
{
   const float device_depth = SceneDepthTexture.Sample(SceneDepthTextureSampler, min(uv, max_uv)).x;
#if WEAPON
   return 1.0 / max(device_depth * MinZ_MaxZRatio.z * 10.0 - MinZ_MaxZRatio.w, 0.0);
#else
   return 1.0 / max(device_depth * MinZ_MaxZRatio.z - MinZ_MaxZRatio.w, 0.0);
#endif
}

void main(float4 v0 : TEXCOORD0, float4 v1 : TEXCOORD1, float4 v2 : TEXCOORD2, float4 v3 : TEXCOORD3, float2 v4 : TEXCOORD4, out float4 o0 : SV_Target0)
{
   float2 texture_size;
   SceneDepthTexture.GetDimensions(texture_size.x, texture_size.y);
   const float2 max_uv = (LumaData.GameData.SceneRenderSize - 0.5) / texture_size;

   const float center = OutlineViewDepth(v4, max_uv);
   float4 axis = float4(OutlineViewDepth(v2.xy, max_uv), OutlineViewDepth(v2.zw, max_uv), OutlineViewDepth(v3.xy, max_uv), OutlineViewDepth(v3.zw, max_uv));
   float4 diagonal = float4(OutlineViewDepth(v0.xy, max_uv), OutlineViewDepth(v0.zw, max_uv), OutlineViewDepth(v1.xy, max_uv), OutlineViewDepth(v1.zw, max_uv));
#if !WEAPON
   // Only neighbors farther than the center make an edge
   axis = max(axis, center);
   diagonal = max(center, diagonal);
#endif
   axis = (axis - center) / center;
   diagonal = (diagonal - center) / center;
   const float horizontal = dot(axis * HFilterAxisCoeff + diagonal * HFilterDiagCoeff, 1.0);
   const float vertical = dot(axis * VFilterAxisCoeff + diagonal * VFilterDiagCoeff, 1.0);
#if WEAPON
   float edge = saturate(sqrt(horizontal * horizontal + vertical * vertical));
#else
   float edge = min(sqrt(horizontal * horizontal + vertical * vertical), 1.0);
#endif
   edge = 1.0 - exp2(log2(max(edge, 0.0001)) * MiscParameters.z);
   const float3 color = max(edge, 0.0);
   o0 = float4(color, color.z > 0.956863 ? 0.0 : 1.0);
}
