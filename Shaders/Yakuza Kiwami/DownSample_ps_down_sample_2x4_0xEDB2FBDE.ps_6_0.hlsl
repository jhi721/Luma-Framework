// Yakuza Kiwami ps_down_sample_2x4 (0xEDB2FBDE, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: its scene samples go through YKR_SampleVanillaScene (vanilla-equivalent input on the draws main.cpp flags).

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

SamplerState s0_s : register(s0);

float4 main(
    linear float4 TEXCOORD : TEXCOORD) : SV_Target
{
   float4 SV_Target = 0;
   float4 _7 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _12 = YKR_SampleVanillaScene(s0_t, s0_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _17 = _12.x + _7.x;
   float _18 = _17 * 0.5f;
   float _19 = _12.y + _7.y;
   float _20 = _19 * 0.5f;
   float _21 = _12.z + _7.z;
   float _22 = _21 * 0.5f;
   float _23 = _12.w + _7.w;
   float _24 = _23 * 0.5f;
   SV_Target.x = _18;
   SV_Target.y = _20;
   SV_Target.z = _22;
   SV_Target.w = _24;
   return SV_Target;
}
