// Yakuza Kiwami material s_o1dszt (0x5872B33F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s13_t : register(t13);

Texture2D<float4> s14_t : register(t14);

cbuffer cb12 : register(b12)
{
   float4 c_matP[4] : packoffset(c000.x);
   float4 c_matV[4] : packoffset(c004.x);
   float4 c_matVP[4] : packoffset(c008.x);
   float4 c_matT[4][4] : packoffset(c012.x);
   float4 c_rvtx_param : packoffset(c028.x);
   float4 c_rt_size : packoffset(c029.x);
   float3 c_location : packoffset(c030.x);
};

cbuffer cb13 : register(b13)
{
   uint c_bool : packoffset(c000.x);
};

cbuffer cb6 : register(b6)
{
   float3 c_diffuse_scale : packoffset(c000.x);
   float c_global_opacity : packoffset(c000.w);
   float3 c_specular_scale : packoffset(c001.x);
   float c_emissive_scale : packoffset(c001.w);
};

cbuffer cb7 : register(b7)
{
   float4 c_attribute_vs[4] : packoffset(c000.x);
   float4 c_attribute_ps[4] : packoffset(c004.x);
   float c_mat_punchthrough_ref : packoffset(c008.x);
   float c_mat_glare_ref : packoffset(c008.y);
   float c_mat_fresnel : packoffset(c008.z);
   uint c_mat_custom_shader_flags : packoffset(c008.w);
   float3 c_mat_diffuse : packoffset(c009.x);
   float c_mat_opacity : packoffset(c009.w);
   float3 c_mat_specular : packoffset(c010.x);
   float c_mat_power : packoffset(c010.w);
   float3 c_mat_emissive : packoffset(c011.x);
   float c_mat_ambient : packoffset(c011.w);
};

cbuffer cb5 : register(b5)
{
   float2 c_uv0_offset : packoffset(c000.x);
   float2 c_uv1_offset : packoffset(c000.z);
   float2 c_uv2_offset : packoffset(c001.x);
   float c_blend_scale : packoffset(c001.z);
   float c_opacity_scale : packoffset(c001.w);
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

cbuffer cb2 : register(b2)
{
   float4 c_ps_scratch[92] : packoffset(c000.x);
};

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s2_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _54 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _62 = _57.x * _57.x;
   float _63 = _57.y * _57.y;
   float _64 = _57.z * _57.z;
   float4 _65 = s3_t.Sample(s3_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _69 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _74 = c_mat_power * _54.z;
   float _75 = max(1.0f, _74);
   float _76 = _69.x * 2.0f;
   float _77 = _69.y * 2.0f;
   float _78 = _76 + -1.0f;
   float _79 = _77 + -1.0f;
   float _80 = dot(float2(_78, _79), float2(_78, _79));
   float _81 = 1.0f - _80;
   float _82 = saturate(_81);
   float _83 = sqrt(_82);
   float _84 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _85 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _86 = _84 - _85;
   float _87 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _88 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _89 = _87 - _88;
   float _90 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _91 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _92 = _90 - _91;
   float _93 = _78 * TEXCOORD_3.x;
   float _94 = _78 * TEXCOORD_3.y;
   float _95 = _78 * TEXCOORD_3.z;
   float _96 = _79 * TEXCOORD_3.w;
   float _97 = _96 * _86;
   float _98 = _96 * _89;
   float _99 = _96 * _92;
   float _100 = _97 + _93;
   float _101 = _98 + _94;
   float _102 = _99 + _95;
   float _103 = _83 * TEXCOORD_2.x;
   float _104 = _83 * TEXCOORD_2.y;
   float _105 = _83 * TEXCOORD_2.z;
   float _106 = _100 + _103;
   float _107 = _101 + _104;
   float _108 = _102 + _105;
   float _109 = dot(float3(_106, _107, _108), float3(_106, _107, _108));
   float _110 = rsqrt(_109);
   float _111 = _106 * _110;
   float _112 = _107 * _110;
   float _113 = _108 * _110;
   float _118 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _119 = rsqrt(_118);
   float _120 = _119 * TEXCOORD_1.x;
   float _121 = _119 * TEXCOORD_1.y;
   float _122 = _119 * TEXCOORD_1.z;
   float _123 = _120 + (c_light_vec[0].x);
   float _124 = _121 + (c_light_vec[0].y);
   float _125 = _122 + (c_light_vec[0].z);
   float _126 = dot(float3(_123, _124, _125), float3(_123, _124, _125));
   float _127 = rsqrt(_126);
   float _128 = _123 * _127;
   float _129 = _124 * _127;
   float _130 = _125 * _127;
   float _131 = _112 + 0.10000000149011612f;
   float _132 = saturate(_131);
   float _135 = (c_attribute_ps[2].w) + 2.0f;
   float _137 = (c_attribute_ps[2].z) + 2.5f;
   float _138 = -0.0f - _120;
   float _139 = -0.0f - _121;
   float _140 = -0.0f - _122;
   float _141 = dot(float3(_138, _139, _140), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _142 = 1.0f - _141;
   float _143 = saturate(_142);
   float _144 = dot(float3(_138, _139, _140), float3(_111, _112, _113));
   float _145 = 1.0f - _144;
   float _146 = saturate(_145);
   float _147 = _146 * _143;
   float _148 = log2(_147);
   float _149 = _148 * _135;
   float _150 = exp2(_149);
   float _151 = _150 * _137;
   int _154 = c_bool & 4096;
   bool _155 = (_154 == 0);
   float _164;
   float _188;
   float _189;
   float _201;
   float _202;
   float _390;
   float _391;
   float _392;
   float _393;
   float _394;
   float _395;
   float _428;
   float _429;
   float _430;
   float _431;
   float _596;
   float _766;
   float _767;
   float _768;
   float _844;
   float _845;
   float _846;
   float _862;
   float _863;
   float _864;
   float _889;
   float _890;
   float _891;
   if (!_155)
   {
      float _160 = (c_ps_user[9].w) * _148;
      float _161 = exp2(_160);
      float _162 = _161 * (c_ps_user[9].z);
      _164 = _162;
   }
   else
   {
      _164 = _151;
   }
   float _165 = _132 * _164;
   float _166 = _54.y * _165;
   float _169 = _166 * (c_ps_user[2].x);
   float _170 = _169 + 1.0f;
   float _171 = _62 * _170;
   float _172 = _63 * _170;
   float _173 = _64 * _170;
   float _177 = c_rt_size.z * SV_Position.x;
   float _178 = c_rt_size.w * SV_Position.y;
   int _181 = c_bool & 8;
   bool _182 = (_181 == 0);
   if (!_182)
   {
      float4 _184 = s13_t.Sample(s13_s, float2(_177, _178));
      _188 = _184.x;
      _189 = _184.y;
   }
   else
   {
      _188 = 1.0f;
      _189 = 1.0f;
   }
   if (!_182)
   {
      float _194 = (c_sdw_param_ch[0].x) * _188;
      float _195 = (c_sdw_param_ch[0].z) * _189;
      float _198 = _194 + (c_sdw_param_ch[0].y);
      float _199 = _195 + (c_sdw_param_ch[0].w);
      _201 = _198;
      _202 = _199;
   }
   else
   {
      _201 = _188;
      _202 = _189;
   }
   float _203 = min(_201, _202);
   float _212 = (c_light_vec[1].x) + _120;
   float _213 = (c_light_vec[1].y) + _121;
   float _214 = (c_light_vec[1].z) + _122;
   float _215 = dot(float3(_212, _213, _214), float3(_212, _213, _214));
   float _216 = rsqrt(_215);
   float _217 = _212 * _216;
   float _218 = _213 * _216;
   float _219 = _214 * _216;
   float _220 = -0.0f - (c_light_vec[1].x);
   float _221 = -0.0f - (c_light_vec[1].y);
   float _222 = -0.0f - (c_light_vec[1].z);
   float _223 = -0.0f - (c_light_vec[0].x);
   float _224 = -0.0f - (c_light_vec[0].y);
   float _225 = -0.0f - (c_light_vec[0].z);
   float _226 = dot(float3(_120, _121, _122), float3(_111, _112, _113));
   float _227 = _226 * 2.0f;
   float _228 = _227 * _111;
   float _229 = _227 * _112;
   float _230 = _227 * _113;
   float _231 = _120 - _228;
   float _232 = _121 - _229;
   float _233 = _122 - _230;
   float _234 = dot(float3(_231, _232, _233), float3(_223, _224, _225));
   float _235 = dot(float3(_231, _232, _233), float3(_220, _221, _222));
   float _236 = saturate(_234);
   float _237 = saturate(_235);
   float _238 = log2(_236);
   float _239 = log2(_237);
   float _240 = _238 * _75;
   float _241 = _239 * _75;
   float _242 = exp2(_240);
   float _243 = exp2(_241);
   float _248 = c_mat_fresnel * c_tone_control.w;
   float _249 = _75 * 0.03974359855055809f;
   float _250 = _249 + 0.0856831967830658f;
   float _251 = _243 * _250;
   float _252 = dot(float3(_120, _121, _122), float3(_128, _129, _130));
   float _253 = dot(float3(_120, _121, _122), float3(_217, _218, _219));
   float _254 = 1.0f - _252;
   float _255 = 1.0f - _253;
   float _256 = _254 * _254;
   float _257 = _255 * _255;
   float _258 = 1.0f - _248;
   float _259 = _256 * _256;
   float _260 = _259 * _254;
   float _261 = _260 * _258;
   float _262 = _255 * _258;
   float _263 = _257 * _257;
   float _264 = _263 * _262;
   float _265 = _261 + _248;
   float _266 = _264 + _248;
   float _267 = dot(float3(_111, _112, _113), float3(_223, _224, _225));
   float _268 = saturate(_267);
   float _269 = dot(float3(_111, _112, _113), float3(_220, _221, _222));
   float _270 = saturate(_269);
   float _271 = _251 * _270;
   float _272 = _271 * _266;
   float _281 = c_specular_scale.x * c_mat_specular.x;
   float _282 = c_specular_scale.y * c_mat_specular.y;
   float _283 = c_specular_scale.z * c_mat_specular.z;
   float _288 = _203 * _250;
   float _289 = _288 * _242;
   float _290 = _289 * _268;
   float _291 = _290 * _265;
   float _292 = (c_light_col[0].x) * _291;
   float _293 = (c_light_col[0].y) * _291;
   float _294 = (c_light_col[0].z) * _291;
   float _299 = (c_light_col[1].x) * _272;
   float _300 = (c_light_col[1].y) * _272;
   float _301 = (c_light_col[1].z) * _272;
   float _302 = _299 + _292;
   float _303 = _300 + _293;
   float _304 = _301 + _294;
   float _305 = _281 * _302;
   float _306 = _282 * _303;
   float _307 = _283 * _304;
   int _311 = c_bool & 1;
   bool _312 = (_311 == 0);
   if (!_312)
   {
      float _314 = -0.0f - TEXCOORD_4.x;
      float _315 = -0.0f - TEXCOORD_4.y;
      float _316 = -0.0f - TEXCOORD_4.z;
      float _317 = dot(float3(_111, _112, _113), float3(_314, _315, _316));
      float _318 = saturate(_317);
      float _319 = _318 * TEXCOORD_1.w;
      float _320 = _318 * TEXCOORD_2.w;
      float _321 = _318 * TEXCOORD_4.w;
      float _322 = max(0.0f, _319);
      float _323 = max(0.0f, _320);
      float _324 = max(0.0f, _321);
      float _325 = -0.0f - TEXCOORD_5.x;
      float _326 = -0.0f - TEXCOORD_5.y;
      float _327 = -0.0f - TEXCOORD_5.z;
      float _328 = dot(float3(_111, _112, _113), float3(_325, _326, _327));
      float _329 = saturate(_328);
      float _330 = _329 * COLOR.w;
      float _331 = _329 * COLOR_1.w;
      float _332 = _329 * TEXCOORD_5.w;
      float _333 = max(0.0f, _330);
      float _334 = max(0.0f, _331);
      float _335 = max(0.0f, _332);
      float _336 = _333 + _322;
      float _337 = _334 + _323;
      float _338 = _335 + _324;
      float _339 = _120 + TEXCOORD_4.x;
      float _340 = _121 + TEXCOORD_4.y;
      float _341 = _122 + TEXCOORD_4.z;
      float _342 = dot(float3(_339, _340, _341), float3(_339, _340, _341));
      float _343 = rsqrt(_342);
      float _344 = _120 + TEXCOORD_5.x;
      float _345 = _121 + TEXCOORD_5.y;
      float _346 = _122 + TEXCOORD_5.z;
      float _347 = dot(float3(_344, _345, _346), float3(_344, _345, _346));
      float _348 = rsqrt(_347);
      float _349 = _344 * _348;
      float _350 = -0.0f - _349;
      float _351 = _345 * _348;
      float _352 = -0.0f - _351;
      float _353 = _346 * _348;
      float _354 = -0.0f - _353;
      float _355 = _339 * _343;
      float _356 = -0.0f - _355;
      float _357 = _340 * _343;
      float _358 = -0.0f - _357;
      float _359 = _341 * _343;
      float _360 = -0.0f - _359;
      float _361 = dot(float3(_356, _358, _360), float3(_111, _112, _113));
      float _362 = dot(float3(_350, _352, _354), float3(_111, _112, _113));
      float _363 = saturate(_361);
      float _364 = saturate(_362);
      float _365 = log2(_363);
      float _366 = log2(_364);
      float _367 = _365 * _75;
      float _368 = _366 * _75;
      float _369 = exp2(_367);
      float _370 = exp2(_368);
      float _371 = _369 * TEXCOORD_1.w;
      float _372 = _369 * TEXCOORD_2.w;
      float _373 = _369 * TEXCOORD_4.w;
      float _374 = _370 * COLOR.w;
      float _375 = _370 * COLOR_1.w;
      float _376 = _370 * TEXCOORD_5.w;
      float _377 = _374 + _371;
      float _378 = _375 + _372;
      float _379 = _376 + _373;
      float _380 = max(0.0f, _377);
      float _381 = max(0.0f, _378);
      float _382 = max(0.0f, _379);
      float _383 = _281 * _380;
      float _384 = _282 * _381;
      float _385 = _283 * _382;
      float _386 = min(_383, 1.0f);
      float _387 = min(_384, 1.0f);
      float _388 = min(_385, 1.0f);
      _390 = _386;
      _391 = _387;
      _392 = _388;
      _393 = _336;
      _394 = _337;
      _395 = _338;
   }
   else
   {
      _390 = 0.0f;
      _391 = 0.0f;
      _392 = 0.0f;
      _393 = 0.0f;
      _394 = 0.0f;
      _395 = 0.0f;
   }
   float _396 = c_rt_size.z * SV_Position.x;
   float _397 = c_rt_size.w * SV_Position.y;
   float4 _398 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_396, _397)));
   int _405 = c_bool & 16384;
   bool _406 = (_405 == 0);
   if (!_406)
   {
      float _408 = _398.x * 4.0f;
      float _409 = _398.y * 4.0f;
      float _410 = _398.z * 4.0f;
      float _411 = _398.w * 4.0f;
      _428 = _408;
      _429 = _409;
      _430 = _410;
      _431 = _411;
   }
   else
   {
      int _413 = c_bool & 8192;
      bool _414 = (_413 == 0);
      if (!_414)
      {
         float _416 = _398.x * 2.0f;
         float _417 = _398.y * 2.0f;
         float _418 = _398.z * 2.0f;
         float _419 = _398.w * 2.0f;
         _428 = _416;
         _429 = _417;
         _430 = _418;
         _431 = _419;
      }
      else
      {
         float _423 = _398.x / c_tone_control.z;
         float _424 = _398.y / c_tone_control.z;
         float _425 = _398.z / c_tone_control.z;
         float _426 = _398.w / c_tone_control.z;
         _428 = _423;
         _429 = _424;
         _430 = _425;
         _431 = _426;
      }
   }
   float _432 = _428 + _393;
   float _433 = _429 + _394;
   float _434 = _430 + _395;
   float _435 = _431 + _390;
   float _436 = _431 + _391;
   float _437 = _431 + _392;
   float _500 = dot(float3(_111, _112, _113), float3(_111, _112, _113));
   float _501 = rsqrt(_500);
   float _502 = _501 * _111;
   float _503 = _501 * _112;
   float _504 = _501 * _113;
   float _505 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_502, _503, _504, 1.0f));
   float _506 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_502, _503, _504, 1.0f));
   float _507 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_502, _503, _504, 1.0f));
   float _508 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_502, _503, _504, 1.0f));
   float _509 = dot(float4(_502, _503, _504, 1.0f), float4(_505, _506, _507, _508));
   float _510 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_502, _503, _504, 1.0f));
   float _511 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_502, _503, _504, 1.0f));
   float _512 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_502, _503, _504, 1.0f));
   float _513 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_502, _503, _504, 1.0f));
   float _514 = dot(float4(_502, _503, _504, 1.0f), float4(_510, _511, _512, _513));
   float _515 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_502, _503, _504, 1.0f));
   float _516 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_502, _503, _504, 1.0f));
   float _517 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_502, _503, _504, 1.0f));
   float _518 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_502, _503, _504, 1.0f));
   float _519 = dot(float4(_502, _503, _504, 1.0f), float4(_515, _516, _517, _518));
   float _520 = max(_509, 0.0625f);
   float _521 = max(_514, 0.0625f);
   float _522 = max(_519, 0.0625f);
   float _523 = _520 * (c_ps_scratch[79].x);
   float _524 = _521 * (c_ps_scratch[79].x);
   float _525 = _522 * (c_ps_scratch[79].x);
   float _530 = -0.0f - (c_light_vec[0].x);
   float _531 = -0.0f - (c_light_vec[0].y);
   float _532 = -0.0f - (c_light_vec[0].z);
   float _533 = dot(float3(_111, _112, _113), float3(_530, _531, _532));
   float _534 = -0.0f - (c_light_vec[1].x);
   float _535 = -0.0f - (c_light_vec[1].y);
   float _536 = -0.0f - (c_light_vec[1].z);
   float _537 = dot(float3(_111, _112, _113), float3(_534, _535, _536));
   float _546 = _112 * 0.5f;
   float _547 = _546 + 0.5f;
   float _548 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _549 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _550 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _551 = _548 * _547;
   float _552 = _549 * _547;
   float _553 = _550 * _547;
   float _554 = saturate(_533);
   float _555 = saturate(_537);
   float _560 = (c_light_col[0].x) * _554;
   float _561 = (c_light_col[0].y) * _554;
   float _562 = (c_light_col[0].z) * _554;
   float _567 = (c_light_col[1].x) * _555;
   float _568 = (c_light_col[1].y) * _555;
   float _569 = (c_light_col[1].z) * _555;
   float _570 = _560 * _203;
   float _571 = _561 * _203;
   float _572 = _562 * _203;
   float _573 = (c_light_vec[4].x) + _523;
   float _574 = _573 + _551;
   float _575 = _574 + _432;
   float _576 = _575 + _567;
   float _577 = _576 + _570;
   float _578 = (c_light_vec[4].y) + _524;
   float _579 = _578 + _552;
   float _580 = _579 + _433;
   float _581 = _580 + _568;
   float _582 = _581 + _571;
   float _583 = (c_light_vec[4].z) + _525;
   float _584 = _583 + _553;
   float _585 = _584 + _434;
   float _586 = _585 + _569;
   float _587 = _586 + _572;
   int _588 = c_bool & 64;
   bool _589 = (_588 == 0);
   if (!_589)
   {
      float _591 = 1.0f - _54.y;
      float _592 = _203 * _591;
      float _593 = _592 * _554;
      float _594 = _593 + _54.y;
      _596 = _594;
   }
   else
   {
      _596 = _54.y;
   }
   float _597 = _435 + _305;
   float _598 = _436 + _306;
   float _599 = _437 + _307;
   int _600 = c_bool & 32768;
   bool _601 = (_600 == 0);
   if (!_601)
   {
      float _613 = (c_ps_user[11].x) * _171;
      float _614 = (c_ps_user[11].x) * _172;
      float _615 = (c_ps_user[11].x) * _173;
      float _617 = _111 + 1.0f;
      float _618 = _112 + 1.0f;
      float _619 = _113 + 1.0f;
      float _620 = _617 * 0.5f;
      float _621 = _618 * 0.5f;
      float _622 = _619 * 0.5f;
      float _623 = saturate(_620);
      float _624 = saturate(_621);
      float _625 = saturate(_622);
      float _626 = (c_ps_user[11].y) * _623;
      float _627 = _624 * (c_ps_user[11].y);
      float _628 = _625 * (c_ps_user[11].y);
      float _638 = (c_ps_user[11].z) * _65.x;
      float _639 = (c_ps_user[11].z) * _65.y;
      float _640 = (c_ps_user[11].z) * _65.z;
      float _642 = (c_ps_user[11].w) * _597;
      float _645 = (c_ps_user[12].x) * _203;
      float _647 = (c_ps_user[12].y) * _54.y;
      float _652 = _145 * _145;
      float _653 = _145 * 0.9722222089767456f;
      float _654 = _652 * _652;
      float _655 = _654 * _653;
      float _656 = _655 + 0.02777777425944805f;
      float _657 = (c_ps_user[13].z) * _656;
      float _659 = (c_ps_user[13].w) * _574;
      float _660 = (c_ps_user[13].w) * _579;
      float _661 = (c_ps_user[13].w) * _584;
      float _664 = (c_ps_user[14].y) * _560;
      float _665 = (c_ps_user[14].y) * _561;
      float _666 = (c_ps_user[14].y) * _562;
      float _668 = (c_ps_user[14].z) * _567;
      float _669 = (c_ps_user[14].z) * _568;
      float _670 = (c_ps_user[14].z) * _569;
      float _672 = (c_ps_user[14].w) * _432;
      float _673 = (c_ps_user[14].w) * _433;
      float _674 = (c_ps_user[14].w) * _434;
      float _679 = (c_ps_user[15].z) * _435;
      float _680 = (c_ps_user[15].z) * _436;
      float _681 = (c_ps_user[15].z) * _437;
      float _682 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _683 = c_specular_scale.x * _682;
      float _684 = _683 * _597;
      float _685 = _664 + (c_ps_user[13].x);
      float _686 = _685 + _668;
      float _687 = c_diffuse_scale.x * _686;
      float _688 = _687 * c_mat_diffuse.x;
      float _689 = (c_ps_user[13].y) + _638;
      float _690 = _689 * c_specular_scale.x;
      float _691 = _690 + _684;
      float _692 = _691 * c_mat_specular.x;
      float _693 = _626 + _613;
      float _694 = _693 + _642;
      float _695 = _694 + _645;
      float _696 = _695 + _647;
      float _697 = _696 + _659;
      float _698 = _697 + _657;
      float _699 = _698 + _672;
      float _700 = _699 + _679;
      float _701 = _700 + _688;
      float _702 = _701 + _692;
      float _703 = c_specular_scale.y * _682;
      float _704 = _703 * _598;
      float _705 = _665 + (c_ps_user[13].x);
      float _706 = _705 + _669;
      float _707 = c_diffuse_scale.y * _706;
      float _708 = _707 * c_mat_diffuse.y;
      float _709 = (c_ps_user[13].y) + _639;
      float _710 = _709 * c_specular_scale.y;
      float _711 = _710 + _704;
      float _712 = _711 * c_mat_specular.y;
      float _713 = _642 + _614;
      float _714 = _713 + _627;
      float _715 = _714 + _645;
      float _716 = _715 + _647;
      float _717 = _716 + _660;
      float _718 = _717 + _657;
      float _719 = _718 + _673;
      float _720 = _719 + _680;
      float _721 = _720 + _708;
      float _722 = _721 + _712;
      float _723 = c_specular_scale.z * _682;
      float _724 = _723 * _599;
      float _725 = _666 + (c_ps_user[13].x);
      float _726 = _725 + _670;
      float _727 = c_diffuse_scale.z * _726;
      float _728 = _727 * c_mat_diffuse.z;
      float _729 = (c_ps_user[13].y) + _640;
      float _730 = _729 * c_specular_scale.z;
      float _731 = _730 + _724;
      float _732 = _731 * c_mat_specular.z;
      float _733 = _642 + _615;
      float _734 = _733 + _628;
      float _735 = _734 + _645;
      float _736 = _735 + _647;
      float _737 = _736 + _661;
      float _738 = _737 + _657;
      float _739 = _738 + _674;
      float _740 = _739 + _681;
      float _741 = _740 + _728;
      float _742 = _741 + _732;
      float _743 = saturate(_702);
      float _744 = saturate(_722);
      float _745 = saturate(_742);
      _766 = _743;
      _767 = _744;
      _768 = _745;
   }
   else
   {
      float _747 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _748 = _747 * _171;
      float _749 = _748 * _577;
      float _750 = _749 * _596;
      float _751 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _752 = _751 * _172;
      float _753 = _752 * _582;
      float _754 = _753 * _596;
      float _755 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _756 = _755 * _173;
      float _757 = _756 * _587;
      float _758 = _757 * _596;
      float _759 = _597 * _65.x;
      float _760 = _598 * _65.y;
      float _761 = _599 * _65.z;
      float _762 = _750 + _759;
      float _763 = _754 + _760;
      float _764 = _758 + _761;
      _766 = _762;
      _767 = _763;
      _768 = _764;
   }
   float _773 = c_mat_opacity * _57.w;
   float _774 = _773 * c_opacity_scale;
   float _775 = _774 * c_global_opacity;
   float _776 = _766 * COLOR.x;
   float _777 = _767 * COLOR.y;
   float _778 = _768 * COLOR.z;
   int _779 = c_bool & 16;
   bool _780 = (_779 == 0);
   if (!_780)
   {
      float _784 = c_tone_control.x * _776;
      float _785 = c_tone_control.x * _777;
      float _786 = c_tone_control.x * _778;
      float _788 = log2(_784);
      float _789 = log2(_785);
      float _790 = log2(_786);
      float _791 = _788 * c_tone_control.y;
      float _792 = _789 * c_tone_control.y;
      float _793 = _790 * c_tone_control.y;
      float _794 = exp2(_791);
      float _795 = exp2(_792);
      float _796 = exp2(_793);
      _844 = _794;
      _845 = _795;
      _846 = _796;
   }
   else
   {
      if (_601)
      {
         float _801 = c_tone_control.x * _776;
         float _802 = c_tone_control.x * _777;
         float _803 = c_tone_control.x * _778;
         float _804 = _801 * 2.0f;
         float _805 = _802 * 2.0f;
         float _806 = _803 * 2.0f;
         float _807 = _801 * 0.30000001192092896f;
         float _808 = _802 * 0.30000001192092896f;
         float _809 = _803 * 0.30000001192092896f;
         float _810 = _807 + 0.05000000074505806f;
         float _811 = _808 + 0.05000000074505806f;
         float _812 = _809 + 0.05000000074505806f;
         float _813 = _810 * _804;
         float _814 = _811 * _805;
         float _815 = _812 * _806;
         float _816 = _813 + 0.004000000189989805f;
         float _817 = _814 + 0.004000000189989805f;
         float _818 = _815 + 0.004000000189989805f;
         float _819 = _807 + 0.5f;
         float _820 = _808 + 0.5f;
         float _821 = _809 + 0.5f;
         float _822 = _819 * _804;
         float _823 = _820 * _805;
         float _824 = _821 * _806;
         float _825 = _822 + 0.06000000238418579f;
         float _826 = _823 + 0.06000000238418579f;
         float _827 = _824 + 0.06000000238418579f;
         float _828 = _816 / _825;
         float _829 = _817 / _826;
         float _830 = _818 / _827;
         float _831 = _828 + -0.06666666269302368f;
         float _832 = _829 + -0.06666666269302368f;
         float _833 = _830 + -0.06666666269302368f;
         float _834 = _831 * 1.3790643215179443f;
         float _835 = _832 * 1.3790643215179443f;
         float _836 = _833 * 1.3790643215179443f;
         float _837 = YKR_MaterialCurve(_834, _801);
         float _838 = YKR_MaterialCurve(_835, _802);
         float _839 = YKR_MaterialCurve(_836, _803);
         float _840 = sqrt(_837);
         float _841 = sqrt(_838);
         float _842 = sqrt(_839);
         _844 = _840;
         _845 = _841;
         _846 = _842;
      }
      else
      {
         _844 = _776;
         _845 = _777;
         _846 = _778;
      }
   }
   int _847 = c_bool & 4;
   bool _848 = (_847 == 0);
   if (_848)
   {
      float _850 = saturate(COLOR_1.x);
      float _855 = (c_fog_param[0].x) - _844;
      float _856 = (c_fog_param[0].y) - _845;
      float _857 = (c_fog_param[0].z) - _846;
      float _858 = _855 * _850;
      float _859 = _856 * _850;
      float _860 = _857 * _850;
      _862 = _858;
      _863 = _859;
      _864 = _860;
   }
   else
   {
      _862 = COLOR_1.x;
      _863 = COLOR_1.y;
      _864 = COLOR_1.z;
   }
   float _865 = _862 + _844;
   float _866 = _863 + _845;
   float _867 = _864 + _846;
   if (!_601)
   {
      float _875 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _876 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _877 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _879 = (c_ps_user[12].w) * COLOR.x;
      float _880 = (c_ps_user[12].w) * COLOR.y;
      float _881 = (c_ps_user[12].w) * COLOR.z;
      float _882 = _879 + _865;
      float _883 = _882 + _875;
      float _884 = _880 + _866;
      float _885 = _884 + _876;
      float _886 = _881 + _867;
      float _887 = _886 + _877;
      _889 = _883;
      _890 = _885;
      _891 = _887;
   }
   else
   {
      _889 = _865;
      _890 = _866;
      _891 = _867;
   }
   float _897 = (c_ps_col_scl[0].x) * _889;
   float _898 = (c_ps_col_scl[0].y) * _890;
   float _899 = (c_ps_col_scl[0].z) * _891;
   float _900 = (c_ps_col_scl[0].w) * _775;
   SV_Target.x = _897;
   SV_Target.y = _898;
   SV_Target.z = _899;
   SV_Target.w = _900;
   int _903 = c_bool & 8;
   bool _904 = (_903 != 0);
   float _905 = select(_904, _889, _897);
   float _906 = select(_904, _890, _898);
   float _907 = select(_904, _891, _899);
   float _908 = select(_904, _775, _900);
   SV_Target.x = _905;
   SV_Target.y = _906;
   SV_Target.z = _907;
   SV_Target.w = _908;
   return SV_Target;
}
