// Yakuza Kiwami ps_ccr_hls_sc_gm_gi_of_ca (0xEEDDE2B4, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
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
   float _10 = 1.0f - (c_ps_user[2].w);
   float _11 = _10 - (c_ps_user[2].w);
   float _12 = _11 * TEXCOORD.x;
   float _13 = _11 * TEXCOORD.y;
   float _14 = _12 + (c_ps_user[2].w);
   float _15 = _13 + (c_ps_user[2].w);
   float4 _16 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(_14, _15)));
   float _20 = 1.0f - (c_ps_user[3].w);
   float _21 = _20 - (c_ps_user[3].w);
   float _22 = _21 * TEXCOORD.x;
   float _23 = _21 * TEXCOORD.y;
   float _24 = _22 + (c_ps_user[3].w);
   float _25 = _23 + (c_ps_user[3].w);
   float4 _26 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(_24, _25)));
   float _30 = 1.0f - (c_ps_user[4].w);
   float _31 = _30 - (c_ps_user[4].w);
   float _32 = _31 * TEXCOORD.x;
   float _33 = _31 * TEXCOORD.y;
   float _34 = _32 + (c_ps_user[4].w);
   float _35 = _33 + (c_ps_user[4].w);
   float4 _36 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(_34, _35)));
   float _38 = dot(float3(_16.x, _26.y, _36.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _39 = 1.0f - _38;
   float _40 = _39 * _39;
   float _41 = _38 * _38;
   float _42 = 1.0f - _41;
   float _43 = _42 - _40;
   float _44 = saturate(_40);
   float _45 = saturate(_43);
   float _46 = saturate(_41);
   bool _47 = (_16.x > _26.y);
   float _48 = select(_47, _16.x, _26.y);
   float _49 = select(_47, _26.y, _16.x);
   bool _50 = (_49 > _36.z);
   float _51 = select(_50, _36.z, _49);
   bool _52 = (_48 < _36.z);
   float _53 = select(_52, _36.z, _48);
   float _54 = _53 - _51;
   float _55 = _53 + _51;
   float _60 = dot(float3((c_ps_user[4].x), (c_ps_user[4].y), (c_ps_user[4].z)), float3(_44, _45, _46));
   float _65 = dot(float3((c_ps_user[8].x), (c_ps_user[8].y), (c_ps_user[8].z)), float3(_44, _45, _46));
   float _69 = (c_ps_user[0].z) + _60;
   float _70 = (c_ps_user[0].w) + _65;
   float _71 = _55 * 0.5f;
   float _73 = (c_ps_user[0].y) + _71;
   bool _74 = (_73 < 0.5f);
   float _75 = 2.0f - _55;
   float _76 = select(_74, _55, _75);
   float _77 = _54 / _76;
   float _78 = _69 * _77;
   float _79 = _78 + _77;
   float _80 = _26.y - _36.z;
   float _81 = _36.z - _16.x;
   float _82 = _16.x - _26.y;
   float _83 = _80 / _54;
   float _84 = _81 / _54;
   float _85 = _82 / _54;
   float _87 = _83 + (c_ps_user[0].x);
   float _88 = (c_ps_user[0].x) + 2.0f;
   float _89 = _88 + _84;
   float _90 = (c_ps_user[0].x) + 4.0f;
   float _91 = _90 + _85;
   bool _92 = !(_26.y == _16.x);
   bool _93 = !(_36.z == _16.x);
   float _94 = float((bool)_92);
   float _95 = float((bool)_93);
   bool _96 = !(_36.z == _26.y);
   float _97 = float((bool)_96);
   bool _98 = (_16.x == _53);
   bool _99 = (_26.y == _53);
   bool _100 = (_36.z == _53);
   float _101 = float((bool)_98);
   float _102 = float((bool)_99);
   float _103 = float((bool)_100);
   float _104 = _102 * _94;
   float _105 = _97 * _95;
   float _106 = _105 * _103;
   float _107 = dot(float3(_87, _89, _91), float3(_101, _104, _106));
   float _108 = saturate(_73);
   float _109 = saturate(_79);
   float _110 = _107 * 0.1666666716337204f;
   float4 _111 = s1_t.Sample(s1_s, float2(_110, _109));
   bool _115 = !(_108 <= 0.5f);
   float _138;
   float _139;
   float _140;
   if (!_115)
   {
      float _117 = 0.5f - _108;
      float _118 = _117 * 2.0f;
      float _119 = _111.x * _118;
      float _120 = _111.y * _118;
      float _121 = _111.z * _118;
      float _122 = _111.x - _119;
      float _123 = _111.y - _120;
      float _124 = _111.z - _121;
      _138 = _122;
      _139 = _123;
      _140 = _124;
   }
   else
   {
      float _126 = 1.0f - _111.x;
      float _127 = 1.0f - _111.y;
      float _128 = 1.0f - _111.z;
      float _129 = _108 + -0.5f;
      float _130 = _129 * 2.0f;
      float _131 = _126 * _130;
      float _132 = _127 * _130;
      float _133 = _128 * _130;
      float _134 = _131 + _111.x;
      float _135 = _132 + _111.y;
      float _136 = _133 + _111.z;
      _138 = _134;
      _139 = _135;
      _140 = _136;
   }
   float _141 = _70 * 2.0f;
   float _142 = 1.0f - _141;
   float _143 = 1.0f / _142;
   float _144 = _143 * _138;
   float _145 = _143 * _139;
   float _146 = _143 * _140;
   float _147 = _144 - _70;
   float _148 = _145 - _70;
   float _149 = _146 - _70;
   float _150 = saturate(_147);
   float _151 = saturate(_148);
   float _152 = saturate(_149);
   float _157 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_44, _45, _46));
   float _162 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_44, _45, _46));
   float _167 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_44, _45, _46));
   float _172 = (c_ps_user[1].x) + _157;
   float _173 = (c_ps_user[1].y) + _162;
   float _174 = (c_ps_user[1].z) + _167;
   float _175 = _173 * 0.45454543828964233f;
   float _176 = _174 * 0.45454543828964233f;
   float _177 = log2(_150);
   float _178 = log2(_151);
   float _179 = log2(_152);
   float _180 = _177 * 0.45454543828964233f;
   float _181 = _180 * _172;
   float _182 = _175 * _178;
   float _183 = _176 * _179;
   float _184 = exp2(_181);
   float _185 = exp2(_182);
   float _186 = exp2(_183);
   float _187 = saturate(_184);
   float _188 = saturate(_185);
   float _189 = saturate(_186);
   float _194 = (c_ps_user[6].x) * _44;
   float _195 = (c_ps_user[6].y) * _44;
   float _196 = (c_ps_user[6].z) * _44;
   float _197 = 1.0f - _194;
   float _198 = 1.0f - _195;
   float _199 = 1.0f - _196;
   float _204 = (c_ps_user[10].x) * _45;
   float _205 = (c_ps_user[10].y) * _45;
   float _206 = (c_ps_user[10].z) * _45;
   float _207 = 1.0f - _204;
   float _208 = 1.0f - _205;
   float _209 = 1.0f - _206;
   float _214 = (c_ps_user[14].x) * _46;
   float _215 = (c_ps_user[14].y) * _46;
   float _216 = (c_ps_user[14].z) * _46;
   float _217 = 1.0f - _214;
   float _218 = 1.0f - _215;
   float _219 = 1.0f - _216;
   float _224 = _197 * _187;
   float _225 = _224 * (c_ps_user[2].x);
   float _226 = _225 * _207;
   float _227 = _226 * _217;
   float _228 = _198 * _188;
   float _229 = _228 * (c_ps_user[2].y);
   float _230 = _229 * _208;
   float _231 = _230 * _218;
   float _232 = _199 * _189;
   float _233 = _232 * (c_ps_user[2].z);
   float _234 = _233 * _209;
   float _235 = _234 * _219;
   float _236 = saturate(_227);
   float _237 = saturate(_231);
   float _238 = saturate(_235);
   float _243 = (c_ps_user[7].x) * _44;
   float _244 = (c_ps_user[7].y) * _44;
   float _245 = (c_ps_user[7].z) * _44;
   float _250 = (c_ps_user[11].x) * _45;
   float _251 = (c_ps_user[11].y) * _45;
   float _252 = (c_ps_user[11].z) * _45;
   float _257 = (c_ps_user[15].x) * _46;
   float _258 = (c_ps_user[15].y) * _46;
   float _259 = (c_ps_user[15].z) * _46;
   float _264 = _243 + _236;
   float _265 = _264 + _250;
   float _266 = _265 + _257;
   float _267 = _266 + (c_ps_user[3].x);
   float _268 = _244 + _237;
   float _269 = _268 + _251;
   float _270 = _269 + _258;
   float _271 = _270 + (c_ps_user[3].y);
   float _272 = _245 + _238;
   float _273 = _272 + _252;
   float _274 = _273 + _259;
   float _275 = _274 + (c_ps_user[3].z);
   float _276 = saturate(_267);
   float _277 = saturate(_271);
   float _278 = saturate(_275);
   SV_Target.x = _276;
   SV_Target.y = _277;
   SV_Target.z = _278;
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
