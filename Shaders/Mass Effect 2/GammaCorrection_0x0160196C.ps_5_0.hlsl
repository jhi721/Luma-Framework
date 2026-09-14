// UE3 FGammaCorrection: drawn instead of the uber when a frame has no post chain, so t0 is the linear fp16 scene.
// Not yet observed drawing; the HDR path maps and encodes like the material instead of clamping.
#include "Luma_ME2_Tonemap.hlsl"

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
   const float3 scene = ApplyDgvMask(MatSceneTexture.Sample(MatSceneSampler_s, v5.xy), DgvMaskT0, DgvFillT0).rgb;
   // lerp(scene * ColorScale, OverlayColor, OverlayColor.w)
   const float3 c = lerp(scene * PsConstants[8].rgb, PsConstants[10].rgb, PsConstants[10].w);
#if TONEMAP_TYPE >= 1
   o0.rgb = Sanitize(EncodeME2Canvas(max(c, 0.0)));
#else
   o0.rgb = PowUE3(max(saturate(c), 1e-4), PsConstants[11].xxx);
#endif
   o0.w = 1.0;
}
