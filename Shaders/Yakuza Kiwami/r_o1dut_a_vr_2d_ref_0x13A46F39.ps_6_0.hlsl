// Yakuza Kiwami material r_o1dut_a(vr)2d[ref] (0x13A46F39, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float _64 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _65 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _66 = s3_t.Sample(s3_s, float2(_64, _65));
   float _72 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _73 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _74 = s5_t.Sample(s5_s, float2(_72, _73));
   float4 _77 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _81 = _74.x * 2.0f;
   float _82 = _74.y * 2.0f;
   float _83 = _81 + -1.0f;
   float _84 = _82 + -1.0f;
   float _85 = dot(float2(_83, _84), float2(_83, _84));
   float _86 = 1.0f - _85;
   float _87 = saturate(_86);
   float _88 = sqrt(_87);
   float _89 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _90 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _91 = _89 - _90;
   float _92 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _93 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _94 = _92 - _93;
   float _95 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _96 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _97 = _95 - _96;
   float _98 = _83 * TEXCOORD_6.x;
   float _99 = _83 * TEXCOORD_6.y;
   float _100 = _83 * TEXCOORD_6.z;
   float _101 = _84 * TEXCOORD_6.w;
   float _102 = _101 * _91;
   float _103 = _101 * _94;
   float _104 = _101 * _97;
   float _105 = _102 + _98;
   float _106 = _103 + _99;
   float _107 = _104 + _100;
   float _108 = _88 * TEXCOORD_2.x;
   float _109 = _88 * TEXCOORD_2.y;
   float _110 = _88 * TEXCOORD_2.z;
   float _111 = _105 + _108;
   float _112 = _106 + _109;
   float _113 = _107 + _110;
   float _114 = dot(float3(_111, _112, _113), float3(_111, _112, _113));
   float _115 = rsqrt(_114);
   float _116 = _111 * _115;
   float _117 = _112 * _115;
   float _118 = _113 * _115;
   float _119 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _120 = rsqrt(_119);
   float _121 = _120 * TEXCOORD_3.x;
   float _122 = _120 * TEXCOORD_3.y;
   float _123 = _120 * TEXCOORD_3.z;
   float _128 = -0.0f - (c_light_vec[0].x);
   float _129 = -0.0f - (c_light_vec[0].y);
   float _130 = -0.0f - (c_light_vec[0].z);
   float _131 = dot(float3(_116, _117, _118), float3(_128, _129, _130));
   float _135 = c_rt_size.z * SV_Position.x;
   float _136 = c_rt_size.w * SV_Position.y;
   int _139 = c_bool & 8;
   bool _140 = (_139 == 0);
   float _147;
   float _148;
   float _149;
   float _161;
   float _162;
   float _198;
   float _199;
   float _200;
   float _201;
   float _215;
   float _216;
   float _251;
   float _252;
   float _253;
   float _287;
   float _288;
   float _289;
   float _290;
   float _291;
   float _292;
   float _446;
   float _447;
   float _448;
   float _449;
   float _450;
   float _451;
   float _623;
   float _624;
   float _625;
   float _626;
   float _627;
   float _628;
   float _756;
   float _757;
   float _758;
   float _774;
   float _775;
   float _776;
   if (!_140)
   {
      float4 _142 = s13_t.Sample(s13_s, float2(_135, _136));
      _147 = _142.x;
      _148 = _142.y;
      _149 = _142.w;
   }
   else
   {
      _147 = 1.0f;
      _148 = 1.0f;
      _149 = 1.0f;
   }
   if (!_140)
   {
      float _154 = (c_sdw_param[0].x) * _147;
      float _155 = (c_sdw_param[0].z) * _148;
      float _158 = _154 + (c_sdw_param[0].y);
      float _159 = _155 + (c_sdw_param[0].w);
      _161 = _158;
      _162 = _159;
   }
   else
   {
      _161 = _147;
      _162 = _148;
   }
   float _166 = c_rt_size.z * SV_Position.x;
   float _167 = c_rt_size.w * SV_Position.y;
   float4 _168 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_166, _167)));
   int _175 = c_bool & 16384;
   bool _176 = (_175 == 0);
   if (!_176)
   {
      float _178 = _168.x * 2.0f;
      float _179 = _168.y * 2.0f;
      float _180 = _168.z * 2.0f;
      float _181 = _168.w * 2.0f;
      _198 = _178;
      _199 = _179;
      _200 = _180;
      _201 = _181;
   }
   else
   {
      int _183 = c_bool & 8192;
      bool _184 = (_183 == 0);
      if (!_184)
      {
         float _186 = _168.x * 2.0f;
         float _187 = _168.y * 2.0f;
         float _188 = _168.z * 2.0f;
         float _189 = _168.w * 2.0f;
         _198 = _186;
         _199 = _187;
         _200 = _188;
         _201 = _189;
      }
      else
      {
         float _193 = _168.x / c_tone_control.z;
         float _194 = _168.y / c_tone_control.z;
         float _195 = _168.z / c_tone_control.z;
         float _196 = _168.w / c_tone_control.z;
         _198 = _193;
         _199 = _194;
         _200 = _195;
         _201 = _196;
      }
   }
   int _202 = c_bool & 8;
   bool _203 = (_202 == 0);
   if (!_203)
   {
      float _208 = (c_sdw_param[0].x) * _161;
      float _209 = (c_sdw_param[0].z) * _162;
      float _212 = _208 + (c_sdw_param[0].y);
      float _213 = _209 + (c_sdw_param[0].w);
      _215 = _212;
      _216 = _213;
   }
   else
   {
      _215 = _161;
      _216 = _162;
   }
   float _217 = _216 * _215;
   float _222 = 1.0f - (c_light_col[5].x);
   float _223 = 1.0f - (c_light_col[5].y);
   float _224 = 1.0f - (c_light_col[5].z);
   float _225 = _222 * _217;
   float _226 = _223 * _217;
   float _227 = _224 * _217;
   float _228 = _225 + (c_light_col[5].x);
   float _229 = _226 + (c_light_col[5].y);
   float _230 = _227 + (c_light_col[5].z);
   float _231 = _149 * COLOR.x;
   float _232 = _149 * COLOR.y;
   float _233 = _149 * COLOR.z;
   if (!_203)
   {
      float _237 = (c_sdw_param[2].x) * _228;
      float _238 = (c_sdw_param[2].x) * _229;
      float _239 = (c_sdw_param[2].x) * _230;
      float _241 = _237 + (c_sdw_param[2].y);
      float _242 = _238 + (c_sdw_param[2].y);
      float _243 = _239 + (c_sdw_param[2].y);
      float _246 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _247 = _241 * _246;
      float _248 = _242 * _246;
      float _249 = _243 * _246;
      _251 = _247;
      _252 = _248;
      _253 = _249;
   }
   else
   {
      _251 = _228;
      _252 = _229;
      _253 = _230;
   }
   float _254 = _251 * _149;
   float _255 = _252 * _149;
   float _256 = _253 * _149;
   float _267 = c_mat_emissive.x * _58;
   float _268 = _267 * c_emissive_scale;
   float _269 = c_mat_emissive.y * _59;
   float _270 = _269 * c_emissive_scale;
   float _271 = c_mat_emissive.z * _60;
   float _272 = _271 * c_emissive_scale;
   int _273 = c_bool & 2048;
   bool _274 = (_273 == 0);
   if (_274)
   {
      float _276 = _216 * _198;
      float _277 = _216 * _199;
      float _278 = _216 * _200;
      float _279 = _276 + TEXCOORD_4.x;
      float _280 = _277 + TEXCOORD_4.y;
      float _281 = _278 + TEXCOORD_4.z;
      float _282 = _216 * _201;
      float _283 = _282 + TEXCOORD_5.x;
      float _284 = _282 + TEXCOORD_5.y;
      float _285 = _282 + TEXCOORD_5.z;
      _287 = _279;
      _288 = _280;
      _289 = _281;
      _290 = _283;
      _291 = _284;
      _292 = _285;
   }
   else
   {
      _287 = TEXCOORD_4.x;
      _288 = TEXCOORD_4.y;
      _289 = TEXCOORD_4.z;
      _290 = TEXCOORD_5.x;
      _291 = TEXCOORD_5.y;
      _292 = TEXCOORD_5.z;
   }
   float _297 = -0.0f - (c_light_vec[1].x);
   float _298 = -0.0f - (c_light_vec[1].y);
   float _299 = -0.0f - (c_light_vec[1].z);
   float _300 = dot(float3(_116, _117, _118), float3(_297, _298, _299));
   float _301 = saturate(_300);
   float _306 = saturate(_131);
   float _307 = (c_light_col[0].x) * _306;
   float _308 = (c_light_col[0].y) * _306;
   float _309 = (c_light_col[0].z) * _306;
   float _314 = (c_light_col[1].x) * _301;
   float _315 = (c_light_col[1].y) * _301;
   float _316 = (c_light_col[1].z) * _301;
   float _325 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _326 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _327 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _328 = _307 * _228;
   float _329 = _308 * _229;
   float _330 = _309 * _230;
   float _331 = _314 * _216;
   float _332 = _315 * _216;
   float _333 = _316 * _216;
   float _334 = _331 + _328;
   float _335 = _332 + _329;
   float _336 = _333 + _330;
   float _337 = _325 * _334;
   float _338 = _326 * _335;
   float _339 = _327 * _336;
   float _340 = _268 + c_scene_ambient.x;
   float _341 = _340 + _337;
   float _342 = _270 + c_scene_ambient.y;
   float _343 = _342 + _338;
   float _344 = _272 + c_scene_ambient.z;
   float _345 = _344 + _339;
   float _346 = _341 * _231;
   float _347 = _343 * _232;
   float _348 = _345 * _233;
   float _349 = _287 * _254;
   float _350 = _288 * _255;
   float _351 = _289 * _256;
   float _352 = _346 + _349;
   float _353 = _347 + _350;
   float _354 = _348 + _351;
   float _359 = (c_light_vec[0].x) + _121;
   float _360 = (c_light_vec[0].y) + _122;
   float _361 = (c_light_vec[0].z) + _123;
   float _362 = dot(float3(_359, _360, _361), float3(_359, _360, _361));
   float _363 = rsqrt(_362);
   float _364 = _359 * _363;
   float _365 = -0.0f - _364;
   float _366 = _360 * _363;
   float _367 = -0.0f - _366;
   float _368 = _361 * _363;
   float _369 = -0.0f - _368;
   float _370 = dot(float3(_365, _367, _369), float3(_116, _117, _118));
   float _371 = saturate(_370);
   float _372 = _122 + -0.30000001192092896f;
   float _373 = dot(float3(_121, _372, _123), float3(_121, _372, _123));
   float _374 = rsqrt(_373);
   float _375 = _121 * _374;
   float _376 = -0.0f - _375;
   float _377 = _372 * _374;
   float _378 = -0.0f - _377;
   float _379 = _123 * _374;
   float _380 = -0.0f - _379;
   float _381 = dot(float3(_376, _378, _380), float3(_116, _117, _118));
   float _382 = saturate(_381);
   float _385 = log2(_371);
   float _386 = log2(_382);
   float _387 = _385 * c_mat_power;
   float _388 = _386 * c_mat_power;
   float _389 = exp2(_387);
   float _390 = exp2(_388);
   float _393 = _389 * (c_light_col[0].x);
   float _394 = _389 * (c_light_col[0].y);
   float _395 = _389 * (c_light_col[0].z);
   float _396 = _390 * 2.0f;
   float _397 = _396 * (c_light_vec[5].x);
   float _398 = _397 * (c_light_col[1].x);
   float _399 = _397 * (c_light_col[1].y);
   float _400 = _397 * (c_light_col[1].z);
   float _401 = _393 * _228;
   float _402 = _394 * _229;
   float _403 = _395 * _230;
   float _404 = _398 * _301;
   float _405 = _399 * _301;
   float _406 = _400 * _301;
   float _407 = _404 + _401;
   float _408 = _405 + _402;
   float _409 = _406 + _403;
   float _416 = c_mat_specular.x * c_specular_scale.x;
   float _417 = _416 * _231;
   float _418 = _417 * _407;
   float _419 = c_mat_specular.y * c_specular_scale.y;
   float _420 = _419 * _232;
   float _421 = _420 * _408;
   float _422 = c_mat_specular.z * c_specular_scale.z;
   float _423 = _422 * _233;
   float _424 = _423 * _409;
   float _425 = _290 * _254;
   float _426 = _425 * _390;
   float _427 = _291 * _255;
   float _428 = _427 * _390;
   float _429 = _292 * _256;
   float _430 = _429 * _390;
   float _431 = _418 + _426;
   float _432 = _421 + _428;
   float _433 = _424 + _430;
   if (!_274)
   {
      float _435 = _216 * _198;
      float _436 = _216 * _199;
      float _437 = _216 * _200;
      float _438 = _352 + _435;
      float _439 = _353 + _436;
      float _440 = _354 + _437;
      float _441 = _216 * _201;
      float _442 = _431 + _441;
      float _443 = _432 + _441;
      float _444 = _433 + _441;
      _446 = _438;
      _447 = _439;
      _448 = _440;
      _449 = _442;
      _450 = _443;
      _451 = _444;
   }
   else
   {
      _446 = _352;
      _447 = _353;
      _448 = _354;
      _449 = _431;
      _450 = _432;
      _451 = _433;
   }
   int _452 = c_bool & 128;
   bool _453 = (_452 != 0);
   float _454 = select(_453, 0.0f, _449);
   float _455 = select(_453, 0.0f, _450);
   float _456 = select(_453, 0.0f, _451);
   float _457 = _446 * _58;
   float _458 = _447 * _59;
   float _459 = _448 * _60;
   float _460 = _454 * _66.y;
   float _461 = _455 * _66.y;
   float _462 = _456 * _66.y;
   int _463 = c_bool & 32768;
   bool _464 = (_463 == 0);
   if (!_464)
   {
      float _468 = (c_ps_user[11].x) * _58;
      float _469 = (c_ps_user[11].x) * _59;
      float _470 = (c_ps_user[11].x) * _60;
      float _472 = _116 + 1.0f;
      float _473 = _117 + 1.0f;
      float _474 = _118 + 1.0f;
      float _475 = _472 * 0.5f;
      float _476 = _473 * 0.5f;
      float _477 = _474 * 0.5f;
      float _478 = saturate(_475);
      float _479 = saturate(_476);
      float _480 = saturate(_477);
      float _481 = (c_ps_user[11].y) * _478;
      float _482 = _479 * (c_ps_user[11].y);
      float _483 = _480 * (c_ps_user[11].y);
      float _485 = (c_ps_user[11].z) * _66.y;
      float _487 = c_mat_power * 0.009999999776482582f;
      float _488 = _487 * (c_ps_user[11].w);
      float _491 = (c_ps_user[12].x) * _228;
      float _492 = (c_ps_user[12].x) * _229;
      float _493 = (c_ps_user[12].x) * _230;
      float _495 = (c_ps_user[12].y) * _231;
      float _496 = (c_ps_user[12].y) * _232;
      float _497 = (c_ps_user[12].y) * _233;
      float _499 = (c_ps_user[12].z) * _254;
      float _500 = (c_ps_user[12].z) * _255;
      float _501 = (c_ps_user[12].z) * _256;
      float _507 = -0.0f - _121;
      float _508 = -0.0f - _122;
      float _509 = -0.0f - _123;
      float _510 = dot(float3(_507, _508, _509), float3(_116, _117, _118));
      float _511 = 1.0f - _510;
      float _512 = _511 * _511;
      float _513 = _511 * 0.9722222089767456f;
      float _514 = _512 * _512;
      float _515 = _514 * _513;
      float _516 = _515 + 0.02777777425944805f;
      float _517 = _516 * (c_ps_user[13].z);
      float _519 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _520 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _521 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _524 = (c_ps_user[14].x) * _268;
      float _525 = (c_ps_user[14].x) * _270;
      float _526 = (c_ps_user[14].x) * _272;
      float _528 = (c_ps_user[14].y) * _307;
      float _529 = (c_ps_user[14].y) * _308;
      float _530 = (c_ps_user[14].y) * _309;
      float _532 = (c_ps_user[14].z) * _314;
      float _533 = (c_ps_user[14].z) * _315;
      float _534 = (c_ps_user[14].z) * _316;
      float _536 = (c_ps_user[14].w) * _287;
      float _537 = (c_ps_user[14].w) * _288;
      float _538 = (c_ps_user[14].w) * _289;
      float _541 = (c_ps_user[15].x) * _393;
      float _542 = (c_ps_user[15].x) * _394;
      float _543 = (c_ps_user[15].x) * _395;
      float _545 = _416 * _398;
      float _546 = _545 * (c_ps_user[15].y);
      float _547 = _419 * _399;
      float _548 = _547 * (c_ps_user[15].y);
      float _549 = _422 * _400;
      float _550 = _549 * (c_ps_user[15].y);
      float _552 = (c_ps_user[15].z) * _290;
      float _553 = (c_ps_user[15].z) * _291;
      float _554 = (c_ps_user[15].z) * _292;
      float _555 = _546 + _552;
      float _556 = _555 * _390;
      float _557 = _528 + (c_ps_user[13].x);
      float _558 = _557 + _532;
      float _559 = c_diffuse_scale.x * _558;
      float _560 = _559 * c_mat_diffuse.x;
      float _561 = (c_ps_user[13].y) + _485;
      float _562 = _561 + _541;
      float _563 = c_mat_specular.x * _562;
      float _564 = _563 * c_specular_scale.x;
      float _565 = _481 + _468;
      float _566 = _565 + _488;
      float _567 = _566 + (c_ps_user[12].w);
      float _568 = _567 + _491;
      float _569 = _568 + _495;
      float _570 = _569 + _499;
      float _571 = _570 + _519;
      float _572 = _571 + _524;
      float _573 = _572 + _536;
      float _574 = _573 + _517;
      float _575 = _574 + _556;
      float _576 = _575 + _560;
      float _577 = _576 + _564;
      float _578 = _548 + _553;
      float _579 = _578 * _390;
      float _580 = _529 + (c_ps_user[13].x);
      float _581 = _580 + _533;
      float _582 = c_diffuse_scale.y * _581;
      float _583 = _582 * c_mat_diffuse.y;
      float _584 = _561 + _542;
      float _585 = c_mat_specular.y * _584;
      float _586 = _585 * c_specular_scale.y;
      float _587 = _482 + _469;
      float _588 = _587 + _488;
      float _589 = _588 + (c_ps_user[12].w);
      float _590 = _589 + _492;
      float _591 = _590 + _496;
      float _592 = _591 + _500;
      float _593 = _592 + _520;
      float _594 = _593 + _525;
      float _595 = _594 + _537;
      float _596 = _595 + _517;
      float _597 = _596 + _579;
      float _598 = _597 + _583;
      float _599 = _598 + _586;
      float _600 = _550 + _554;
      float _601 = _600 * _390;
      float _602 = _530 + (c_ps_user[13].x);
      float _603 = _602 + _534;
      float _604 = c_diffuse_scale.z * _603;
      float _605 = _604 * c_mat_diffuse.z;
      float _606 = _561 + _543;
      float _607 = c_mat_specular.z * _606;
      float _608 = _607 * c_specular_scale.z;
      float _609 = _488 + _470;
      float _610 = _609 + _483;
      float _611 = _610 + (c_ps_user[12].w);
      float _612 = _611 + _493;
      float _613 = _612 + _497;
      float _614 = _613 + _501;
      float _615 = _614 + _521;
      float _616 = _615 + _526;
      float _617 = _616 + _538;
      float _618 = _617 + _517;
      float _619 = _618 + _601;
      float _620 = _619 + _605;
      float _621 = _620 + _608;
      _623 = _577;
      _624 = _599;
      _625 = _621;
      _626 = 0.0f;
      _627 = 0.0f;
      _628 = 0.0f;
   }
   else
   {
      _623 = _457;
      _624 = _458;
      _625 = _459;
      _626 = _460;
      _627 = _461;
      _628 = _462;
   }
   float _629 = _77.x * _77.x;
   float _630 = _629 * COLOR_1.w;
   float _631 = _77.y * _77.y;
   float _632 = _631 * COLOR_1.w;
   float _633 = _77.z * _77.z;
   float _634 = _633 * COLOR_1.w;
   float _635 = dot(float3(_121, _122, _123), float3(_116, _117, _118));
   float _636 = _635 * 2.0f;
   float _637 = _636 * _116;
   float _638 = _636 * _117;
   float _639 = _636 * _118;
   float _640 = _121 - _637;
   float _641 = _122 - _638;
   float _642 = _123 - _639;
   float _643 = -0.0f - _640;
   float4 _644 = s6c_t.Sample(s6c_s, float3(_643, _641, _642));
   float _651 = _162 * _161;
   float _652 = _131 + 0.75f;
   float _653 = saturate(_652);
   float _654 = _651 * _653;
   float _655 = max(_654, (c_light_vec[5].y));
   float _656 = max(_644.w, 0.003921568859368563f);
   float _657 = 0.25f / _656;
   float _658 = _657 * _644.x;
   float _659 = _657 * _644.y;
   float _660 = _657 * _644.z;
   float _661 = _66.x * TEXCOORD_2.w;
   float _662 = _661 * _655;
   float _663 = _658 * _658;
   float _664 = _663 * _662;
   float _665 = _659 * _659;
   float _666 = _665 * _662;
   float _667 = _660 * _660;
   float _668 = _667 * _662;
   float _669 = _623 + _630;
   float _670 = _669 + _626;
   float _671 = _670 + _664;
   float _672 = _624 + _632;
   float _673 = _672 + _627;
   float _674 = _673 + _666;
   float _675 = _625 + _634;
   float _676 = _675 + _628;
   float _677 = _676 + _668;
   float _684 = c_mat_opacity * _53.w;
   float _685 = _684 * c_opacity_scale;
   float _686 = _685 * c_global_opacity;
   int _689 = c_bool & 16;
   bool _690 = (_689 == 0);
   if (!_690)
   {
      float _694 = c_tone_control.x * _671;
      float _695 = c_tone_control.x * _674;
      float _696 = c_tone_control.x * _677;
      float _698 = log2(_694);
      float _699 = log2(_695);
      float _700 = log2(_696);
      float _701 = _698 * c_tone_control.y;
      float _702 = _699 * c_tone_control.y;
      float _703 = _700 * c_tone_control.y;
      float _704 = exp2(_701);
      float _705 = exp2(_702);
      float _706 = exp2(_703);
      _756 = _704;
      _757 = _705;
      _758 = _706;
   }
   else
   {
      int _708 = c_bool & 32768;
      bool _709 = (_708 == 0);
      if (_709)
      {
         float _713 = c_tone_control.x * _671;
         float _714 = c_tone_control.x * _674;
         float _715 = c_tone_control.x * _677;
         float _716 = _713 * 2.0f;
         float _717 = _714 * 2.0f;
         float _718 = _715 * 2.0f;
         float _719 = _713 * 0.30000001192092896f;
         float _720 = _714 * 0.30000001192092896f;
         float _721 = _715 * 0.30000001192092896f;
         float _722 = _719 + 0.05000000074505806f;
         float _723 = _720 + 0.05000000074505806f;
         float _724 = _721 + 0.05000000074505806f;
         float _725 = _722 * _716;
         float _726 = _723 * _717;
         float _727 = _724 * _718;
         float _728 = _725 + 0.004000000189989805f;
         float _729 = _726 + 0.004000000189989805f;
         float _730 = _727 + 0.004000000189989805f;
         float _731 = _719 + 0.5f;
         float _732 = _720 + 0.5f;
         float _733 = _721 + 0.5f;
         float _734 = _731 * _716;
         float _735 = _732 * _717;
         float _736 = _733 * _718;
         float _737 = _734 + 0.06000000238418579f;
         float _738 = _735 + 0.06000000238418579f;
         float _739 = _736 + 0.06000000238418579f;
         float _740 = _728 / _737;
         float _741 = _729 / _738;
         float _742 = _730 / _739;
         float _743 = _740 + -0.06666666269302368f;
         float _744 = _741 + -0.06666666269302368f;
         float _745 = _742 + -0.06666666269302368f;
         float _746 = _743 * 1.3790643215179443f;
         float _747 = _744 * 1.3790643215179443f;
         float _748 = _745 * 1.3790643215179443f;
         float _749 = YKR_MaterialCurve(_746, _713);
         float _750 = YKR_MaterialCurve(_747, _714);
         float _751 = YKR_MaterialCurve(_748, _715);
         float _752 = sqrt(_749);
         float _753 = sqrt(_750);
         float _754 = sqrt(_751);
         _756 = _752;
         _757 = _753;
         _758 = _754;
      }
      else
      {
         _756 = _671;
         _757 = _674;
         _758 = _677;
      }
   }
   int _759 = c_bool & 4;
   bool _760 = (_759 == 0);
   if (_760)
   {
      float _762 = saturate(COLOR_1.x);
      float _767 = (c_fog_param[0].x) - _756;
      float _768 = (c_fog_param[0].y) - _757;
      float _769 = (c_fog_param[0].z) - _758;
      float _770 = _767 * _762;
      float _771 = _768 * _762;
      float _772 = _769 * _762;
      _774 = _770;
      _775 = _771;
      _776 = _772;
   }
   else
   {
      _774 = COLOR_1.x;
      _775 = COLOR_1.y;
      _776 = COLOR_1.z;
   }
   float _777 = _774 + _756;
   float _778 = _775 + _757;
   float _779 = _776 + _758;
   float _785 = (c_ps_col_scl[0].x) * _777;
   float _786 = (c_ps_col_scl[0].y) * _778;
   float _787 = (c_ps_col_scl[0].z) * _779;
   float _788 = (c_ps_col_scl[0].w) * _686;
   SV_Target.x = _785;
   SV_Target.y = _786;
   SV_Target.z = _787;
   SV_Target.w = _788;
   int _791 = c_bool & 8;
   bool _792 = (_791 != 0);
   float _793 = select(_792, _777, _785);
   float _794 = select(_792, _778, _786);
   float _795 = select(_792, _779, _787);
   float _796 = select(_792, _686, _788);
   SV_Target.x = _793;
   SV_Target.y = _794;
   SV_Target.z = _795;
   SV_Target.w = _796;
   return SV_Target;
}
