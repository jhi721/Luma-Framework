// Yakuza Kiwami material fx_dropping_coin (0xC65BED84, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s13_t : register(t13);

cbuffer cb13 : register(b13)
{
   uint c_bool : packoffset(c000.x);
};

cbuffer cb_user : register(b9)
{
   float4 c_ps_user[16] : packoffset(c000.x);
};

cbuffer cb1 : register(b1)
{
   float4 c_tone_control : packoffset(c000.x);
   float4 c_fog_param[2] : packoffset(c001.x);
   float4 c_ps_col_scl[1] : packoffset(c003.x);
   float4 c_ls_param[4] : packoffset(c004.x);
   float4 c_shadowmap_offset[1] : packoffset(c008.x);
   float4 c_sdw_param[3] : packoffset(c009.x);
   float4 c_sdw_param_ch[1] : packoffset(c012.x);
   float4 c_scene_ambient : packoffset(c013.x);
};

SamplerState s0_s : register(s0);

SamplerState s13_s : register(s13);

float4 main(
    noperspective float4 SV_Position : SV_Position,
    linear float2 TEXCOORD : TEXCOORD,
    linear float3 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    linear float3 TEXCOORD_7 : TEXCOORD7) : SV_Target
{
   float4 SV_Target = 0;
   float _33 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _34 = rsqrt(_33);
   float _35 = _34 * TEXCOORD_3.x;
   float _36 = _34 * TEXCOORD_3.y;
   float _37 = _34 * TEXCOORD_3.z;
   float _38 = dot(float3(_35, _36, _37), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _41 = (c_ps_user[0].x) * TEXCOORD_3.z;
   float _42 = TEXCOORD_4.x - _41;
   float _43 = _41 + TEXCOORD_4.x;
   float _44 = -1.0f / _38;
   float _45 = TEXCOORD_2.w + TEXCOORD_4.y;
   float _46 = _44 * _45;
   float _47 = TEXCOORD_2.w - TEXCOORD_4.y;
   float _48 = _44 * _47;
   float _49 = min(_46, _48);
   float _50 = max(_46, _48);
   float _51 = _49 * _35;
   float _52 = _49 * _36;
   float _53 = _49 * _37;
   float _54 = _51 - TEXCOORD_1.x;
   float _55 = _52 - TEXCOORD_1.y;
   float _56 = _53 - TEXCOORD_1.z;
   float _57 = _50 * _35;
   float _58 = _50 * _36;
   float _59 = _50 * _37;
   float _60 = _57 - TEXCOORD_1.x;
   float _61 = _58 - TEXCOORD_1.y;
   float _62 = _59 - TEXCOORD_1.z;
   bool _63 = (_38 < 0.0f);
   float _64 = select(_63, 0.0f, 1.0f);
   float _65 = TEXCOORD_4.y * 2.0f;
   float _66 = _65 * _64;
   float _67 = _66 - TEXCOORD_4.y;
   float _68 = _67 * TEXCOORD_2.x;
   float _69 = _67 * TEXCOORD_2.y;
   float _70 = _67 * TEXCOORD_2.z;
   float _71 = _54 + _68;
   float _72 = _55 + _69;
   float _73 = _56 + _70;
   float _74 = _60 - _68;
   float _75 = _61 - _69;
   float _76 = _62 - _70;
   float _77 = TEXCOORD_2.w * TEXCOORD_2.x;
   float _78 = TEXCOORD_2.w * TEXCOORD_2.y;
   float _79 = TEXCOORD_2.w * TEXCOORD_2.z;
   float _80 = TEXCOORD_1.x + _77;
   float _81 = TEXCOORD_1.y + _78;
   float _82 = TEXCOORD_1.z + _79;
   float _83 = _38 * TEXCOORD_2.x;
   float _84 = _38 * TEXCOORD_2.y;
   float _85 = _38 * TEXCOORD_2.z;
   float _86 = _35 - _83;
   float _87 = _36 - _84;
   float _88 = _37 - _85;
   float _89 = dot(float3(_86, _87, _88), float3(_86, _87, _88));
   float _90 = dot(float3(_86, _87, _88), float3(_80, _81, _82));
   float _91 = _90 / _89;
   float _92 = max(_91, _49);
   float _93 = min(_92, _50);
   float _94 = _93 * _86;
   float _95 = _93 * _87;
   float _96 = _93 * _88;
   float _97 = _94 - _80;
   float _98 = _95 - _81;
   float _99 = _96 - _82;
   float _100 = _97 * _97;
   float _101 = _98 * _98;
   float _102 = _100 + _101;
   float _103 = _99 * _99;
   float _104 = _102 + _103;
   float _105 = sqrt(_104);
   float _106 = _105 - _43;
   float _107 = _42 - _43;
   float _108 = _106 / _107;
   float _109 = saturate(_108);
   float _110 = _43 * _43;
   float _111 = _74 - _71;
   float _112 = _75 - _72;
   float _113 = _76 - _73;
   float _114 = dot(float3(_111, _112, _113), float3(_111, _112, _113));
   float _115 = dot(float3(_111, _112, _113), float3(_71, _72, _73));
   float _116 = dot(float3(_71, _72, _73), float3(_71, _72, _73));
   float _117 = _110 - _116;
   float _118 = _117 * _114;
   float _119 = _115 * _115;
   float _120 = _118 + _119;
   bool _121 = (_120 < 0.0f);
   if (_121)
      discard;
   float _122 = sqrt(_120);
   float _123 = -1.0f / _114;
   float _124 = _122 + _115;
   float _125 = _124 * _123;
   float _126 = _115 - _122;
   float _127 = _126 * _123;
   float _128 = max(_125, _127);
   bool _129 = (_128 < 0.0f);
   if (_129)
      discard;
   float _130 = min(_125, _127);
   float _131 = 1.0f - _130;
   bool _132 = (_131 < 0.0f);
   if (_132)
      discard;
   float _133 = _130 * _111;
   float _134 = _130 * _112;
   float _135 = _130 * _113;
   float _136 = _133 + _71;
   float _137 = _134 + _72;
   float _138 = _135 + _73;
   float _139 = dot(float3(_136, _137, _138), float3(_136, _137, _138));
   float _140 = rsqrt(_139);
   float _141 = _140 * _136;
   float _142 = _140 * _137;
   float _143 = _140 * _138;
   float _144 = _71 * _71;
   float _145 = _72 * _72;
   float _146 = _144 + _145;
   float _147 = _73 * _73;
   float _148 = _146 + _147;
   float _149 = sqrt(_148);
   float _150 = _149 - _43;
   float _151 = _150 / _107;
   float _152 = saturate(_151);
   float _153 = TEXCOORD.x * 0.5f;
   float _154 = select(_63, 0.0f, 0.5f);
   float _155 = _154 + _153;
   float4 _156 = s0_t.Sample(s0_s, float2(_155, TEXCOORD.y));
   float _160 = dot(float3(_156.x, _156.y, _156.z), float3(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f));
   float _161 = dot(float3(_35, _36, _37), float3(_141, _142, _143));
   float _162 = abs(_161);
   float _163 = 1.0f - _162;
   float _164 = max(0.0f, _163);
   float _165 = log2(_164);
   float _166 = _165 * TEXCOORD_6.w;
   float _167 = exp2(_166);
   float _168 = _167 * _160;
   float _169 = TEXCOORD_7.x - TEXCOORD_6.x;
   float _170 = TEXCOORD_7.y - TEXCOORD_6.y;
   float _171 = TEXCOORD_7.z - TEXCOORD_6.z;
   float _172 = _168 * _169;
   float _173 = _168 * _170;
   float _174 = _168 * _171;
   float _175 = _172 + TEXCOORD_6.x;
   float _176 = _173 + TEXCOORD_6.y;
   float _177 = _174 + TEXCOORD_6.z;
   float _178 = abs(_38);
   float _179 = 1.0f - _178;
   float _180 = max(0.0f, _179);
   float _181 = log2(_180);
   float _182 = _181 * TEXCOORD_6.w;
   float _183 = exp2(_182);
   float _184 = _152 + _109;
   float _185 = _152 / _184;
   float _186 = _183 - _167;
   float _187 = _160 * _186;
   float _188 = _169 * _187;
   float _189 = _183 - _167;
   float _190 = _160 * _189;
   float _191 = _170 * _190;
   float _192 = _183 - _167;
   float _193 = _160 * _192;
   float _194 = _171 * _193;
   float _195 = _188 * _185;
   float _196 = _191 * _185;
   float _197 = _194 * _185;
   float _198 = _175 + _195;
   float _199 = _198 * _156.x;
   float _200 = _176 + _196;
   float _201 = _200 * _156.y;
   float _202 = _177 + _197;
   float _203 = _202 * _156.z;
   float _204 = 1.0f - _109;
   float _205 = _152 * _204;
   float _206 = _205 + _109;
   int _209 = c_bool & 8;
   bool _210 = (_209 == 0);
   float _218;
   float _219;
   float _231;
   float _232;
   float _315;
   float _316;
   float _317;
   if (!_210)
   {
      float _212 = TEXCOORD_5.x / TEXCOORD_5.w;
      float _213 = TEXCOORD_5.y / TEXCOORD_5.w;
      float4 _214 = s13_t.Sample(s13_s, float2(_212, _213));
      _218 = _214.x;
      _219 = _214.y;
   }
   else
   {
      _218 = 1.0f;
      _219 = 1.0f;
   }
   if (!_210)
   {
      float _224 = (c_sdw_param[0].x) * _218;
      float _225 = (c_sdw_param[0].z) * _219;
      float _228 = _224 + (c_sdw_param[0].y);
      float _229 = _225 + (c_sdw_param[0].w);
      _231 = _228;
      _232 = _229;
   }
   else
   {
      _231 = _218;
      _232 = _219;
   }
   float _233 = _232 * _231;
   float _236 = 1.0f - (c_ps_user[0].y);
   float _237 = _233 * _236;
   float _238 = _237 + (c_ps_user[0].y);
   float _239 = 1.0f - _238;
   float _240 = _239 * TEXCOORD_4.w;
   float _241 = _240 + _238;
   float _242 = _199 * _241;
   float _243 = _201 * _241;
   float _244 = _203 * _241;
   float _245 = max(9.999999717180685e-10f, _242);
   float _246 = max(9.999999717180685e-10f, _243);
   float _247 = max(9.999999717180685e-10f, _244);
   int _248 = c_bool & 16;
   bool _249 = (_248 == 0);
   if (!_249)
   {
      float _253 = c_tone_control.x * _245;
      float _254 = c_tone_control.x * _246;
      float _255 = c_tone_control.x * _247;
      float _257 = log2(_253);
      float _258 = log2(_254);
      float _259 = log2(_255);
      float _260 = _257 * c_tone_control.y;
      float _261 = _258 * c_tone_control.y;
      float _262 = _259 * c_tone_control.y;
      float _263 = exp2(_260);
      float _264 = exp2(_261);
      float _265 = exp2(_262);
      _315 = _263;
      _316 = _264;
      _317 = _265;
   }
   else
   {
      int _267 = c_bool & 32768;
      bool _268 = (_267 == 0);
      if (_268)
      {
         float _272 = c_tone_control.x * _245;
         float _273 = c_tone_control.x * _246;
         float _274 = c_tone_control.x * _247;
         float _275 = _272 * 2.0f;
         float _276 = _273 * 2.0f;
         float _277 = _274 * 2.0f;
         float _278 = _272 * 0.30000001192092896f;
         float _279 = _273 * 0.30000001192092896f;
         float _280 = _274 * 0.30000001192092896f;
         float _281 = _278 + 0.05000000074505806f;
         float _282 = _279 + 0.05000000074505806f;
         float _283 = _280 + 0.05000000074505806f;
         float _284 = _281 * _275;
         float _285 = _282 * _276;
         float _286 = _283 * _277;
         float _287 = _284 + 0.004000000189989805f;
         float _288 = _285 + 0.004000000189989805f;
         float _289 = _286 + 0.004000000189989805f;
         float _290 = _278 + 0.5f;
         float _291 = _279 + 0.5f;
         float _292 = _280 + 0.5f;
         float _293 = _290 * _275;
         float _294 = _291 * _276;
         float _295 = _292 * _277;
         float _296 = _293 + 0.06000000238418579f;
         float _297 = _294 + 0.06000000238418579f;
         float _298 = _295 + 0.06000000238418579f;
         float _299 = _287 / _296;
         float _300 = _288 / _297;
         float _301 = _289 / _298;
         float _302 = _299 + -0.06666666269302368f;
         float _303 = _300 + -0.06666666269302368f;
         float _304 = _301 + -0.06666666269302368f;
         float _305 = _302 * 1.3790643215179443f;
         float _306 = _303 * 1.3790643215179443f;
         float _307 = _304 * 1.3790643215179443f;
         float _308 = YKR_MaterialCurve(_305, _272);
         float _309 = YKR_MaterialCurve(_306, _273);
         float _310 = YKR_MaterialCurve(_307, _274);
         float _311 = sqrt(_308);
         float _312 = sqrt(_309);
         float _313 = sqrt(_310);
         _315 = _311;
         _316 = _312;
         _317 = _313;
      }
      else
      {
         _315 = _245;
         _316 = _246;
         _317 = _247;
      }
   }
   float _323 = (c_ps_col_scl[0].x) * _315;
   float _324 = (c_ps_col_scl[0].y) * _316;
   float _325 = (c_ps_col_scl[0].z) * _317;
   float _326 = (c_ps_col_scl[0].w) * _206;
   SV_Target.x = _323;
   SV_Target.y = _324;
   SV_Target.z = _325;
   SV_Target.w = _326;
   int _329 = c_bool & 8;
   bool _330 = (_329 != 0);
   float _331 = select(_330, _315, _323);
   float _332 = select(_330, _316, _324);
   float _333 = select(_330, _317, _325);
   float _334 = select(_330, _206, _326);
   SV_Target.x = _331;
   SV_Target.y = _332;
   SV_Target.z = _333;
   SV_Target.w = _334;
   return SV_Target;
}
