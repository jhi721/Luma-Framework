// DLAA passes for Assassin's Creed Valhalla, drawn by main.cpp. The scene (S) is the finished post image TAA reads: 8 bit display encoded, or in the
// game's HDR fp16 holding BT.2020 PQ, what its composite decodes.
// "scene_copy_ps", "motion_vectors_ps", "depth_ps" and "reactive_ps" run on the game's root signature at its dither blend and TAA draws, whose arguments stay
// bound: release builds don't know the resources behind the SRVs, and the states and the constants are the ones the game's shaders read.

// "PQ_to_Linear()" stays above zero on the saturated code values it gets here, which the compiler can't see
#pragma warning(disable : 3571)
#include "../Includes/Color.hlsl"

Texture2D<float4> game_scene : register(t0, space2);
Texture2D<float4> game_packed_motion : register(t1, space2);
Texture2D<float> game_view_distance : register(t2, space2);
Texture2D<float> game_reactive : register(t3, space2); // HairDepthAndResponsiveAA.y, written by the translucents that opt in

// c6..c8 view to world, c38..c41 previous view projection, c54.y the far plane's device Z, c69..c72 clip to view
cbuffer GameView : register(b0, space1)
{
   float4 view_constants[73];
}

// c6 the view distance's linearization
cbuffer GameTAA : register(b0, space2)
{
   float4 taa_constants[7];
}

// The game's dither blend only writes its tiles, and the rest of its target is garbage (the memory is aliased): S everywhere first, for DLSS to read it all
float4 scene_copy_ps(float4 position : SV_Position) : SV_Target
{
   return game_scene.Load(int3(position.xy, 0));
}

bool IsSky(float view_distance)
{
   return view_distance >= (taa_constants[6].x - taa_constants[6].y) / (taa_constants[6].z - taa_constants[6].w);
}

// As the TAA's far branch: the sky isn't in the motion vector target
float2 SkyMotion(float2 uv)
{
   const float4 clip = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), view_constants[54].y, 1.0);
   const float3 view = float3(dot(clip, view_constants[69]), dot(clip, view_constants[70]), dot(clip, view_constants[71])) / dot(clip, view_constants[72]);
   const float4 view_h = float4(view, 1.0);
   const float4 world = float4(dot(view_h, view_constants[6]), dot(view_h, view_constants[7]), dot(view_h, view_constants[8]), 1.0);
   const float2 previous_ndc = float2(dot(world, view_constants[38]), dot(world, view_constants[39])) / dot(world, view_constants[41]);
   return uv - (previous_ndc * float2(0.5, -0.5) + 0.5);
}

// 12 + 12 bits in RGB, 0.2 * v^4 in UV
float2 DecodeMotion(float3 packed)
{
   const float green = packed.g * 15.9375;
   const float2 encoded = float2(packed.r * 4080.0 + floor(green), frac(green) * 4096.0 + packed.b * 255.0);
   const float2 v = clamp(encoded * (2.0 / 4095.0) - 1.0, -1.0, 1.0);
   const float2 v2 = v * v;
   return sign(v) * 0.2 * v2 * v2;
}

// UV deltas, previous minus current for DLSS: the game's are current minus previous, unjittered. A non finite one would spread through DLSS' history
float2 motion_vectors_ps(float4 position : SV_Position) : SV_Target
{
   const int3 pixel = int3(position.xy, 0);
   // Not TAA's c1, which is its output's size when it upsamples
   float2 size_in_pixels;
   game_view_distance.GetDimensions(size_in_pixels.x, size_in_pixels.y);
   const float2 uv = position.xy / size_in_pixels;
   const float2 motion = -(IsSky(game_view_distance.Load(pixel)) ? SkyMotion(uv) : DecodeMotion(game_packed_motion.Load(pixel).rgb));
   return all(isfinite(motion)) ? motion : 0.0;
}

// Reversed Z, as the G-buffer's depth buffer: near / distance, 0 for the sky
float depth_ps(float4 position : SV_Position) : SV_Target
{
   const float view_distance = game_view_distance.Load(int3(position.xy, 0));
   return IsSky(view_distance) ? 0.0 : saturate(-taa_constants[6].y / view_distance);
}

// The mask the game's TAA rejects history with, capped as AMD asks (FSR-Best-Practices FMSK-4)
float reactive_ps(float4 position : SV_Position) : SV_Target
{
   return min(game_reactive.Load(int3(position.xy, 0)), 0.9);
}

Texture2D<float4> game_ui : register(t0, space4);
Texture3D<float4> game_ui_lut : register(t3, space4);
SamplerState game_ui_lut_sampler : register(s0, space5);

// The UI half of the game's HDR composite (0xEF757BEB), premultiplied, on its root signature at its draw
float4 composite_ui_ps(float4 position : SV_Position) : SV_Target
{
   const float4 ui = game_ui.Load(int3(position.xy, 0));
   const float3 lut_uvw = saturate(ui.rgb / max(ui.a, 0.0)) * 0.96875 + 0.015625;
   return float4(game_ui_lut.SampleLevel(game_ui_lut_sampler, lut_uvw, 0.0).rgb * ui.a, ui.a);
}

Texture2D<float4> source : register(t0);
Texture2D<float4> composite_ui : register(t1);

// "DrawFullscreenPassD3D12()"'s constants
cbuffer Constants : register(b0)
{
   float2 size;
   float2 params; // Of each pass, see their names
}

// The rest of that composite, over the upscaler's output: PQ, then BT.2020 to scRGB (HDRSCRGB), or left in PQ (HDR, HDRGameDVR)
float4 composite_ps(float4 position : SV_Position) : SV_Target
{
   const bool pq_output = params.x != 0.0;
   const float scene_limit = params.y; // The composite's c0.w
   const int3 pixel = int3(position.xy, 0);
   const float4 ui = composite_ui.Load(pixel);
   const float3 pq = (1.0 - ui.a) * min(source.Load(pixel).rgb, scene_limit) + ui.rgb;
   return float4(pq_output ? pq : BT2020_To_BT709(PQ_to_Linear(pq, GCT_SATURATE) * HDR10_MaxWhiteNits) / sRGB_WhiteLevelNits, 1.0);
}

SamplerState linear_sampler : register(s0);

// "encoded" scaled down into TAA's target, for the readers of it after TAA that aren't replaced
float4 resample_ps(float4 position : SV_Position) : SV_Target
{
   return float4(source.SampleLevel(linear_sampler, position.xy / size, 0.0).rgb, 1.0);
}

// S into the upscaler's input: with "hdr", light in BT.2020 at scRGB's scale (1 = 80 nits). The upscalers need the signal's real level: the game's own "x^4" before EASU
// isn't that, it reads a dark scene 100 times too dark for them
float4 decode_ps(float4 position : SV_Position) : SV_Target
{
   const bool hdr = params.x != 0.0;
   const float3 color = source.Load(int3(position.xy, 0)).rgb;
   return float4(hdr ? PQ_to_Linear(color, GCT_SATURATE) * (HDR10_MaxWhiteNits / sRGB_WhiteLevelNits) : color, 1.0);
}

// DLSS' output back into what TAA wrote
float4 encode_ps(float4 position : SV_Position) : SV_Target
{
   const bool hdr = params.x != 0.0;
   const float3 color = source.Load(int3(position.xy, 0)).rgb;
   return float4(hdr ? Linear_to_PQ(color * (sRGB_WhiteLevelNits / HDR10_MaxWhiteNits), GCT_POSITIVE) : color, 1.0);
}
