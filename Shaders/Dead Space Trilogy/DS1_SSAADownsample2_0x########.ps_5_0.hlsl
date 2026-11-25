// Hashes registered from C++ ("redirected_shader_hashes"): originally 0xB10E5C9B.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space (2008): DeadSpaceFixes' SSAA downsample (SSAAScale above 1, dgVoodoo 2.87.6 -> ps_5_0 0xB10E5C9B 2x2 taps, 0x06CF49DC 3x3,
// 0x5406A8BC 4x4), the supersampled frame box averaged into the back buffer (Dead Space 2's MarkerPatch SSAA replacement). Transcribed
// from the translation: TAPS x TAPS bilinear taps spread evenly over the back buffer pixel's footprint. DeadSpaceFixes averages in
// linear (SRGBTEXTURE in, SRGBWRITEENABLE out, where the device supports them): the fp16 mirrors dropped that pair, so the sRGB
// decode and encode are done here.

#ifndef SSAA_TAPS
#define SSAA_TAPS 2
#endif

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.xy one back buffer pixel in texture coordinates ("g_DestPixel")
}

Texture2D<float4> t0 : register(t0); // The supersampled frame
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   float3 sum = 0.0;
   [unroll] for (int y = 0; y < SSAA_TAPS; y++)
   {
      [unroll] for (int x = 0; x < SSAA_TAPS; x++)
      {
         const float2 offset = (float2(x, y) + 0.5) / SSAA_TAPS - 0.5;
         const float3 color = ApplyDgvMask(t0.SampleLevel(s0_s, cb4[8].xy * offset + v5.xy, 0.0), cb3[44], cb3[45]).rgb;
         sum += gamma_sRGB_to_linear(color, GCT_MIRROR);
      }
   }
   o0 = float4(linear_to_sRGB_gamma(sum / (SSAA_TAPS * SSAA_TAPS), GCT_MIRROR), 1.0);
}
