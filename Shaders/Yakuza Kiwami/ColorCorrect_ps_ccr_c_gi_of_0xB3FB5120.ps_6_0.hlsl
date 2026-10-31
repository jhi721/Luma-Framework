// Yakuza Kiwami ps_ccr______c____gi_of (0xB3FB5120, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _10 = dot(float3(_6.x, _6.y, _6.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _11 = 1.0f - _10;
   float _12 = _11 * _11;
   float _13 = _10 * _10;
   float _14 = 1.0f - _13;
   float _15 = _14 - _12;
   float _16 = saturate(_12);
   float _17 = saturate(_15);
   float _18 = saturate(_13);
   float _21 = (c_ps_user[0].w) * 2.0f;
   float _22 = 1.0f - _21;
   float _23 = 1.0f / _22;
   float _24 = _23 * _6.x;
   float _25 = _23 * _6.y;
   float _26 = _23 * _6.z;
   float _27 = _24 - (c_ps_user[0].w);
   float _28 = _25 - (c_ps_user[0].w);
   float _29 = _26 - (c_ps_user[0].w);
   float _30 = saturate(_27);
   float _31 = saturate(_28);
   float _32 = saturate(_29);
   float _37 = (c_ps_user[6].x) * _16;
   float _38 = (c_ps_user[6].y) * _16;
   float _39 = (c_ps_user[6].z) * _16;
   float _40 = 1.0f - _37;
   float _41 = 1.0f - _38;
   float _42 = 1.0f - _39;
   float _47 = (c_ps_user[10].x) * _17;
   float _48 = (c_ps_user[10].y) * _17;
   float _49 = (c_ps_user[10].z) * _17;
   float _50 = 1.0f - _47;
   float _51 = 1.0f - _48;
   float _52 = 1.0f - _49;
   float _57 = (c_ps_user[14].x) * _18;
   float _58 = (c_ps_user[14].y) * _18;
   float _59 = (c_ps_user[14].z) * _18;
   float _60 = 1.0f - _57;
   float _61 = 1.0f - _58;
   float _62 = 1.0f - _59;
   float _67 = _40 * _30;
   float _68 = _67 * (c_ps_user[2].x);
   float _69 = _68 * _50;
   float _70 = _69 * _60;
   float _71 = _41 * _31;
   float _72 = _71 * (c_ps_user[2].y);
   float _73 = _72 * _51;
   float _74 = _73 * _61;
   float _75 = _42 * _32;
   float _76 = _75 * (c_ps_user[2].z);
   float _77 = _76 * _52;
   float _78 = _77 * _62;
   float _79 = saturate(_70);
   float _80 = saturate(_74);
   float _81 = saturate(_78);
   float _86 = (c_ps_user[7].x) * _16;
   float _87 = (c_ps_user[7].y) * _16;
   float _88 = (c_ps_user[7].z) * _16;
   float _93 = (c_ps_user[11].x) * _17;
   float _94 = (c_ps_user[11].y) * _17;
   float _95 = (c_ps_user[11].z) * _17;
   float _100 = (c_ps_user[15].x) * _18;
   float _101 = (c_ps_user[15].y) * _18;
   float _102 = (c_ps_user[15].z) * _18;
   float _107 = _86 + _79;
   float _108 = _107 + _93;
   float _109 = _108 + _100;
   float _110 = _109 + (c_ps_user[3].x);
   float _111 = _87 + _80;
   float _112 = _111 + _94;
   float _113 = _112 + _101;
   float _114 = _113 + (c_ps_user[3].y);
   float _115 = _88 + _81;
   float _116 = _115 + _95;
   float _117 = _116 + _102;
   float _118 = _117 + (c_ps_user[3].z);
   float _119 = saturate(_110);
   float _120 = saturate(_114);
   float _121 = saturate(_118);
   SV_Target.x = _119;
   SV_Target.y = _120;
   SV_Target.z = _121;
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
