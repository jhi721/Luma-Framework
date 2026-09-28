// Saints Row: Gat out of Hell - Luma HDR replacements for the rl_hdr final composite. The game ships Saints Row IV's
// rl_hdr shaders byte-for-byte (same hashes as the 2022 Re-Elected build, where these were verified).
//
// The game writes the final composite straight into the r8g8b8a8 swapchain; the PostProcess 2 diffusion DoF and the
// rl_prim_2d UI then run on that display-encoded image. So the output stays gamma-encoded
// (POST_PROCESS_SPACE_TYPE 0, 1.0 = paper white) and the Core display composition linearizes at present.
//
// Every rl_hdr final (exe technique names in brackets) first builds its scene input: distortion offsets from t4, a
// depth-of-field lerp towards the t3 blur by a t5 depth/focal weight, and in the "diffracted" techniques a sine wobble
// that splits the channels. Then Saints Row: The Third's grade, unchanged:
//   lerp(weightedRGB, scene, sat) * tint + bloom -> saturate(c / 1.49) -> 1.5u - 0.5u^3 -> vignette -> film grain ->
//   32^3 LUT read at sqrt(t) from a 256x128 atlas (8x4 slices, sliced by blue), or pow(1/2.2) without a LUT.
// The LUT is per game state (blended every frame by rl_lut_blend), so the vanilla result through the live LUT stays the
// colour reference and HDR only decides how much brighter it gets: the cubic shoulder continued along its own tangent
// past a pivot, as ONE scalar over the clamped vanilla output (the Mass Effect 2 filmic recovery construction).
// rl_hdr_09 0xED6DDA24 [final] (PostProcess 1/2), rl_hdr_10 0xD742C62C [final_no_lut], rl_hdr_08 0x966367E4
// [final_diffracted], rl_hdr_06 0x9F1F6557 [final_no_lut_diffracted]: that path.
// rl_hdr_05 0xC235DDDD [no_tonemapping] (PostProcess 0): no bloom, shoulder or LUT; the grain's saturate and the UNORM
// swapchain were its only clip.
// Left vanilla: rl_hdr_07 0xFEE7D6DC (no technique name, not seen; its Debug_bloom_buffer / Debug_lummap switches mark a
// debug view) and the whole rl_hdr_prince family (never loaded by either exe).
//
// Scene peak and gamut containment are DICE's; nothing after it re-clamps.

// clang-format off
// ORDER IS LOAD-BEARING - the game-local Common defines the settings struct before the shared includes.
#include "Includes/Common.hlsl"
#include "../Includes/DICE.hlsl"
// clang-format on

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif

// --- Game bindings (must match the original shaders exactly) ---
cbuffer vc0 : register(b0)
{
   float4 Grain_params2 : packoffset(c1); // x grain amount, y colour grain amount, z vignette strength, w vignette power
   float Blur_scale : packoffset(c3.x);
   float3 Distortion_scale : packoffset(c4);
   float4 Blur_tint : packoffset(c5);
   float4 Near_clip_params : packoffset(c6); // x near, y far
   float4 Focal_params : packoffset(c7);     // near blur end, focus start... (x < y <= z < w, view depth)
   float2 Override_blur_percent : packoffset(c8);
   float Depth_map_scale : packoffset(c9.x);
   float Bloom_amount : packoffset(c11.x);
   float3 Signal_noise : packoffset(c12); // x frequency, y amplitude, z phase (diffracted techniques only)
}

SamplerState Base_samplerSampler : register(s0);
SamplerState Grain_textureSampler : register(s2);
SamplerState blurred_samplerSampler : register(s3);
SamplerState distortion_samplerSampler : register(s4);
SamplerState depth_samplerSampler : register(s5);
SamplerState Final_bloomSampler : register(s6);
SamplerState Lut_sampler_2dSampler : register(s8);
Texture2D<float4> Base_samplerTexture : register(t0);
Texture2D<float4> Grain_textureTexture : register(t2);
Texture2D<float4> blurred_samplerTexture : register(t3);
Texture2D<float4> distortion_samplerTexture : register(t4);
Texture2D<float4> depth_samplerTexture : register(t5);
Texture2D<float4> Final_bloomTexture : register(t6);
Texture2D<float4> Lut_sampler_2dTexture : register(t8);

#include "Includes/Grade.hlsl"

// The scene input every final starts from, in the pass's "TEXCOORD1" space (texcoord1.xy = screen UV): the distortion
// map's offsets, in the diffracted techniques a per-channel sine-wobbled split, then the DoF lerp towards the blur.
// Luma upgrades the r8g8b8a8 distortion map to FP16 with the swapchain-sized targets, which would store its 0.5
// neutral as exactly 0.5 instead of 128/255 and shift the whole scene by the decode's 1 + 0.5/255 bias: re-quantizing to
// the 8-bit code the game wrote restores the vanilla offsets (a no-op on the UNORM map).
float4 GOOH_SceneInput(float4 texcoord1, bool diffracted, out float2 uv)
{
   const float4 distortion = round(saturate(distortion_samplerTexture.Sample(distortion_samplerSampler, texcoord1.xy)) * 255.0) / 255.0;
   float4 scene;
   if (diffracted)
   {
      const float2 offset = floor(distortion.xy * 256.0) * (1.0 / 128.0) - 1.0;
      uv = offset * Distortion_scale.xy + texcoord1.xy;
      const float2 spread = offset * Distortion_scale.xy + 0.0005;
      const float wobble = Signal_noise.y * sin((texcoord1.y + Signal_noise.z) * Signal_noise.x * 3.141593) + (1.0 - distortion.w);
      scene = Base_samplerTexture.Sample(Base_samplerSampler, uv);
      scene.g = Base_samplerTexture.Sample(Base_samplerSampler, uv + 0.5 * float2(wobble * spread.x, (1.0 - distortion.w) * spread.y)).g;
      scene.b = Base_samplerTexture.Sample(Base_samplerSampler, uv + spread * wobble).b;
   }
   else
   {
      uv = (distortion.xy * 2.0 - (1.0 + 0.5 / 255.0)) * Distortion_scale.xy + texcoord1.xy; // bias bit-exact with vanilla's 0xBF804040
      scene = Base_samplerTexture.Sample(Base_samplerSampler, uv);
   }

   // DoF weight from view depth z: 1 outside [Focal.x, Focal.w], ramping from 1 to 0 over [x, y] and from 0 to 1
   // over [z, w], 0 in focus; then the distortion map's blur and the override floor.
   const float near = Near_clip_params.x;
   const float far = Near_clip_params.y;
   const float depth = depth_samplerTexture.Sample(depth_samplerSampler, uv).x * Depth_map_scale;
   const float z = min(max(near * far / (far - (far - near) * depth), 0.0), Override_blur_percent.y);
   const float4 f = Focal_params;
   float blur = 0.0;
   if (f.x < z && z < f.y)
      blur = (f.y - z) / (f.y - f.x);
   if (f.z < z && z < f.w)
      blur = 1.0 - (f.w - z) / (f.w - f.z);
   if (f.w < z || z < f.x)
      blur = 1.0;
   blur = max(max(blur, min(distortion.z * Blur_scale, 1.0)), Override_blur_percent.x);
   const float3 blurred = blurred_samplerTexture.Sample(blurred_samplerSampler, uv).rgb * Blur_tint.rgb;
   return float4(lerp(scene.rgb, blurred, blur), scene.a);
}

// Radial vignette and the two film grain layers the finals apply after the shoulder (or the tint, in no_tonemapping).
// The monochrome grain ends in a saturate in vanilla; the HDR no_tonemapping path passes `clampGrain` false.
float3 GOOH_VignetteAndGrain(float3 color, float2 texcoord0, float4 texcoord1, bool clampGrain)
{
   const float2 centered = (texcoord1.xy - 0.5) * 2.0;
   color *= 1.0 - saturate(pow(length(centered), Grain_params2.w) * Grain_params2.z);
   const float grain = dot(Grain_textureTexture.Sample(Grain_textureSampler, texcoord1.zw).rgb, 1.0) - 1.0;
   color += max(color, 0.02) * grain * Grain_params2.x;
   color = clampGrain ? saturate(color) : max(color, 0.0);
   return color * (1.0 + Grain_params2.y * (Grain_textureTexture.Sample(Grain_textureSampler, texcoord0).rgb - 1.0));
}

// final / final_no_lut / final_diffracted / final_no_lut_diffracted. Returns the gamma-encoded colour; alpha is the
// scene's, as vanilla. The vignette and grain after the shoulder scale the vanilla output the recovery gain multiplies,
// so they carry into the highlights.
float4 GOOH_TonemapFinal(float2 texcoord0, float4 texcoord1, bool diffracted, bool lut)
{
   float2 uv;
   const float4 scene = GOOH_SceneInput(texcoord1, diffracted, uv);
   const float3 bloom = Final_bloomTexture.Sample(Final_bloomSampler, uv).rgb * Bloom_amount * LumaSettings.GameSettings.BloomIntensity * (1.0 / 3.0);
   const float3 u = (SR_SaturateAndTint(scene.rgb) + bloom) * LumaSettings.GameSettings.Exposure * (1.0 / 1.49);
   const float3 curved = GOOH_VignetteAndGrain(SR_Shoulder(saturate(u)), texcoord0, texcoord1, true);
   float3 vanilla;
   if (lut)
   {
      const float3 index = sqrt(curved);
      // The LUT's identity returns its own index, so the grading fades out in the encoded space it was authored in.
      vanilla = gamma_to_linear(lerp(index, SR_SampleLUT(index), LumaSettings.GameSettings.ColorGradingIntensity));
   }
   else
   {
      // The vanilla pow(1/2.2) is decoded exactly by gamma 2.2, so the linear reference is the graded shoulder itself.
      vanilla = curved;
   }
   return SR_Output(vanilla, SR_RecoveryGain(u), texcoord1.xy, scene.a);
}

// rl_hdr_05 0xC235DDDD [no_tonemapping] (PostProcess 0): vanilla lost everything above 1 to the grain's saturate.
float4 GOOH_TonemapNoPost(float2 texcoord0, float4 texcoord1)
{
   float2 uv;
   const float4 scene = GOOH_SceneInput(texcoord1, false, uv);
   const float3 color = SR_SaturateAndTint(scene.rgb) * LumaSettings.GameSettings.Exposure;
   return SR_Output(GOOH_VignetteAndGrain(color, texcoord0, texcoord1, TONEMAP_TYPE <= 0), 1.0, texcoord1.xy, scene.a);
}
