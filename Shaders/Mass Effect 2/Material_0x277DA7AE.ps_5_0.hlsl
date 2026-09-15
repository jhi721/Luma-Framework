// Mass Effect 2 (2010) BioSceneEffect / ImageAdjustments material, vignette only (film grain OFF). This pass, not
// the uber, is the frame's LAST colour pass: its render target feeds the closing present blit. See Luma_ME2_Tonemap.hlsl.
#include "Luma_ME2_Tonemap.hlsl"

// dgVoodoo's fixed interpolator layout: EVERY entry must be declared, in order, even the unread ones - VS->PS
// linkage is by REGISTER, so dropping one shifts every later TEXCOORD.
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
   // Vanilla writes v10.w into alpha, which the pass's 0x7 write mask then discards; kept for fidelity.
   o0 = float4(RunME2Material(v5.xy, v10.xyw), v10.w);
}
