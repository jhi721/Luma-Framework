#include "Includes/Common.hlsl"

// hdr_filter compose of the tonemapped scene, the separate GUI layer and film noise onto the swapchain. Hashes in
// main.cpp: the SDR perm and every HDR_DISPLAY scene perm, so the game's own HDR setting (its NVAPI/AGS scRGB
// output, brightness, saturation, compare and calibration views) no longer changes the image: all of them run the
// SDR perm's math and Luma owns the display encoding.
// Vanilla divides the tonemap's 4x4 Bayer dither back out in the darks; the HDR tonemap writes no dither, so that
// step only runs in SDR. The GUI blend stays in gamma space, as vanilla (UI_DRAW_TYPE 2 scales the scene instead).
// The Bink video, drawn into the GUI layer in vanilla, comes from Luma's FP16 video layer (premultiplied, encoded like the GUI layer),
// composed right under the GUI layer; it's unbound (all zero) in frames without video. "Hide Gameplay UI" skips the GUI layer only.

cbuffer CB_CUSTOM : register(b10)
{
   float4 cb10[5]; // COMPOSE_PARAMS
}

#define NoiseLevel  cb10[0].x
#define NoiseOffset asint(cb10[1].zw)

Texture2D<float4> SceneImage : register(t0);
Texture2D<float4> GuiImage : register(t1);
Texture2D<float4> NoiseTexture : register(t4);
Texture2D<float4> VideoImage : register(t8);
SamplerState Sampler_Linear_CC : register(s2);

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   float3 scene = SceneImage.SampleLevel(Sampler_Linear_CC, uv, 0).rgb;

   if (LumaSettings.DisplayMode != 1)
   {
      const float weight = saturate(dot(scene, float3(0.3, 0.5, 0.2)) * -2.0 + 2.0);
      scene /= (SRTTR_Bayer(uint2(pos.xy)) * weight) * kBayerScale + 1.0;
   }

   const uint2 noisePos = uint2(float2(NoiseOffset << 3) + pos.xy) & 63u;
   scene += (abs(NoiseTexture.Load(int3(noisePos, 0)).x) * 2.0 - 1.0) * NoiseLevel;

   const float4 video = VideoImage.SampleLevel(Sampler_Linear_CC, uv, 0);
   float3 color = video.rgb + scene * (1.0 - video.a);
   if (LumaSettings.GameSettings.HideGameplayUI <= 0.5)
   {
      const float4 gui = GuiImage.SampleLevel(Sampler_Linear_CC, uv, 0);
      color = gui.a * (gui.rgb - color) + color;
   }
   SRTTR_DitherOutput(color, uv);
   return float4(color, 1.0);
}
