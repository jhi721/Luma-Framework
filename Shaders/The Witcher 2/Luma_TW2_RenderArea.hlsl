// Render scale (main.cpp "RenderArea"): the scene renders into the top-left area of the engine's full size surfaces, and the post
// chain then reads them whole. The G-buffer's linear depth (DoF, light shafts) is stretched over its surface, nearest, from a copy.
// On a frame without an upscaled scene the scene color is stretched too (bilinear), so the post chain never shows it in the corner.

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

// The scene color in the area stretched over the surface (bilinear, clamped to the area), from a copy (Saints Row IV's
// "color_upscale_ps")
Texture2D<float4> scene_color : register(t0);

float4 color_stretch_ps(float4 position : SV_Position) : SV_Target
{
   uint2 size;
   scene_color.GetDimensions(size.x, size.y);
   const int2 last = int2(round(area_scale * size)) - 1;
   const float2 texel = position.xy * area_scale - 0.5;
   const int2 base = int2(floor(texel));
   const float2 weight = texel - base;
   const int2 a = clamp(base, 0, last), b = clamp(base + 1, 0, last);
   const float4 top = lerp(scene_color.Load(int3(a.x, a.y, 0)), scene_color.Load(int3(b.x, a.y, 0)), weight.x);
   const float4 bottom = lerp(scene_color.Load(int3(a.x, b.y, 0)), scene_color.Load(int3(b.x, b.y, 0)), weight.x);
   return lerp(top, bottom, weight.y);
}
