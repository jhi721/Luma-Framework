// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x113CFB69.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 2: MarkerPatch's SSAA downsample (ImprovedAntiAliasingMode 3/4, dgVoodoo 2.87.6 -> ps_5_0 0x113CFB69), the supersampled
// frame box averaged into the back buffer. Transcribed from the translation. MarkerPatch averages in linear (SRGBTEXTURE in,
// SRGBWRITEENABLE out): the fp16 mirrors dropped that pair, so the sRGB decode and encode are done here.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c0.xy one tap's step (1 / (taps * back buffer size)), c0.z taps per axis
}

Texture2D<float4> t0 : register(t0); // The supersampled frame
SamplerState s0_s : register(s0);

void main(DGV_MAIN_SIGNATURE)
{
   const float taps = cb4[8].z;
   const float2 start = cb4[8].xy * ((taps - 1.0) * -0.5) + v5.xy;
   float3 sum = 0.0;
   [loop] for (float y = 0.0; y < taps; y += 1.0)
   {
      [loop] for (float x = 0.0; x < taps; x += 1.0)
      {
         const float3 color = ApplyDgvMask(t0.SampleLevel(s0_s, cb4[8].xy * float2(x, y) + start, 0.0), cb3[44], cb3[45]).rgb;
         sum += gamma_sRGB_to_linear(color, GCT_MIRROR);
      }
   }
   o0 = float4(linear_to_sRGB_gamma(sum * DgVoodooRcp(taps * taps), GCT_MIRROR), 1.0);
}
