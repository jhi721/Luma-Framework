// Yakuza Kiwami ps_ccr_hls_sc_gm_gi_of (0x064069A9, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _34 = dot(float3((c_ps_user[4].x), (c_ps_user[4].y), (c_ps_user[4].z)), float3(_18, _19, _20));
   float _39 = dot(float3((c_ps_user[8].x), (c_ps_user[8].y), (c_ps_user[8].z)), float3(_18, _19, _20));
   float _43 = (c_ps_user[0].z) + _34;
   float _44 = (c_ps_user[0].w) + _39;
   float _45 = _29 * 0.5f;
   float _47 = (c_ps_user[0].y) + _45;
   bool _48 = (_47 < 0.5f);
   float _49 = 2.0f - _29;
   float _50 = select(_48, _29, _49);
   float _51 = _28 / _50;
   float _52 = _43 * _51;
   float _53 = _52 + _51;
   float _54 = _8.y - _8.z;
   float _55 = _8.z - _8.x;
   float _56 = _8.x - _8.y;
   float _57 = _54 / _28;
   float _58 = _55 / _28;
   float _59 = _56 / _28;
   float _61 = _57 + (c_ps_user[0].x);
   float _62 = (c_ps_user[0].x) + 2.0f;
   float _63 = _62 + _58;
   float _64 = (c_ps_user[0].x) + 4.0f;
   float _65 = _64 + _59;
   bool _66 = !(_8.y == _8.x);
   bool _67 = !(_8.z == _8.x);
   float _68 = float((bool)_66);
   float _69 = float((bool)_67);
   bool _70 = !(_8.z == _8.y);
   float _71 = float((bool)_70);
   bool _72 = (_8.x == _27);
   bool _73 = (_8.y == _27);
   bool _74 = (_8.z == _27);
   float _75 = float((bool)_72);
   float _76 = float((bool)_73);
   float _77 = float((bool)_74);
   float _78 = _76 * _68;
   float _79 = _71 * _69;
   float _80 = _79 * _77;
   float _81 = dot(float3(_61, _63, _65), float3(_75, _78, _80));
   float _82 = saturate(_47);
   float _83 = saturate(_53);
   float _84 = _81 * 0.1666666716337204f;
   float4 _85 = s1_t.Sample(s1_s, float2(_84, _83));
   bool _89 = !(_82 <= 0.5f);
   float _112;
   float _113;
   float _114;
   if (!_89)
   {
      float _91 = 0.5f - _82;
      float _92 = _91 * 2.0f;
      float _93 = _85.x * _92;
      float _94 = _85.y * _92;
      float _95 = _85.z * _92;
      float _96 = _85.x - _93;
      float _97 = _85.y - _94;
      float _98 = _85.z - _95;
      _112 = _96;
      _113 = _97;
      _114 = _98;
   }
   else
   {
      float _100 = 1.0f - _85.x;
      float _101 = 1.0f - _85.y;
      float _102 = 1.0f - _85.z;
      float _103 = _82 + -0.5f;
      float _104 = _103 * 2.0f;
      float _105 = _100 * _104;
      float _106 = _101 * _104;
      float _107 = _102 * _104;
      float _108 = _105 + _85.x;
      float _109 = _106 + _85.y;
      float _110 = _107 + _85.z;
      _112 = _108;
      _113 = _109;
      _114 = _110;
   }
   float _115 = _44 * 2.0f;
   float _116 = 1.0f - _115;
   float _117 = 1.0f / _116;
   float _118 = _117 * _112;
   float _119 = _117 * _113;
   float _120 = _117 * _114;
   float _121 = _118 - _44;
   float _122 = _119 - _44;
   float _123 = _120 - _44;
   float _124 = saturate(_121);
   float _125 = saturate(_122);
   float _126 = saturate(_123);
   float _131 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_18, _19, _20));
   float _136 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_18, _19, _20));
   float _141 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_18, _19, _20));
   float _146 = (c_ps_user[1].x) + _131;
   float _147 = (c_ps_user[1].y) + _136;
   float _148 = (c_ps_user[1].z) + _141;
   float _149 = _147 * 0.45454543828964233f;
   float _150 = _148 * 0.45454543828964233f;
   float _151 = log2(_124);
   float _152 = log2(_125);
   float _153 = log2(_126);
   float _154 = _151 * 0.45454543828964233f;
   float _155 = _154 * _146;
   float _156 = _149 * _152;
   float _157 = _150 * _153;
   float _158 = exp2(_155);
   float _159 = exp2(_156);
   float _160 = exp2(_157);
   float _161 = saturate(_158);
   float _162 = saturate(_159);
   float _163 = saturate(_160);
   float _168 = (c_ps_user[6].x) * _18;
   float _169 = (c_ps_user[6].y) * _18;
   float _170 = (c_ps_user[6].z) * _18;
   float _171 = 1.0f - _168;
   float _172 = 1.0f - _169;
   float _173 = 1.0f - _170;
   float _178 = (c_ps_user[10].x) * _19;
   float _179 = (c_ps_user[10].y) * _19;
   float _180 = (c_ps_user[10].z) * _19;
   float _181 = 1.0f - _178;
   float _182 = 1.0f - _179;
   float _183 = 1.0f - _180;
   float _188 = (c_ps_user[14].x) * _20;
   float _189 = (c_ps_user[14].y) * _20;
   float _190 = (c_ps_user[14].z) * _20;
   float _191 = 1.0f - _188;
   float _192 = 1.0f - _189;
   float _193 = 1.0f - _190;
   float _198 = _171 * _161;
   float _199 = _198 * (c_ps_user[2].x);
   float _200 = _199 * _181;
   float _201 = _200 * _191;
   float _202 = _172 * _162;
   float _203 = _202 * (c_ps_user[2].y);
   float _204 = _203 * _182;
   float _205 = _204 * _192;
   float _206 = _173 * _163;
   float _207 = _206 * (c_ps_user[2].z);
   float _208 = _207 * _183;
   float _209 = _208 * _193;
   float _210 = saturate(_201);
   float _211 = saturate(_205);
   float _212 = saturate(_209);
   float _217 = (c_ps_user[7].x) * _18;
   float _218 = (c_ps_user[7].y) * _18;
   float _219 = (c_ps_user[7].z) * _18;
   float _224 = (c_ps_user[11].x) * _19;
   float _225 = (c_ps_user[11].y) * _19;
   float _226 = (c_ps_user[11].z) * _19;
   float _231 = (c_ps_user[15].x) * _20;
   float _232 = (c_ps_user[15].y) * _20;
   float _233 = (c_ps_user[15].z) * _20;
   float _238 = _217 + _210;
   float _239 = _238 + _224;
   float _240 = _239 + _231;
   float _241 = _240 + (c_ps_user[3].x);
   float _242 = _218 + _211;
   float _243 = _242 + _225;
   float _244 = _243 + _232;
   float _245 = _244 + (c_ps_user[3].y);
   float _246 = _219 + _212;
   float _247 = _246 + _226;
   float _248 = _247 + _233;
   float _249 = _248 + (c_ps_user[3].z);
   float _250 = saturate(_241);
   float _251 = saturate(_245);
   float _252 = saturate(_249);
   SV_Target.x = _250;
   SV_Target.y = _251;
   SV_Target.z = _252;
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
