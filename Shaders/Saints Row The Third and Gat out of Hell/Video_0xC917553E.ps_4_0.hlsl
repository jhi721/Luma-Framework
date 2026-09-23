// rl_prim_2d_bink_s_01: Bink video, BT.601 limited-range Y'CbCr -> R'G'B' straight onto the swapchain.
// Vanilla relied on the r8g8b8a8 swapchain to clamp the conversion; on the upgraded FP16 swapchain it leaks negatives
// (black bars and dark scenes, ~46% of a frame) and whites up to 1.25. saturate() restores exactly that clamp: a UNORM
// target clamps the source colour before blending. In HDR, an optional light AutoHDR adds highlights on top; it stays
// conservative because the videos are low bitrate and compression artifacts blow up when pushed hard.

#include "Includes/Common.hlsl"

// Video AutoHDR peak at full boost.
static const float VideoAutoHDRPeakNits = 250.0;

cbuffer vc1 : register(b1)
{
   float2 Prim_tex_texel_size : packoffset(c8);
}

cbuffer vc4 : register(b4)
{
   float4 Tint_color : packoffset(c1);
   float Alpha_test_ref : packoffset(c17.x);
}

SamplerState Y_texSampler : register(s0);
SamplerState Cr_texSampler : register(s1);
SamplerState Cb_texSampler : register(s2);
Texture2D<float4> Y_texTexture : register(t0);
Texture2D<float4> Cr_texTexture : register(t1);
Texture2D<float4> Cb_texTexture : register(t2);

void main(float4 pos : SV_Position, float2 uv : TEXCOORD0, float4 vertexColor : COLOR0, out float4 o0 : SV_Target0)
{
   const float y = Y_texTexture.Sample(Y_texSampler, uv + Prim_tex_texel_size).x;
   const float cr = Cr_texTexture.Sample(Cr_texSampler, uv).x;
   const float cb = Cb_texTexture.Sample(Cb_texSampler, uv).x;
   float4 color;
   color.r = dot(float3(1.164124, 1.595795, -0.870655), float3(y, cr, 1.0));
   color.g = dot(float4(1.164124, -0.813477, -0.391449, 0.529705), float4(y, cr, cb, 1.0));
   color.b = dot(float3(1.164124, 2.017822, -1.081669), float3(y, cb, 1.0));
   color.a = 1.0;
   color *= vertexColor;
   if (color.a * Tint_color.a - Alpha_test_ref < 0.0)
      discard;
   o0 = saturate(color * Tint_color);

   float3 lin = gamma_to_linear(o0.rgb);
   // Boost 0 = peak at sRGB white, where PumboAutoHDR no-ops; it also no-ops in SDR, where the peak is paper white.
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      const float peakNits = lerp(sRGB_WhiteLevelNits, VideoAutoHDRPeakNits, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      lin = PumboAutoHDR(lin, peakNits, GamePaperWhiteNits);
   }
   // Drawn with the UI, so pre-scaled like the scene to land at the scene paper white.
   o0.rgb = SR_EncodeOutput(lin);
}
