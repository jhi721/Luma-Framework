// Saints Row IV (2022 Re-Elected build) — Luma HDR replacements for the rl_hdr final composite.
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
// Left vanilla: rl_hdr_07 0xFEE7D6DC (no technique name, not seen) and the whole rl_hdr_prince family (never loaded by
// the exe).
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

cbuffer vc4 : register(b4)
{
   float2 Tint_saturation : packoffset(c0);
   float4 Tint_color : packoffset(c1);
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

// Saturation around the game's (0.30, 0.59, 0.11) weighted sum of the linear scene, then tint: the head of every rl_hdr final.
float3 SR4_ApplyTintSaturation(float3 scene)
{
   const float weightedRGB = dot(float3(0.3, 0.59, 0.11), scene);
   return lerp(weightedRGB, scene, Tint_saturation.x) * Tint_color.rgb;
}

// The vanilla cubic shoulder on the 1.49 clip, u = c / 1.49.
float SR4_Shoulder(float u)
{
   return 1.5 * u - 0.5 * u * u * u;
}
float3 SR4_Shoulder(float3 u)
{
   return 1.5 * u - 0.5 * u * u * u;
}

// The vanilla LUT read, transcribed from the disassembly: slice = floor(b * 31) picks one 32x32 tile in an 8x4 grid,
// red and green address it with a half-texel centre, and the next slice (capped at 31) is lerped by b's fraction.
// `index` must already be in [0, 1]: past it the tile maths walks into the neighbouring slice and row.
float3 SR4_SampleLUT(float3 index)
{
   const float slice = floor(index.b * 31.0);
   const float nextSlice = min(slice + 1.0, 31.0);
   const float2 tileUV = (index.rg * 31.0 + 0.5) * float2(1.0 / 256.0, 1.0 / 128.0);
   const float2 uv0 = float2(trunc(frac(slice * 0.125) * 8.0), trunc(slice * 0.125)) * float2(0.125, 0.25) + tileUV;
   const float2 uv1 = float2(trunc(frac(nextSlice * 0.125) * 8.0), trunc(nextSlice * 0.125)) * float2(0.125, 0.25) + tileUV;
   const float3 lut0 = Lut_sampler_2dTexture.Sample(Lut_sampler_2dSampler, uv0).rgb;
   const float3 lut1 = Lut_sampler_2dTexture.Sample(Lut_sampler_2dSampler, uv1).rgb;
   return lerp(lut0, lut1, index.b * 31.0 - slice);
}

// Linear HDR (1.0 = paper white) -> the gamma-encoded post-process space, via DICE to the user's peak.
float3 SR4_DisplayMap(float3 hdrLinear, float2 uv)
{
   hdrLinear = IsNaN_Strict(hdrLinear) ? 0.0 : hdrLinear; // NaN -> 0; "x != x" can be compiled away
   hdrLinear = max(hdrLinear, 0.0);
   const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
   const float peakWhite = PeakWhiteNits / sRGB_WhiteLevelNits;
   // User contrast before the display map, so DICE contains whatever it pushes up. Multiplicative around mid-gray (TW2's
   // form); the floored log2 keeps black at 0 for Contrast 0 instead of pow(0, 0) = NaN. Gated so 1 stays bit-exact.
   [branch] if (LumaSettings.GameSettings.Contrast != 1.0)
   {
      hdrLinear = exp2(LumaSettings.GameSettings.Contrast * log2(max(hdrLinear / MidGray, 1e-30))) * MidGray;
   }
   DICESettings settings = DefaultDICESettings(DICE_TYPE_BY_LUMINANCE_PQ_CORRECT_CHANNELS_BEYOND_PEAK_WHITE);
   // Ramps on the max channel between a third of peak and peak, inside DICE's containment: mid-tones stay untouched.
   settings.HighlightsDesaturation = LumaSettings.GameSettings.HighlightDechroma;
   float3 encoded = SR4_EncodeOutput(DICETonemap(hdrLinear * paperWhite, peakWhite, settings) / paperWhite);
   // Anti-banding dither, one step of the output quantizer: the 8-bit code in SDR, 10-bit BT.2020 PQ in HDR. The
   // composition scales this image by UIPaperWhite, so that is the PQ scale.
   [branch] if (LumaSettings.GameSettings.Dithering > 0.5)
   {
      if (LumaSettings.DisplayMode == 0)
         ApplyDithering(encoded, uv, true, 1.0, 8u, LumaSettings.FrameIndex, true);
      else
      {
         const float pqScale = max(LumaSettings.UIPaperWhiteNits, 1.0) / HDR10_MaxWhiteNits;
         float3 pq = Linear_to_PQ(BT709_To_BT2020(gamma_to_linear(encoded, GCT_MIRROR) * pqScale), GCT_MIRROR);
         ApplyDithering(pq, uv, true, 1.0, 10u, LumaSettings.FrameIndex, true);
         encoded = linear_to_gamma(BT2020_To_BT709(PQ_to_Linear(pq, GCT_MIRROR)) / pqScale, GCT_MIRROR);
      }
   }
   return encoded;
}

// The pass result: the vanilla SDR reference (TONEMAP_TYPE 0), or the HDR recovery of it through DICE.
float4 SR4_Output(float3 vanillaLinear, float recoveryGain, float2 uv, float alpha)
{
#if TONEMAP_TYPE <= 0
   return float4(SR4_EncodeOutput(vanillaLinear), alpha);
#else
   return float4(SR4_DisplayMap(Saturation(vanillaLinear, LumaSettings.GameSettings.Saturation) * recoveryGain, uv), alpha);
#endif
}

// HDR brightness recovery: how much brighter the clamped vanilla output gets, one scalar for all channels.
// Onset in u, the curve's own input. F'(p) = 1.5(1 - p^2), and the tangent's intercept F(p) - p F'(p) = p^3 is positive
// while F'' = -3u < 0, so every pivot in (0, 1) extends above the curve and the gain is >= 1. 0.35 transfers the
// Mass Effect 2 recovery by its metrics (the onset at 43% of the curve's white gives 0.30, SDR white recovering 1.35x
// gives 0.36): SDR white recovers 1.36x and scene 10 reaches ~8.9x paper white before DICE. MELE / RenoDX
// Hejl-Dawson pivot at mid-gray instead (0.12 here, 80% of a daytime frame) because they feed the continuation through
// an unclamped LUT; this consumer is the Mass Effect 2 one. The vignette and grain after the shoulder scale the
// vanilla output this multiplies, so they carry into the highlights.
float SR4_RecoveryGain(float3 u)
{
   const float pivot = 0.35;
   // The hottest channel is the one the per-channel shoulder compresses first; F is monotone, so max3(F(u)) == F(max3(u)).
   const float sourcePeak = max3(u);
   if (sourcePeak <= pivot)
      return 1.0;
   const float extended = SR4_Shoulder(pivot) + 1.5 * (1.0 - pivot * pivot) * (sourcePeak - pivot);
   // max(1, ...) holds "never darker than vanilla" through float rounding at the pivot, and turns NaN into vanilla.
   return max(1.0, extended / SR4_Shoulder(min(sourcePeak, 1.0)));
}

// The scene input every final starts from, in the pass's "TEXCOORD1" space (texcoord1.xy = screen UV): the distortion
// map's offsets, in the diffracted techniques a per-channel sine-wobbled split, then the DoF lerp towards the blur.
// Luma upgrades the r8g8b8a8 distortion map to FP16 with the swapchain-sized targets, which would store its 0.5
// neutral as exactly 0.5 instead of 128/255 and shift the whole scene by the decode's 1 + 0.5/255 bias: re-quantizing to
// the 8-bit code the game wrote restores the vanilla offsets (a no-op on the UNORM map).
float4 SR4_SceneInput(float4 texcoord1, bool diffracted, out float2 uv)
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
float3 SR4_VignetteAndGrain(float3 color, float2 texcoord0, float4 texcoord1, bool clampGrain)
{
   const float2 centered = (texcoord1.xy - 0.5) * 2.0;
   color *= 1.0 - saturate(pow(length(centered), Grain_params2.w) * Grain_params2.z);
   const float grain = dot(Grain_textureTexture.Sample(Grain_textureSampler, texcoord1.zw).rgb, 1.0) - 1.0;
   color += max(color, 0.02) * grain * Grain_params2.x;
   color = clampGrain ? saturate(color) : max(color, 0.0);
   return color * (1.0 + Grain_params2.y * (Grain_textureTexture.Sample(Grain_textureSampler, texcoord0).rgb - 1.0));
}

// final / final_no_lut / final_diffracted / final_no_lut_diffracted. Returns the gamma-encoded colour; alpha is the
// scene's, as vanilla.
float4 SR4_TonemapFinal(float2 texcoord0, float4 texcoord1, bool diffracted, bool lut)
{
   float2 uv;
   const float4 scene = SR4_SceneInput(texcoord1, diffracted, uv);
   const float3 bloom = Final_bloomTexture.Sample(Final_bloomSampler, uv).rgb * Bloom_amount * LumaSettings.GameSettings.BloomIntensity * (1.0 / 3.0);
   const float3 u = (SR4_ApplyTintSaturation(scene.rgb) + bloom) * LumaSettings.GameSettings.Exposure * (1.0 / 1.49);
   const float3 curved = SR4_VignetteAndGrain(SR4_Shoulder(saturate(u)), texcoord0, texcoord1, true);
   float3 vanilla;
   if (lut)
   {
      const float3 index = sqrt(curved);
      // The LUT's identity returns its own index, so the grading fades out in the encoded space it was authored in.
      vanilla = gamma_to_linear(lerp(index, SR4_SampleLUT(index), LumaSettings.GameSettings.ColorGradingIntensity));
   }
   else
   {
      // The vanilla pow(1/2.2) is decoded exactly by gamma 2.2, so the linear reference is the graded shoulder itself.
      vanilla = curved;
   }
   return SR4_Output(vanilla, SR4_RecoveryGain(u), texcoord1.xy, scene.a);
}

// rl_hdr_05 0xC235DDDD [no_tonemapping] (PostProcess 0): vanilla lost everything above 1 to the grain's saturate.
float4 SR4_TonemapNoPost(float2 texcoord0, float4 texcoord1)
{
   float2 uv;
   const float4 scene = SR4_SceneInput(texcoord1, false, uv);
   const float3 color = SR4_ApplyTintSaturation(scene.rgb) * LumaSettings.GameSettings.Exposure;
   return SR4_Output(SR4_VignetteAndGrain(color, texcoord0, texcoord1, TONEMAP_TYPE <= 0), 1.0, texcoord1.xy, scene.a);
}
