// Mirror's Edge - FGammaCorrectionPixelShader: the grade when the TdTonemapping system setting is off, and the end of the menu's
// 1280x720 scene pass (fp16 scene -> gamma-encoded canvas). The original is pow(saturate(lerp(scene * ColorScale, OverlayColor,
// OverlayColor.a)), InverseGamma). HDR: the saturate as max(0), then TdToneMapping's display map and canvas encode. Mass Effect 2
// replaces the same shader (same hash), inside its own pipeline.
#include "Includes/Common.hlsl"
#include "Includes/Tonemap.hlsl"

#define ColorScale   PsConstants[8]  // .xyz
#define OverlayColor PsConstants[10] // .xyz fade color, .w fade amount
#define InverseGamma PsConstants[11] // .x

Texture2D<float4> SceneColorTexture : register(t0); // .w = linear depth
SamplerState SceneColorSampler : register(s0);

// dgVoodoo's fixed interpolator layout: every entry is declared, linkage is by register.
void main(DGV_MAIN_SIGNATURE)
{
   const float3 scene = ApplyDgvMask(SceneColorTexture.Sample(SceneColorSampler, v5.xy), DgvConstants[44], DgvConstants[45]).xyz * LumaSettings.GameSettings.Exposure;
   const float3 faded = lerp(scene * ColorScale.xyz, OverlayColor.xyz, OverlayColor.w);

#if TONEMAP_TYPE >= 1
   const float3 displayLinear = DisplayMapTd(gamma_to_linear(PowTd(max(0.0, faded), InverseGamma.xxx)));
#else
   const float3 displayLinear = gamma_to_linear(PowTd(saturate(faded), InverseGamma.xxx));
#endif

   o0 = float4(EncodeTdCanvas(displayLinear, v5.xy), 1.0);
}
