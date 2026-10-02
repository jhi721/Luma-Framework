// Render scale (main.cpp "RenderArea"): the scene renders into the top-left area of the engine's full size surfaces, and the post
// chain then reads them whole. The G-buffer's linear depth (DoF, light shafts) is stretched over its surface, nearest, from a copy.

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
