// Mass Effect 3 (2012) Bink movie pass (YUV->RGB), byte-identical to ME2 2010 and BL2/TPS. It draws onto the 4K fp16
// canvas (measured), never through the uber, so without this a movie sits flat at paper white and its YUV overshoot
// is no longer clipped. Restores the vanilla clamp, then a LIGHT PumboAutoHDR. Port of ME2's Video_0xE41621CF.

// clang-format off
#include "Includes/Common.hlsl"       // game-local: GameCBuffers (VideoAutoHDR* fields) + shared Common
#include "Includes/GameBindings.hlsl" // b3/b4, ApplyDgvMask
// clang-format on

// Peak kept low: Bink is low-bitrate and a high peak amplifies block artifacts.
#ifndef VIDEO_AUTO_HDR_PEAK_NITS
#define VIDEO_AUTO_HDR_PEAK_NITS 250.0
#endif

Texture2D<float4> t0 : register(t0); // Y plane
Texture2D<float4> t1 : register(t1); // U plane
Texture2D<float4> t2 : register(t2); // V plane

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);
SamplerState s2_s : register(s2);

// Rows named per pass: the same cb4 rows are DoF/grade parameters in the uber.
#define BinkRowR     PsConstants[8]
#define BinkRowG     PsConstants[9]
#define BinkRowB     PsConstants[10]
#define BinkConstant PsConstants[11] // .x = the constant the matrix's fourth column multiplies, .w = output alpha

void main(DGV_MAIN_SIGNATURE)
{
   float4 yuv1;
   yuv1.x = ApplyDgvMask(t0.Sample(s0_s, v5.xy), 0).x;
   yuv1.y = ApplyDgvMask(t1.Sample(s1_s, v5.xy), 1).x;
   yuv1.z = ApplyDgvMask(t2.Sample(s2_s, v5.xy), 2).x;
   yuv1.w = BinkConstant.x;

   // YUV -> RGB (verbatim dp4 rows).
   float3 rgb;
   rgb.r = dot(BinkRowR, yuv1);
   rgb.g = dot(BinkRowG, yuv1);
   rgb.b = dot(BinkRowB, yuv1);

   // The vanilla clamp: vanilla got it free from the 8-bit UNORM canvas; the fp16 canvas clips nothing.
   rgb = saturate(rgb);

   float3 lin = gamma_to_linear(rgb);
#if TONEMAP_TYPE >= 1
   // Gated on TONEMAP_TYPE, not display mode: PumboAutoHDR's SDR self-noop does not cover TONEMAP_TYPE 0 on scRGB.
   if (LumaSettings.GameSettings.VideoAutoHDREnable > 0.5)
   {
      // Boost 0 = peak at sRGB white -> no-op; 1 = full VIDEO_AUTO_HDR_PEAK_NITS.
      const float peakNits = lerp(sRGB_WhiteLevelNits, VIDEO_AUTO_HDR_PEAK_NITS, saturate(LumaSettings.GameSettings.VideoAutoHDRBoost));
      lin = PumboAutoHDR(lin, peakNits, LumaSettings.GamePaperWhiteNits);
   }
#endif

   // Same UI Paper White pre-scale as the uber (which applies it on both TONEMAP_TYPEs), so movies land at gameplay
   // brightness; the canvas stays gamma, Core's composition decodes it at present.
   o0.rgb = linear_to_gamma(lin * GetUIPaperWhitePreScale());
   o0.w = BinkConstant.w; // vanilla alpha (constant)
}
