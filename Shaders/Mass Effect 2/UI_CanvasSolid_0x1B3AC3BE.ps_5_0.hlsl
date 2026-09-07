// Mass Effect 2 (2010) UE3 Canvas solid-colour quad: no texture, colour straight from the vertex colour. Menu
// backdrops and full-screen fades. See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"
Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). This shader reads COLOR0 (v2) only.
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
   o0 = saturate(ME2_CanvasTransform(v2));
}
