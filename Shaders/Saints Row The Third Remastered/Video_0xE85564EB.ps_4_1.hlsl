// rl_prim_2d_bink_s_01: Bink video, BT.601 limited-range Y'CbCr -> R'G'B', drawn with the GUI.
// Luma redirects these draws from the RGBA8 GUI layer to its own FP16 video layer, which compose puts between the scene and the GUI,
// so the AutoHDR highlights aren't clamped. saturate() keeps the clamp the RGBA8 GUI layer applied to the conversion (negatives and whites up to 1.25).
// The AutoHDR stays conservative because the videos are low bitrate and compression artifacts blow up when pushed hard (as in Saints Row: The Third).

#include "Includes/Common.hlsl"

// Video AutoHDR peak at full boost.
static const float VideoAutoHDRPeakNits = 250.0;

cbuffer CB_PIXEL : register(b3)
{
   float Alpha_test_ref : packoffset(c0.x);
   float4 Tint_color : packoffset(c10);
}

SamplerState Sampler_Aniso_WW : register(s1);
Texture2D<float4> Y_texTexture : register(t0);
Texture2D<float4> Cr_texTexture : register(t1);
Texture2D<float4> Cb_texTexture : register(t2);

void main(float4 pos : SV_Position, float2 uv : TEXCOORD0, float4 vertexColor : COLOR0, out float4 o0 : SV_Target0)
{
   const float y = Y_texTexture.Sample(Sampler_Aniso_WW, uv).x;
   const float cr = Cr_texTexture.Sample(Sampler_Aniso_WW, uv).x;
   const float cb = Cb_texTexture.Sample(Sampler_Aniso_WW, uv).x;
   float4 color = float4(YUVtoRGB(y, cr, cb, 3), 1.0) * vertexColor; // BT.601 limited range, the vanilla coefficients
   if (color.a * Tint_color.a - Alpha_test_ref < 0.0)
      discard;
   color = saturate(color * Tint_color);

   float3 lin = gamma_to_linear(color.rgb);
   // Boost 0 = peak at sRGB white, where PumboAutoHDR no-ops; it also no-ops in SDR, where the peak is paper white.
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      const float peakNits = lerp(sRGB_WhiteLevelNits, VideoAutoHDRPeakNits, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      lin = PumboAutoHDR(lin, peakNits, LumaSettings.GamePaperWhiteNits);
   }
   // Encoded like the GUI layer (gamma, 1 = UI paper white), but at the scene paper white, like the tonemapped scene
   o0 = float4(linear_to_gamma(lin * SRTTR_SceneToUIScale(), GCT_NONE), color.a);
}
