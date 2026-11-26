// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x86AF7742.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space (2008): DeadSpaceFixes' FXAA (UseFXAA, its replacement of the game's EdgeAAFast in the standalone EdgeAA pass that runs on a
// copy of the graded frame; dgVoodoo 2.87.6 -> ps_5_0 0x86AF7742), the counterpart of Dead Space 2's MarkerPatch FXAA. The translation is
// stock FXAA 3.11 Quality: preset 29, subpix 0.5, edge threshold 0.125, min 0.0625, luma = BT.601 weights on the gamma encoded frame,
// posM half a texel off the texcoord ("g_halfPixelOffset".zw). Re-expressed through the shared include with a sampler that applies
// dgVoodoo's masks and puts that luma in alpha. With an upscaler or Luma's SMAA (LumaData.CustomData1 != 0, t0 is then Luma's output
// in this slot) the frame is already antialiased and is only copied. The display map runs after the post chain (main.cpp).

#ifndef FXAA_HALF_PIXEL_OFFSET
#define FXAA_HALF_PIXEL_OFFSET cb4[9] // c1 (EdgeAAFast's register)
#endif

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [44]/[45] t0's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: "g_halfPixelOffset" = (0, 0, 0.5 / width, 0.5 / height)
}

Texture2D<float4> t0 : register(t0); // The frame copy
SamplerState s0_s : register(s0);

float4 SampleFrame(float2 uv)
{
   return ApplyDgvMask(t0.SampleLevel(s0_s, uv, 0.0), cb3[44], cb3[45]);
}

// RGB plus the translation's luma in alpha
float4 SampleRGBL(float2 uv)
{
   const float3 rgb = SampleFrame(uv).rgb;
   return float4(rgb, dot(rgb, float3(0.299, 0.587, 0.114)));
}

#define FXAA_PC              1
#define FXAA_QUALITY__PRESET 29
#define FXAA_GATHER4_ALPHA   0
// No FXAA_HLSL_* API: the texture macros below read through "SampleRGBL"
#define FxaaInt2               float2
#define FxaaTex                uint
#define FxaaTexTop(t, p)       SampleRGBL(p)
#define FxaaTexOff(t, p, o, r) SampleRGBL((p) + (o) * (r))
#include "../Includes/FXAA.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float2 rcp_frame = FXAA_HALF_PIXEL_OFFSET.zw * 2.0;
   const float2 pos = v5.xy + FXAA_HALF_PIXEL_OFFSET.zw;
   const float4 frame = SampleFrame(pos);
   float3 color = frame.rgb;
   [branch] if (LumaData.CustomData1 == 0)
   {
      const FxaaTex unused = 0;
      color = FxaaPixelShader(pos, 0.0, unused, unused, unused, rcp_frame, 0.0, 0.0, 0.0, 0.5, 0.125, 0.0625, 0.0, 0.0, 0.0, 0.0).rgb;
   }
   o0 = float4(color, frame.a);
}
