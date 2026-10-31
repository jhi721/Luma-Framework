// Yakuza Kiwami ps_ccr_hls_______gi_of (0x56F12D55, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

SamplerState s1_s : register(s1);

float4 YKR_ColorCorrectGrade(float2 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _8 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _12 = dot(float3(_8.x, _8.y, _8.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _13 = 1.0f - _12;
   float _14 = _13 * _13;
   float _15 = _12 * _12;
   float _16 = 1.0f - _15;
   float _17 = _16 - _14;
   float _18 = saturate(_14);
   float _19 = saturate(_17);
   float _20 = saturate(_15);
   bool _21 = (_8.x > _8.y);
   float _22 = select(_21, _8.x, _8.y);
   float _23 = select(_21, _8.y, _8.x);
   bool _24 = (_23 > _8.z);
   float _25 = select(_24, _8.z, _23);
   bool _26 = (_22 < _8.z);
   float _27 = select(_26, _8.z, _22);
   float _28 = _27 - _25;
   float _29 = _27 + _25;
   float _30 = _29 * 0.5f;
   float _33 = _30 + (c_ps_user[0].y);
   bool _34 = (_33 < 0.5f);
   float _35 = 2.0f - _29;
   float _36 = select(_34, _29, _35);
   float _37 = _28 / _36;
   float _38 = _8.y - _8.z;
   float _39 = _8.z - _8.x;
   float _40 = _8.x - _8.y;
   float _41 = _38 / _28;
   float _42 = _39 / _28;
   float _43 = _40 / _28;
   float _45 = _41 + (c_ps_user[0].x);
   float _46 = (c_ps_user[0].x) + 2.0f;
   float _47 = _46 + _42;
   float _48 = (c_ps_user[0].x) + 4.0f;
   float _49 = _48 + _43;
   bool _50 = !(_8.y == _8.x);
   bool _51 = !(_8.z == _8.x);
   float _52 = float((bool)_50);
   float _53 = float((bool)_51);
   bool _54 = !(_8.z == _8.y);
   float _55 = float((bool)_54);
   bool _56 = (_8.x == _27);
   bool _57 = (_8.y == _27);
   bool _58 = (_8.z == _27);
   float _59 = float((bool)_56);
   float _60 = float((bool)_57);
   float _61 = float((bool)_58);
   float _62 = _60 * _52;
   float _63 = _55 * _53;
   float _64 = _63 * _61;
   float _65 = dot(float3(_45, _47, _49), float3(_59, _62, _64));
   float _66 = saturate(_33);
   float _67 = saturate(_37);
   float _68 = _65 * 0.1666666716337204f;
   float4 _69 = s1_t.Sample(s1_s, float2(_68, _67));
   bool _73 = !(_66 <= 0.5f);
   float _96;
   float _97;
   float _98;
   if (!_73)
   {
      float _75 = 0.5f - _66;
      float _76 = _75 * 2.0f;
      float _77 = _69.x * _76;
      float _78 = _69.y * _76;
      float _79 = _69.z * _76;
      float _80 = _69.x - _77;
      float _81 = _69.y - _78;
      float _82 = _69.z - _79;
      _96 = _80;
      _97 = _81;
      _98 = _82;
   }
   else
   {
      float _84 = 1.0f - _69.x;
      float _85 = 1.0f - _69.y;
      float _86 = 1.0f - _69.z;
      float _87 = _66 + -0.5f;
      float _88 = _87 * 2.0f;
      float _89 = _84 * _88;
      float _90 = _85 * _88;
      float _91 = _86 * _88;
      float _92 = _89 + _69.x;
      float _93 = _90 + _69.y;
      float _94 = _91 + _69.z;
      _96 = _92;
      _97 = _93;
      _98 = _94;
   }
   float _99 = saturate(_96);
   float _100 = saturate(_97);
   float _101 = saturate(_98);
   float _106 = (c_ps_user[6].x) * _18;
   float _107 = (c_ps_user[6].y) * _18;
   float _108 = (c_ps_user[6].z) * _18;
   float _109 = 1.0f - _106;
   float _110 = 1.0f - _107;
   float _111 = 1.0f - _108;
   float _116 = (c_ps_user[10].x) * _19;
   float _117 = (c_ps_user[10].y) * _19;
   float _118 = (c_ps_user[10].z) * _19;
   float _119 = 1.0f - _116;
   float _120 = 1.0f - _117;
   float _121 = 1.0f - _118;
   float _126 = (c_ps_user[14].x) * _20;
   float _127 = (c_ps_user[14].y) * _20;
   float _128 = (c_ps_user[14].z) * _20;
   float _129 = 1.0f - _126;
   float _130 = 1.0f - _127;
   float _131 = 1.0f - _128;
   float _136 = _109 * _99;
   float _137 = _136 * (c_ps_user[2].x);
   float _138 = _137 * _119;
   float _139 = _138 * _129;
   float _140 = _110 * _100;
   float _141 = _140 * (c_ps_user[2].y);
   float _142 = _141 * _120;
   float _143 = _142 * _130;
   float _144 = _111 * _101;
   float _145 = _144 * (c_ps_user[2].z);
   float _146 = _145 * _121;
   float _147 = _146 * _131;
   float _148 = saturate(_139);
   float _149 = saturate(_143);
   float _150 = saturate(_147);
   float _155 = (c_ps_user[7].x) * _18;
   float _156 = (c_ps_user[7].y) * _18;
   float _157 = (c_ps_user[7].z) * _18;
   float _162 = (c_ps_user[11].x) * _19;
   float _163 = (c_ps_user[11].y) * _19;
   float _164 = (c_ps_user[11].z) * _19;
   float _169 = (c_ps_user[15].x) * _20;
   float _170 = (c_ps_user[15].y) * _20;
   float _171 = (c_ps_user[15].z) * _20;
   float _176 = _155 + _148;
   float _177 = _176 + _162;
   float _178 = _177 + _169;
   float _179 = _178 + (c_ps_user[3].x);
   float _180 = _156 + _149;
   float _181 = _180 + _163;
   float _182 = _181 + _170;
   float _183 = _182 + (c_ps_user[3].y);
   float _184 = _157 + _150;
   float _185 = _184 + _164;
   float _186 = _185 + _171;
   float _187 = _186 + (c_ps_user[3].z);
   float _188 = saturate(_179);
   float _189 = saturate(_183);
   float _190 = saturate(_187);
   SV_Target.x = _188;
   SV_Target.y = _189;
   SV_Target.z = _190;
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
