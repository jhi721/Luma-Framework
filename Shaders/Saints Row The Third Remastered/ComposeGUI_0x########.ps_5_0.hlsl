// hdr_filter NO_SCENE composes (menus, loading screens; hashes in main.cpp). They run the SDR NO_SCENE perm's math
// (0xA283B6FB) so the game's HDR setting does not change the GUI: the GUI layer over a 2^-14 black.
// With Video AutoHDR, a menu Bink video comes from Luma's FP16 video layer (t8, unbound = zero without one) instead of the
// GUI layer. The UI was blended "over" the video in vanilla (colour srcA / 1 - srcA, alpha added up), so without the video
// the GUI layer holds premultiplied UI colour and its coverage: the video goes back under it as video * (1 - coverage).

#include "Includes/Common.hlsl"

Texture2D<float4> GuiImage : register(t1);
Texture2D<float4> VideoImage : register(t8);
SamplerState Sampler_Linear_CC : register(s2);

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   const float4 gui = GuiImage.SampleLevel(Sampler_Linear_CC, uv, 0);
   const float4 video = VideoImage.SampleLevel(Sampler_Linear_CC, uv, 0);
   float3 color = 6.103515625e-05;
   if (video.a > 0.0)
      color = gui.rgb + (video.rgb + color * (1.0 - video.a)) * (1.0 - gui.a);
   else
      color = gui.a * (gui.rgb - color) + color;
   SRTTR_DitherOutput(color, uv);
   return float4(color, 1.0);
}
