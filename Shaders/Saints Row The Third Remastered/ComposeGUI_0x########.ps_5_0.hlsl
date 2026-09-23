// hdr_filter HDR_DISPLAY NO_SCENE composes (menus, loading screens; hashes in main.cpp). They run the SDR NO_SCENE
// perm's math (0xA283B6FB) so the game's HDR setting does not change the GUI: the GUI layer over a 2^-14 black.
// The Bink video comes from Luma's FP16 video layer, under the GUI layer (see the scene compose).

#include "Includes/Common.hlsl"

Texture2D<float4> GuiImage : register(t1);
Texture2D<float4> VideoImage : register(t8);
SamplerState Sampler_Linear_CC : register(s2);

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   const float4 video = VideoImage.SampleLevel(Sampler_Linear_CC, uv, 0);
   const float4 gui = GuiImage.SampleLevel(Sampler_Linear_CC, uv, 0);
   float3 color = video.rgb + 6.103515625e-05 * (1.0 - video.a);
   color = gui.a * (gui.rgb - color) + color;
   SRTTR_DitherOutput(color, uv);
   return float4(color, 1.0);
}
