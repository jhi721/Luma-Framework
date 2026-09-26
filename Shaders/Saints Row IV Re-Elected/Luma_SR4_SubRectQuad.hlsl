// Upscaling prototype: the scene's screen space quads drawn into the render sub-rect (see "g_render_scale" in main.cpp), in place of
// the game's VS: deferred lights (0x0FFC4B94, 0x086E02B3), the full screen quad (0x58DBDDA3: rl_restore_depth, the particle depth
// downsample) and the SSAO blur and apply (0x9669662B), in output and half output sized targets. Their
// UVs (0..1 over the full target) are scaled to the sub-rect; positions stay NDC from the vertex buffer. Same signatures as the
// game's VS, so its input layouts and pixel shaders still fit. Also the scene depth for post, over the full target after the upscaler.

cbuffer SubRect : register(b9) // The motion vector jitter buffer
{
   float2 jitter_ndc; // Not applied: the quads aren't projected
   float2 uv_scale;   // The sub-rect's share of the target
};

cbuffer vc2 : register(b2)
{
   float4 vc2_constants[30]; // IR_Light_Inv_Proj_TM at c26-c29
};

void quad_vs(float3 position : POSITION0, float2 uv : TEXCOORD0, out float4 o_position : SV_Position, out float2 o_uv : TEXCOORD0)
{
   o_position = float4(position, 1.0);
   o_uv = uv * uv_scale;
}

// The SSAO apply: the screen UV (xy) and the SSAO target's UV (zw, "ssao_uv_scale_offset"), both from the sub-rect's screen UV
cbuffer SSAO : register(b0)
{
   float4 ssao_uv_scale_offset;
};

void ssao_vs(float3 position : POSITION0, float2 uv : TEXCOORD0, out float4 o_position : SV_Position, out float4 o_uv : TEXCOORD0)
{
   o_position = float4(position, 1.0);
   const float2 scene_uv = uv * uv_scale;
   o_uv = float4(scene_uv, scene_uv * ssao_uv_scale_offset.xy + ssao_uv_scale_offset.zw);
}

// The view ray through the quad's NDC (z unused), divided by its z too in 0x0FFC4B94 (LIGHT_VIEW_RAY_UNIT_Z)
void light_vs(float3 position : POSITION0, float2 uv : TEXCOORD0, out float4 o_position : SV_Position, out float2 o_uv : TEXCOORD0, out float3 o_view_ray : TEXCOORD1)
{
   o_position = float4(position, 1.0);
   o_uv = uv * uv_scale;
   const float4 ray = position.y * vc2_constants[27] + vc2_constants[26] * position.x + vc2_constants[29];
   o_view_ray = ray.xyz / ray.w;
#if LIGHT_VIEW_RAY_UNIT_Z
   o_view_ray /= o_view_ray.z;
#endif
}

// The scene's depth (the G-buffer's, in the sub-rect) over the full target, nearest: post (DoF, the final composite) reads it at UVs over
// the full target once the upscaler has filled it
Texture2D<float> scene_depth : register(t0);

float depth_upscale_ps(float4 position : SV_Position) : SV_Depth
{
   return scene_depth.Load(int3(position.xy * uv_scale, 0));
}
