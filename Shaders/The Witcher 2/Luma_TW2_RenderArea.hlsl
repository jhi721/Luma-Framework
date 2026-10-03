// Render scale (main.cpp "RenderArea"): the scene renders into the top-left area of the engine's full size surfaces, and the post
// chain then reads them whole. The G-buffer's linear depth (DoF, light shafts) is stretched over its surface, nearest, from a copy.
// On a frame without an upscaled scene the scene color is stretched too (bilinear), so the post chain never shows it in the corner.

#include "../Includes/AreaStretch.hlsl"

cbuffer RenderArea : register(b0) // The motion vector jitter buffer ("MotionVectorPatches::jitter_slot")
{
   float2 jitter_ndc; // Not applied
   float2 area_scale; // The render area's share of the surface
};

Texture2D<float> linear_depth : register(t0);

float depth_stretch_ps(float4 position : SV_Position) : SV_Target
{
   return linear_depth.Load(int3(position.xy * area_scale, 0));
}

// The scene color in the area stretched over the surface, from a copy
Texture2D<float4> scene_color : register(t0);

float4 color_stretch_ps(float4 position : SV_Position) : SV_Target
{
   return StretchAreaBilinear(scene_color, position.xy, area_scale);
}
