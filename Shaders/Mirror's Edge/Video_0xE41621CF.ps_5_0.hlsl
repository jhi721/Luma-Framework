// Mirror's Edge - Bink movie pass (Y'CbCr -> R'G'B', D3D9 Q131073). Same hash and body as BL2's (Video_0xE41621CF there).
// The Y'CbCr -> R'G'B' matrix is cb4[8..10], cb4[11] = (constant column, -, -, alpha). dgVoodoo dropped the SM3
// saturate (the UNORM canvas clamped for free), so it is restored before the light AutoHDR.
// clang-format off
#include "Includes/Common.hlsl"
#include "../Includes/DgVoodoo.hlsl"
// clang-format on

// 1 = the alpha variant (0xB19ED21F): alpha from a 4th plane (t3) times cb4[11].w
#ifndef BINK_ALPHA
#define BINK_ALPHA 0
#endif

// Bink is low bitrate: a low peak keeps compression blocks out of the highlights
#define VIDEO_AUTO_HDR_PEAK_NITS 250.0

Texture2D<float4> t0 : register(t0); // Y'
Texture2D<float4> t1 : register(t1); // Cb/Cr
Texture2D<float4> t2 : register(t2); // Cr/Cb
#if BINK_ALPHA
Texture2D<float4> t3 : register(t3); // alpha
#endif
SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);
#if BINK_ALPHA
SamplerState s3_s : register(s3);
#endif

cbuffer cb3 : register(b3)
{
   float4 cb3[77];
}
cbuffer cb4 : register(b4)
{
   float4 cb4[236];
}

void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   const float luma = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).x; // Y'
   const float cb = ApplyDgvMask(t1.Sample(s1_s, v5.xy), cb3[46], cb3[47]).x;
   const float cr = ApplyDgvMask(t2.Sample(s2_s, v5.xy), cb3[48], cb3[49]).x;
   const float4 ycbcr = float4(luma, cb, cr, cb4[11].x);
   float3 color = saturate(float3(dot(cb4[8], ycbcr), dot(cb4[9], ycbcr), dot(cb4[10], ycbcr)));
#if BINK_ALPHA
   o0.w = ApplyDgvMask(t3.Sample(s3_s, v5.xy), cb3[50], cb3[51]).x * cb4[11].w;
#else
   o0.w = cb4[11].w;
#endif

   color = gamma_to_linear(color);
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      // Boost 0 = peak at SDR white: PumboAutoHDR no-ops. It also no-ops in SDR output (peak == paper white).
      const float peakNits = lerp(sRGB_WhiteLevelNits, VIDEO_AUTO_HDR_PEAK_NITS, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      color = PumboAutoHDR(color, peakNits, LumaSettings.GamePaperWhiteNits);
   }
   o0.xyz = linear_to_gamma(PreScaleForUIPaperWhite(color));
}
