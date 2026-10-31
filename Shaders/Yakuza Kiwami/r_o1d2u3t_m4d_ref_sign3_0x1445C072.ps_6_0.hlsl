// Yakuza Kiwami material r_o1d2u3t_m4d[ref][sign3] (0x1445C072, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float4 TEXCOORD_1 : TEXCOORD1,
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
   float4 _56 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _61 = _56.x * _56.x;
   float _62 = _56.y * _56.y;
   float _63 = _56.z * _56.z;
   float4 _64 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _67 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _70 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _73 = _67.x * 2.0f;
   float _74 = _67.y * 2.0f;
   float _75 = _73 + -1.0f;
   float _76 = _74 + -1.0f;
   float _77 = dot(float2(_75, _76), float2(_75, _76));
   float _78 = 1.0f - _77;
   float _79 = saturate(_78);
   float _80 = sqrt(_79);
   float _81 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _82 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _83 = _81 - _82;
   float _84 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _85 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _86 = _84 - _85;
   float _87 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _88 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _89 = _87 - _88;
   float _90 = _75 * TEXCOORD_6.x;
   float _91 = _75 * TEXCOORD_6.y;
   float _92 = _75 * TEXCOORD_6.z;
   float _93 = _76 * TEXCOORD_6.w;
   float _94 = _93 * _83;
   float _95 = _93 * _86;
   float _96 = _93 * _89;
   float _97 = _94 + _90;
   float _98 = _95 + _91;
   float _99 = _96 + _92;
   float _100 = _80 * TEXCOORD_2.x;
   float _101 = _80 * TEXCOORD_2.y;
   float _102 = _80 * TEXCOORD_2.z;
   float _103 = _97 + _100;
   float _104 = _98 + _101;
   float _105 = _99 + _102;
   float _106 = dot(float3(_103, _104, _105), float3(_103, _104, _105));
   float _107 = rsqrt(_106);
   float _108 = _103 * _107;
   float _109 = _104 * _107;
   float _110 = _105 * _107;
   float _111 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _112 = rsqrt(_111);
   float _113 = _112 * TEXCOORD_3.x;
   float _114 = _112 * TEXCOORD_3.y;
   float _115 = _112 * TEXCOORD_3.z;
   float _120 = -0.0f - (c_light_vec[0].x);
   float _121 = -0.0f - (c_light_vec[0].y);
   float _122 = -0.0f - (c_light_vec[0].z);
   float _123 = dot(float3(_108, _109, _110), float3(_120, _121, _122));
   float _127 = c_rt_size.z * SV_Position.x;
   float _128 = c_rt_size.w * SV_Position.y;
   int _131 = c_bool & 8;
   bool _132 = (_131 == 0);
   float _139;
   float _140;
   float _141;
   float _153;
   float _154;
   float _209;
   float _210;
   float _211;
   float _212;
   float _226;
   float _227;
   float _273;
   float _274;
   float _275;
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
   float _781;
   float _782;
   float _783;
   float _799;
   float _800;
   float _801;
   if (!_132)
   {
      float4 _134 = s13_t.Sample(s13_s, float2(_127, _128));
      _139 = _134.x;
      _140 = _134.y;
      _141 = _134.w;
   }
   else
   {
      _139 = 1.0f;
      _140 = 1.0f;
      _141 = 1.0f;
   }
   if (!_132)
   {
      float _146 = (c_sdw_param[0].x) * _139;
      float _147 = (c_sdw_param[0].z) * _140;
      float _150 = _146 + (c_sdw_param[0].y);
      float _151 = _147 + (c_sdw_param[0].w);
      _153 = _150;
      _154 = _151;
   }
   else
   {
      _153 = _139;
      _154 = _140;
   }
   float _161 = (c_sdw_param[1].x) * _70.y;
   float _163 = _161 + (c_sdw_param[1].y);
   float _164 = _163 * _70.x;
   float _165 = 1.0f - (c_light_col[5].x);
   float _166 = 1.0f - (c_light_col[5].y);
   float _167 = 1.0f - (c_light_col[5].z);
   float _168 = _164 * _165;
   float _169 = _164 * _166;
   float _170 = _164 * _167;
   float _171 = _168 + (c_light_col[5].x);
   float _172 = _169 + (c_light_col[5].y);
   float _173 = _170 + (c_light_col[5].z);
   float _177 = c_rt_size.z * SV_Position.x;
   float _178 = c_rt_size.w * SV_Position.y;
   float4 _179 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_177, _178)));
   int _186 = c_bool & 16384;
   bool _187 = (_186 == 0);
   if (!_187)
   {
      float _189 = _179.x * 2.0f;
      float _190 = _179.y * 2.0f;
      float _191 = _179.z * 2.0f;
      float _192 = _179.w * 2.0f;
      _209 = _189;
      _210 = _190;
      _211 = _191;
      _212 = _192;
   }
   else
   {
      int _194 = c_bool & 8192;
      bool _195 = (_194 == 0);
      if (!_195)
      {
         float _197 = _179.x * 2.0f;
         float _198 = _179.y * 2.0f;
         float _199 = _179.z * 2.0f;
         float _200 = _179.w * 2.0f;
         _209 = _197;
         _210 = _198;
         _211 = _199;
         _212 = _200;
      }
      else
      {
         float _204 = _179.x / c_tone_control.z;
         float _205 = _179.y / c_tone_control.z;
         float _206 = _179.z / c_tone_control.z;
         float _207 = _179.w / c_tone_control.z;
         _209 = _204;
         _210 = _205;
         _211 = _206;
         _212 = _207;
      }
   }
   int _213 = c_bool & 8;
   bool _214 = (_213 == 0);
   if (!_214)
   {
      float _219 = (c_sdw_param[0].x) * _153;
      float _220 = (c_sdw_param[0].z) * _154;
      float _223 = _219 + (c_sdw_param[0].y);
      float _224 = _220 + (c_sdw_param[0].w);
      _226 = _223;
      _227 = _224;
   }
   else
   {
      _226 = _153;
      _227 = _154;
   }
   float _228 = _227 * _226;
   float _233 = 1.0f - (c_light_col[5].x);
   float _234 = 1.0f - (c_light_col[5].y);
   float _235 = 1.0f - (c_light_col[5].z);
   float _236 = _233 * _228;
   float _237 = _234 * _228;
   float _238 = _235 * _228;
   float _239 = _236 + (c_light_col[5].x);
   float _240 = _237 + (c_light_col[5].y);
   float _241 = _238 + (c_light_col[5].z);
   float _242 = _141 * COLOR.x;
   float _243 = _242 * _171;
   float _244 = _141 * COLOR.y;
   float _245 = _244 * _172;
   float _246 = _141 * COLOR.z;
   float _247 = _246 * _173;
   float _248 = _239 * _171;
   float _249 = _240 * _172;
   float _250 = _241 * _173;
   if (!_214)
   {
      float _254 = (c_sdw_param[2].x) * _239;
      float _255 = (c_sdw_param[2].x) * _240;
      float _256 = (c_sdw_param[2].x) * _241;
      float _258 = _254 + (c_sdw_param[2].y);
      float _259 = _255 + (c_sdw_param[2].y);
      float _260 = _256 + (c_sdw_param[2].y);
      float _262 = (c_sdw_param[2].z) * _171;
      float _263 = (c_sdw_param[2].z) * _172;
      float _264 = (c_sdw_param[2].z) * _173;
      float _266 = _262 + (c_sdw_param[2].w);
      float _267 = _263 + (c_sdw_param[2].w);
      float _268 = _264 + (c_sdw_param[2].w);
      float _269 = _266 * _258;
      float _270 = _267 * _259;
      float _271 = _268 * _260;
      _273 = _269;
      _274 = _270;
      _275 = _271;
   }
   else
   {
      _273 = _248;
      _274 = _249;
      _275 = _250;
   }
   float _276 = _141 * _70.x;
   float _277 = _273 * _276;
   float _278 = _274 * _276;
   float _279 = _275 * _276;
   float _292 = (c_ps_user[1].z) * c_emissive_scale;
   float _293 = _292 * c_mat_emissive.x;
   float _294 = _292 * c_mat_emissive.y;
   float _295 = _292 * c_mat_emissive.z;
   float _297 = _293 + (c_ps_user[1].w);
   float _298 = _294 + (c_ps_user[1].w);
   float _299 = _295 + (c_ps_user[1].w);
   float _300 = _297 * _61;
   float _301 = _298 * _62;
   float _302 = _299 * _63;
   int _303 = c_bool & 2048;
   bool _304 = (_303 == 0);
   if (_304)
   {
      float _306 = _227 * _209;
      float _307 = _227 * _210;
      float _308 = _227 * _211;
      float _309 = _306 + TEXCOORD_4.x;
      float _310 = _307 + TEXCOORD_4.y;
      float _311 = _308 + TEXCOORD_4.z;
      float _312 = _227 * _212;
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
   float _330 = dot(float3(_108, _109, _110), float3(_327, _328, _329));
   float _331 = saturate(_330);
   float _336 = saturate(_123);
   float _337 = (c_light_col[0].x) * _336;
   float _338 = (c_light_col[0].y) * _336;
   float _339 = (c_light_col[0].z) * _336;
   float _344 = (c_light_col[1].x) * _331;
   float _345 = (c_light_col[1].y) * _331;
   float _346 = (c_light_col[1].z) * _331;
   float _355 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _356 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _357 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _358 = _337 * _239;
   float _359 = _338 * _240;
   float _360 = _339 * _241;
   float _361 = _344 * _227;
   float _362 = _345 * _227;
   float _363 = _346 * _227;
   float _364 = _361 + _358;
   float _365 = _362 + _359;
   float _366 = _363 + _360;
   float _367 = _355 * _364;
   float _368 = _356 * _365;
   float _369 = _357 * _366;
   float _370 = _300 + c_scene_ambient.x;
   float _371 = _370 + _367;
   float _372 = _301 + c_scene_ambient.y;
   float _373 = _372 + _368;
   float _374 = _302 + c_scene_ambient.z;
   float _375 = _374 + _369;
   float _376 = _371 * _243;
   float _377 = _373 * _245;
   float _378 = _375 * _247;
   float _379 = _317 * _277;
   float _380 = _318 * _278;
   float _381 = _319 * _279;
   float _382 = _376 + _379;
   float _383 = _377 + _380;
   float _384 = _378 + _381;
   float _389 = (c_light_vec[0].x) + _113;
   float _390 = (c_light_vec[0].y) + _114;
   float _391 = (c_light_vec[0].z) + _115;
   float _392 = dot(float3(_389, _390, _391), float3(_389, _390, _391));
   float _393 = rsqrt(_392);
   float _394 = _389 * _393;
   float _395 = -0.0f - _394;
   float _396 = _390 * _393;
   float _397 = -0.0f - _396;
   float _398 = _391 * _393;
   float _399 = -0.0f - _398;
   float _400 = dot(float3(_395, _397, _399), float3(_108, _109, _110));
   float _401 = saturate(_400);
   float _402 = _114 + -0.30000001192092896f;
   float _403 = dot(float3(_113, _402, _115), float3(_113, _402, _115));
   float _404 = rsqrt(_403);
   float _405 = _113 * _404;
   float _406 = -0.0f - _405;
   float _407 = _402 * _404;
   float _408 = -0.0f - _407;
   float _409 = _115 * _404;
   float _410 = -0.0f - _409;
   float _411 = dot(float3(_406, _408, _410), float3(_108, _109, _110));
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
   float _431 = _423 * _239;
   float _432 = _424 * _240;
   float _433 = _425 * _241;
   float _434 = _428 * _331;
   float _435 = _429 * _331;
   float _436 = _430 * _331;
   float _437 = _434 + _431;
   float _438 = _435 + _432;
   float _439 = _436 + _433;
   float _446 = c_mat_specular.x * c_specular_scale.x;
   float _447 = _446 * _243;
   float _448 = _447 * _437;
   float _449 = c_mat_specular.y * c_specular_scale.y;
   float _450 = _449 * _245;
   float _451 = _450 * _438;
   float _452 = c_mat_specular.z * c_specular_scale.z;
   float _453 = _452 * _247;
   float _454 = _453 * _439;
   float _455 = _320 * _277;
   float _456 = _455 * _420;
   float _457 = _321 * _278;
   float _458 = _457 * _420;
   float _459 = _322 * _279;
   float _460 = _459 * _420;
   float _461 = _448 + _456;
   float _462 = _451 + _458;
   float _463 = _454 + _460;
   if (!_304)
   {
      float _465 = _227 * _209;
      float _466 = _227 * _210;
      float _467 = _227 * _211;
      float _468 = _382 + _465;
      float _469 = _383 + _466;
      float _470 = _384 + _467;
      float _471 = _227 * _212;
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
   float _487 = _476 * _61;
   float _488 = _477 * _62;
   float _489 = _478 * _63;
   float _490 = _484 * _64.y;
   float _491 = _485 * _64.y;
   float _492 = _486 * _64.y;
   int _493 = c_bool & 32768;
   bool _494 = (_493 == 0);
   if (!_494)
   {
      float _498 = (c_ps_user[11].x) * _61;
      float _499 = (c_ps_user[11].x) * _62;
      float _500 = (c_ps_user[11].x) * _63;
      float _502 = _108 + 1.0f;
      float _503 = _109 + 1.0f;
      float _504 = _110 + 1.0f;
      float _505 = _502 * 0.5f;
      float _506 = _503 * 0.5f;
      float _507 = _504 * 0.5f;
      float _508 = saturate(_505);
      float _509 = saturate(_506);
      float _510 = saturate(_507);
      float _511 = (c_ps_user[11].y) * _508;
      float _512 = _509 * (c_ps_user[11].y);
      float _513 = _510 * (c_ps_user[11].y);
      float _515 = (c_ps_user[11].z) * _64.y;
      float _517 = c_mat_power * 0.009999999776482582f;
      float _518 = _517 * (c_ps_user[11].w);
      float _521 = (c_ps_user[12].x) * _239;
      float _522 = (c_ps_user[12].x) * _240;
      float _523 = (c_ps_user[12].x) * _241;
      float _525 = (c_ps_user[12].y) * _243;
      float _526 = (c_ps_user[12].y) * _245;
      float _527 = (c_ps_user[12].y) * _247;
      float _529 = (c_ps_user[12].z) * _277;
      float _530 = (c_ps_user[12].z) * _278;
      float _531 = (c_ps_user[12].z) * _279;
      float _537 = -0.0f - _113;
      float _538 = -0.0f - _114;
      float _539 = -0.0f - _115;
      float _540 = dot(float3(_537, _538, _539), float3(_108, _109, _110));
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
      float _554 = (c_ps_user[14].x) * _300;
      float _555 = (c_ps_user[14].x) * _301;
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
   float _662 = dot(float3(_113, _114, _115), float3(_108, _109, _110));
   float _663 = _662 * 2.0f;
   float _664 = _663 * _108;
   float _665 = _663 * _109;
   float _666 = _663 * _110;
   float _667 = _113 - _664;
   float _668 = _114 - _665;
   float _669 = _115 - _666;
   float _670 = -0.0f - _667;
   float4 _671 = s6c_t.Sample(s6c_s, float3(_670, _668, _669));
   float _678 = _154 * _153;
   float _679 = _123 + 0.75f;
   float _680 = saturate(_679);
   float _681 = _678 * _680;
   float _682 = max(_681, (c_light_vec[5].y));
   float _683 = max(_671.w, 0.003921568859368563f);
   float _684 = 0.25f / _683;
   float _685 = _684 * _671.x;
   float _686 = _684 * _671.y;
   float _687 = _684 * _671.z;
   float _688 = _64.x * TEXCOORD_2.w;
   float _689 = _688 * _70.x;
   float _690 = _682 * _689;
   float _691 = _690 * _171;
   float _692 = _685 * _685;
   float _693 = _692 * _691;
   float _694 = _690 * _172;
   float _695 = _686 * _686;
   float _696 = _695 * _694;
   float _697 = _690 * _173;
   float _698 = _687 * _687;
   float _699 = _698 * _697;
   float _700 = _659 + _693;
   float _701 = _660 + _696;
   float _702 = _661 + _699;
   float _709 = c_mat_opacity * _56.w;
   float _710 = _709 * c_opacity_scale;
   float _711 = _710 * c_global_opacity;
   int _714 = c_bool & 16;
   bool _715 = (_714 == 0);
   if (!_715)
   {
      float _719 = c_tone_control.x * _700;
      float _720 = c_tone_control.x * _701;
      float _721 = c_tone_control.x * _702;
      float _723 = log2(_719);
      float _724 = log2(_720);
      float _725 = log2(_721);
      float _726 = _723 * c_tone_control.y;
      float _727 = _724 * c_tone_control.y;
      float _728 = _725 * c_tone_control.y;
      float _729 = exp2(_726);
      float _730 = exp2(_727);
      float _731 = exp2(_728);
      _781 = _729;
      _782 = _730;
      _783 = _731;
   }
   else
   {
      int _733 = c_bool & 32768;
      bool _734 = (_733 == 0);
      if (_734)
      {
         float _738 = c_tone_control.x * _700;
         float _739 = c_tone_control.x * _701;
         float _740 = c_tone_control.x * _702;
         float _741 = _738 * 2.0f;
         float _742 = _739 * 2.0f;
         float _743 = _740 * 2.0f;
         float _744 = _738 * 0.30000001192092896f;
         float _745 = _739 * 0.30000001192092896f;
         float _746 = _740 * 0.30000001192092896f;
         float _747 = _744 + 0.05000000074505806f;
         float _748 = _745 + 0.05000000074505806f;
         float _749 = _746 + 0.05000000074505806f;
         float _750 = _747 * _741;
         float _751 = _748 * _742;
         float _752 = _749 * _743;
         float _753 = _750 + 0.004000000189989805f;
         float _754 = _751 + 0.004000000189989805f;
         float _755 = _752 + 0.004000000189989805f;
         float _756 = _744 + 0.5f;
         float _757 = _745 + 0.5f;
         float _758 = _746 + 0.5f;
         float _759 = _756 * _741;
         float _760 = _757 * _742;
         float _761 = _758 * _743;
         float _762 = _759 + 0.06000000238418579f;
         float _763 = _760 + 0.06000000238418579f;
         float _764 = _761 + 0.06000000238418579f;
         float _765 = _753 / _762;
         float _766 = _754 / _763;
         float _767 = _755 / _764;
         float _768 = _765 + -0.06666666269302368f;
         float _769 = _766 + -0.06666666269302368f;
         float _770 = _767 + -0.06666666269302368f;
         float _771 = _768 * 1.3790643215179443f;
         float _772 = _769 * 1.3790643215179443f;
         float _773 = _770 * 1.3790643215179443f;
         float _774 = YKR_MaterialCurve(_771, _738);
         float _775 = YKR_MaterialCurve(_772, _739);
         float _776 = YKR_MaterialCurve(_773, _740);
         float _777 = sqrt(_774);
         float _778 = sqrt(_775);
         float _779 = sqrt(_776);
         _781 = _777;
         _782 = _778;
         _783 = _779;
      }
      else
      {
         _781 = _700;
         _782 = _701;
         _783 = _702;
      }
   }
   int _784 = c_bool & 4;
   bool _785 = (_784 == 0);
   if (_785)
   {
      float _787 = saturate(COLOR_1.x);
      float _792 = (c_fog_param[0].x) - _781;
      float _793 = (c_fog_param[0].y) - _782;
      float _794 = (c_fog_param[0].z) - _783;
      float _795 = _792 * _787;
      float _796 = _793 * _787;
      float _797 = _794 * _787;
      _799 = _795;
      _800 = _796;
      _801 = _797;
   }
   else
   {
      _799 = COLOR_1.x;
      _800 = COLOR_1.y;
      _801 = COLOR_1.z;
   }
   float _802 = _799 + _781;
   float _803 = _800 + _782;
   float _804 = _801 + _783;
   float _810 = (c_ps_col_scl[0].x) * _802;
   float _811 = (c_ps_col_scl[0].y) * _803;
   float _812 = (c_ps_col_scl[0].z) * _804;
   float _813 = (c_ps_col_scl[0].w) * _711;
   SV_Target.x = _810;
   SV_Target.y = _811;
   SV_Target.z = _812;
   SV_Target.w = _813;
   int _816 = c_bool & 8;
   bool _817 = (_816 != 0);
   float _818 = select(_817, _802, _810);
   float _819 = select(_817, _803, _811);
   float _820 = select(_817, _804, _812);
   float _821 = select(_817, _711, _813);
   SV_Target.x = _818;
   SV_Target.y = _819;
   SV_Target.z = _820;
   SV_Target.w = _821;
   return SV_Target;
}
