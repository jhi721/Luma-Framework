// Yakuza Kiwami ps_ccr______c_________ (0x9ECD97D6, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

float4 YKR_ColorCorrectGrade(float2 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _6 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _12 = (c_ps_user[0].w) * 2.0f;
   float _13 = 1.0f - _12;
   float _14 = 1.0f / _13;
   float _15 = _14 * _6.x;
   float _16 = _14 * _6.y;
   float _17 = _14 * _6.z;
   float _18 = _15 - (c_ps_user[0].w);
   float _19 = _16 - (c_ps_user[0].w);
   float _20 = _17 - (c_ps_user[0].w);
   float _21 = saturate(_18);
   float _22 = saturate(_19);
   float _23 = saturate(_20);
   SV_Target.x = _21;
   SV_Target.y = _22;
   SV_Target.z = _23;
   SV_Target.w = 1.0f;
   return SV_Target;
}

float4 main(
    linear float2 TEXCOORD : TEXCOORD) : SV_Target
{
   const float4 vanilla_graded = YKR_ColorCorrectGrade(TEXCOORD);
   float4 hdr_graded = vanilla_graded;
   [branch] if (YKR_ColorCorrectHDR())
   {
      ykr_color_correct_compressed = true;
      hdr_graded = YKR_ColorCorrectGrade(TEXCOORD);
   }
   return YKR_ColorCorrectOutput(vanilla_graded, hdr_graded);
}
