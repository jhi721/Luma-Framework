// Yakuza Kiwami material s_o1dzt[rd][rs][dedit] (0x81FFAD9B, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s7_t : register(t7);

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

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _58 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _62 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _62.x * _62.x;
   float _68 = _62.y * _62.y;
   float _69 = _62.z * _62.z;
   float4 _70 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _73 = s4_t.Sample(s4_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _75 = 1.0f - _73.x;
   float _76 = _75 * _58.z;
   float _77 = 1.0f - _76;
   float _82 = _67 * (c_ps_user[3].x);
   float _83 = _68 * (c_ps_user[3].y);
   float _84 = _69 * (c_ps_user[3].z);
   float _87 = _70.x * 2.0f;
   float _88 = _70.y * 2.0f;
   float _89 = _87 + -1.0f;
   float _90 = _88 + -1.0f;
   float _91 = dot(float2(_89, _90), float2(_89, _90));
   float _92 = 1.0f - _91;
   float _93 = saturate(_92);
   float _94 = sqrt(_93);
   float _95 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _96 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _97 = _95 - _96;
   float _98 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _99 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _100 = _98 - _99;
   float _101 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _102 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _103 = _101 - _102;
   float _104 = _89 * TEXCOORD_3.x;
   float _105 = _89 * TEXCOORD_3.y;
   float _106 = _89 * TEXCOORD_3.z;
   float _107 = _90 * TEXCOORD_3.w;
   float _108 = _107 * _97;
   float _109 = _107 * _100;
   float _110 = _107 * _103;
   float _111 = _108 + _104;
   float _112 = _109 + _105;
   float _113 = _110 + _106;
   float _114 = _94 * TEXCOORD_2.x;
   float _115 = _94 * TEXCOORD_2.y;
   float _116 = _94 * TEXCOORD_2.z;
   float _117 = _111 + _114;
   float _118 = _112 + _115;
   float _119 = _113 + _116;
   float _120 = dot(float3(_117, _118, _119), float3(_117, _118, _119));
   float _121 = rsqrt(_120);
   float _122 = _117 * _121;
   float _123 = _118 * _121;
   float _124 = _119 * _121;
   float _129 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _130 = rsqrt(_129);
   float _131 = _130 * TEXCOORD_1.x;
   float _132 = _130 * TEXCOORD_1.y;
   float _133 = _130 * TEXCOORD_1.z;
   float _134 = _131 + (c_light_vec[0].x);
   float _135 = _132 + (c_light_vec[0].y);
   float _136 = _133 + (c_light_vec[0].z);
   float _137 = dot(float3(_134, _135, _136), float3(_134, _135, _136));
   float _138 = rsqrt(_137);
   float _139 = _134 * _138;
   float _140 = _135 * _138;
   float _141 = _136 * _138;
   float _145 = (c_attribute_ps[2].x) * TEXCOORD.x;
   float _146 = (c_attribute_ps[2].y) * TEXCOORD.y;
   float4 _147 = s7_t.Sample(s7_s, float2(_145, _146));
   float _152 = _147.x * _147.x;
   float _153 = _147.y * _147.y;
   float _154 = _147.z * _147.z;
   float _155 = 1.0f - _152;
   float _156 = 1.0f - _153;
   float _157 = 1.0f - _154;
   float _158 = 1.0f - _58.z;
   float _159 = 1.0f - _147.w;
   float _160 = _159 * _158;
   float _161 = _160 * _155;
   float _162 = _160 * _156;
   float _163 = _160 * _157;
   float _164 = _161 + _152;
   float _165 = _162 + _153;
   float _166 = _163 + _154;
   float _167 = _82 * _164;
   float _168 = _83 * _165;
   float _169 = _84 * _166;
   float _170 = _123 + 0.10000000149011612f;
   float _171 = saturate(_170);
   float _174 = (c_attribute_ps[2].w) + 2.0f;
   float _176 = (c_attribute_ps[2].z) + 2.5f;
   float _177 = -0.0f - _131;
   float _178 = -0.0f - _132;
   float _179 = -0.0f - _133;
   float _180 = dot(float3(_177, _178, _179), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _181 = 1.0f - _180;
   float _182 = saturate(_181);
   float _183 = dot(float3(_177, _178, _179), float3(_122, _123, _124));
   float _184 = 1.0f - _183;
   float _185 = saturate(_184);
   float _186 = _185 * _182;
   float _187 = log2(_186);
   float _188 = _187 * _174;
   float _189 = exp2(_188);
   float _190 = _189 * _176;
   int _193 = c_bool & 4096;
   bool _194 = (_193 == 0);
   float _203;
   float _227;
   float _228;
   float _240;
   float _241;
   float _429;
   float _430;
   float _431;
   float _432;
   float _433;
   float _434;
   float _467;
   float _468;
   float _469;
   float _470;
   float _635;
   float _802;
   float _803;
   float _804;
   float _880;
   float _881;
   float _882;
   float _898;
   float _899;
   float _900;
   float _925;
   float _926;
   float _927;
   if (!_194)
   {
      float _199 = (c_ps_user[9].w) * _187;
      float _200 = exp2(_199);
      float _201 = _200 * (c_ps_user[9].z);
      _203 = _201;
   }
   else
   {
      _203 = _190;
   }
   float _204 = _171 * _203;
   float _205 = _58.y * _204;
   float _208 = _205 * (c_ps_user[2].x);
   float _209 = _208 + 1.0f;
   float _210 = _167 * _209;
   float _211 = _168 * _209;
   float _212 = _169 * _209;
   float _216 = c_rt_size.z * SV_Position.x;
   float _217 = c_rt_size.w * SV_Position.y;
   int _220 = c_bool & 8;
   bool _221 = (_220 == 0);
   if (!_221)
   {
      float4 _223 = s13_t.Sample(s13_s, float2(_216, _217));
      _227 = _223.x;
      _228 = _223.y;
   }
   else
   {
      _227 = 1.0f;
      _228 = 1.0f;
   }
   if (!_221)
   {
      float _233 = (c_sdw_param_ch[0].x) * _227;
      float _234 = (c_sdw_param_ch[0].z) * _228;
      float _237 = _233 + (c_sdw_param_ch[0].y);
      float _238 = _234 + (c_sdw_param_ch[0].w);
      _240 = _237;
      _241 = _238;
   }
   else
   {
      _240 = _227;
      _241 = _228;
   }
   float _242 = min(_240, _241);
   float _251 = (c_light_vec[1].x) + _131;
   float _252 = (c_light_vec[1].y) + _132;
   float _253 = (c_light_vec[1].z) + _133;
   float _254 = dot(float3(_251, _252, _253), float3(_251, _252, _253));
   float _255 = rsqrt(_254);
   float _256 = _251 * _255;
   float _257 = _252 * _255;
   float _258 = _253 * _255;
   float _259 = -0.0f - (c_light_vec[1].x);
   float _260 = -0.0f - (c_light_vec[1].y);
   float _261 = -0.0f - (c_light_vec[1].z);
   float _262 = -0.0f - (c_light_vec[0].x);
   float _263 = -0.0f - (c_light_vec[0].y);
   float _264 = -0.0f - (c_light_vec[0].z);
   float _265 = dot(float3(_131, _132, _133), float3(_122, _123, _124));
   float _266 = _265 * 2.0f;
   float _267 = _266 * _122;
   float _268 = _266 * _123;
   float _269 = _266 * _124;
   float _270 = _131 - _267;
   float _271 = _132 - _268;
   float _272 = _133 - _269;
   float _273 = dot(float3(_270, _271, _272), float3(_262, _263, _264));
   float _274 = dot(float3(_270, _271, _272), float3(_259, _260, _261));
   float _275 = saturate(_273);
   float _276 = saturate(_274);
   float _277 = log2(_275);
   float _278 = log2(_276);
   float _279 = _277 * c_mat_power;
   float _280 = _278 * c_mat_power;
   float _281 = exp2(_279);
   float _282 = exp2(_280);
   float _287 = c_mat_fresnel * c_tone_control.w;
   float _288 = c_mat_power * 0.03974359855055809f;
   float _289 = _288 + 0.0856831967830658f;
   float _290 = _282 * _289;
   float _291 = dot(float3(_131, _132, _133), float3(_139, _140, _141));
   float _292 = dot(float3(_131, _132, _133), float3(_256, _257, _258));
   float _293 = 1.0f - _291;
   float _294 = 1.0f - _292;
   float _295 = _293 * _293;
   float _296 = _294 * _294;
   float _297 = 1.0f - _287;
   float _298 = _295 * _295;
   float _299 = _298 * _293;
   float _300 = _299 * _297;
   float _301 = _294 * _297;
   float _302 = _296 * _296;
   float _303 = _302 * _301;
   float _304 = _300 + _287;
   float _305 = _303 + _287;
   float _306 = dot(float3(_122, _123, _124), float3(_262, _263, _264));
   float _307 = saturate(_306);
   float _308 = dot(float3(_122, _123, _124), float3(_259, _260, _261));
   float _309 = saturate(_308);
   float _310 = _290 * _309;
   float _311 = _310 * _305;
   float _320 = c_specular_scale.x * c_mat_specular.x;
   float _321 = c_specular_scale.y * c_mat_specular.y;
   float _322 = c_specular_scale.z * c_mat_specular.z;
   float _327 = _242 * _289;
   float _328 = _327 * _281;
   float _329 = _328 * _307;
   float _330 = _329 * _304;
   float _331 = (c_light_col[0].x) * _330;
   float _332 = (c_light_col[0].y) * _330;
   float _333 = (c_light_col[0].z) * _330;
   float _338 = (c_light_col[1].x) * _311;
   float _339 = (c_light_col[1].y) * _311;
   float _340 = (c_light_col[1].z) * _311;
   float _341 = _338 + _331;
   float _342 = _339 + _332;
   float _343 = _340 + _333;
   float _344 = _320 * _341;
   float _345 = _321 * _342;
   float _346 = _322 * _343;
   int _350 = c_bool & 1;
   bool _351 = (_350 == 0);
   if (!_351)
   {
      float _353 = -0.0f - TEXCOORD_4.x;
      float _354 = -0.0f - TEXCOORD_4.y;
      float _355 = -0.0f - TEXCOORD_4.z;
      float _356 = dot(float3(_122, _123, _124), float3(_353, _354, _355));
      float _357 = saturate(_356);
      float _358 = _357 * TEXCOORD_1.w;
      float _359 = _357 * TEXCOORD_2.w;
      float _360 = _357 * TEXCOORD_4.w;
      float _361 = max(0.0f, _358);
      float _362 = max(0.0f, _359);
      float _363 = max(0.0f, _360);
      float _364 = -0.0f - TEXCOORD_5.x;
      float _365 = -0.0f - TEXCOORD_5.y;
      float _366 = -0.0f - TEXCOORD_5.z;
      float _367 = dot(float3(_122, _123, _124), float3(_364, _365, _366));
      float _368 = saturate(_367);
      float _369 = _368 * COLOR.w;
      float _370 = _368 * COLOR_1.w;
      float _371 = _368 * TEXCOORD_5.w;
      float _372 = max(0.0f, _369);
      float _373 = max(0.0f, _370);
      float _374 = max(0.0f, _371);
      float _375 = _372 + _361;
      float _376 = _373 + _362;
      float _377 = _374 + _363;
      float _378 = _131 + TEXCOORD_4.x;
      float _379 = _132 + TEXCOORD_4.y;
      float _380 = _133 + TEXCOORD_4.z;
      float _381 = dot(float3(_378, _379, _380), float3(_378, _379, _380));
      float _382 = rsqrt(_381);
      float _383 = _131 + TEXCOORD_5.x;
      float _384 = _132 + TEXCOORD_5.y;
      float _385 = _133 + TEXCOORD_5.z;
      float _386 = dot(float3(_383, _384, _385), float3(_383, _384, _385));
      float _387 = rsqrt(_386);
      float _388 = _383 * _387;
      float _389 = -0.0f - _388;
      float _390 = _384 * _387;
      float _391 = -0.0f - _390;
      float _392 = _385 * _387;
      float _393 = -0.0f - _392;
      float _394 = _378 * _382;
      float _395 = -0.0f - _394;
      float _396 = _379 * _382;
      float _397 = -0.0f - _396;
      float _398 = _380 * _382;
      float _399 = -0.0f - _398;
      float _400 = dot(float3(_395, _397, _399), float3(_122, _123, _124));
      float _401 = dot(float3(_389, _391, _393), float3(_122, _123, _124));
      float _402 = saturate(_400);
      float _403 = saturate(_401);
      float _404 = log2(_402);
      float _405 = log2(_403);
      float _406 = _404 * c_mat_power;
      float _407 = _405 * c_mat_power;
      float _408 = exp2(_406);
      float _409 = exp2(_407);
      float _410 = _408 * TEXCOORD_1.w;
      float _411 = _408 * TEXCOORD_2.w;
      float _412 = _408 * TEXCOORD_4.w;
      float _413 = _409 * COLOR.w;
      float _414 = _409 * COLOR_1.w;
      float _415 = _409 * TEXCOORD_5.w;
      float _416 = _413 + _410;
      float _417 = _414 + _411;
      float _418 = _415 + _412;
      float _419 = max(0.0f, _416);
      float _420 = max(0.0f, _417);
      float _421 = max(0.0f, _418);
      float _422 = _320 * _419;
      float _423 = _321 * _420;
      float _424 = _322 * _421;
      float _425 = min(_422, 1.0f);
      float _426 = min(_423, 1.0f);
      float _427 = min(_424, 1.0f);
      _429 = _425;
      _430 = _426;
      _431 = _427;
      _432 = _375;
      _433 = _376;
      _434 = _377;
   }
   else
   {
      _429 = 0.0f;
      _430 = 0.0f;
      _431 = 0.0f;
      _432 = 0.0f;
      _433 = 0.0f;
      _434 = 0.0f;
   }
   float _435 = c_rt_size.z * SV_Position.x;
   float _436 = c_rt_size.w * SV_Position.y;
   float4 _437 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_435, _436)));
   int _444 = c_bool & 16384;
   bool _445 = (_444 == 0);
   if (!_445)
   {
      float _447 = _437.x * 4.0f;
      float _448 = _437.y * 4.0f;
      float _449 = _437.z * 4.0f;
      float _450 = _437.w * 4.0f;
      _467 = _447;
      _468 = _448;
      _469 = _449;
      _470 = _450;
   }
   else
   {
      int _452 = c_bool & 8192;
      bool _453 = (_452 == 0);
      if (!_453)
      {
         float _455 = _437.x * 2.0f;
         float _456 = _437.y * 2.0f;
         float _457 = _437.z * 2.0f;
         float _458 = _437.w * 2.0f;
         _467 = _455;
         _468 = _456;
         _469 = _457;
         _470 = _458;
      }
      else
      {
         float _462 = _437.x / c_tone_control.z;
         float _463 = _437.y / c_tone_control.z;
         float _464 = _437.z / c_tone_control.z;
         float _465 = _437.w / c_tone_control.z;
         _467 = _462;
         _468 = _463;
         _469 = _464;
         _470 = _465;
      }
   }
   float _471 = _467 + _432;
   float _472 = _468 + _433;
   float _473 = _469 + _434;
   float _474 = _470 + _429;
   float _475 = _470 + _430;
   float _476 = _470 + _431;
   float _539 = dot(float3(_122, _123, _124), float3(_122, _123, _124));
   float _540 = rsqrt(_539);
   float _541 = _540 * _122;
   float _542 = _540 * _123;
   float _543 = _540 * _124;
   float _544 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_541, _542, _543, 1.0f));
   float _545 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_541, _542, _543, 1.0f));
   float _546 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_541, _542, _543, 1.0f));
   float _547 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_541, _542, _543, 1.0f));
   float _548 = dot(float4(_541, _542, _543, 1.0f), float4(_544, _545, _546, _547));
   float _549 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_541, _542, _543, 1.0f));
   float _550 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_541, _542, _543, 1.0f));
   float _551 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_541, _542, _543, 1.0f));
   float _552 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_541, _542, _543, 1.0f));
   float _553 = dot(float4(_541, _542, _543, 1.0f), float4(_549, _550, _551, _552));
   float _554 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_541, _542, _543, 1.0f));
   float _555 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_541, _542, _543, 1.0f));
   float _556 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_541, _542, _543, 1.0f));
   float _557 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_541, _542, _543, 1.0f));
   float _558 = dot(float4(_541, _542, _543, 1.0f), float4(_554, _555, _556, _557));
   float _559 = max(_548, 0.0625f);
   float _560 = max(_553, 0.0625f);
   float _561 = max(_558, 0.0625f);
   float _562 = _559 * (c_ps_scratch[79].x);
   float _563 = _560 * (c_ps_scratch[79].x);
   float _564 = _561 * (c_ps_scratch[79].x);
   float _569 = -0.0f - (c_light_vec[0].x);
   float _570 = -0.0f - (c_light_vec[0].y);
   float _571 = -0.0f - (c_light_vec[0].z);
   float _572 = dot(float3(_122, _123, _124), float3(_569, _570, _571));
   float _573 = -0.0f - (c_light_vec[1].x);
   float _574 = -0.0f - (c_light_vec[1].y);
   float _575 = -0.0f - (c_light_vec[1].z);
   float _576 = dot(float3(_122, _123, _124), float3(_573, _574, _575));
   float _585 = _123 * 0.5f;
   float _586 = _585 + 0.5f;
   float _587 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _588 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _589 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _590 = _587 * _586;
   float _591 = _588 * _586;
   float _592 = _589 * _586;
   float _593 = saturate(_572);
   float _594 = saturate(_576);
   float _599 = (c_light_col[0].x) * _593;
   float _600 = (c_light_col[0].y) * _593;
   float _601 = (c_light_col[0].z) * _593;
   float _606 = (c_light_col[1].x) * _594;
   float _607 = (c_light_col[1].y) * _594;
   float _608 = (c_light_col[1].z) * _594;
   float _609 = _599 * _242;
   float _610 = _600 * _242;
   float _611 = _601 * _242;
   float _612 = (c_light_vec[4].x) + _562;
   float _613 = _612 + _590;
   float _614 = _613 + _471;
   float _615 = _614 + _606;
   float _616 = _615 + _609;
   float _617 = (c_light_vec[4].y) + _563;
   float _618 = _617 + _591;
   float _619 = _618 + _472;
   float _620 = _619 + _607;
   float _621 = _620 + _610;
   float _622 = (c_light_vec[4].z) + _564;
   float _623 = _622 + _592;
   float _624 = _623 + _473;
   float _625 = _624 + _608;
   float _626 = _625 + _611;
   int _627 = c_bool & 64;
   bool _628 = (_627 == 0);
   if (!_628)
   {
      float _630 = 1.0f - _58.y;
      float _631 = _242 * _630;
      float _632 = _631 * _593;
      float _633 = _632 + _58.y;
      _635 = _633;
   }
   else
   {
      _635 = _58.y;
   }
   float _636 = _474 + _344;
   float _637 = _475 + _345;
   float _638 = _476 + _346;
   int _639 = c_bool & 32768;
   bool _640 = (_639 == 0);
   if (!_640)
   {
      float _652 = (c_ps_user[11].x) * _210;
      float _653 = (c_ps_user[11].x) * _211;
      float _654 = (c_ps_user[11].x) * _212;
      float _656 = _122 + 1.0f;
      float _657 = _123 + 1.0f;
      float _658 = _124 + 1.0f;
      float _659 = _656 * 0.5f;
      float _660 = _657 * 0.5f;
      float _661 = _658 * 0.5f;
      float _662 = saturate(_659);
      float _663 = saturate(_660);
      float _664 = saturate(_661);
      float _665 = (c_ps_user[11].y) * _662;
      float _666 = _663 * (c_ps_user[11].y);
      float _667 = _664 * (c_ps_user[11].y);
      float _677 = (c_ps_user[11].z) * _58.x;
      float _679 = (c_ps_user[11].w) * _636;
      float _682 = (c_ps_user[12].x) * _242;
      float _684 = (c_ps_user[12].y) * _58.y;
      float _689 = _184 * _184;
      float _690 = _184 * 0.9722222089767456f;
      float _691 = _689 * _689;
      float _692 = _691 * _690;
      float _693 = _692 + 0.02777777425944805f;
      float _694 = (c_ps_user[13].z) * _693;
      float _696 = (c_ps_user[13].w) * _613;
      float _697 = (c_ps_user[13].w) * _618;
      float _698 = (c_ps_user[13].w) * _623;
      float _701 = (c_ps_user[14].y) * _599;
      float _702 = (c_ps_user[14].y) * _600;
      float _703 = (c_ps_user[14].y) * _601;
      float _705 = (c_ps_user[14].z) * _606;
      float _706 = (c_ps_user[14].z) * _607;
      float _707 = (c_ps_user[14].z) * _608;
      float _709 = (c_ps_user[14].w) * _471;
      float _710 = (c_ps_user[14].w) * _472;
      float _711 = (c_ps_user[14].w) * _473;
      float _716 = (c_ps_user[15].z) * _474;
      float _717 = (c_ps_user[15].z) * _475;
      float _718 = (c_ps_user[15].z) * _476;
      float _719 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _720 = c_specular_scale.x * _719;
      float _721 = _720 * _636;
      float _722 = _701 + (c_ps_user[13].x);
      float _723 = _722 + _705;
      float _724 = c_diffuse_scale.x * _723;
      float _725 = _724 * c_mat_diffuse.x;
      float _726 = (c_ps_user[13].y) + _677;
      float _727 = _726 * c_specular_scale.x;
      float _728 = _727 + _721;
      float _729 = _728 * c_mat_specular.x;
      float _730 = _665 + _652;
      float _731 = _730 + _679;
      float _732 = _731 + _682;
      float _733 = _732 + _684;
      float _734 = _733 + _696;
      float _735 = _734 + _694;
      float _736 = _735 + _709;
      float _737 = _736 + _716;
      float _738 = _737 + _725;
      float _739 = _738 + _729;
      float _740 = c_specular_scale.y * _719;
      float _741 = _740 * _637;
      float _742 = _702 + (c_ps_user[13].x);
      float _743 = _742 + _706;
      float _744 = c_diffuse_scale.y * _743;
      float _745 = _744 * c_mat_diffuse.y;
      float _746 = _726 * c_specular_scale.y;
      float _747 = _746 + _741;
      float _748 = _747 * c_mat_specular.y;
      float _749 = _679 + _653;
      float _750 = _749 + _666;
      float _751 = _750 + _682;
      float _752 = _751 + _684;
      float _753 = _752 + _697;
      float _754 = _753 + _694;
      float _755 = _754 + _710;
      float _756 = _755 + _717;
      float _757 = _756 + _745;
      float _758 = _757 + _748;
      float _759 = c_specular_scale.z * _719;
      float _760 = _759 * _638;
      float _761 = _703 + (c_ps_user[13].x);
      float _762 = _761 + _707;
      float _763 = c_diffuse_scale.z * _762;
      float _764 = _763 * c_mat_diffuse.z;
      float _765 = _726 * c_specular_scale.z;
      float _766 = _765 + _760;
      float _767 = _766 * c_mat_specular.z;
      float _768 = _679 + _654;
      float _769 = _768 + _667;
      float _770 = _769 + _682;
      float _771 = _770 + _684;
      float _772 = _771 + _698;
      float _773 = _772 + _694;
      float _774 = _773 + _711;
      float _775 = _774 + _718;
      float _776 = _775 + _764;
      float _777 = _776 + _767;
      float _778 = saturate(_739);
      float _779 = saturate(_758);
      float _780 = saturate(_777);
      _802 = _778;
      _803 = _779;
      _804 = _780;
   }
   else
   {
      float _782 = _635 * _77;
      float _783 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _784 = _783 * _210;
      float _785 = _784 * _616;
      float _786 = _785 * _782;
      float _787 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _788 = _787 * _211;
      float _789 = _788 * _621;
      float _790 = _789 * _782;
      float _791 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _792 = _791 * _212;
      float _793 = _792 * _626;
      float _794 = _793 * _782;
      float _795 = _636 * _58.x;
      float _796 = _637 * _58.x;
      float _797 = _638 * _58.x;
      float _798 = _786 + _795;
      float _799 = _790 + _796;
      float _800 = _794 + _797;
      _802 = _798;
      _803 = _799;
      _804 = _800;
   }
   float _809 = c_mat_opacity * _62.w;
   float _810 = _809 * c_opacity_scale;
   float _811 = _810 * c_global_opacity;
   float _812 = _802 * COLOR.x;
   float _813 = _803 * COLOR.y;
   float _814 = _804 * COLOR.z;
   int _815 = c_bool & 16;
   bool _816 = (_815 == 0);
   if (!_816)
   {
      float _820 = c_tone_control.x * _812;
      float _821 = c_tone_control.x * _813;
      float _822 = c_tone_control.x * _814;
      float _824 = log2(_820);
      float _825 = log2(_821);
      float _826 = log2(_822);
      float _827 = _824 * c_tone_control.y;
      float _828 = _825 * c_tone_control.y;
      float _829 = _826 * c_tone_control.y;
      float _830 = exp2(_827);
      float _831 = exp2(_828);
      float _832 = exp2(_829);
      _880 = _830;
      _881 = _831;
      _882 = _832;
   }
   else
   {
      if (_640)
      {
         float _837 = c_tone_control.x * _812;
         float _838 = c_tone_control.x * _813;
         float _839 = c_tone_control.x * _814;
         float _840 = _837 * 2.0f;
         float _841 = _838 * 2.0f;
         float _842 = _839 * 2.0f;
         float _843 = _837 * 0.30000001192092896f;
         float _844 = _838 * 0.30000001192092896f;
         float _845 = _839 * 0.30000001192092896f;
         float _846 = _843 + 0.05000000074505806f;
         float _847 = _844 + 0.05000000074505806f;
         float _848 = _845 + 0.05000000074505806f;
         float _849 = _846 * _840;
         float _850 = _847 * _841;
         float _851 = _848 * _842;
         float _852 = _849 + 0.004000000189989805f;
         float _853 = _850 + 0.004000000189989805f;
         float _854 = _851 + 0.004000000189989805f;
         float _855 = _843 + 0.5f;
         float _856 = _844 + 0.5f;
         float _857 = _845 + 0.5f;
         float _858 = _855 * _840;
         float _859 = _856 * _841;
         float _860 = _857 * _842;
         float _861 = _858 + 0.06000000238418579f;
         float _862 = _859 + 0.06000000238418579f;
         float _863 = _860 + 0.06000000238418579f;
         float _864 = _852 / _861;
         float _865 = _853 / _862;
         float _866 = _854 / _863;
         float _867 = _864 + -0.06666666269302368f;
         float _868 = _865 + -0.06666666269302368f;
         float _869 = _866 + -0.06666666269302368f;
         float _870 = _867 * 1.3790643215179443f;
         float _871 = _868 * 1.3790643215179443f;
         float _872 = _869 * 1.3790643215179443f;
         float _873 = YKR_MaterialCurve(_870, _837);
         float _874 = YKR_MaterialCurve(_871, _838);
         float _875 = YKR_MaterialCurve(_872, _839);
         float _876 = sqrt(_873);
         float _877 = sqrt(_874);
         float _878 = sqrt(_875);
         _880 = _876;
         _881 = _877;
         _882 = _878;
      }
      else
      {
         _880 = _812;
         _881 = _813;
         _882 = _814;
      }
   }
   int _883 = c_bool & 4;
   bool _884 = (_883 == 0);
   if (_884)
   {
      float _886 = saturate(COLOR_1.x);
      float _891 = (c_fog_param[0].x) - _880;
      float _892 = (c_fog_param[0].y) - _881;
      float _893 = (c_fog_param[0].z) - _882;
      float _894 = _891 * _886;
      float _895 = _892 * _886;
      float _896 = _893 * _886;
      _898 = _894;
      _899 = _895;
      _900 = _896;
   }
   else
   {
      _898 = COLOR_1.x;
      _899 = COLOR_1.y;
      _900 = COLOR_1.z;
   }
   float _901 = _898 + _880;
   float _902 = _899 + _881;
   float _903 = _900 + _882;
   if (!_640)
   {
      float _911 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _912 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _913 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _915 = (c_ps_user[12].w) * COLOR.x;
      float _916 = (c_ps_user[12].w) * COLOR.y;
      float _917 = (c_ps_user[12].w) * COLOR.z;
      float _918 = _915 + _901;
      float _919 = _918 + _911;
      float _920 = _916 + _902;
      float _921 = _920 + _912;
      float _922 = _917 + _903;
      float _923 = _922 + _913;
      _925 = _919;
      _926 = _921;
      _927 = _923;
   }
   else
   {
      _925 = _901;
      _926 = _902;
      _927 = _903;
   }
   float _933 = (c_ps_col_scl[0].x) * _925;
   float _934 = (c_ps_col_scl[0].y) * _926;
   float _935 = (c_ps_col_scl[0].z) * _927;
   float _936 = (c_ps_col_scl[0].w) * _811;
   SV_Target.x = _933;
   SV_Target.y = _934;
   SV_Target.z = _935;
   SV_Target.w = _936;
   int _939 = c_bool & 8;
   bool _940 = (_939 != 0);
   float _941 = select(_940, _925, _933);
   float _942 = select(_940, _926, _934);
   float _943 = select(_940, _927, _935);
   float _944 = select(_940, _811, _936);
   SV_Target.x = _941;
   SV_Target.y = _942;
   SV_Target.z = _943;
   SV_Target.w = _944;
   return SV_Target;
}
