// SMAA (https://github.com/iryoku/smaa) in place of the game's compute FXAA resolve: ULTRA preset, color edge detection and depth
// predication. The input is a snapshot of the fp16 swapchain, gamma-encoded (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white), so
// highlights run past 1. Edge detection reads it as stored; blending reads its linear decode (Luma_BL_SMAALinearize) and re-encodes.

#include "../Includes/Common.hlsl"

// Written by main.cpp's FXAA resolve replacement. SmaaRtMetrics: (1/W, 1/H, W, H) at output resolution.
cbuffer SmaaMetricsCB : register(b1)
{
   float4 SmaaRtMetrics;
   // x = predication threshold scale: 2 with a valid same-size scene depth (raises the off-edge threshold against noise, lowered back
   // on geometry edges); 1 when it is missing or mismatched, which with the null predication texture is the plain ULTRA threshold
   // (0.05) rather than a frame-wide doubled 0.10 that under-detects edges. yzw unused.
   float4 SmaaPredication;
}

#define SMAA_RT_METRICS SmaaRtMetrics
#define SMAA_PRESET_ULTRA
#define SMAA_PREDICATION       1
#define SMAA_PREDICATION_SCALE SmaaPredication.x
// Predication budget:
//  - flat threshold       = SCALE * SMAA_THRESHOLD                  = 2 * 0.05       = 0.10 (rejects texture colour noise)
//  - silhouette threshold = SCALE * SMAA_THRESHOLD * (1 - STRENGTH) = 2 * 0.05 * 0.5 = 0.05, the plain ULTRA base: predication only
//    relaxes geometric edges back to base sensitivity, never below.
// THRESHOLD is 0.5 because Luma_BL_DepthExtract.hlsl feeds a unitless edge-ness in [0,1], not a depth: half-way means "the extract
// called this a silhouette". Calibrate its tolerance, not this number.
#define SMAA_PREDICATION_STRENGTH  0.5
#define SMAA_PREDICATION_THRESHOLD 0.5
#define SMAA_CUSTOM_SL
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
#define SMAATexture2D(tex)                            Texture2D tex
#define SMAATexturePass2D(tex)                        tex
#define SMAASampleLevelZero(tex, coord)               tex.SampleLevel(LinearSampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord)          tex.SampleLevel(PointSampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SMAASample(tex, coord)                        tex.Sample(LinearSampler, coord)
#define SMAASamplePoint(tex, coord)                   tex.Sample(PointSampler, coord)
#define SMAASampleOffset(tex, coord, offset)          tex.Sample(LinearSampler, coord, offset)
#define SMAA_FLATTEN                                  [flatten]
#define SMAA_BRANCH                                   [branch]
#define SMAATexture2DMS2(tex)                         Texture2DMS<float4, 2> tex
#define SMAALoad(tex, pos, sample)                    tex.Load(pos, sample)
#define SMAAGather(tex, coord)                        tex.Gather(LinearSampler, coord, 0)
#include "../Includes/SMAA.hlsl"

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
Texture2D tex2 : register(t2);

void fullscreen_triangle(uint id, out float4 position, out float2 texcoord)
{
   texcoord = float2((id << 1) & 2, id & 2);
   position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// SMAAEdgeDetection
void smaa_edge_detection_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float4 offset[3] : TEXCOORD1)
{
   fullscreen_triangle(id, position, texcoord);
   SMAAEdgeDetectionVS(texcoord, offset);
}

float2 smaa_edge_detection_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float4 offset[3] : TEXCOORD1) : SV_Target
{
   // tex0 = colorTexGamma (stored scene snapshot)
   // tex1 = predicationTex (plane-deviation edge mask)
   return SMAAColorEdgeDetectionPS(texcoord, offset, tex0, tex1);
}

// SMAABlendingWeightCalculation
void smaa_blending_weight_calculation_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float2 pixcoord : TEXCOORD1, out float4 offset[3] : TEXCOORD2)
{
   fullscreen_triangle(id, position, texcoord);
   SMAABlendingWeightCalculationVS(texcoord, pixcoord, offset);
}

float4 smaa_blending_weight_calculation_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float2 pixcoord : TEXCOORD1, float4 offset[3] : TEXCOORD2) : SV_Target
{
   // tex0 = edgesTex, tex1 = areaTex, tex2 = searchTex
   return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, tex0, tex1, tex2, 0);
}

// SMAANeighborhoodBlending
void smaa_neighborhood_blending_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float4 offset : TEXCOORD1)
{
   fullscreen_triangle(id, position, texcoord);
   SMAANeighborhoodBlendingVS(texcoord, offset);
}

float4 smaa_neighborhood_blending_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float4 offset : TEXCOORD1) : SV_Target
{
   // tex0 = colorTex (linear copy), tex1 = blendTex. Re-encode to the canvas' gamma.
   float4 color = SMAANeighborhoodBlendingPS(texcoord, offset, tex0, tex1);
   color.rgb = linear_to_gamma(color.rgb, GCT_MIRROR);
   return color;
}
