// Yakuza Kiwami ps_down_sample_4x4 (0x53B63FF4, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: its scene samples go through YKR_SampleVanillaScene (vanilla-equivalent input on the draws main.cpp flags).

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

SamplerState s0_s : register(s0);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1) : SV_Target
{
   float4 SV_Target = 0;
   float4 _11 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _16 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _21 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _26 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _31 = _16.x + _11.x;
   float _32 = _16.y + _11.y;
   float _33 = _16.z + _11.z;
   float _34 = _16.w + _11.w;
   float _35 = _31 + _21.x;
   float _36 = _32 + _21.y;
   float _37 = _33 + _21.z;
   float _38 = _34 + _21.w;
   float _39 = _35 + _26.x;
   float _40 = _39 * 0.25f;
   float _41 = _36 + _26.y;
   float _42 = _41 * 0.25f;
   float _43 = _37 + _26.z;
   float _44 = _43 * 0.25f;
   float _45 = _38 + _26.w;
   float _46 = _45 * 0.25f;
   SV_Target.x = _40;
   SV_Target.y = _42;
   SV_Target.z = _44;
   SV_Target.w = _46;
   return SV_Target;
}
