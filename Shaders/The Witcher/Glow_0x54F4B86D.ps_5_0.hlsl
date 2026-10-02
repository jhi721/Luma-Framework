// The Witcher EE glow composite ("glow_fx.bfx" ps_1_1, dgVoodoo 2.87.3 -> ps_5_0 0x54F4B86D), the last pass of the Lua
// "ColorGlow" effect: backbuffer = glow_intensity * blurred bright pass (t0, quarter size) + scene copy "full" (t1).
// Transcribed from the translated disassembly. dgVoodoo ends every ps_1_x with "mov_sat o0" (ps_1_x's [0, 1] output range);
// the 8-bit canvas clamped anyway, the upgraded fp16 canvas would not, so the colour is left unclamped to keep the scene's
// highlights above 1. Alpha keeps the clamp.

cbuffer cb4 : register(b4)
{
   float4 cb4[236]; // D3D9 constants, c<N> at cb4[N] for pixel shaders; c0 = glow_intensity
}

Texture2D<float4> t0 : register(t0); // blurred bright pass
Texture2D<float4> t1 : register(t1); // scene copy

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

// Full dgVoodoo interpolator set; v5 = TEXCOORD0 and v6 = TEXCOORD1 carry the two UVs
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
   const float4 glow = t0.Sample(s0_s, v5.xy);
   const float4 scene = t1.Sample(s1_s, v6.xy);
   o0 = cb4[0] * glow + scene;
   o0.w = saturate(o0.w);
}
