// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings (VideoAutoHDR*) before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// The Witcher EE Bink movie pass ("_yuv2rgb_.bfx" ps_2_0, dgVoodoo 2.87.3 -> ps_5_0 0x72C37F0F), YUV -> RGB straight onto the
// canvas. Transcribed from the translated disassembly, then the vanilla clamp (the 8-bit canvas gave it, the fp16 mirror doesn't)
// and a light PumboAutoHDR (Mass Effect 2007's). The canvas stays gamma encoded, 1 = paper white: the present blit maps it.

// Light AutoHDR on movies. Peak kept low: Bink is low-bitrate and pushing peak amplifies block artifacts.
// PumboAutoHDR self-noops in SDR (display peak == paper white), so no display branch is needed.
#ifndef ENABLE_VIDEO_AUTO_HDR
#define ENABLE_VIDEO_AUTO_HDR 1
#endif
#ifndef VIDEO_AUTO_HDR_PEAK_NITS
#define VIDEO_AUTO_HDR_PEAK_NITS 250.0
#endif

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo's own constants: the texture format emulation masks, one (and, or) pair per sampler, s0 at 44/45
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants, c<N> at cb4[N] for pixel shaders; c8-c10 = the YUV -> RGB rows
}

Texture2D<float4> t0 : register(t0); // Y plane
Texture2D<float4> t1 : register(t1); // chroma plane
Texture2D<float4> t2 : register(t2); // chroma plane

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

// Full dgVoodoo interpolator set; v5 = TEXCOORD0 (Y) and v6 = TEXCOORD1 (chroma planes) carry the UVs
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
   float4 yuv1;
   yuv1.x = ApplyDgvMask(t0.Sample(s0_s, v5.xy), cb3[44], cb3[45]).x;
   yuv1.y = ApplyDgvMask(t1.Sample(s1_s, v6.xy), cb3[46], cb3[47]).x;
   yuv1.z = ApplyDgvMask(t2.Sample(s2_s, v6.xy), cb3[48], cb3[49]).x;
   yuv1.w = 1.0;
   float3 rgb = float3(dot(cb4[8], yuv1), dot(cb4[9], yuv1), dot(cb4[10], yuv1));

   rgb = saturate(rgb);
#if ENABLE_VIDEO_AUTO_HDR
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      // boost 0 = peak at sRGB white (80 nits) -> PumboAutoHDR no-ops (off); 1 = full VIDEO_AUTO_HDR_PEAK_NITS.
      const float peakNits = lerp(sRGB_WhiteLevelNits, VIDEO_AUTO_HDR_PEAK_NITS, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      rgb = linear_to_gamma(PumboAutoHDR(gamma_to_linear(rgb), peakNits, LumaSettings.GamePaperWhiteNits));
   }
#endif
   o0 = float4(rgb, 1.0);
}
