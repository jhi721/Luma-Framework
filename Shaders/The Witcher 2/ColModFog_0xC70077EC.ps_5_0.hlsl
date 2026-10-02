// The Witcher 2 scene "enrichment" pass (postfx_sceneEnrichment.fx, CEnvColorModOpaqueParameters) with the game's sharpen
// removed: Luma's RCAS sharpens instead, after anti-aliasing ("RCAS Sharpness"; the game's own ran on the opaque scene before
// it, which DLSS/FSR then took as detail and a jittered input). Fog + ColMod + sharpen, DX9 6125c190 (dgVoodoo 2.87.3
// 0xC70077EC, 2.81.3 0x5359EFBC); with COLMOD_FOG 0 the ColMod + sharpen perm b1a9f097 (0xD1C83A90 / 0xF4037776).
// The body is the game's own perm without the sharpen (fog + ColMod 3d9a8657 = 0xE95D69DE, ColMod d46fe415 = 0xFA3EE277),
// translated from dgVoodoo's DXBC with its guarded rcp/rsq/pow: same registers, the sharpen perm only adds its texel size
// at c67 (cb4[75]) and the 4-tap block.
// Runs before the exposure on the linear scene: no Luma HDR handling needed here.
#include "../Includes/Math.hlsl" // IsNaN_Strict
#include "Includes/GameBindings.hlsl"

#ifndef COLMOD_FOG
#define COLMOD_FOG 1
#endif

Texture2D<float4> t0 : register(t0); // scene (sTextureColor)
Texture2D<float4> t1 : register(t1); // linear view depth (sTextureDepth)

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

// DX9 cN at cb4[N + 8]
#define PSC_CameraDepthRange      cb4[56]
#define vFogParamsScene           cb4[61]
#define vFogParamsSky             cb4[62]
#define vColorSceneNear           cb4[63]
#define vColorSceneFar            cb4[64]
#define vColorSunNear             cb4[65]
#define vColorSunFar              cb4[66]
#define vSunDirAndStartFactor     cb4[67]
#define vAreaParamSizesScale      cb4[69]
#define vAreaParamSizesBias       cb4[70]
#define vAreaParamRangesPreScale  cb4[71]
#define vAreaParamRangesPreBias   cb4[72]
#define vAreaParamRangesPostScale cb4[73]
#define vAreaParamRangesPostBias  cb4[74]
#define vColModNearLerpParams     cb4[76]
#define vColModInvRangeLerpParams cb4[77]
#define vColModStrengths          cb4[78]
#define vColModNearLumMinColor    cb4[79]
#define vColModNearLumMaxColor    cb4[80]
#define vColModFarLumMinColor     cb4[81]
#define vColModFarLumMaxColor     cb4[82]
#define vColModLumParams          cb4[83]
#define vColModContrastLumWeights cb4[84]

void main(
    float4 v0 : SV_POSITION0,
    float4 v1 : TEXCOORD8,
    float4 v2 : COLOR0,
    float4 v3 : COLOR1,
    float4 v4 : TEXCOORD9,
    float4 v5 : TEXCOORD0,
    float4 v6 : TEXCOORD1,
    float4 v7 : TEXCOORD2,
    float4 v8 : TEXCOORD3,
    float4 v9 : TEXCOORD4,
    float4 v10 : TEXCOORD5,
    float4 v11 : TEXCOORD6,
    float4 v12 : TEXCOORD7,
    out float4 o0 : SV_TARGET0)
{
   const float depth = ApplyDgvMask(t1.SampleLevel(s1_s, v5.xy, 0), DgvMaskT1, DgvFillT1).x;

   // ColMod: contrast around the weighted sum, lerped from near to far by depth, then a tint keyed on the weighted sum (linear light, near and far)
   const float2 near_far = saturate((depth - vColModNearLerpParams.yz) * vColModInvRangeLerpParams.yz);
   const float contrast = near_far.x * (vColModStrengths.w - vColModStrengths.y) + vColModStrengths.y;
   const float3 color = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0).rgb;
   const float weighted = dot(vColModContrastLumWeights.xyz, color);
   const float3 contrasted = max(contrast * (color - weighted) + weighted, 0.0);
   const float tint_key = saturate(dot(contrasted, vColModLumParams.xyz) - vColModLumParams.w);
   const float3 near_tint = tint_key * (vColModNearLumMaxColor.xyz - vColModNearLumMinColor.xyz) + vColModNearLumMinColor.xyz;
   const float3 far_tint = tint_key * (vColModFarLumMaxColor.xyz - vColModFarLumMinColor.xyz) + vColModFarLumMinColor.xyz;
   const float3 tint = near_far.y * (far_tint - near_tint) + near_tint;

#if COLMOD_FOG
   // Distance fog (scene or sky parameters), sun scattering and the area colors, as the game's 3d9a8657
   const float4 fog_params = ((PSC_CameraDepthRange.w - depth) >= 0.0 ? 0.0 : 1.0) * (vFogParamsSky - vFogParamsScene) + vFogParamsScene;
   float3 view = depth * v7.xyz;
   const float height_term = fog_params.w * view.y;
   float density = (abs(view.y) <= 0.001) ? 1.0 : DgVoodooRcp(height_term) * (1.0 - exp2(height_term * -1.442695));
   const float length_squared = dot(view, view);
   const float inverse_length_raw = rsqrt(abs(length_squared));
   const float inverse_length = (asuint(inverse_length_raw) == 0x7f800000u) ? DGVOODOO_BIG : inverse_length_raw;
   const float distance = DgVoodooRcp(inverse_length);
   view *= inverse_length;
   float3 sun = saturate(dot(vSunDirAndStartFactor.xyz, view) * vAreaParamSizesScale.xyz + vAreaParamSizesBias.xyz);
   sun *= sun;
   density *= distance - vSunDirAndStartFactor.w;
   const float extinction = 1.0 - saturate(exp2(-(fog_params.z * density)));
   float power = log2(abs(extinction)) * fog_params.y;
   power = IsNaN_Strict(power) ? 0.0 : power; // vanilla zeroes a NaN (log2(0) * 0); a bit test fxc can't drop
   const float fog_amount = saturate(fog_params.x * exp2(power));
   const float3 sun_color = fog_amount * (vColorSunFar.xyz - vColorSunNear.xyz) + vColorSunNear.xyz;
   const float3 ranges = saturate(depth * vAreaParamRangesPreScale.xyz + vAreaParamRangesPreBias.xyz) * vAreaParamRangesPostScale.xyz + vAreaParamRangesPostBias.xyz;
   const float sun_weight = sun.x * ranges.x;
   const float area_weight = dot(sun.yz, ranges.yz) + 0.0;
   const float scene_t = saturate((depth - vColorSceneNear.w) * vColorSceneFar.w);
   const float3 scene_color = scene_t * (vColorSceneFar.xyz - vColorSceneNear.xyz) + vColorSceneNear.xyz;
   const float3 fog_color_base = sun_weight * (sun_color - scene_color) + scene_color;
   const float3 fog_color = area_weight * fog_color_base + fog_color_base;
   o0.rgb = fog_amount * (contrasted * -tint + fog_color) + contrasted * tint;
#else
   o0.rgb = contrasted * tint;
#endif
   o0.a = depth;
}
