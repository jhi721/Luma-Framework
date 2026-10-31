// Yakuza Kiwami material r_b1mt (0x2983644E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _44 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _49 = _44.x * _44.x;
   float _50 = _44.y * _44.y;
   float _51 = _44.z * _44.z;
   float _55 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _56 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _57 = s5_t.Sample(s5_s, float2(_55, _56));
   float _60 = _57.x * 2.0f;
   float _61 = _57.y * 2.0f;
   float _62 = _60 + -1.0f;
   float _63 = _61 + -1.0f;
   float _64 = dot(float2(_62, _63), float2(_62, _63));
   float _65 = 1.0f - _64;
   float _66 = saturate(_65);
   float _67 = sqrt(_66);
   float _68 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _69 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _70 = _68 - _69;
   float _71 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _72 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _73 = _71 - _72;
   float _74 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _75 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _76 = _74 - _75;
   float _77 = _62 * TEXCOORD_6.x;
   float _78 = _62 * TEXCOORD_6.y;
   float _79 = _62 * TEXCOORD_6.z;
   float _80 = _63 * TEXCOORD_6.w;
   float _81 = _80 * _70;
   float _82 = _80 * _73;
   float _83 = _80 * _76;
   float _84 = _67 * TEXCOORD_2.x;
   float _85 = _67 * TEXCOORD_2.y;
   float _86 = _67 * TEXCOORD_2.z;
   float _87 = _84 + _77;
   float _88 = _87 + _81;
   float _89 = _85 + _78;
   float _90 = _89 + _82;
   float _91 = _86 + _79;
   float _92 = _91 + _83;
   float _93 = dot(float3(_88, _90, _92), float3(_88, _90, _92));
   float _94 = rsqrt(_93);
   float _95 = _88 * _94;
   float _96 = _90 * _94;
   float _97 = _92 * _94;
   float _98 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _99 = rsqrt(_98);
   float _100 = _99 * TEXCOORD_3.x;
   float _101 = _99 * TEXCOORD_3.y;
   float _102 = _99 * TEXCOORD_3.z;
   float _107 = -0.0f - (c_light_vec[0].x);
   float _108 = -0.0f - (c_light_vec[0].y);
   float _109 = -0.0f - (c_light_vec[0].z);
   float _110 = dot(float3(_95, _96, _97), float3(_107, _108, _109));
   float _114 = c_rt_size.z * SV_Position.x;
   float _115 = c_rt_size.w * SV_Position.y;
   int _118 = c_bool & 8;
   bool _119 = (_118 == 0);
   float _126;
   float _127;
   float _128;
   float _140;
   float _141;
   float _177;
   float _178;
   float _179;
   float _180;
   float _194;
   float _195;
   float _230;
   float _231;
   float _232;
   float _266;
   float _267;
   float _268;
   float _269;
   float _270;
   float _271;
   float _425;
   float _426;
   float _427;
   float _428;
   float _429;
   float _430;
   float _602;
   float _603;
   float _604;
   float _605;
   float _606;
   float _607;
   float _683;
   float _684;
   float _685;
   float _701;
   float _702;
   float _703;
   if (!_119)
   {
      float4 _121 = s13_t.Sample(s13_s, float2(_114, _115));
      _126 = _121.x;
      _127 = _121.y;
      _128 = _121.w;
   }
   else
   {
      _126 = 1.0f;
      _127 = 1.0f;
      _128 = 1.0f;
   }
   if (!_119)
   {
      float _133 = (c_sdw_param[0].x) * _126;
      float _134 = (c_sdw_param[0].z) * _127;
      float _137 = _133 + (c_sdw_param[0].y);
      float _138 = _134 + (c_sdw_param[0].w);
      _140 = _137;
      _141 = _138;
   }
   else
   {
      _140 = _126;
      _141 = _127;
   }
   float _145 = c_rt_size.z * SV_Position.x;
   float _146 = c_rt_size.w * SV_Position.y;
   float4 _147 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_145, _146)));
   int _154 = c_bool & 16384;
   bool _155 = (_154 == 0);
   if (!_155)
   {
      float _157 = _147.x * 2.0f;
      float _158 = _147.y * 2.0f;
      float _159 = _147.z * 2.0f;
      float _160 = _147.w * 2.0f;
      _177 = _157;
      _178 = _158;
      _179 = _159;
      _180 = _160;
   }
   else
   {
      int _162 = c_bool & 8192;
      bool _163 = (_162 == 0);
      if (!_163)
      {
         float _165 = _147.x * 2.0f;
         float _166 = _147.y * 2.0f;
         float _167 = _147.z * 2.0f;
         float _168 = _147.w * 2.0f;
         _177 = _165;
         _178 = _166;
         _179 = _167;
         _180 = _168;
      }
      else
      {
         float _172 = _147.x / c_tone_control.z;
         float _173 = _147.y / c_tone_control.z;
         float _174 = _147.z / c_tone_control.z;
         float _175 = _147.w / c_tone_control.z;
         _177 = _172;
         _178 = _173;
         _179 = _174;
         _180 = _175;
      }
   }
   int _181 = c_bool & 8;
   bool _182 = (_181 == 0);
   if (!_182)
   {
      float _187 = (c_sdw_param[0].x) * _140;
      float _188 = (c_sdw_param[0].z) * _141;
      float _191 = _187 + (c_sdw_param[0].y);
      float _192 = _188 + (c_sdw_param[0].w);
      _194 = _191;
      _195 = _192;
   }
   else
   {
      _194 = _140;
      _195 = _141;
   }
   float _196 = _195 * _194;
   float _201 = 1.0f - (c_light_col[5].x);
   float _202 = 1.0f - (c_light_col[5].y);
   float _203 = 1.0f - (c_light_col[5].z);
   float _204 = _201 * _196;
   float _205 = _202 * _196;
   float _206 = _203 * _196;
   float _207 = _204 + (c_light_col[5].x);
   float _208 = _205 + (c_light_col[5].y);
   float _209 = _206 + (c_light_col[5].z);
   float _210 = _128 * COLOR.x;
   float _211 = _128 * COLOR.y;
   float _212 = _128 * COLOR.z;
   if (!_182)
   {
      float _216 = (c_sdw_param[2].x) * _207;
      float _217 = (c_sdw_param[2].x) * _208;
      float _218 = (c_sdw_param[2].x) * _209;
      float _220 = _216 + (c_sdw_param[2].y);
      float _221 = _217 + (c_sdw_param[2].y);
      float _222 = _218 + (c_sdw_param[2].y);
      float _225 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _226 = _220 * _225;
      float _227 = _221 * _225;
      float _228 = _222 * _225;
      _230 = _226;
      _231 = _227;
      _232 = _228;
   }
   else
   {
      _230 = _207;
      _231 = _208;
      _232 = _209;
   }
   float _233 = _230 * _128;
   float _234 = _231 * _128;
   float _235 = _232 * _128;
   float _246 = c_mat_emissive.x * _49;
   float _247 = _246 * c_emissive_scale;
   float _248 = c_mat_emissive.y * _50;
   float _249 = _248 * c_emissive_scale;
   float _250 = c_mat_emissive.z * _51;
   float _251 = _250 * c_emissive_scale;
   int _252 = c_bool & 2048;
   bool _253 = (_252 == 0);
   if (_253)
   {
      float _255 = _195 * _177;
      float _256 = _195 * _178;
      float _257 = _195 * _179;
      float _258 = _255 + TEXCOORD_4.x;
      float _259 = _256 + TEXCOORD_4.y;
      float _260 = _257 + TEXCOORD_4.z;
      float _261 = _195 * _180;
      float _262 = _261 + TEXCOORD_5.x;
      float _263 = _261 + TEXCOORD_5.y;
      float _264 = _261 + TEXCOORD_5.z;
      _266 = _262;
      _267 = _263;
      _268 = _264;
      _269 = _258;
      _270 = _259;
      _271 = _260;
   }
   else
   {
      _266 = TEXCOORD_5.x;
      _267 = TEXCOORD_5.y;
      _268 = TEXCOORD_5.z;
      _269 = TEXCOORD_4.x;
      _270 = TEXCOORD_4.y;
      _271 = TEXCOORD_4.z;
   }
   float _276 = -0.0f - (c_light_vec[1].x);
   float _277 = -0.0f - (c_light_vec[1].y);
   float _278 = -0.0f - (c_light_vec[1].z);
   float _279 = dot(float3(_95, _96, _97), float3(_276, _277, _278));
   float _280 = saturate(_279);
   float _285 = saturate(_110);
   float _286 = (c_light_col[0].x) * _285;
   float _287 = (c_light_col[0].y) * _285;
   float _288 = (c_light_col[0].z) * _285;
   float _293 = (c_light_col[1].x) * _280;
   float _294 = (c_light_col[1].y) * _280;
   float _295 = (c_light_col[1].z) * _280;
   float _304 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _305 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _306 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _307 = _286 * _207;
   float _308 = _287 * _208;
   float _309 = _288 * _209;
   float _310 = _293 * _195;
   float _311 = _294 * _195;
   float _312 = _295 * _195;
   float _313 = _310 + _307;
   float _314 = _311 + _308;
   float _315 = _312 + _309;
   float _316 = _304 * _313;
   float _317 = _305 * _314;
   float _318 = _306 * _315;
   float _319 = _247 + c_scene_ambient.x;
   float _320 = _319 + _316;
   float _321 = _249 + c_scene_ambient.y;
   float _322 = _321 + _317;
   float _323 = _251 + c_scene_ambient.z;
   float _324 = _323 + _318;
   float _325 = _320 * _210;
   float _326 = _322 * _211;
   float _327 = _324 * _212;
   float _328 = _269 * _233;
   float _329 = _270 * _234;
   float _330 = _271 * _235;
   float _331 = _325 + _328;
   float _332 = _326 + _329;
   float _333 = _327 + _330;
   float _338 = (c_light_vec[0].x) + _100;
   float _339 = (c_light_vec[0].y) + _101;
   float _340 = (c_light_vec[0].z) + _102;
   float _341 = dot(float3(_338, _339, _340), float3(_338, _339, _340));
   float _342 = rsqrt(_341);
   float _343 = _338 * _342;
   float _344 = -0.0f - _343;
   float _345 = _339 * _342;
   float _346 = -0.0f - _345;
   float _347 = _340 * _342;
   float _348 = -0.0f - _347;
   float _349 = dot(float3(_344, _346, _348), float3(_95, _96, _97));
   float _350 = saturate(_349);
   float _351 = _101 + -0.30000001192092896f;
   float _352 = dot(float3(_100, _351, _102), float3(_100, _351, _102));
   float _353 = rsqrt(_352);
   float _354 = _100 * _353;
   float _355 = -0.0f - _354;
   float _356 = _351 * _353;
   float _357 = -0.0f - _356;
   float _358 = _102 * _353;
   float _359 = -0.0f - _358;
   float _360 = dot(float3(_355, _357, _359), float3(_95, _96, _97));
   float _361 = saturate(_360);
   float _364 = log2(_350);
   float _365 = log2(_361);
   float _366 = _364 * c_mat_power;
   float _367 = _365 * c_mat_power;
   float _368 = exp2(_366);
   float _369 = exp2(_367);
   float _372 = _368 * (c_light_col[0].x);
   float _373 = _368 * (c_light_col[0].y);
   float _374 = _368 * (c_light_col[0].z);
   float _375 = _369 * 2.0f;
   float _376 = _375 * (c_light_vec[5].x);
   float _377 = _376 * (c_light_col[1].x);
   float _378 = _376 * (c_light_col[1].y);
   float _379 = _376 * (c_light_col[1].z);
   float _380 = _372 * _207;
   float _381 = _373 * _208;
   float _382 = _374 * _209;
   float _383 = _377 * _280;
   float _384 = _378 * _280;
   float _385 = _379 * _280;
   float _386 = _383 + _380;
   float _387 = _384 + _381;
   float _388 = _385 + _382;
   float _395 = c_mat_specular.x * c_specular_scale.x;
   float _396 = _395 * _210;
   float _397 = _396 * _386;
   float _398 = c_mat_specular.y * c_specular_scale.y;
   float _399 = _398 * _211;
   float _400 = _399 * _387;
   float _401 = c_mat_specular.z * c_specular_scale.z;
   float _402 = _401 * _212;
   float _403 = _402 * _388;
   float _404 = _266 * _233;
   float _405 = _404 * _369;
   float _406 = _267 * _234;
   float _407 = _406 * _369;
   float _408 = _268 * _235;
   float _409 = _408 * _369;
   float _410 = _397 + _405;
   float _411 = _400 + _407;
   float _412 = _403 + _409;
   if (!_253)
   {
      float _414 = _195 * _177;
      float _415 = _195 * _178;
      float _416 = _195 * _179;
      float _417 = _331 + _414;
      float _418 = _332 + _415;
      float _419 = _333 + _416;
      float _420 = _195 * _180;
      float _421 = _410 + _420;
      float _422 = _411 + _420;
      float _423 = _412 + _420;
      _425 = _417;
      _426 = _418;
      _427 = _419;
      _428 = _421;
      _429 = _422;
      _430 = _423;
   }
   else
   {
      _425 = _331;
      _426 = _332;
      _427 = _333;
      _428 = _410;
      _429 = _411;
      _430 = _412;
   }
   int _431 = c_bool & 128;
   bool _432 = (_431 != 0);
   float _433 = select(_432, 0.0f, _428);
   float _434 = select(_432, 0.0f, _429);
   float _435 = select(_432, 0.0f, _430);
   float _436 = _425 * _49;
   float _437 = _426 * _50;
   float _438 = _427 * _51;
   float _439 = _433 * _44.w;
   float _440 = _434 * _44.w;
   float _441 = _435 * _44.w;
   int _442 = c_bool & 32768;
   bool _443 = (_442 == 0);
   if (!_443)
   {
      float _447 = (c_ps_user[11].x) * _49;
      float _448 = (c_ps_user[11].x) * _50;
      float _449 = (c_ps_user[11].x) * _51;
      float _451 = _95 + 1.0f;
      float _452 = _96 + 1.0f;
      float _453 = _97 + 1.0f;
      float _454 = _451 * 0.5f;
      float _455 = _452 * 0.5f;
      float _456 = _453 * 0.5f;
      float _457 = saturate(_454);
      float _458 = saturate(_455);
      float _459 = saturate(_456);
      float _460 = (c_ps_user[11].y) * _457;
      float _461 = _458 * (c_ps_user[11].y);
      float _462 = _459 * (c_ps_user[11].y);
      float _464 = (c_ps_user[11].z) * _44.w;
      float _466 = c_mat_power * 0.009999999776482582f;
      float _467 = _466 * (c_ps_user[11].w);
      float _470 = (c_ps_user[12].x) * _207;
      float _471 = (c_ps_user[12].x) * _208;
      float _472 = (c_ps_user[12].x) * _209;
      float _474 = (c_ps_user[12].y) * _210;
      float _475 = (c_ps_user[12].y) * _211;
      float _476 = (c_ps_user[12].y) * _212;
      float _478 = (c_ps_user[12].z) * _233;
      float _479 = (c_ps_user[12].z) * _234;
      float _480 = (c_ps_user[12].z) * _235;
      float _486 = -0.0f - _100;
      float _487 = -0.0f - _101;
      float _488 = -0.0f - _102;
      float _489 = dot(float3(_486, _487, _488), float3(_95, _96, _97));
      float _490 = 1.0f - _489;
      float _491 = _490 * _490;
      float _492 = _490 * 0.9722222089767456f;
      float _493 = _491 * _491;
      float _494 = _493 * _492;
      float _495 = _494 + 0.02777777425944805f;
      float _496 = _495 * (c_ps_user[13].z);
      float _498 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _499 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _500 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _503 = (c_ps_user[14].x) * _247;
      float _504 = (c_ps_user[14].x) * _249;
      float _505 = (c_ps_user[14].x) * _251;
      float _507 = (c_ps_user[14].y) * _286;
      float _508 = (c_ps_user[14].y) * _287;
      float _509 = (c_ps_user[14].y) * _288;
      float _511 = (c_ps_user[14].z) * _293;
      float _512 = (c_ps_user[14].z) * _294;
      float _513 = (c_ps_user[14].z) * _295;
      float _515 = (c_ps_user[14].w) * _269;
      float _516 = (c_ps_user[14].w) * _270;
      float _517 = (c_ps_user[14].w) * _271;
      float _520 = (c_ps_user[15].x) * _372;
      float _521 = (c_ps_user[15].x) * _373;
      float _522 = (c_ps_user[15].x) * _374;
      float _524 = _395 * _377;
      float _525 = _524 * (c_ps_user[15].y);
      float _526 = _398 * _378;
      float _527 = _526 * (c_ps_user[15].y);
      float _528 = _401 * _379;
      float _529 = _528 * (c_ps_user[15].y);
      float _531 = (c_ps_user[15].z) * _266;
      float _532 = (c_ps_user[15].z) * _267;
      float _533 = (c_ps_user[15].z) * _268;
      float _534 = _525 + _531;
      float _535 = _534 * _369;
      float _536 = _507 + (c_ps_user[13].x);
      float _537 = _536 + _511;
      float _538 = c_diffuse_scale.x * _537;
      float _539 = _538 * c_mat_diffuse.x;
      float _540 = (c_ps_user[13].y) + _464;
      float _541 = _540 + _520;
      float _542 = c_mat_specular.x * _541;
      float _543 = _542 * c_specular_scale.x;
      float _544 = _460 + _447;
      float _545 = _544 + _467;
      float _546 = _545 + (c_ps_user[12].w);
      float _547 = _546 + _470;
      float _548 = _547 + _474;
      float _549 = _548 + _478;
      float _550 = _549 + _498;
      float _551 = _550 + _503;
      float _552 = _551 + _515;
      float _553 = _552 + _496;
      float _554 = _553 + _535;
      float _555 = _554 + _539;
      float _556 = _555 + _543;
      float _557 = _527 + _532;
      float _558 = _557 * _369;
      float _559 = _508 + (c_ps_user[13].x);
      float _560 = _559 + _512;
      float _561 = c_diffuse_scale.y * _560;
      float _562 = _561 * c_mat_diffuse.y;
      float _563 = _540 + _521;
      float _564 = c_mat_specular.y * _563;
      float _565 = _564 * c_specular_scale.y;
      float _566 = _461 + _448;
      float _567 = _566 + _467;
      float _568 = _567 + (c_ps_user[12].w);
      float _569 = _568 + _471;
      float _570 = _569 + _475;
      float _571 = _570 + _479;
      float _572 = _571 + _499;
      float _573 = _572 + _504;
      float _574 = _573 + _516;
      float _575 = _574 + _496;
      float _576 = _575 + _558;
      float _577 = _576 + _562;
      float _578 = _577 + _565;
      float _579 = _529 + _533;
      float _580 = _579 * _369;
      float _581 = _509 + (c_ps_user[13].x);
      float _582 = _581 + _513;
      float _583 = c_diffuse_scale.z * _582;
      float _584 = _583 * c_mat_diffuse.z;
      float _585 = _540 + _522;
      float _586 = c_mat_specular.z * _585;
      float _587 = _586 * c_specular_scale.z;
      float _588 = _467 + _449;
      float _589 = _588 + _462;
      float _590 = _589 + (c_ps_user[12].w);
      float _591 = _590 + _472;
      float _592 = _591 + _476;
      float _593 = _592 + _480;
      float _594 = _593 + _500;
      float _595 = _594 + _505;
      float _596 = _595 + _517;
      float _597 = _596 + _496;
      float _598 = _597 + _580;
      float _599 = _598 + _584;
      float _600 = _599 + _587;
      _602 = 0.0f;
      _603 = 0.0f;
      _604 = 0.0f;
      _605 = _556;
      _606 = _578;
      _607 = _600;
   }
   else
   {
      _602 = _439;
      _603 = _440;
      _604 = _441;
      _605 = _436;
      _606 = _437;
      _607 = _438;
   }
   float _608 = _605 + _602;
   float _609 = _606 + _603;
   float _610 = _607 + _604;
   float _615 = c_mat_opacity * COLOR.w;
   float _616 = _615 * c_global_opacity;
   float _617 = _616 * c_opacity_scale;
   int _618 = c_bool & 16;
   bool _619 = (_618 == 0);
   if (!_619)
   {
      float _623 = c_tone_control.x * _608;
      float _624 = c_tone_control.x * _609;
      float _625 = c_tone_control.x * _610;
      float _627 = log2(_623);
      float _628 = log2(_624);
      float _629 = log2(_625);
      float _630 = _627 * c_tone_control.y;
      float _631 = _628 * c_tone_control.y;
      float _632 = _629 * c_tone_control.y;
      float _633 = exp2(_630);
      float _634 = exp2(_631);
      float _635 = exp2(_632);
      _683 = _633;
      _684 = _634;
      _685 = _635;
   }
   else
   {
      if (_443)
      {
         float _640 = c_tone_control.x * _608;
         float _641 = c_tone_control.x * _609;
         float _642 = c_tone_control.x * _610;
         float _643 = _640 * 2.0f;
         float _644 = _641 * 2.0f;
         float _645 = _642 * 2.0f;
         float _646 = _640 * 0.30000001192092896f;
         float _647 = _641 * 0.30000001192092896f;
         float _648 = _642 * 0.30000001192092896f;
         float _649 = _646 + 0.05000000074505806f;
         float _650 = _647 + 0.05000000074505806f;
         float _651 = _648 + 0.05000000074505806f;
         float _652 = _649 * _643;
         float _653 = _650 * _644;
         float _654 = _651 * _645;
         float _655 = _652 + 0.004000000189989805f;
         float _656 = _653 + 0.004000000189989805f;
         float _657 = _654 + 0.004000000189989805f;
         float _658 = _646 + 0.5f;
         float _659 = _647 + 0.5f;
         float _660 = _648 + 0.5f;
         float _661 = _658 * _643;
         float _662 = _659 * _644;
         float _663 = _660 * _645;
         float _664 = _661 + 0.06000000238418579f;
         float _665 = _662 + 0.06000000238418579f;
         float _666 = _663 + 0.06000000238418579f;
         float _667 = _655 / _664;
         float _668 = _656 / _665;
         float _669 = _657 / _666;
         float _670 = _667 + -0.06666666269302368f;
         float _671 = _668 + -0.06666666269302368f;
         float _672 = _669 + -0.06666666269302368f;
         float _673 = _670 * 1.3790643215179443f;
         float _674 = _671 * 1.3790643215179443f;
         float _675 = _672 * 1.3790643215179443f;
         float _676 = YKR_MaterialCurve(_673, _640);
         float _677 = YKR_MaterialCurve(_674, _641);
         float _678 = YKR_MaterialCurve(_675, _642);
         float _679 = sqrt(_676);
         float _680 = sqrt(_677);
         float _681 = sqrt(_678);
         _683 = _679;
         _684 = _680;
         _685 = _681;
      }
      else
      {
         _683 = _608;
         _684 = _609;
         _685 = _610;
      }
   }
   int _686 = c_bool & 4;
   bool _687 = (_686 == 0);
   if (_687)
   {
      float _689 = saturate(COLOR_1.x);
      float _694 = (c_fog_param[0].x) - _683;
      float _695 = (c_fog_param[0].y) - _684;
      float _696 = (c_fog_param[0].z) - _685;
      float _697 = _694 * _689;
      float _698 = _695 * _689;
      float _699 = _696 * _689;
      _701 = _697;
      _702 = _698;
      _703 = _699;
   }
   else
   {
      _701 = COLOR_1.x;
      _702 = COLOR_1.y;
      _703 = COLOR_1.z;
   }
   float _704 = _701 + _683;
   float _705 = _702 + _684;
   float _706 = _703 + _685;
   float _712 = (c_ps_col_scl[0].x) * _704;
   float _713 = (c_ps_col_scl[0].y) * _705;
   float _714 = (c_ps_col_scl[0].z) * _706;
   float _715 = (c_ps_col_scl[0].w) * _617;
   SV_Target.x = _712;
   SV_Target.y = _713;
   SV_Target.z = _714;
   SV_Target.w = _715;
   int _718 = c_bool & 8;
   bool _719 = (_718 != 0);
   float _720 = select(_719, _704, _712);
   float _721 = select(_719, _705, _713);
   float _722 = select(_719, _706, _714);
   float _723 = select(_719, _617, _715);
   SV_Target.x = _720;
   SV_Target.y = _721;
   SV_Target.z = _722;
   SV_Target.w = _723;
   return SV_Target;
}
