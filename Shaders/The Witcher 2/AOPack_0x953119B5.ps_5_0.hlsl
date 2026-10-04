// The Witcher 2 AO pack (dgVoodoo -> ps_5_0, hash 0x953119B5): the first pass after the AO generator, packing the AO target's .x and
// the full size linear depth into one R16G16 target the blurs read. Vanilla samples both at the same scene UV (v5).
// Below native render scale the AO generator (XeGTAO, or the native one in "RunNativeAOWholeTarget") fills the whole AO target instead
// of the scene's share, so the AO UV is scaled onto it by LumaData.CustomData3/4 (output size over render size, else 1; main.cpp
// "ao_uv_scale"). The depth stays at the scene UV.
#include "Includes/Common.hlsl"
#include "Includes/GameBindings.hlsl" // b3, the dgVoodoo masks, ApplyDgvMask

Texture2D<float4> t0 : register(t0); // full size linear view depth
Texture2D<float4> t1 : register(t1); // AO target (.x)

SamplerState s0_s : register(s0);
SamplerState s1_s : register(s1);

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
   const float ao = ApplyDgvMask(t1.Sample(s1_s, v5.xy * float2(LumaData.CustomData3, LumaData.CustomData4)), DgvMaskT1, DgvFillT1).x;
   const float depth = ApplyDgvMask(t0.Sample(s0_s, v5.xy), DgvMaskT0, DgvFillT0).x;
   o0 = float4(ao, depth, 0.0, 0.0);
}
