// Yakuza Kiwami material r_o1ds_b(vr)2ds_m4d (0xE922799A, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _61 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _62 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _63 = s3_t.Sample(s3_s, float2(_61, _62));
   float4 _67 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _72 = _67.x * _67.x;
   float _73 = _67.y * _67.y;
   float _74 = _67.z * _67.z;
   float _78 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _79 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _80 = s4_t.Sample(s4_s, float2(_78, _79));
   float4 _84 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _87 = _67.w * COLOR_1.w;
   float _88 = _72 - _55;
   float _89 = _73 - _56;
   float _90 = _74 - _57;
   float _91 = _88 * _87;
   float _92 = _89 * _87;
   float _93 = _90 * _87;
   float _94 = _91 + _55;
   float _95 = _92 + _56;
   float _96 = _93 + _57;
   float _97 = _80.x - _63.x;
   float _98 = _80.y - _63.y;
   float _99 = _80.z - _63.z;
   float _100 = _97 * _87;
   float _101 = _98 * _87;
   float _102 = _99 * _87;
   float _103 = _100 + _63.x;
   float _104 = _101 + _63.y;
   float _105 = _102 + _63.z;
   float _106 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _107 = rsqrt(_106);
   float _108 = _107 * TEXCOORD_2.x;
   float _109 = _107 * TEXCOORD_2.y;
   float _110 = _107 * TEXCOORD_2.z;
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
   float _310;
   float _311;
   float _312;
   float _313;
   float _314;
   float _315;
   float _469;
   float _470;
   float _471;
   float _472;
   float _473;
   float _474;
   float _650;
   float _651;
   float _652;
   float _653;
   float _654;
   float _655;
   float _731;
   float _732;
   float _733;
   float _749;
   float _750;
   float _751;
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
   float _161 = (c_sdw_param[1].x) * _84.y;
   float _163 = _161 + (c_sdw_param[1].y);
   float _164 = _163 * _84.x;
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
   float _276 = _141 * _84.x;
   float _277 = _273 * _276;
   float _278 = _274 * _276;
   float _279 = _275 * _276;
   float _290 = c_mat_emissive.x * _94;
   float _291 = _290 * c_emissive_scale;
   float _292 = c_mat_emissive.y * _95;
   float _293 = _292 * c_emissive_scale;
   float _294 = c_mat_emissive.z * _96;
   float _295 = _294 * c_emissive_scale;
   int _296 = c_bool & 2048;
   bool _297 = (_296 == 0);
   if (_297)
   {
      float _299 = _227 * _209;
      float _300 = _227 * _210;
      float _301 = _227 * _211;
      float _302 = _299 + TEXCOORD_4.x;
      float _303 = _300 + TEXCOORD_4.y;
      float _304 = _301 + TEXCOORD_4.z;
      float _305 = _227 * _212;
      float _306 = _305 + TEXCOORD_5.x;
      float _307 = _305 + TEXCOORD_5.y;
      float _308 = _305 + TEXCOORD_5.z;
      _310 = _302;
      _311 = _303;
      _312 = _304;
      _313 = _306;
      _314 = _307;
      _315 = _308;
   }
   else
   {
      _310 = TEXCOORD_4.x;
      _311 = TEXCOORD_4.y;
      _312 = TEXCOORD_4.z;
      _313 = TEXCOORD_5.x;
      _314 = TEXCOORD_5.y;
      _315 = TEXCOORD_5.z;
   }
   float _320 = -0.0f - (c_light_vec[1].x);
   float _321 = -0.0f - (c_light_vec[1].y);
   float _322 = -0.0f - (c_light_vec[1].z);
   float _323 = dot(float3(_108, _109, _110), float3(_320, _321, _322));
   float _324 = saturate(_323);
   float _329 = saturate(_123);
   float _330 = (c_light_col[0].x) * _329;
   float _331 = (c_light_col[0].y) * _329;
   float _332 = (c_light_col[0].z) * _329;
   float _337 = (c_light_col[1].x) * _324;
   float _338 = (c_light_col[1].y) * _324;
   float _339 = (c_light_col[1].z) * _324;
   float _348 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _349 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _350 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _351 = _330 * _239;
   float _352 = _331 * _240;
   float _353 = _332 * _241;
   float _354 = _337 * _227;
   float _355 = _338 * _227;
   float _356 = _339 * _227;
   float _357 = _354 + _351;
   float _358 = _355 + _352;
   float _359 = _356 + _353;
   float _360 = _348 * _357;
   float _361 = _349 * _358;
   float _362 = _350 * _359;
   float _363 = _291 + c_scene_ambient.x;
   float _364 = _363 + _360;
   float _365 = _293 + c_scene_ambient.y;
   float _366 = _365 + _361;
   float _367 = _295 + c_scene_ambient.z;
   float _368 = _367 + _362;
   float _369 = _364 * _243;
   float _370 = _366 * _245;
   float _371 = _368 * _247;
   float _372 = _310 * _277;
   float _373 = _311 * _278;
   float _374 = _312 * _279;
   float _375 = _369 + _372;
   float _376 = _370 + _373;
   float _377 = _371 + _374;
   float _382 = (c_light_vec[0].x) + _113;
   float _383 = (c_light_vec[0].y) + _114;
   float _384 = (c_light_vec[0].z) + _115;
   float _385 = dot(float3(_382, _383, _384), float3(_382, _383, _384));
   float _386 = rsqrt(_385);
   float _387 = _382 * _386;
   float _388 = -0.0f - _387;
   float _389 = _383 * _386;
   float _390 = -0.0f - _389;
   float _391 = _384 * _386;
   float _392 = -0.0f - _391;
   float _393 = dot(float3(_388, _390, _392), float3(_108, _109, _110));
   float _394 = saturate(_393);
   float _395 = _114 + -0.30000001192092896f;
   float _396 = dot(float3(_113, _395, _115), float3(_113, _395, _115));
   float _397 = rsqrt(_396);
   float _398 = _113 * _397;
   float _399 = -0.0f - _398;
   float _400 = _395 * _397;
   float _401 = -0.0f - _400;
   float _402 = _115 * _397;
   float _403 = -0.0f - _402;
   float _404 = dot(float3(_399, _401, _403), float3(_108, _109, _110));
   float _405 = saturate(_404);
   float _408 = log2(_394);
   float _409 = log2(_405);
   float _410 = _408 * c_mat_power;
   float _411 = _409 * c_mat_power;
   float _412 = exp2(_410);
   float _413 = exp2(_411);
   float _416 = _412 * (c_light_col[0].x);
   float _417 = _412 * (c_light_col[0].y);
   float _418 = _412 * (c_light_col[0].z);
   float _419 = _413 * 2.0f;
   float _420 = _419 * (c_light_vec[5].x);
   float _421 = _420 * (c_light_col[1].x);
   float _422 = _420 * (c_light_col[1].y);
   float _423 = _420 * (c_light_col[1].z);
   float _424 = _416 * _239;
   float _425 = _417 * _240;
   float _426 = _418 * _241;
   float _427 = _421 * _324;
   float _428 = _422 * _324;
   float _429 = _423 * _324;
   float _430 = _427 + _424;
   float _431 = _428 + _425;
   float _432 = _429 + _426;
   float _439 = c_mat_specular.x * c_specular_scale.x;
   float _440 = _439 * _243;
   float _441 = _440 * _430;
   float _442 = c_mat_specular.y * c_specular_scale.y;
   float _443 = _442 * _245;
   float _444 = _443 * _431;
   float _445 = c_mat_specular.z * c_specular_scale.z;
   float _446 = _445 * _247;
   float _447 = _446 * _432;
   float _448 = _313 * _277;
   float _449 = _448 * _413;
   float _450 = _314 * _278;
   float _451 = _450 * _413;
   float _452 = _315 * _279;
   float _453 = _452 * _413;
   float _454 = _441 + _449;
   float _455 = _444 + _451;
   float _456 = _447 + _453;
   if (!_297)
   {
      float _458 = _227 * _209;
      float _459 = _227 * _210;
      float _460 = _227 * _211;
      float _461 = _375 + _458;
      float _462 = _376 + _459;
      float _463 = _377 + _460;
      float _464 = _227 * _212;
      float _465 = _454 + _464;
      float _466 = _455 + _464;
      float _467 = _456 + _464;
      _469 = _461;
      _470 = _462;
      _471 = _463;
      _472 = _465;
      _473 = _466;
      _474 = _467;
   }
   else
   {
      _469 = _375;
      _470 = _376;
      _471 = _377;
      _472 = _454;
      _473 = _455;
      _474 = _456;
   }
   int _475 = c_bool & 128;
   bool _476 = (_475 != 0);
   float _477 = select(_476, 0.0f, _472);
   float _478 = select(_476, 0.0f, _473);
   float _479 = select(_476, 0.0f, _474);
   float _480 = _469 * _94;
   float _481 = _470 * _95;
   float _482 = _471 * _96;
   float _483 = _477 * _103;
   float _484 = _478 * _104;
   float _485 = _479 * _105;
   int _486 = c_bool & 32768;
   bool _487 = (_486 == 0);
   if (!_487)
   {
      float _491 = (c_ps_user[11].x) * _94;
      float _492 = (c_ps_user[11].x) * _95;
      float _493 = (c_ps_user[11].x) * _96;
      float _495 = _108 + 1.0f;
      float _496 = _109 + 1.0f;
      float _497 = _110 + 1.0f;
      float _498 = _495 * 0.5f;
      float _499 = _496 * 0.5f;
      float _500 = _497 * 0.5f;
      float _501 = saturate(_498);
      float _502 = saturate(_499);
      float _503 = saturate(_500);
      float _504 = (c_ps_user[11].y) * _501;
      float _505 = _502 * (c_ps_user[11].y);
      float _506 = _503 * (c_ps_user[11].y);
      float _508 = (c_ps_user[11].z) * _103;
      float _509 = (c_ps_user[11].z) * _104;
      float _510 = (c_ps_user[11].z) * _105;
      float _512 = c_mat_power * 0.009999999776482582f;
      float _513 = _512 * (c_ps_user[11].w);
      float _516 = (c_ps_user[12].x) * _239;
      float _517 = (c_ps_user[12].x) * _240;
      float _518 = (c_ps_user[12].x) * _241;
      float _520 = (c_ps_user[12].y) * _243;
      float _521 = (c_ps_user[12].y) * _245;
      float _522 = (c_ps_user[12].y) * _247;
      float _524 = (c_ps_user[12].z) * _277;
      float _525 = (c_ps_user[12].z) * _278;
      float _526 = (c_ps_user[12].z) * _279;
      float _532 = -0.0f - _113;
      float _533 = -0.0f - _114;
      float _534 = -0.0f - _115;
      float _535 = dot(float3(_532, _533, _534), float3(_108, _109, _110));
      float _536 = 1.0f - _535;
      float _537 = _536 * _536;
      float _538 = _536 * 0.9722222089767456f;
      float _539 = _537 * _537;
      float _540 = _539 * _538;
      float _541 = _540 + 0.02777777425944805f;
      float _542 = _541 * (c_ps_user[13].z);
      float _544 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _545 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _546 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _549 = (c_ps_user[14].x) * _291;
      float _550 = (c_ps_user[14].x) * _293;
      float _551 = (c_ps_user[14].x) * _295;
      float _553 = (c_ps_user[14].y) * _330;
      float _554 = (c_ps_user[14].y) * _331;
      float _555 = (c_ps_user[14].y) * _332;
      float _557 = (c_ps_user[14].z) * _337;
      float _558 = (c_ps_user[14].z) * _338;
      float _559 = (c_ps_user[14].z) * _339;
      float _561 = (c_ps_user[14].w) * _310;
      float _562 = (c_ps_user[14].w) * _311;
      float _563 = (c_ps_user[14].w) * _312;
      float _566 = (c_ps_user[15].x) * _416;
      float _567 = (c_ps_user[15].x) * _417;
      float _568 = (c_ps_user[15].x) * _418;
      float _570 = _439 * _421;
      float _571 = _570 * (c_ps_user[15].y);
      float _572 = _442 * _422;
      float _573 = _572 * (c_ps_user[15].y);
      float _574 = _445 * _423;
      float _575 = _574 * (c_ps_user[15].y);
      float _577 = (c_ps_user[15].z) * _313;
      float _578 = (c_ps_user[15].z) * _314;
      float _579 = (c_ps_user[15].z) * _315;
      float _580 = _571 + _577;
      float _581 = _580 * _413;
      float _582 = _553 + (c_ps_user[13].x);
      float _583 = _582 + _557;
      float _584 = c_diffuse_scale.x * _583;
      float _585 = _584 * c_mat_diffuse.x;
      float _586 = (c_ps_user[13].y) + _508;
      float _587 = _586 + _566;
      float _588 = c_mat_specular.x * _587;
      float _589 = _588 * c_specular_scale.x;
      float _590 = _504 + _491;
      float _591 = _590 + _513;
      float _592 = _591 + (c_ps_user[12].w);
      float _593 = _592 + _516;
      float _594 = _593 + _520;
      float _595 = _594 + _524;
      float _596 = _595 + _544;
      float _597 = _596 + _549;
      float _598 = _597 + _561;
      float _599 = _598 + _542;
      float _600 = _599 + _581;
      float _601 = _600 + _585;
      float _602 = _601 + _589;
      float _603 = _573 + _578;
      float _604 = _603 * _413;
      float _605 = _554 + (c_ps_user[13].x);
      float _606 = _605 + _558;
      float _607 = c_diffuse_scale.y * _606;
      float _608 = _607 * c_mat_diffuse.y;
      float _609 = (c_ps_user[13].y) + _509;
      float _610 = _609 + _567;
      float _611 = c_mat_specular.y * _610;
      float _612 = _611 * c_specular_scale.y;
      float _613 = _505 + _492;
      float _614 = _613 + _513;
      float _615 = _614 + (c_ps_user[12].w);
      float _616 = _615 + _517;
      float _617 = _616 + _521;
      float _618 = _617 + _525;
      float _619 = _618 + _545;
      float _620 = _619 + _550;
      float _621 = _620 + _562;
      float _622 = _621 + _542;
      float _623 = _622 + _604;
      float _624 = _623 + _608;
      float _625 = _624 + _612;
      float _626 = _575 + _579;
      float _627 = _626 * _413;
      float _628 = _555 + (c_ps_user[13].x);
      float _629 = _628 + _559;
      float _630 = c_diffuse_scale.z * _629;
      float _631 = _630 * c_mat_diffuse.z;
      float _632 = (c_ps_user[13].y) + _510;
      float _633 = _632 + _568;
      float _634 = c_mat_specular.z * _633;
      float _635 = _634 * c_specular_scale.z;
      float _636 = _513 + _493;
      float _637 = _636 + _506;
      float _638 = _637 + (c_ps_user[12].w);
      float _639 = _638 + _518;
      float _640 = _639 + _522;
      float _641 = _640 + _526;
      float _642 = _641 + _546;
      float _643 = _642 + _551;
      float _644 = _643 + _563;
      float _645 = _644 + _542;
      float _646 = _645 + _627;
      float _647 = _646 + _631;
      float _648 = _647 + _635;
      _650 = _602;
      _651 = _625;
      _652 = _648;
      _653 = 0.0f;
      _654 = 0.0f;
      _655 = 0.0f;
   }
   else
   {
      _650 = _480;
      _651 = _481;
      _652 = _482;
      _653 = _483;
      _654 = _484;
      _655 = _485;
   }
   float _656 = _653 + _650;
   float _657 = _654 + _651;
   float _658 = _655 + _652;
   float _663 = c_mat_opacity * _50.w;
   float _664 = _663 * c_global_opacity;
   float _665 = _664 * c_opacity_scale;
   int _666 = c_bool & 16;
   bool _667 = (_666 == 0);
   if (!_667)
   {
      float _671 = c_tone_control.x * _656;
      float _672 = c_tone_control.x * _657;
      float _673 = c_tone_control.x * _658;
      float _675 = log2(_671);
      float _676 = log2(_672);
      float _677 = log2(_673);
      float _678 = _675 * c_tone_control.y;
      float _679 = _676 * c_tone_control.y;
      float _680 = _677 * c_tone_control.y;
      float _681 = exp2(_678);
      float _682 = exp2(_679);
      float _683 = exp2(_680);
      _731 = _681;
      _732 = _682;
      _733 = _683;
   }
   else
   {
      if (_487)
      {
         float _688 = c_tone_control.x * _656;
         float _689 = c_tone_control.x * _657;
         float _690 = c_tone_control.x * _658;
         float _691 = _688 * 2.0f;
         float _692 = _689 * 2.0f;
         float _693 = _690 * 2.0f;
         float _694 = _688 * 0.30000001192092896f;
         float _695 = _689 * 0.30000001192092896f;
         float _696 = _690 * 0.30000001192092896f;
         float _697 = _694 + 0.05000000074505806f;
         float _698 = _695 + 0.05000000074505806f;
         float _699 = _696 + 0.05000000074505806f;
         float _700 = _697 * _691;
         float _701 = _698 * _692;
         float _702 = _699 * _693;
         float _703 = _700 + 0.004000000189989805f;
         float _704 = _701 + 0.004000000189989805f;
         float _705 = _702 + 0.004000000189989805f;
         float _706 = _694 + 0.5f;
         float _707 = _695 + 0.5f;
         float _708 = _696 + 0.5f;
         float _709 = _706 * _691;
         float _710 = _707 * _692;
         float _711 = _708 * _693;
         float _712 = _709 + 0.06000000238418579f;
         float _713 = _710 + 0.06000000238418579f;
         float _714 = _711 + 0.06000000238418579f;
         float _715 = _703 / _712;
         float _716 = _704 / _713;
         float _717 = _705 / _714;
         float _718 = _715 + -0.06666666269302368f;
         float _719 = _716 + -0.06666666269302368f;
         float _720 = _717 + -0.06666666269302368f;
         float _721 = _718 * 1.3790643215179443f;
         float _722 = _719 * 1.3790643215179443f;
         float _723 = _720 * 1.3790643215179443f;
         float _724 = YKR_MaterialCurve(_721, _688);
         float _725 = YKR_MaterialCurve(_722, _689);
         float _726 = YKR_MaterialCurve(_723, _690);
         float _727 = sqrt(_724);
         float _728 = sqrt(_725);
         float _729 = sqrt(_726);
         _731 = _727;
         _732 = _728;
         _733 = _729;
      }
      else
      {
         _731 = _656;
         _732 = _657;
         _733 = _658;
      }
   }
   int _734 = c_bool & 4;
   bool _735 = (_734 == 0);
   if (_735)
   {
      float _737 = saturate(COLOR_1.x);
      float _742 = (c_fog_param[0].x) - _731;
      float _743 = (c_fog_param[0].y) - _732;
      float _744 = (c_fog_param[0].z) - _733;
      float _745 = _742 * _737;
      float _746 = _743 * _737;
      float _747 = _744 * _737;
      _749 = _745;
      _750 = _746;
      _751 = _747;
   }
   else
   {
      _749 = COLOR_1.x;
      _750 = COLOR_1.y;
      _751 = COLOR_1.z;
   }
   float _752 = _749 + _731;
   float _753 = _750 + _732;
   float _754 = _751 + _733;
   float _760 = (c_ps_col_scl[0].x) * _752;
   float _761 = (c_ps_col_scl[0].y) * _753;
   float _762 = (c_ps_col_scl[0].z) * _754;
   float _763 = (c_ps_col_scl[0].w) * _665;
   SV_Target.x = _760;
   SV_Target.y = _761;
   SV_Target.z = _762;
   SV_Target.w = _763;
   int _766 = c_bool & 8;
   bool _767 = (_766 != 0);
   float _768 = select(_767, _752, _760);
   float _769 = select(_767, _753, _761);
   float _770 = select(_767, _754, _762);
   float _771 = select(_767, _665, _763);
   SV_Target.x = _768;
   SV_Target.y = _769;
   SV_Target.z = _770;
   SV_Target.w = _771;
   return SV_Target;
}
