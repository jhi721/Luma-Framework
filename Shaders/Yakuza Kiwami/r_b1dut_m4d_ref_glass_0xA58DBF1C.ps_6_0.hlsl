// Yakuza Kiwami material r_b1dut_m4d[ref][glass] (0xA58DBF1C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

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
   float4 _77 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _80 = _74.x * 2.0f;
   float _81 = _74.y * 2.0f;
   float _82 = _80 + -1.0f;
   float _83 = _81 + -1.0f;
   float _84 = dot(float2(_82, _83), float2(_82, _83));
   float _85 = 1.0f - _84;
   float _86 = saturate(_85);
   float _87 = sqrt(_86);
   float _88 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _89 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _90 = _88 - _89;
   float _91 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _92 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _93 = _91 - _92;
   float _94 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _95 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _96 = _94 - _95;
   float _97 = _82 * TEXCOORD_6.x;
   float _98 = _82 * TEXCOORD_6.y;
   float _99 = _82 * TEXCOORD_6.z;
   float _100 = _83 * TEXCOORD_6.w;
   float _101 = _100 * _90;
   float _102 = _100 * _93;
   float _103 = _100 * _96;
   float _104 = _101 + _97;
   float _105 = _102 + _98;
   float _106 = _103 + _99;
   float _107 = _87 * TEXCOORD_2.x;
   float _108 = _87 * TEXCOORD_2.y;
   float _109 = _87 * TEXCOORD_2.z;
   float _110 = _104 + _107;
   float _111 = _105 + _108;
   float _112 = _106 + _109;
   float _113 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _114 = rsqrt(_113);
   float _115 = _110 * _114;
   float _116 = _111 * _114;
   float _117 = _112 * _114;
   float _118 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _119 = rsqrt(_118);
   float _120 = _119 * TEXCOORD_3.x;
   float _121 = _119 * TEXCOORD_3.y;
   float _122 = _119 * TEXCOORD_3.z;
   float _127 = -0.0f - (c_light_vec[0].x);
   float _128 = -0.0f - (c_light_vec[0].y);
   float _129 = -0.0f - (c_light_vec[0].z);
   float _130 = dot(float3(_115, _116, _117), float3(_127, _128, _129));
   float _134 = c_rt_size.z * SV_Position.x;
   float _135 = c_rt_size.w * SV_Position.y;
   int _138 = c_bool & 8;
   bool _139 = (_138 == 0);
   float _146;
   float _147;
   float _148;
   float _160;
   float _161;
   float _216;
   float _217;
   float _218;
   float _219;
   float _233;
   float _234;
   float _280;
   float _281;
   float _282;
   float _317;
   float _318;
   float _319;
   float _320;
   float _321;
   float _322;
   float _476;
   float _477;
   float _478;
   float _479;
   float _480;
   float _481;
   float _653;
   float _654;
   float _655;
   float _656;
   float _657;
   float _658;
   float _790;
   float _791;
   float _792;
   float _808;
   float _809;
   float _810;
   if (!_139)
   {
      float4 _141 = s13_t.Sample(s13_s, float2(_134, _135));
      _146 = _141.x;
      _147 = _141.y;
      _148 = _141.w;
   }
   else
   {
      _146 = 1.0f;
      _147 = 1.0f;
      _148 = 1.0f;
   }
   if (!_139)
   {
      float _153 = (c_sdw_param[0].x) * _146;
      float _154 = (c_sdw_param[0].z) * _147;
      float _157 = _153 + (c_sdw_param[0].y);
      float _158 = _154 + (c_sdw_param[0].w);
      _160 = _157;
      _161 = _158;
   }
   else
   {
      _160 = _146;
      _161 = _147;
   }
   float _168 = (c_sdw_param[1].x) * _77.y;
   float _170 = _168 + (c_sdw_param[1].y);
   float _171 = _170 * _77.x;
   float _172 = 1.0f - (c_light_col[5].x);
   float _173 = 1.0f - (c_light_col[5].y);
   float _174 = 1.0f - (c_light_col[5].z);
   float _175 = _171 * _172;
   float _176 = _171 * _173;
   float _177 = _171 * _174;
   float _178 = _175 + (c_light_col[5].x);
   float _179 = _176 + (c_light_col[5].y);
   float _180 = _177 + (c_light_col[5].z);
   float _184 = c_rt_size.z * SV_Position.x;
   float _185 = c_rt_size.w * SV_Position.y;
   float4 _186 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_184, _185)));
   int _193 = c_bool & 16384;
   bool _194 = (_193 == 0);
   if (!_194)
   {
      float _196 = _186.x * 2.0f;
      float _197 = _186.y * 2.0f;
      float _198 = _186.z * 2.0f;
      float _199 = _186.w * 2.0f;
      _216 = _196;
      _217 = _197;
      _218 = _198;
      _219 = _199;
   }
   else
   {
      int _201 = c_bool & 8192;
      bool _202 = (_201 == 0);
      if (!_202)
      {
         float _204 = _186.x * 2.0f;
         float _205 = _186.y * 2.0f;
         float _206 = _186.z * 2.0f;
         float _207 = _186.w * 2.0f;
         _216 = _204;
         _217 = _205;
         _218 = _206;
         _219 = _207;
      }
      else
      {
         float _211 = _186.x / c_tone_control.z;
         float _212 = _186.y / c_tone_control.z;
         float _213 = _186.z / c_tone_control.z;
         float _214 = _186.w / c_tone_control.z;
         _216 = _211;
         _217 = _212;
         _218 = _213;
         _219 = _214;
      }
   }
   int _220 = c_bool & 8;
   bool _221 = (_220 == 0);
   if (!_221)
   {
      float _226 = (c_sdw_param[0].x) * _160;
      float _227 = (c_sdw_param[0].z) * _161;
      float _230 = _226 + (c_sdw_param[0].y);
      float _231 = _227 + (c_sdw_param[0].w);
      _233 = _230;
      _234 = _231;
   }
   else
   {
      _233 = _160;
      _234 = _161;
   }
   float _235 = _234 * _233;
   float _240 = 1.0f - (c_light_col[5].x);
   float _241 = 1.0f - (c_light_col[5].y);
   float _242 = 1.0f - (c_light_col[5].z);
   float _243 = _240 * _235;
   float _244 = _241 * _235;
   float _245 = _242 * _235;
   float _246 = _243 + (c_light_col[5].x);
   float _247 = _244 + (c_light_col[5].y);
   float _248 = _245 + (c_light_col[5].z);
   float _249 = _148 * COLOR.x;
   float _250 = _249 * _178;
   float _251 = _148 * COLOR.y;
   float _252 = _251 * _179;
   float _253 = _148 * COLOR.z;
   float _254 = _253 * _180;
   float _255 = _246 * _178;
   float _256 = _247 * _179;
   float _257 = _248 * _180;
   if (!_221)
   {
      float _261 = (c_sdw_param[2].x) * _246;
      float _262 = (c_sdw_param[2].x) * _247;
      float _263 = (c_sdw_param[2].x) * _248;
      float _265 = _261 + (c_sdw_param[2].y);
      float _266 = _262 + (c_sdw_param[2].y);
      float _267 = _263 + (c_sdw_param[2].y);
      float _269 = (c_sdw_param[2].z) * _178;
      float _270 = (c_sdw_param[2].z) * _179;
      float _271 = (c_sdw_param[2].z) * _180;
      float _273 = _269 + (c_sdw_param[2].w);
      float _274 = _270 + (c_sdw_param[2].w);
      float _275 = _271 + (c_sdw_param[2].w);
      float _276 = _273 * _265;
      float _277 = _274 * _266;
      float _278 = _275 * _267;
      _280 = _276;
      _281 = _277;
      _282 = _278;
   }
   else
   {
      _280 = _255;
      _281 = _256;
      _282 = _257;
   }
   float _283 = _148 * _77.x;
   float _284 = _280 * _283;
   float _285 = _281 * _283;
   float _286 = _282 * _283;
   float _297 = c_mat_emissive.x * _58;
   float _298 = _297 * c_emissive_scale;
   float _299 = c_mat_emissive.y * _59;
   float _300 = _299 * c_emissive_scale;
   float _301 = c_mat_emissive.z * _60;
   float _302 = _301 * c_emissive_scale;
   int _303 = c_bool & 2048;
   bool _304 = (_303 == 0);
   if (_304)
   {
      float _306 = _234 * _216;
      float _307 = _234 * _217;
      float _308 = _234 * _218;
      float _309 = _306 + TEXCOORD_4.x;
      float _310 = _307 + TEXCOORD_4.y;
      float _311 = _308 + TEXCOORD_4.z;
      float _312 = _234 * _219;
      float _313 = _312 + TEXCOORD_5.x;
      float _314 = _312 + TEXCOORD_5.y;
      float _315 = _312 + TEXCOORD_5.z;
      _317 = _309;
      _318 = _310;
      _319 = _311;
      _320 = _313;
      _321 = _314;
      _322 = _315;
   }
   else
   {
      _317 = TEXCOORD_4.x;
      _318 = TEXCOORD_4.y;
      _319 = TEXCOORD_4.z;
      _320 = TEXCOORD_5.x;
      _321 = TEXCOORD_5.y;
      _322 = TEXCOORD_5.z;
   }
   float _327 = -0.0f - (c_light_vec[1].x);
   float _328 = -0.0f - (c_light_vec[1].y);
   float _329 = -0.0f - (c_light_vec[1].z);
   float _330 = dot(float3(_115, _116, _117), float3(_327, _328, _329));
   float _331 = saturate(_330);
   float _336 = saturate(_130);
   float _337 = (c_light_col[0].x) * _336;
   float _338 = (c_light_col[0].y) * _336;
   float _339 = (c_light_col[0].z) * _336;
   float _344 = (c_light_col[1].x) * _331;
   float _345 = (c_light_col[1].y) * _331;
   float _346 = (c_light_col[1].z) * _331;
   float _355 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _356 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _357 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _358 = _337 * _246;
   float _359 = _338 * _247;
   float _360 = _339 * _248;
   float _361 = _344 * _234;
   float _362 = _345 * _234;
   float _363 = _346 * _234;
   float _364 = _361 + _358;
   float _365 = _362 + _359;
   float _366 = _363 + _360;
   float _367 = _355 * _364;
   float _368 = _356 * _365;
   float _369 = _357 * _366;
   float _370 = _298 + c_scene_ambient.x;
   float _371 = _370 + _367;
   float _372 = _300 + c_scene_ambient.y;
   float _373 = _372 + _368;
   float _374 = _302 + c_scene_ambient.z;
   float _375 = _374 + _369;
   float _376 = _371 * _250;
   float _377 = _373 * _252;
   float _378 = _375 * _254;
   float _379 = _317 * _284;
   float _380 = _318 * _285;
   float _381 = _319 * _286;
   float _382 = _376 + _379;
   float _383 = _377 + _380;
   float _384 = _378 + _381;
   float _389 = (c_light_vec[0].x) + _120;
   float _390 = (c_light_vec[0].y) + _121;
   float _391 = (c_light_vec[0].z) + _122;
   float _392 = dot(float3(_389, _390, _391), float3(_389, _390, _391));
   float _393 = rsqrt(_392);
   float _394 = _389 * _393;
   float _395 = -0.0f - _394;
   float _396 = _390 * _393;
   float _397 = -0.0f - _396;
   float _398 = _391 * _393;
   float _399 = -0.0f - _398;
   float _400 = dot(float3(_395, _397, _399), float3(_115, _116, _117));
   float _401 = saturate(_400);
   float _402 = _121 + -0.30000001192092896f;
   float _403 = dot(float3(_120, _402, _122), float3(_120, _402, _122));
   float _404 = rsqrt(_403);
   float _405 = _120 * _404;
   float _406 = -0.0f - _405;
   float _407 = _402 * _404;
   float _408 = -0.0f - _407;
   float _409 = _122 * _404;
   float _410 = -0.0f - _409;
   float _411 = dot(float3(_406, _408, _410), float3(_115, _116, _117));
   float _412 = saturate(_411);
   float _415 = log2(_401);
   float _416 = log2(_412);
   float _417 = _415 * c_mat_power;
   float _418 = _416 * c_mat_power;
   float _419 = exp2(_417);
   float _420 = exp2(_418);
   float _423 = _419 * (c_light_col[0].x);
   float _424 = _419 * (c_light_col[0].y);
   float _425 = _419 * (c_light_col[0].z);
   float _426 = _420 * 2.0f;
   float _427 = _426 * (c_light_vec[5].x);
   float _428 = _427 * (c_light_col[1].x);
   float _429 = _427 * (c_light_col[1].y);
   float _430 = _427 * (c_light_col[1].z);
   float _431 = _423 * _246;
   float _432 = _424 * _247;
   float _433 = _425 * _248;
   float _434 = _428 * _331;
   float _435 = _429 * _331;
   float _436 = _430 * _331;
   float _437 = _434 + _431;
   float _438 = _435 + _432;
   float _439 = _436 + _433;
   float _446 = c_mat_specular.x * c_specular_scale.x;
   float _447 = _446 * _250;
   float _448 = _447 * _437;
   float _449 = c_mat_specular.y * c_specular_scale.y;
   float _450 = _449 * _252;
   float _451 = _450 * _438;
   float _452 = c_mat_specular.z * c_specular_scale.z;
   float _453 = _452 * _254;
   float _454 = _453 * _439;
   float _455 = _320 * _284;
   float _456 = _455 * _420;
   float _457 = _321 * _285;
   float _458 = _457 * _420;
   float _459 = _322 * _286;
   float _460 = _459 * _420;
   float _461 = _448 + _456;
   float _462 = _451 + _458;
   float _463 = _454 + _460;
   if (!_304)
   {
      float _465 = _234 * _216;
      float _466 = _234 * _217;
      float _467 = _234 * _218;
      float _468 = _382 + _465;
      float _469 = _383 + _466;
      float _470 = _384 + _467;
      float _471 = _234 * _219;
      float _472 = _461 + _471;
      float _473 = _462 + _471;
      float _474 = _463 + _471;
      _476 = _468;
      _477 = _469;
      _478 = _470;
      _479 = _472;
      _480 = _473;
      _481 = _474;
   }
   else
   {
      _476 = _382;
      _477 = _383;
      _478 = _384;
      _479 = _461;
      _480 = _462;
      _481 = _463;
   }
   int _482 = c_bool & 128;
   bool _483 = (_482 != 0);
   float _484 = select(_483, 0.0f, _479);
   float _485 = select(_483, 0.0f, _480);
   float _486 = select(_483, 0.0f, _481);
   float _487 = _476 * _58;
   float _488 = _477 * _59;
   float _489 = _478 * _60;
   float _490 = _484 * _66.y;
   float _491 = _485 * _66.y;
   float _492 = _486 * _66.y;
   int _493 = c_bool & 32768;
   bool _494 = (_493 == 0);
   if (!_494)
   {
      float _498 = (c_ps_user[11].x) * _58;
      float _499 = (c_ps_user[11].x) * _59;
      float _500 = (c_ps_user[11].x) * _60;
      float _502 = _115 + 1.0f;
      float _503 = _116 + 1.0f;
      float _504 = _117 + 1.0f;
      float _505 = _502 * 0.5f;
      float _506 = _503 * 0.5f;
      float _507 = _504 * 0.5f;
      float _508 = saturate(_505);
      float _509 = saturate(_506);
      float _510 = saturate(_507);
      float _511 = (c_ps_user[11].y) * _508;
      float _512 = _509 * (c_ps_user[11].y);
      float _513 = _510 * (c_ps_user[11].y);
      float _515 = (c_ps_user[11].z) * _66.y;
      float _517 = c_mat_power * 0.009999999776482582f;
      float _518 = _517 * (c_ps_user[11].w);
      float _521 = (c_ps_user[12].x) * _246;
      float _522 = (c_ps_user[12].x) * _247;
      float _523 = (c_ps_user[12].x) * _248;
      float _525 = (c_ps_user[12].y) * _250;
      float _526 = (c_ps_user[12].y) * _252;
      float _527 = (c_ps_user[12].y) * _254;
      float _529 = (c_ps_user[12].z) * _284;
      float _530 = (c_ps_user[12].z) * _285;
      float _531 = (c_ps_user[12].z) * _286;
      float _537 = -0.0f - _120;
      float _538 = -0.0f - _121;
      float _539 = -0.0f - _122;
      float _540 = dot(float3(_537, _538, _539), float3(_115, _116, _117));
      float _541 = 1.0f - _540;
      float _542 = _541 * _541;
      float _543 = _541 * 0.9722222089767456f;
      float _544 = _542 * _542;
      float _545 = _544 * _543;
      float _546 = _545 + 0.02777777425944805f;
      float _547 = _546 * (c_ps_user[13].z);
      float _549 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _550 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _551 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _554 = (c_ps_user[14].x) * _298;
      float _555 = (c_ps_user[14].x) * _300;
      float _556 = (c_ps_user[14].x) * _302;
      float _558 = (c_ps_user[14].y) * _337;
      float _559 = (c_ps_user[14].y) * _338;
      float _560 = (c_ps_user[14].y) * _339;
      float _562 = (c_ps_user[14].z) * _344;
      float _563 = (c_ps_user[14].z) * _345;
      float _564 = (c_ps_user[14].z) * _346;
      float _566 = (c_ps_user[14].w) * _317;
      float _567 = (c_ps_user[14].w) * _318;
      float _568 = (c_ps_user[14].w) * _319;
      float _571 = (c_ps_user[15].x) * _423;
      float _572 = (c_ps_user[15].x) * _424;
      float _573 = (c_ps_user[15].x) * _425;
      float _575 = _446 * _428;
      float _576 = _575 * (c_ps_user[15].y);
      float _577 = _449 * _429;
      float _578 = _577 * (c_ps_user[15].y);
      float _579 = _452 * _430;
      float _580 = _579 * (c_ps_user[15].y);
      float _582 = (c_ps_user[15].z) * _320;
      float _583 = (c_ps_user[15].z) * _321;
      float _584 = (c_ps_user[15].z) * _322;
      float _585 = _576 + _582;
      float _586 = _585 * _420;
      float _587 = _558 + (c_ps_user[13].x);
      float _588 = _587 + _562;
      float _589 = c_diffuse_scale.x * _588;
      float _590 = _589 * c_mat_diffuse.x;
      float _591 = (c_ps_user[13].y) + _515;
      float _592 = _591 + _571;
      float _593 = c_mat_specular.x * _592;
      float _594 = _593 * c_specular_scale.x;
      float _595 = _511 + _498;
      float _596 = _595 + _518;
      float _597 = _596 + (c_ps_user[12].w);
      float _598 = _597 + _521;
      float _599 = _598 + _525;
      float _600 = _599 + _529;
      float _601 = _600 + _549;
      float _602 = _601 + _554;
      float _603 = _602 + _566;
      float _604 = _603 + _547;
      float _605 = _604 + _586;
      float _606 = _605 + _590;
      float _607 = _606 + _594;
      float _608 = _578 + _583;
      float _609 = _608 * _420;
      float _610 = _559 + (c_ps_user[13].x);
      float _611 = _610 + _563;
      float _612 = c_diffuse_scale.y * _611;
      float _613 = _612 * c_mat_diffuse.y;
      float _614 = _591 + _572;
      float _615 = c_mat_specular.y * _614;
      float _616 = _615 * c_specular_scale.y;
      float _617 = _512 + _499;
      float _618 = _617 + _518;
      float _619 = _618 + (c_ps_user[12].w);
      float _620 = _619 + _522;
      float _621 = _620 + _526;
      float _622 = _621 + _530;
      float _623 = _622 + _550;
      float _624 = _623 + _555;
      float _625 = _624 + _567;
      float _626 = _625 + _547;
      float _627 = _626 + _609;
      float _628 = _627 + _613;
      float _629 = _628 + _616;
      float _630 = _580 + _584;
      float _631 = _630 * _420;
      float _632 = _560 + (c_ps_user[13].x);
      float _633 = _632 + _564;
      float _634 = c_diffuse_scale.z * _633;
      float _635 = _634 * c_mat_diffuse.z;
      float _636 = _591 + _573;
      float _637 = c_mat_specular.z * _636;
      float _638 = _637 * c_specular_scale.z;
      float _639 = _518 + _500;
      float _640 = _639 + _513;
      float _641 = _640 + (c_ps_user[12].w);
      float _642 = _641 + _523;
      float _643 = _642 + _527;
      float _644 = _643 + _531;
      float _645 = _644 + _551;
      float _646 = _645 + _556;
      float _647 = _646 + _568;
      float _648 = _647 + _547;
      float _649 = _648 + _631;
      float _650 = _649 + _635;
      float _651 = _650 + _638;
      _653 = _607;
      _654 = _629;
      _655 = _651;
      _656 = 0.0f;
      _657 = 0.0f;
      _658 = 0.0f;
   }
   else
   {
      _653 = _487;
      _654 = _488;
      _655 = _489;
      _656 = _490;
      _657 = _491;
      _658 = _492;
   }
   float _659 = _656 + _653;
   float _660 = _657 + _654;
   float _661 = _658 + _655;
   float _662 = dot(float3(_120, _121, _122), float3(_115, _116, _117));
   float _663 = _662 * 2.0f;
   float _664 = _663 * _115;
   float _665 = _663 * _116;
   float _666 = _663 * _117;
   float _667 = _120 - _664;
   float _668 = _121 - _665;
   float _669 = _122 - _666;
   float _670 = -0.0f - _667;
   float4 _671 = s6c_t.Sample(s6c_s, float3(_670, _668, _669));
   float _678 = _161 * _160;
   float _679 = _130 + 0.75f;
   float _680 = saturate(_679);
   float _681 = _678 * _680;
   float _682 = max(_681, (c_light_vec[5].y));
   float _683 = max(_671.w, 0.003921568859368563f);
   float _684 = 0.25f / _683;
   float _685 = _684 * _671.x;
   float _686 = _684 * _671.y;
   float _687 = _684 * _671.z;
   float _688 = _66.x * TEXCOORD_2.w;
   float _689 = _688 * _77.x;
   float _690 = _682 * _689;
   float _691 = _690 * _178;
   float _692 = _685 * _685;
   float _693 = _692 * _691;
   float _694 = _690 * _179;
   float _695 = _686 * _686;
   float _696 = _695 * _694;
   float _697 = _690 * _180;
   float _698 = _687 * _687;
   float _699 = _698 * _697;
   float _700 = _659 + _693;
   float _701 = _660 + _696;
   float _702 = _661 + _699;
   float _709 = _53.w * COLOR.w;
   float _710 = _709 * c_mat_opacity;
   float _711 = _710 * c_opacity_scale;
   float _712 = _711 * c_global_opacity;
   float _713 = _693 * 4.0f;
   float _714 = _696 * 4.0f;
   float _715 = _699 * 4.0f;
   float _716 = _713 + _656;
   float _717 = _714 + _657;
   float _718 = _715 + _658;
   float _719 = dot(float3(_716, _717, _718), float3(0.33000001311302185f, 0.33000001311302185f, 0.33000001311302185f));
   float _720 = _712 + _719;
   int _723 = c_bool & 16;
   bool _724 = (_723 == 0);
   if (!_724)
   {
      float _728 = c_tone_control.x * _700;
      float _729 = c_tone_control.x * _701;
      float _730 = c_tone_control.x * _702;
      float _732 = log2(_728);
      float _733 = log2(_729);
      float _734 = log2(_730);
      float _735 = _732 * c_tone_control.y;
      float _736 = _733 * c_tone_control.y;
      float _737 = _734 * c_tone_control.y;
      float _738 = exp2(_735);
      float _739 = exp2(_736);
      float _740 = exp2(_737);
      _790 = _738;
      _791 = _739;
      _792 = _740;
   }
   else
   {
      int _742 = c_bool & 32768;
      bool _743 = (_742 == 0);
      if (_743)
      {
         float _747 = c_tone_control.x * _700;
         float _748 = c_tone_control.x * _701;
         float _749 = c_tone_control.x * _702;
         float _750 = _747 * 2.0f;
         float _751 = _748 * 2.0f;
         float _752 = _749 * 2.0f;
         float _753 = _747 * 0.30000001192092896f;
         float _754 = _748 * 0.30000001192092896f;
         float _755 = _749 * 0.30000001192092896f;
         float _756 = _753 + 0.05000000074505806f;
         float _757 = _754 + 0.05000000074505806f;
         float _758 = _755 + 0.05000000074505806f;
         float _759 = _756 * _750;
         float _760 = _757 * _751;
         float _761 = _758 * _752;
         float _762 = _759 + 0.004000000189989805f;
         float _763 = _760 + 0.004000000189989805f;
         float _764 = _761 + 0.004000000189989805f;
         float _765 = _753 + 0.5f;
         float _766 = _754 + 0.5f;
         float _767 = _755 + 0.5f;
         float _768 = _765 * _750;
         float _769 = _766 * _751;
         float _770 = _767 * _752;
         float _771 = _768 + 0.06000000238418579f;
         float _772 = _769 + 0.06000000238418579f;
         float _773 = _770 + 0.06000000238418579f;
         float _774 = _762 / _771;
         float _775 = _763 / _772;
         float _776 = _764 / _773;
         float _777 = _774 + -0.06666666269302368f;
         float _778 = _775 + -0.06666666269302368f;
         float _779 = _776 + -0.06666666269302368f;
         float _780 = _777 * 1.3790643215179443f;
         float _781 = _778 * 1.3790643215179443f;
         float _782 = _779 * 1.3790643215179443f;
         float _783 = YKR_MaterialCurve(_780, _747);
         float _784 = YKR_MaterialCurve(_781, _748);
         float _785 = YKR_MaterialCurve(_782, _749);
         float _786 = sqrt(_783);
         float _787 = sqrt(_784);
         float _788 = sqrt(_785);
         _790 = _786;
         _791 = _787;
         _792 = _788;
      }
      else
      {
         _790 = _700;
         _791 = _701;
         _792 = _702;
      }
   }
   int _793 = c_bool & 4;
   bool _794 = (_793 == 0);
   if (_794)
   {
      float _796 = saturate(COLOR_1.x);
      float _801 = (c_fog_param[0].x) - _790;
      float _802 = (c_fog_param[0].y) - _791;
      float _803 = (c_fog_param[0].z) - _792;
      float _804 = _801 * _796;
      float _805 = _802 * _796;
      float _806 = _803 * _796;
      _808 = _804;
      _809 = _805;
      _810 = _806;
   }
   else
   {
      _808 = COLOR_1.x;
      _809 = COLOR_1.y;
      _810 = COLOR_1.z;
   }
   float _811 = _808 + _790;
   float _812 = _809 + _791;
   float _813 = _810 + _792;
   float _816 = _720 - c_mat_punchthrough_ref;
   bool _817 = (_816 < 0.0f);
   if (_817)
      discard;
   float _823 = (c_ps_col_scl[0].x) * _811;
   float _824 = (c_ps_col_scl[0].y) * _812;
   float _825 = (c_ps_col_scl[0].z) * _813;
   float _826 = (c_ps_col_scl[0].w) * _720;
   SV_Target.x = _823;
   SV_Target.y = _824;
   SV_Target.z = _825;
   SV_Target.w = _826;
   int _829 = c_bool & 8;
   bool _830 = (_829 != 0);
   float _831 = select(_830, _811, _823);
   float _832 = select(_830, _812, _824);
   float _833 = select(_830, _813, _825);
   float _834 = select(_830, _720, _826);
   SV_Target.x = _831;
   SV_Target.y = _832;
   SV_Target.z = _833;
   SV_Target.w = _834;
   return SV_Target;
}
