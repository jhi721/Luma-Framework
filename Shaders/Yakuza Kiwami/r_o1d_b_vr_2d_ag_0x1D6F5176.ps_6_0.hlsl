// Yakuza Kiwami material r_o1d_b(vr)2d[ag] (0x1D6F5176, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _39 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _44 = _39.x * _39.x;
   float _45 = _39.y * _39.y;
   float _46 = _39.z * _39.z;
   float4 _47 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _52 = _47.x * _47.x;
   float _53 = _47.y * _47.y;
   float _54 = _47.z * _47.z;
   float _55 = _47.w * COLOR_1.w;
   float _56 = _52 - _44;
   float _57 = _53 - _45;
   float _58 = _54 - _46;
   float _59 = _56 * _55;
   float _60 = _57 * _55;
   float _61 = _58 * _55;
   float _62 = _59 + _44;
   float _63 = _60 + _45;
   float _64 = _61 + _46;
   float _65 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _66 = rsqrt(_65);
   float _67 = _66 * TEXCOORD_2.x;
   float _68 = _66 * TEXCOORD_2.y;
   float _69 = _66 * TEXCOORD_2.z;
   float _70 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _71 = rsqrt(_70);
   float _76 = -0.0f - (c_light_vec[0].x);
   float _77 = -0.0f - (c_light_vec[0].y);
   float _78 = -0.0f - (c_light_vec[0].z);
   float _79 = dot(float3(_67, _68, _69), float3(_76, _77, _78));
   float _83 = c_rt_size.z * SV_Position.x;
   float _84 = c_rt_size.w * SV_Position.y;
   int _87 = c_bool & 8;
   bool _88 = (_87 == 0);
   float _95;
   float _96;
   float _97;
   float _109;
   float _110;
   float _142;
   float _143;
   float _144;
   float _158;
   float _159;
   float _194;
   float _195;
   float _196;
   float _226;
   float _227;
   float _228;
   float _299;
   float _300;
   float _301;
   float _421;
   float _422;
   float _423;
   float _496;
   float _497;
   float _498;
   float _514;
   float _515;
   float _516;
   if (!_88)
   {
      float4 _90 = s13_t.Sample(s13_s, float2(_83, _84));
      _95 = _90.x;
      _96 = _90.y;
      _97 = _90.w;
   }
   else
   {
      _95 = 1.0f;
      _96 = 1.0f;
      _97 = 1.0f;
   }
   if (!_88)
   {
      float _102 = (c_sdw_param[0].x) * _95;
      float _103 = (c_sdw_param[0].z) * _96;
      float _106 = _102 + (c_sdw_param[0].y);
      float _107 = _103 + (c_sdw_param[0].w);
      _109 = _106;
      _110 = _107;
   }
   else
   {
      _109 = _95;
      _110 = _96;
   }
   float _114 = c_rt_size.z * SV_Position.x;
   float _115 = c_rt_size.w * SV_Position.y;
   float4 _116 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_114, _115)));
   int _122 = c_bool & 16384;
   bool _123 = (_122 == 0);
   if (!_123)
   {
      float _125 = _116.x * 2.0f;
      float _126 = _116.y * 2.0f;
      float _127 = _116.z * 2.0f;
      _142 = _125;
      _143 = _126;
      _144 = _127;
   }
   else
   {
      int _129 = c_bool & 8192;
      bool _130 = (_129 == 0);
      if (!_130)
      {
         float _132 = _116.x * 2.0f;
         float _133 = _116.y * 2.0f;
         float _134 = _116.z * 2.0f;
         _142 = _132;
         _143 = _133;
         _144 = _134;
      }
      else
      {
         float _138 = _116.x / c_tone_control.z;
         float _139 = _116.y / c_tone_control.z;
         float _140 = _116.z / c_tone_control.z;
         _142 = _138;
         _143 = _139;
         _144 = _140;
      }
   }
   int _145 = c_bool & 8;
   bool _146 = (_145 == 0);
   if (!_146)
   {
      float _151 = (c_sdw_param[0].x) * _109;
      float _152 = (c_sdw_param[0].z) * _110;
      float _155 = _151 + (c_sdw_param[0].y);
      float _156 = _152 + (c_sdw_param[0].w);
      _158 = _155;
      _159 = _156;
   }
   else
   {
      _158 = _109;
      _159 = _110;
   }
   float _160 = _159 * _158;
   float _165 = 1.0f - (c_light_col[5].x);
   float _166 = 1.0f - (c_light_col[5].y);
   float _167 = 1.0f - (c_light_col[5].z);
   float _168 = _165 * _160;
   float _169 = _166 * _160;
   float _170 = _167 * _160;
   float _171 = _168 + (c_light_col[5].x);
   float _172 = _169 + (c_light_col[5].y);
   float _173 = _170 + (c_light_col[5].z);
   float _174 = _97 * COLOR.x;
   float _175 = _97 * COLOR.y;
   float _176 = _97 * COLOR.z;
   if (!_146)
   {
      float _180 = (c_sdw_param[2].x) * _171;
      float _181 = (c_sdw_param[2].x) * _172;
      float _182 = (c_sdw_param[2].x) * _173;
      float _184 = _180 + (c_sdw_param[2].y);
      float _185 = _181 + (c_sdw_param[2].y);
      float _186 = _182 + (c_sdw_param[2].y);
      float _189 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _190 = _184 * _189;
      float _191 = _185 * _189;
      float _192 = _186 * _189;
      _194 = _190;
      _195 = _191;
      _196 = _192;
   }
   else
   {
      _194 = _171;
      _195 = _172;
      _196 = _173;
   }
   float _197 = _194 * _97;
   float _198 = _195 * _97;
   float _199 = _196 * _97;
   float _210 = c_mat_emissive.x * _62;
   float _211 = _210 * c_emissive_scale;
   float _212 = c_mat_emissive.y * _63;
   float _213 = _212 * c_emissive_scale;
   float _214 = c_mat_emissive.z * _64;
   float _215 = _214 * c_emissive_scale;
   int _216 = c_bool & 2048;
   bool _217 = (_216 == 0);
   if (_217)
   {
      float _219 = _159 * _142;
      float _220 = _159 * _143;
      float _221 = _159 * _144;
      float _222 = _219 + TEXCOORD_4.x;
      float _223 = _220 + TEXCOORD_4.y;
      float _224 = _221 + TEXCOORD_4.z;
      _226 = _222;
      _227 = _223;
      _228 = _224;
   }
   else
   {
      _226 = TEXCOORD_4.x;
      _227 = TEXCOORD_4.y;
      _228 = TEXCOORD_4.z;
   }
   float _233 = -0.0f - (c_light_vec[1].x);
   float _234 = -0.0f - (c_light_vec[1].y);
   float _235 = -0.0f - (c_light_vec[1].z);
   float _236 = dot(float3(_67, _68, _69), float3(_233, _234, _235));
   float _237 = saturate(_236);
   float _242 = saturate(_79);
   float _243 = (c_light_col[0].x) * _242;
   float _244 = (c_light_col[0].y) * _242;
   float _245 = (c_light_col[0].z) * _242;
   float _250 = (c_light_col[1].x) * _237;
   float _251 = (c_light_col[1].y) * _237;
   float _252 = (c_light_col[1].z) * _237;
   float _261 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _262 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _263 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _264 = _243 * _171;
   float _265 = _244 * _172;
   float _266 = _245 * _173;
   float _267 = _250 * _159;
   float _268 = _251 * _159;
   float _269 = _252 * _159;
   float _270 = _267 + _264;
   float _271 = _268 + _265;
   float _272 = _269 + _266;
   float _273 = _261 * _270;
   float _274 = _262 * _271;
   float _275 = _263 * _272;
   float _276 = _211 + c_scene_ambient.x;
   float _277 = _276 + _273;
   float _278 = _213 + c_scene_ambient.y;
   float _279 = _278 + _274;
   float _280 = _215 + c_scene_ambient.z;
   float _281 = _280 + _275;
   float _282 = _277 * _174;
   float _283 = _279 * _175;
   float _284 = _281 * _176;
   float _285 = _226 * _197;
   float _286 = _227 * _198;
   float _287 = _228 * _199;
   float _288 = _282 + _285;
   float _289 = _283 + _286;
   float _290 = _284 + _287;
   if (!_217)
   {
      float _292 = _159 * _142;
      float _293 = _159 * _143;
      float _294 = _159 * _144;
      float _295 = _288 + _292;
      float _296 = _289 + _293;
      float _297 = _290 + _294;
      _299 = _295;
      _300 = _296;
      _301 = _297;
   }
   else
   {
      _299 = _288;
      _300 = _289;
      _301 = _290;
   }
   float _302 = _299 * _62;
   float _303 = _300 * _63;
   float _304 = _301 * _64;
   int _305 = c_bool & 32768;
   bool _306 = (_305 == 0);
   if (!_306)
   {
      float _310 = (c_ps_user[11].x) * _62;
      float _311 = (c_ps_user[11].x) * _63;
      float _312 = (c_ps_user[11].x) * _64;
      float _314 = _67 + 1.0f;
      float _315 = _68 + 1.0f;
      float _316 = _69 + 1.0f;
      float _317 = _314 * 0.5f;
      float _318 = _315 * 0.5f;
      float _319 = _316 * 0.5f;
      float _320 = saturate(_317);
      float _321 = saturate(_318);
      float _322 = saturate(_319);
      float _323 = (c_ps_user[11].y) * _320;
      float _324 = _321 * (c_ps_user[11].y);
      float _325 = _322 * (c_ps_user[11].y);
      float _328 = (c_ps_user[12].x) * _171;
      float _329 = (c_ps_user[12].x) * _172;
      float _330 = (c_ps_user[12].x) * _173;
      float _332 = (c_ps_user[12].y) * _174;
      float _333 = (c_ps_user[12].y) * _175;
      float _334 = (c_ps_user[12].y) * _176;
      float _336 = (c_ps_user[12].z) * _197;
      float _337 = (c_ps_user[12].z) * _198;
      float _338 = (c_ps_user[12].z) * _199;
      float _343 = TEXCOORD_3.x * _71;
      float _344 = -0.0f - _343;
      float _345 = TEXCOORD_3.y * _71;
      float _346 = -0.0f - _345;
      float _347 = TEXCOORD_3.z * _71;
      float _348 = -0.0f - _347;
      float _349 = dot(float3(_344, _346, _348), float3(_67, _68, _69));
      float _350 = 1.0f - _349;
      float _351 = _350 * _350;
      float _352 = _350 * 0.9722222089767456f;
      float _353 = _351 * _351;
      float _354 = _353 * _352;
      float _355 = _354 + 0.02777777425944805f;
      float _356 = _355 * (c_ps_user[13].z);
      float _358 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _359 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _360 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _363 = (c_ps_user[14].x) * _211;
      float _364 = (c_ps_user[14].x) * _213;
      float _365 = (c_ps_user[14].x) * _215;
      float _367 = (c_ps_user[14].y) * _243;
      float _368 = (c_ps_user[14].y) * _244;
      float _369 = (c_ps_user[14].y) * _245;
      float _371 = (c_ps_user[14].z) * _250;
      float _372 = (c_ps_user[14].z) * _251;
      float _373 = (c_ps_user[14].z) * _252;
      float _375 = (c_ps_user[14].w) * _226;
      float _376 = (c_ps_user[14].w) * _227;
      float _377 = (c_ps_user[14].w) * _228;
      float _378 = _367 + (c_ps_user[13].x);
      float _379 = _378 + _371;
      float _380 = c_diffuse_scale.x * _379;
      float _381 = _380 * c_mat_diffuse.x;
      float _382 = _323 + _310;
      float _383 = _382 + (c_ps_user[12].w);
      float _384 = _383 + _328;
      float _385 = _384 + _332;
      float _386 = _385 + _336;
      float _387 = _386 + _358;
      float _388 = _387 + _363;
      float _389 = _388 + _375;
      float _390 = _389 + _356;
      float _391 = _390 + _381;
      float _392 = _368 + (c_ps_user[13].x);
      float _393 = _392 + _372;
      float _394 = c_diffuse_scale.y * _393;
      float _395 = _394 * c_mat_diffuse.y;
      float _396 = _324 + _311;
      float _397 = _396 + (c_ps_user[12].w);
      float _398 = _397 + _329;
      float _399 = _398 + _333;
      float _400 = _399 + _337;
      float _401 = _400 + _359;
      float _402 = _401 + _364;
      float _403 = _402 + _376;
      float _404 = _403 + _356;
      float _405 = _404 + _395;
      float _406 = _369 + (c_ps_user[13].x);
      float _407 = _406 + _373;
      float _408 = c_diffuse_scale.z * _407;
      float _409 = _408 * c_mat_diffuse.z;
      float _410 = _325 + _312;
      float _411 = _410 + (c_ps_user[12].w);
      float _412 = _411 + _330;
      float _413 = _412 + _334;
      float _414 = _413 + _338;
      float _415 = _414 + _360;
      float _416 = _415 + _365;
      float _417 = _416 + _377;
      float _418 = _417 + _356;
      float _419 = _418 + _409;
      _421 = _391;
      _422 = _405;
      _423 = _419;
   }
   else
   {
      _421 = _302;
      _422 = _303;
      _423 = _304;
   }
   float _428 = c_mat_opacity * _39.w;
   float _429 = _428 * c_global_opacity;
   float _430 = _429 * c_opacity_scale;
   int _431 = c_bool & 16;
   bool _432 = (_431 == 0);
   if (!_432)
   {
      float _436 = c_tone_control.x * _421;
      float _437 = c_tone_control.x * _422;
      float _438 = c_tone_control.x * _423;
      float _440 = log2(_436);
      float _441 = log2(_437);
      float _442 = log2(_438);
      float _443 = _440 * c_tone_control.y;
      float _444 = _441 * c_tone_control.y;
      float _445 = _442 * c_tone_control.y;
      float _446 = exp2(_443);
      float _447 = exp2(_444);
      float _448 = exp2(_445);
      _496 = _446;
      _497 = _447;
      _498 = _448;
   }
   else
   {
      if (_306)
      {
         float _453 = c_tone_control.x * _421;
         float _454 = c_tone_control.x * _422;
         float _455 = c_tone_control.x * _423;
         float _456 = _453 * 2.0f;
         float _457 = _454 * 2.0f;
         float _458 = _455 * 2.0f;
         float _459 = _453 * 0.30000001192092896f;
         float _460 = _454 * 0.30000001192092896f;
         float _461 = _455 * 0.30000001192092896f;
         float _462 = _459 + 0.05000000074505806f;
         float _463 = _460 + 0.05000000074505806f;
         float _464 = _461 + 0.05000000074505806f;
         float _465 = _462 * _456;
         float _466 = _463 * _457;
         float _467 = _464 * _458;
         float _468 = _465 + 0.004000000189989805f;
         float _469 = _466 + 0.004000000189989805f;
         float _470 = _467 + 0.004000000189989805f;
         float _471 = _459 + 0.5f;
         float _472 = _460 + 0.5f;
         float _473 = _461 + 0.5f;
         float _474 = _471 * _456;
         float _475 = _472 * _457;
         float _476 = _473 * _458;
         float _477 = _474 + 0.06000000238418579f;
         float _478 = _475 + 0.06000000238418579f;
         float _479 = _476 + 0.06000000238418579f;
         float _480 = _468 / _477;
         float _481 = _469 / _478;
         float _482 = _470 / _479;
         float _483 = _480 + -0.06666666269302368f;
         float _484 = _481 + -0.06666666269302368f;
         float _485 = _482 + -0.06666666269302368f;
         float _486 = _483 * 1.3790643215179443f;
         float _487 = _484 * 1.3790643215179443f;
         float _488 = _485 * 1.3790643215179443f;
         float _489 = YKR_MaterialCurve(_486, _453);
         float _490 = YKR_MaterialCurve(_487, _454);
         float _491 = YKR_MaterialCurve(_488, _455);
         float _492 = sqrt(_489);
         float _493 = sqrt(_490);
         float _494 = sqrt(_491);
         _496 = _492;
         _497 = _493;
         _498 = _494;
      }
      else
      {
         _496 = _421;
         _497 = _422;
         _498 = _423;
      }
   }
   int _499 = c_bool & 4;
   bool _500 = (_499 == 0);
   if (_500)
   {
      float _502 = saturate(COLOR_1.x);
      float _507 = (c_fog_param[0].x) - _496;
      float _508 = (c_fog_param[0].y) - _497;
      float _509 = (c_fog_param[0].z) - _498;
      float _510 = _507 * _502;
      float _511 = _508 * _502;
      float _512 = _509 * _502;
      _514 = _510;
      _515 = _511;
      _516 = _512;
   }
   else
   {
      _514 = COLOR_1.x;
      _515 = COLOR_1.y;
      _516 = COLOR_1.z;
   }
   float _517 = _514 + _496;
   float _518 = _515 + _497;
   float _519 = _516 + _498;
   float _525 = (c_ps_col_scl[0].x) * _517;
   float _526 = (c_ps_col_scl[0].y) * _518;
   float _527 = (c_ps_col_scl[0].z) * _519;
   float _528 = (c_ps_col_scl[0].w) * _430;
   SV_Target.x = _525;
   SV_Target.y = _526;
   SV_Target.z = _527;
   SV_Target.w = _528;
   int _531 = c_bool & 8;
   bool _532 = (_531 != 0);
   float _533 = select(_532, _517, _525);
   float _534 = select(_532, _518, _526);
   float _535 = select(_532, _519, _527);
   float _536 = select(_532, _430, _528);
   SV_Target.x = _533;
   SV_Target.y = _534;
   SV_Target.z = _535;
   SV_Target.w = _536;
   return SV_Target;
}
