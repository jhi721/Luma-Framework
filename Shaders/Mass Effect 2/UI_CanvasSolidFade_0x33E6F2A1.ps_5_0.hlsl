// Mass Effect 2 (2010) UE3 Canvas solid-colour quad with a fade: as 0x1B3AC3BE, plus alpha scaled by COLOR1.w.
// See Includes/CanvasUI.hlsl - vanilla body plus the 8-bit clamp.
#include "Includes/CanvasUI.hlsl"
Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

// Full 13-entry interpolator layout, declared in order even where unread: linkage is by REGISTER (see
// Luma_ME2_Tonemap.hlsl). This shader reads COLOR0 (v2) and COLOR1.w (v3.w).
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
   const float4 c = ME2_CanvasTransform(v2);
   o0 = saturate(float4(c.rgb, c.w * v3.w)); // `mul o0.w, r0.wwww, v3.wwww` in the original
}
