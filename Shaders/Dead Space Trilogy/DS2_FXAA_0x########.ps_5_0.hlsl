// Hashes registered from C++ ("redirected_shader_hashes"): originally 0x0A91885E.
// clang-format off
#include "Includes/Common.hlsl" // game-local: LumaGameSettings before the shared Settings.hlsl, keep FIRST
#include "Includes/DS_DisplayMap.hlsl"
// clang-format on
#include "../Includes/DgVoodoo.hlsl"

// Dead Space 2: MarkerPatch's FXAA (ImprovedAntiAliasingMode 1, in the game's EdgeAA slot: the frame's last pass before the UI; dgVoodoo
// 2.87.6 -> ps_5_0 0x0A91885E), then the HDR display map (Includes/DS_DisplayMap.hlsl), as the SMAA neighborhood replacement does. The
// translation is stock FXAA 3.11 Quality: preset 39, subpix 0.5, edge threshold 0.125, min 0.0312, luma = BT.601 weights on the
// gamma encoded frame, posM half a texel off the texcoord. Re-expressed through the shared include with a sampler that applies dgVoodoo's
// masks and puts that luma in alpha. With an upscaler or Luma's SMAA (LumaData.CustomData1 != 0) the frame is already antialiased:
// only the display map runs.

cbuffer cb3 : register(b3)
{
   float4 cb3[77]; // dgVoodoo state: [46]/[47] t1's masks
}

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants at c<N> = cb4[N + 8]: c1.xy = 1 / frame size
}

Texture2D<float4> t1 : register(t1); // The frame (t0 is an unused 1x1 texture)
SamplerState s1_s : register(s1);

float4 SampleFrame(float2 uv)
{
   return ApplyDgvMask(t1.SampleLevel(s1_s, uv, 0.0), cb3[46], cb3[47]);
}

// RGB plus the translation's luma in alpha
float4 SampleRGBL(float2 uv)
{
   const float3 rgb = SampleFrame(uv).rgb;
   return float4(rgb, dot(rgb, float3(0.299, 0.587, 0.114)));
}

#define FXAA_PC              1
#define FXAA_QUALITY__PRESET 39
#define FXAA_GATHER4_ALPHA   0
// No FXAA_HLSL_* API: the texture macros below read through "SampleRGBL"
#define FxaaInt2               float2
#define FxaaTex                uint
#define FxaaTexTop(t, p)       SampleRGBL(p)
#define FxaaTexOff(t, p, o, r) SampleRGBL((p) + (o) * (r))
#include "../Includes/FXAA.hlsl"

void main(DGV_MAIN_SIGNATURE)
{
   const float2 rcp_frame = cb4[9].xy;
   const float2 pos = v5.xy + 0.5 * rcp_frame;
   const float4 frame = SampleFrame(pos);
   float3 color = frame.rgb;
   [branch] if (LumaData.CustomData1 == 0)
   {
      const FxaaTex unused = 0;
      color = FxaaPixelShader(pos, 0.0, unused, unused, unused, rcp_frame, 0.0, 0.0, 0.0, 0.5, 0.125, 0.0312, 0.0, 0.0, 0.0, 0.0).rgb;
   }
   // LumaData.CustomData3: the display map runs elsewhere (Luma_DS_DisplayMap already ran, or runs later before the UI, or the present
   // blit maps the frame): only frames with the uber post map here
   [branch] if (LumaData.CustomData3 == 0)
   {
      color = DisplayMapCanvas(color, v0.xy * rcp_frame);
   }
   o0 = float4(color, frame.a);
}
