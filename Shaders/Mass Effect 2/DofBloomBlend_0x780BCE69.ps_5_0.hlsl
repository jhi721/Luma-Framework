// Mass Effect 2 (2010) - UE3 FDOFAndBloomBlend, the standalone DoF/bloom composite of the chains that carry a
// DOFAndBloomEffect and NO uber. Verified against the cooked global shader cache (GlobalShaders-PC-D3D-SM3.bin,
// FDOFAndBloomBlendPixelShader, 18 instruction slots) rather than a runtime dump: the pass has never been captured.
//
// Reachable chains, read out of the packages with LegendaryExplorerCore (2707 packages, 384 PostProcessChain objects):
//   BioVFX_DesignerCamera.Drunk_PostProcess            MotionBlur -> DOFAndBloom          (BioD_CitHub_310Lounge + 5)
//   BioVFX_Crt_FlameThrower.PostProcess.FlameThrower_FB_PP   DOFAndBloom -> MaterialEffect (SFXGame)
//   EngineMaterials.DefaultUIPostProcess / DefaultThumbnailPostProcess   DOFAndBloom       (UI / UnrealEd)
// None of them contains PostUberShaderVignette, so the BioSceneEffect material never runs on these frames - this
// pass IS the frame's colour pass, and without a replacement the frame keeps raw scene light (no map, no glow, and
// the replaced gather has already zeroed the vanilla one).
#include "Luma_ME2_Tonemap.hlsl"

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). The original reads v0 = TEXCOORD0 (blur UV, t1) and v1 = TEXCOORD1 (scene UV, t0),
// which dgVoodoo lands on v5 and v6 exactly as it does for the uber.
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
   o0 = RunME2DofBloomBlend(v5.xy, v6.xy);
}
