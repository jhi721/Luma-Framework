// Yakuza Kiwami fx_ccr_hls_sc_gm_gi_of_mask (0x6D1C9FA9, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) and edited by
// a script: it becomes YKR_ColorCorrectGrade, each scene sample through YKR_ColorCorrectScene, run per grade (vanilla, HDR) by the main at the end.

#include "Includes/ColorCorrect.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s2_t : register(t2);

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

SamplerState s0_s : register(s0);

SamplerState s1_s : register(s1);

SamplerState s2_s : register(s2);

float4 YKR_ColorCorrectGrade(float4 COLOR, float4 TEXCOORD)
{
   float4 SV_Target = 0;
   float4 _13 = YKR_ColorCorrectScene(s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y)));
   float _17 = dot(float3(_13.x, _13.y, _13.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _18 = 1.0f - _17;
   float _19 = _18 * _18;
   float _20 = _17 * _17;
   float _21 = 1.0f - _20;
   float _22 = _21 - _19;
   float _23 = saturate(_19);
   float _24 = saturate(_22);
   float _25 = saturate(_20);
   bool _26 = (_13.x > _13.y);
   float _27 = select(_26, _13.x, _13.y);
   float _28 = select(_26, _13.y, _13.x);
   bool _29 = (_28 > _13.z);
   float _30 = select(_29, _13.z, _28);
   bool _31 = (_27 < _13.z);
   float _32 = select(_31, _13.z, _27);
   float _33 = _32 - _30;
   float _34 = _32 + _30;
   float _39 = dot(float3((c_ps_user[4].x), (c_ps_user[4].y), (c_ps_user[4].z)), float3(_23, _24, _25));
   float _44 = dot(float3((c_ps_user[8].x), (c_ps_user[8].y), (c_ps_user[8].z)), float3(_23, _24, _25));
   float _48 = (c_ps_user[0].z) + _39;
   float _49 = (c_ps_user[0].w) + _44;
   float _50 = _34 * 0.5f;
   float _52 = (c_ps_user[0].y) + _50;
   bool _53 = (_52 < 0.5f);
   float _54 = 2.0f - _34;
   float _55 = select(_53, _34, _54);
   float _56 = _33 / _55;
   float _57 = _48 * _56;
   float _58 = _57 + _56;
   float _59 = _13.y - _13.z;
   float _60 = _13.z - _13.x;
   float _61 = _13.x - _13.y;
   float _62 = _59 / _33;
   float _63 = _60 / _33;
   float _64 = _61 / _33;
   float _66 = _62 + (c_ps_user[0].x);
   float _67 = (c_ps_user[0].x) + 2.0f;
   float _68 = _67 + _63;
   float _69 = (c_ps_user[0].x) + 4.0f;
   float _70 = _69 + _64;
   bool _71 = !(_13.y == _13.x);
   bool _72 = !(_13.z == _13.x);
   float _73 = float((bool)_71);
   float _74 = float((bool)_72);
   bool _75 = !(_13.z == _13.y);
   float _76 = float((bool)_75);
   bool _77 = (_13.x == _32);
   bool _78 = (_13.y == _32);
   bool _79 = (_13.z == _32);
   float _80 = float((bool)_77);
   float _81 = float((bool)_78);
   float _82 = float((bool)_79);
   float _83 = _81 * _73;
   float _84 = _76 * _74;
   float _85 = _84 * _82;
   float _86 = dot(float3(_66, _68, _70), float3(_80, _83, _85));
   float _87 = saturate(_52);
   float _88 = saturate(_58);
   float _89 = _86 * 0.1666666716337204f;
   float4 _90 = s1_t.Sample(s1_s, float2(_89, _88));
   bool _94 = !(_87 <= 0.5f);
   float _117;
   float _118;
   float _119;
   if (!_94)
   {
      float _96 = 0.5f - _87;
      float _97 = _96 * 2.0f;
      float _98 = _90.x * _97;
      float _99 = _90.y * _97;
      float _100 = _90.z * _97;
      float _101 = _90.x - _98;
      float _102 = _90.y - _99;
      float _103 = _90.z - _100;
      _117 = _101;
      _118 = _102;
      _119 = _103;
   }
   else
   {
      float _105 = 1.0f - _90.x;
      float _106 = 1.0f - _90.y;
      float _107 = 1.0f - _90.z;
      float _108 = _87 + -0.5f;
      float _109 = _108 * 2.0f;
      float _110 = _105 * _109;
      float _111 = _106 * _109;
      float _112 = _107 * _109;
      float _113 = _110 + _90.x;
      float _114 = _111 + _90.y;
      float _115 = _112 + _90.z;
      _117 = _113;
      _118 = _114;
      _119 = _115;
   }
   float _120 = _49 * 2.0f;
   float _121 = 1.0f - _120;
   float _122 = 1.0f / _121;
   float _123 = _122 * _117;
   float _124 = _122 * _118;
   float _125 = _122 * _119;
   float _126 = _123 - _49;
   float _127 = _124 - _49;
   float _128 = _125 - _49;
   float _129 = saturate(_126);
   float _130 = saturate(_127);
   float _131 = saturate(_128);
   float _136 = dot(float3((c_ps_user[5].x), (c_ps_user[5].y), (c_ps_user[5].z)), float3(_23, _24, _25));
   float _141 = dot(float3((c_ps_user[9].x), (c_ps_user[9].y), (c_ps_user[9].z)), float3(_23, _24, _25));
   float _146 = dot(float3((c_ps_user[13].x), (c_ps_user[13].y), (c_ps_user[13].z)), float3(_23, _24, _25));
   float _151 = (c_ps_user[1].x) + _136;
   float _152 = (c_ps_user[1].y) + _141;
   float _153 = (c_ps_user[1].z) + _146;
   float _154 = _152 * 0.45454543828964233f;
   float _155 = _153 * 0.45454543828964233f;
   float _156 = log2(_129);
   float _157 = log2(_130);
   float _158 = log2(_131);
   float _159 = _156 * 0.45454543828964233f;
   float _160 = _159 * _151;
   float _161 = _154 * _157;
   float _162 = _155 * _158;
   float _163 = exp2(_160);
   float _164 = exp2(_161);
   float _165 = exp2(_162);
   float _166 = saturate(_163);
   float _167 = saturate(_164);
   float _168 = saturate(_165);
   float _173 = (c_ps_user[6].x) * _23;
   float _174 = (c_ps_user[6].y) * _23;
   float _175 = (c_ps_user[6].z) * _23;
   float _176 = 1.0f - _173;
   float _177 = 1.0f - _174;
   float _178 = 1.0f - _175;
   float _183 = (c_ps_user[10].x) * _24;
   float _184 = (c_ps_user[10].y) * _24;
   float _185 = (c_ps_user[10].z) * _24;
   float _186 = 1.0f - _183;
   float _187 = 1.0f - _184;
   float _188 = 1.0f - _185;
   float _193 = (c_ps_user[14].x) * _25;
   float _194 = (c_ps_user[14].y) * _25;
   float _195 = (c_ps_user[14].z) * _25;
   float _196 = 1.0f - _193;
   float _197 = 1.0f - _194;
   float _198 = 1.0f - _195;
   float _203 = _176 * _166;
   float _204 = _203 * (c_ps_user[2].x);
   float _205 = _204 * _186;
   float _206 = _205 * _196;
   float _207 = _177 * _167;
   float _208 = _207 * (c_ps_user[2].y);
   float _209 = _208 * _187;
   float _210 = _209 * _197;
   float _211 = _178 * _168;
   float _212 = _211 * (c_ps_user[2].z);
   float _213 = _212 * _188;
   float _214 = _213 * _198;
   float _215 = saturate(_206);
   float _216 = saturate(_210);
   float _217 = saturate(_214);
   float _222 = (c_ps_user[7].x) * _23;
   float _223 = (c_ps_user[7].y) * _23;
   float _224 = (c_ps_user[7].z) * _23;
   float _229 = (c_ps_user[11].x) * _24;
   float _230 = (c_ps_user[11].y) * _24;
   float _231 = (c_ps_user[11].z) * _24;
   float _236 = (c_ps_user[15].x) * _25;
   float _237 = (c_ps_user[15].y) * _25;
   float _238 = (c_ps_user[15].z) * _25;
   float _243 = _222 + _215;
   float _244 = _243 + _229;
   float _245 = _244 + _236;
   float _246 = _245 + (c_ps_user[3].x);
   float _247 = _223 + _216;
   float _248 = _247 + _230;
   float _249 = _248 + _237;
   float _250 = _249 + (c_ps_user[3].y);
   float _251 = _224 + _217;
   float _252 = _251 + _231;
   float _253 = _252 + _238;
   float _254 = _253 + (c_ps_user[3].z);
   float _255 = saturate(_246);
   float _256 = saturate(_250);
   float _257 = saturate(_254);
   float4 _258 = s2_t.Sample(s2_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _262 = (c_ps_user[1].w) * _258.x;
   float _263 = _255 - _13.x;
   float _264 = _256 - _13.y;
   float _265 = _257 - _13.z;
   float _266 = _262 * _263;
   float _267 = _262 * _264;
   float _268 = _262 * _265;
   float _269 = _266 + _13.x;
   float _270 = _267 + _13.y;
   float _271 = _268 + _13.z;
   SV_Target.x = _269;
   SV_Target.y = _270;
   SV_Target.z = _271;
   SV_Target.w = COLOR.w;
   return SV_Target;
}

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 TEXCOORD : TEXCOORD) : SV_Target
{
   const float4 vanilla_graded = YKR_ColorCorrectGrade(COLOR, TEXCOORD);
   float4 hdr_graded = vanilla_graded;
   [branch] if (YKR_ColorCorrectHDR())
   {
      ykr_color_correct_compressed = true;
      hdr_graded = YKR_ColorCorrectGrade(COLOR, TEXCOORD);
   }
   return YKR_ColorCorrectOutput(vanilla_graded, hdr_graded);
}
