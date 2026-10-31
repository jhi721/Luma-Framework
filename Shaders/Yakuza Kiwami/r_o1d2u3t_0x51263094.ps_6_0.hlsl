// Yakuza Kiwami material r_o1d2u3t (0x51263094, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _49 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _54 = _49.x * _49.x;
   float _55 = _49.y * _49.y;
   float _56 = _49.z * _49.z;
   float4 _57 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _59 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _62 = _59.x * 2.0f;
   float _63 = _59.y * 2.0f;
   float _64 = _62 + -1.0f;
   float _65 = _63 + -1.0f;
   float _66 = dot(float2(_64, _65), float2(_64, _65));
   float _67 = 1.0f - _66;
   float _68 = saturate(_67);
   float _69 = sqrt(_68);
   float _70 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _71 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _72 = _70 - _71;
   float _73 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _74 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _75 = _73 - _74;
   float _76 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _77 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _78 = _76 - _77;
   float _79 = _64 * TEXCOORD_6.x;
   float _80 = _64 * TEXCOORD_6.y;
   float _81 = _64 * TEXCOORD_6.z;
   float _82 = _65 * TEXCOORD_6.w;
   float _83 = _82 * _72;
   float _84 = _82 * _75;
   float _85 = _82 * _78;
   float _86 = _69 * TEXCOORD_2.x;
   float _87 = _69 * TEXCOORD_2.y;
   float _88 = _69 * TEXCOORD_2.z;
   float _89 = _86 + _79;
   float _90 = _89 + _83;
   float _91 = _87 + _80;
   float _92 = _91 + _84;
   float _93 = _88 + _81;
   float _94 = _93 + _85;
   float _95 = dot(float3(_90, _92, _94), float3(_90, _92, _94));
   float _96 = rsqrt(_95);
   float _97 = _90 * _96;
   float _98 = _92 * _96;
   float _99 = _94 * _96;
   float _100 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _101 = rsqrt(_100);
   float _102 = _101 * TEXCOORD_3.x;
   float _103 = _101 * TEXCOORD_3.y;
   float _104 = _101 * TEXCOORD_3.z;
   float _109 = -0.0f - (c_light_vec[0].x);
   float _110 = -0.0f - (c_light_vec[0].y);
   float _111 = -0.0f - (c_light_vec[0].z);
   float _112 = dot(float3(_97, _98, _99), float3(_109, _110, _111));
   float _116 = c_rt_size.z * SV_Position.x;
   float _117 = c_rt_size.w * SV_Position.y;
   int _120 = c_bool & 8;
   bool _121 = (_120 == 0);
   float _128;
   float _129;
   float _130;
   float _142;
   float _143;
   float _179;
   float _180;
   float _181;
   float _182;
   float _196;
   float _197;
   float _232;
   float _233;
   float _234;
   float _268;
   float _269;
   float _270;
   float _271;
   float _272;
   float _273;
   float _427;
   float _428;
   float _429;
   float _430;
   float _431;
   float _432;
   float _604;
   float _605;
   float _606;
   float _607;
   float _608;
   float _609;
   float _685;
   float _686;
   float _687;
   float _703;
   float _704;
   float _705;
   if (!_121)
   {
      float4 _123 = s13_t.Sample(s13_s, float2(_116, _117));
      _128 = _123.x;
      _129 = _123.y;
      _130 = _123.w;
   }
   else
   {
      _128 = 1.0f;
      _129 = 1.0f;
      _130 = 1.0f;
   }
   if (!_121)
   {
      float _135 = (c_sdw_param[0].x) * _128;
      float _136 = (c_sdw_param[0].z) * _129;
      float _139 = _135 + (c_sdw_param[0].y);
      float _140 = _136 + (c_sdw_param[0].w);
      _142 = _139;
      _143 = _140;
   }
   else
   {
      _142 = _128;
      _143 = _129;
   }
   float _147 = c_rt_size.z * SV_Position.x;
   float _148 = c_rt_size.w * SV_Position.y;
   float4 _149 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_147, _148)));
   int _156 = c_bool & 16384;
   bool _157 = (_156 == 0);
   if (!_157)
   {
      float _159 = _149.x * 2.0f;
      float _160 = _149.y * 2.0f;
      float _161 = _149.z * 2.0f;
      float _162 = _149.w * 2.0f;
      _179 = _159;
      _180 = _160;
      _181 = _161;
      _182 = _162;
   }
   else
   {
      int _164 = c_bool & 8192;
      bool _165 = (_164 == 0);
      if (!_165)
      {
         float _167 = _149.x * 2.0f;
         float _168 = _149.y * 2.0f;
         float _169 = _149.z * 2.0f;
         float _170 = _149.w * 2.0f;
         _179 = _167;
         _180 = _168;
         _181 = _169;
         _182 = _170;
      }
      else
      {
         float _174 = _149.x / c_tone_control.z;
         float _175 = _149.y / c_tone_control.z;
         float _176 = _149.z / c_tone_control.z;
         float _177 = _149.w / c_tone_control.z;
         _179 = _174;
         _180 = _175;
         _181 = _176;
         _182 = _177;
      }
   }
   int _183 = c_bool & 8;
   bool _184 = (_183 == 0);
   if (!_184)
   {
      float _189 = (c_sdw_param[0].x) * _142;
      float _190 = (c_sdw_param[0].z) * _143;
      float _193 = _189 + (c_sdw_param[0].y);
      float _194 = _190 + (c_sdw_param[0].w);
      _196 = _193;
      _197 = _194;
   }
   else
   {
      _196 = _142;
      _197 = _143;
   }
   float _198 = _197 * _196;
   float _203 = 1.0f - (c_light_col[5].x);
   float _204 = 1.0f - (c_light_col[5].y);
   float _205 = 1.0f - (c_light_col[5].z);
   float _206 = _203 * _198;
   float _207 = _204 * _198;
   float _208 = _205 * _198;
   float _209 = _206 + (c_light_col[5].x);
   float _210 = _207 + (c_light_col[5].y);
   float _211 = _208 + (c_light_col[5].z);
   float _212 = _130 * COLOR.x;
   float _213 = _130 * COLOR.y;
   float _214 = _130 * COLOR.z;
   if (!_184)
   {
      float _218 = (c_sdw_param[2].x) * _209;
      float _219 = (c_sdw_param[2].x) * _210;
      float _220 = (c_sdw_param[2].x) * _211;
      float _222 = _218 + (c_sdw_param[2].y);
      float _223 = _219 + (c_sdw_param[2].y);
      float _224 = _220 + (c_sdw_param[2].y);
      float _227 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _228 = _222 * _227;
      float _229 = _223 * _227;
      float _230 = _224 * _227;
      _232 = _228;
      _233 = _229;
      _234 = _230;
   }
   else
   {
      _232 = _209;
      _233 = _210;
      _234 = _211;
   }
   float _235 = _232 * _130;
   float _236 = _233 * _130;
   float _237 = _234 * _130;
   float _248 = c_mat_emissive.x * _54;
   float _249 = _248 * c_emissive_scale;
   float _250 = c_mat_emissive.y * _55;
   float _251 = _250 * c_emissive_scale;
   float _252 = c_mat_emissive.z * _56;
   float _253 = _252 * c_emissive_scale;
   int _254 = c_bool & 2048;
   bool _255 = (_254 == 0);
   if (_255)
   {
      float _257 = _197 * _179;
      float _258 = _197 * _180;
      float _259 = _197 * _181;
      float _260 = _257 + TEXCOORD_4.x;
      float _261 = _258 + TEXCOORD_4.y;
      float _262 = _259 + TEXCOORD_4.z;
      float _263 = _197 * _182;
      float _264 = _263 + TEXCOORD_5.x;
      float _265 = _263 + TEXCOORD_5.y;
      float _266 = _263 + TEXCOORD_5.z;
      _268 = _264;
      _269 = _265;
      _270 = _266;
      _271 = _260;
      _272 = _261;
      _273 = _262;
   }
   else
   {
      _268 = TEXCOORD_5.x;
      _269 = TEXCOORD_5.y;
      _270 = TEXCOORD_5.z;
      _271 = TEXCOORD_4.x;
      _272 = TEXCOORD_4.y;
      _273 = TEXCOORD_4.z;
   }
   float _278 = -0.0f - (c_light_vec[1].x);
   float _279 = -0.0f - (c_light_vec[1].y);
   float _280 = -0.0f - (c_light_vec[1].z);
   float _281 = dot(float3(_97, _98, _99), float3(_278, _279, _280));
   float _282 = saturate(_281);
   float _287 = saturate(_112);
   float _288 = (c_light_col[0].x) * _287;
   float _289 = (c_light_col[0].y) * _287;
   float _290 = (c_light_col[0].z) * _287;
   float _295 = (c_light_col[1].x) * _282;
   float _296 = (c_light_col[1].y) * _282;
   float _297 = (c_light_col[1].z) * _282;
   float _306 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _307 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _308 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _309 = _288 * _209;
   float _310 = _289 * _210;
   float _311 = _290 * _211;
   float _312 = _295 * _197;
   float _313 = _296 * _197;
   float _314 = _297 * _197;
   float _315 = _312 + _309;
   float _316 = _313 + _310;
   float _317 = _314 + _311;
   float _318 = _306 * _315;
   float _319 = _307 * _316;
   float _320 = _308 * _317;
   float _321 = _249 + c_scene_ambient.x;
   float _322 = _321 + _318;
   float _323 = _251 + c_scene_ambient.y;
   float _324 = _323 + _319;
   float _325 = _253 + c_scene_ambient.z;
   float _326 = _325 + _320;
   float _327 = _322 * _212;
   float _328 = _324 * _213;
   float _329 = _326 * _214;
   float _330 = _271 * _235;
   float _331 = _272 * _236;
   float _332 = _273 * _237;
   float _333 = _327 + _330;
   float _334 = _328 + _331;
   float _335 = _329 + _332;
   float _340 = (c_light_vec[0].x) + _102;
   float _341 = (c_light_vec[0].y) + _103;
   float _342 = (c_light_vec[0].z) + _104;
   float _343 = dot(float3(_340, _341, _342), float3(_340, _341, _342));
   float _344 = rsqrt(_343);
   float _345 = _340 * _344;
   float _346 = -0.0f - _345;
   float _347 = _341 * _344;
   float _348 = -0.0f - _347;
   float _349 = _342 * _344;
   float _350 = -0.0f - _349;
   float _351 = dot(float3(_346, _348, _350), float3(_97, _98, _99));
   float _352 = saturate(_351);
   float _353 = _103 + -0.30000001192092896f;
   float _354 = dot(float3(_102, _353, _104), float3(_102, _353, _104));
   float _355 = rsqrt(_354);
   float _356 = _102 * _355;
   float _357 = -0.0f - _356;
   float _358 = _353 * _355;
   float _359 = -0.0f - _358;
   float _360 = _104 * _355;
   float _361 = -0.0f - _360;
   float _362 = dot(float3(_357, _359, _361), float3(_97, _98, _99));
   float _363 = saturate(_362);
   float _366 = log2(_352);
   float _367 = log2(_363);
   float _368 = _366 * c_mat_power;
   float _369 = _367 * c_mat_power;
   float _370 = exp2(_368);
   float _371 = exp2(_369);
   float _374 = _370 * (c_light_col[0].x);
   float _375 = _370 * (c_light_col[0].y);
   float _376 = _370 * (c_light_col[0].z);
   float _377 = _371 * 2.0f;
   float _378 = _377 * (c_light_vec[5].x);
   float _379 = _378 * (c_light_col[1].x);
   float _380 = _378 * (c_light_col[1].y);
   float _381 = _378 * (c_light_col[1].z);
   float _382 = _374 * _209;
   float _383 = _375 * _210;
   float _384 = _376 * _211;
   float _385 = _379 * _282;
   float _386 = _380 * _282;
   float _387 = _381 * _282;
   float _388 = _385 + _382;
   float _389 = _386 + _383;
   float _390 = _387 + _384;
   float _397 = c_mat_specular.x * c_specular_scale.x;
   float _398 = _397 * _212;
   float _399 = _398 * _388;
   float _400 = c_mat_specular.y * c_specular_scale.y;
   float _401 = _400 * _213;
   float _402 = _401 * _389;
   float _403 = c_mat_specular.z * c_specular_scale.z;
   float _404 = _403 * _214;
   float _405 = _404 * _390;
   float _406 = _268 * _235;
   float _407 = _406 * _371;
   float _408 = _269 * _236;
   float _409 = _408 * _371;
   float _410 = _270 * _237;
   float _411 = _410 * _371;
   float _412 = _399 + _407;
   float _413 = _402 + _409;
   float _414 = _405 + _411;
   if (!_255)
   {
      float _416 = _197 * _179;
      float _417 = _197 * _180;
      float _418 = _197 * _181;
      float _419 = _333 + _416;
      float _420 = _334 + _417;
      float _421 = _335 + _418;
      float _422 = _197 * _182;
      float _423 = _412 + _422;
      float _424 = _413 + _422;
      float _425 = _414 + _422;
      _427 = _419;
      _428 = _420;
      _429 = _421;
      _430 = _423;
      _431 = _424;
      _432 = _425;
   }
   else
   {
      _427 = _333;
      _428 = _334;
      _429 = _335;
      _430 = _412;
      _431 = _413;
      _432 = _414;
   }
   int _433 = c_bool & 128;
   bool _434 = (_433 != 0);
   float _435 = select(_434, 0.0f, _430);
   float _436 = select(_434, 0.0f, _431);
   float _437 = select(_434, 0.0f, _432);
   float _438 = _427 * _54;
   float _439 = _428 * _55;
   float _440 = _429 * _56;
   float _441 = _435 * _57.y;
   float _442 = _436 * _57.y;
   float _443 = _437 * _57.y;
   int _444 = c_bool & 32768;
   bool _445 = (_444 == 0);
   if (!_445)
   {
      float _449 = (c_ps_user[11].x) * _54;
      float _450 = (c_ps_user[11].x) * _55;
      float _451 = (c_ps_user[11].x) * _56;
      float _453 = _97 + 1.0f;
      float _454 = _98 + 1.0f;
      float _455 = _99 + 1.0f;
      float _456 = _453 * 0.5f;
      float _457 = _454 * 0.5f;
      float _458 = _455 * 0.5f;
      float _459 = saturate(_456);
      float _460 = saturate(_457);
      float _461 = saturate(_458);
      float _462 = (c_ps_user[11].y) * _459;
      float _463 = _460 * (c_ps_user[11].y);
      float _464 = _461 * (c_ps_user[11].y);
      float _466 = (c_ps_user[11].z) * _57.y;
      float _468 = c_mat_power * 0.009999999776482582f;
      float _469 = _468 * (c_ps_user[11].w);
      float _472 = (c_ps_user[12].x) * _209;
      float _473 = (c_ps_user[12].x) * _210;
      float _474 = (c_ps_user[12].x) * _211;
      float _476 = (c_ps_user[12].y) * _212;
      float _477 = (c_ps_user[12].y) * _213;
      float _478 = (c_ps_user[12].y) * _214;
      float _480 = (c_ps_user[12].z) * _235;
      float _481 = (c_ps_user[12].z) * _236;
      float _482 = (c_ps_user[12].z) * _237;
      float _488 = -0.0f - _102;
      float _489 = -0.0f - _103;
      float _490 = -0.0f - _104;
      float _491 = dot(float3(_488, _489, _490), float3(_97, _98, _99));
      float _492 = 1.0f - _491;
      float _493 = _492 * _492;
      float _494 = _492 * 0.9722222089767456f;
      float _495 = _493 * _493;
      float _496 = _495 * _494;
      float _497 = _496 + 0.02777777425944805f;
      float _498 = _497 * (c_ps_user[13].z);
      float _500 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _501 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _502 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _505 = (c_ps_user[14].x) * _249;
      float _506 = (c_ps_user[14].x) * _251;
      float _507 = (c_ps_user[14].x) * _253;
      float _509 = (c_ps_user[14].y) * _288;
      float _510 = (c_ps_user[14].y) * _289;
      float _511 = (c_ps_user[14].y) * _290;
      float _513 = (c_ps_user[14].z) * _295;
      float _514 = (c_ps_user[14].z) * _296;
      float _515 = (c_ps_user[14].z) * _297;
      float _517 = (c_ps_user[14].w) * _271;
      float _518 = (c_ps_user[14].w) * _272;
      float _519 = (c_ps_user[14].w) * _273;
      float _522 = (c_ps_user[15].x) * _374;
      float _523 = (c_ps_user[15].x) * _375;
      float _524 = (c_ps_user[15].x) * _376;
      float _526 = _397 * _379;
      float _527 = _526 * (c_ps_user[15].y);
      float _528 = _400 * _380;
      float _529 = _528 * (c_ps_user[15].y);
      float _530 = _403 * _381;
      float _531 = _530 * (c_ps_user[15].y);
      float _533 = (c_ps_user[15].z) * _268;
      float _534 = (c_ps_user[15].z) * _269;
      float _535 = (c_ps_user[15].z) * _270;
      float _536 = _527 + _533;
      float _537 = _536 * _371;
      float _538 = _509 + (c_ps_user[13].x);
      float _539 = _538 + _513;
      float _540 = c_diffuse_scale.x * _539;
      float _541 = _540 * c_mat_diffuse.x;
      float _542 = (c_ps_user[13].y) + _466;
      float _543 = _542 + _522;
      float _544 = c_mat_specular.x * _543;
      float _545 = _544 * c_specular_scale.x;
      float _546 = _462 + _449;
      float _547 = _546 + _469;
      float _548 = _547 + (c_ps_user[12].w);
      float _549 = _548 + _472;
      float _550 = _549 + _476;
      float _551 = _550 + _480;
      float _552 = _551 + _500;
      float _553 = _552 + _505;
      float _554 = _553 + _517;
      float _555 = _554 + _498;
      float _556 = _555 + _537;
      float _557 = _556 + _541;
      float _558 = _557 + _545;
      float _559 = _529 + _534;
      float _560 = _559 * _371;
      float _561 = _510 + (c_ps_user[13].x);
      float _562 = _561 + _514;
      float _563 = c_diffuse_scale.y * _562;
      float _564 = _563 * c_mat_diffuse.y;
      float _565 = _542 + _523;
      float _566 = c_mat_specular.y * _565;
      float _567 = _566 * c_specular_scale.y;
      float _568 = _463 + _450;
      float _569 = _568 + _469;
      float _570 = _569 + (c_ps_user[12].w);
      float _571 = _570 + _473;
      float _572 = _571 + _477;
      float _573 = _572 + _481;
      float _574 = _573 + _501;
      float _575 = _574 + _506;
      float _576 = _575 + _518;
      float _577 = _576 + _498;
      float _578 = _577 + _560;
      float _579 = _578 + _564;
      float _580 = _579 + _567;
      float _581 = _531 + _535;
      float _582 = _581 * _371;
      float _583 = _511 + (c_ps_user[13].x);
      float _584 = _583 + _515;
      float _585 = c_diffuse_scale.z * _584;
      float _586 = _585 * c_mat_diffuse.z;
      float _587 = _542 + _524;
      float _588 = c_mat_specular.z * _587;
      float _589 = _588 * c_specular_scale.z;
      float _590 = _469 + _451;
      float _591 = _590 + _464;
      float _592 = _591 + (c_ps_user[12].w);
      float _593 = _592 + _474;
      float _594 = _593 + _478;
      float _595 = _594 + _482;
      float _596 = _595 + _502;
      float _597 = _596 + _507;
      float _598 = _597 + _519;
      float _599 = _598 + _498;
      float _600 = _599 + _582;
      float _601 = _600 + _586;
      float _602 = _601 + _589;
      _604 = _558;
      _605 = _580;
      _606 = _602;
      _607 = 0.0f;
      _608 = 0.0f;
      _609 = 0.0f;
   }
   else
   {
      _604 = _438;
      _605 = _439;
      _606 = _440;
      _607 = _441;
      _608 = _442;
      _609 = _443;
   }
   float _610 = _607 + _604;
   float _611 = _608 + _605;
   float _612 = _609 + _606;
   float _617 = c_mat_opacity * _49.w;
   float _618 = _617 * c_global_opacity;
   float _619 = _618 * c_opacity_scale;
   int _620 = c_bool & 16;
   bool _621 = (_620 == 0);
   if (!_621)
   {
      float _625 = c_tone_control.x * _610;
      float _626 = c_tone_control.x * _611;
      float _627 = c_tone_control.x * _612;
      float _629 = log2(_625);
      float _630 = log2(_626);
      float _631 = log2(_627);
      float _632 = _629 * c_tone_control.y;
      float _633 = _630 * c_tone_control.y;
      float _634 = _631 * c_tone_control.y;
      float _635 = exp2(_632);
      float _636 = exp2(_633);
      float _637 = exp2(_634);
      _685 = _635;
      _686 = _636;
      _687 = _637;
   }
   else
   {
      if (_445)
      {
         float _642 = c_tone_control.x * _610;
         float _643 = c_tone_control.x * _611;
         float _644 = c_tone_control.x * _612;
         float _645 = _642 * 2.0f;
         float _646 = _643 * 2.0f;
         float _647 = _644 * 2.0f;
         float _648 = _642 * 0.30000001192092896f;
         float _649 = _643 * 0.30000001192092896f;
         float _650 = _644 * 0.30000001192092896f;
         float _651 = _648 + 0.05000000074505806f;
         float _652 = _649 + 0.05000000074505806f;
         float _653 = _650 + 0.05000000074505806f;
         float _654 = _651 * _645;
         float _655 = _652 * _646;
         float _656 = _653 * _647;
         float _657 = _654 + 0.004000000189989805f;
         float _658 = _655 + 0.004000000189989805f;
         float _659 = _656 + 0.004000000189989805f;
         float _660 = _648 + 0.5f;
         float _661 = _649 + 0.5f;
         float _662 = _650 + 0.5f;
         float _663 = _660 * _645;
         float _664 = _661 * _646;
         float _665 = _662 * _647;
         float _666 = _663 + 0.06000000238418579f;
         float _667 = _664 + 0.06000000238418579f;
         float _668 = _665 + 0.06000000238418579f;
         float _669 = _657 / _666;
         float _670 = _658 / _667;
         float _671 = _659 / _668;
         float _672 = _669 + -0.06666666269302368f;
         float _673 = _670 + -0.06666666269302368f;
         float _674 = _671 + -0.06666666269302368f;
         float _675 = _672 * 1.3790643215179443f;
         float _676 = _673 * 1.3790643215179443f;
         float _677 = _674 * 1.3790643215179443f;
         float _678 = YKR_MaterialCurve(_675, _642);
         float _679 = YKR_MaterialCurve(_676, _643);
         float _680 = YKR_MaterialCurve(_677, _644);
         float _681 = sqrt(_678);
         float _682 = sqrt(_679);
         float _683 = sqrt(_680);
         _685 = _681;
         _686 = _682;
         _687 = _683;
      }
      else
      {
         _685 = _610;
         _686 = _611;
         _687 = _612;
      }
   }
   int _688 = c_bool & 4;
   bool _689 = (_688 == 0);
   if (_689)
   {
      float _691 = saturate(COLOR_1.x);
      float _696 = (c_fog_param[0].x) - _685;
      float _697 = (c_fog_param[0].y) - _686;
      float _698 = (c_fog_param[0].z) - _687;
      float _699 = _696 * _691;
      float _700 = _697 * _691;
      float _701 = _698 * _691;
      _703 = _699;
      _704 = _700;
      _705 = _701;
   }
   else
   {
      _703 = COLOR_1.x;
      _704 = COLOR_1.y;
      _705 = COLOR_1.z;
   }
   float _706 = _703 + _685;
   float _707 = _704 + _686;
   float _708 = _705 + _687;
   float _714 = (c_ps_col_scl[0].x) * _706;
   float _715 = (c_ps_col_scl[0].y) * _707;
   float _716 = (c_ps_col_scl[0].z) * _708;
   float _717 = (c_ps_col_scl[0].w) * _619;
   SV_Target.x = _714;
   SV_Target.y = _715;
   SV_Target.z = _716;
   SV_Target.w = _717;
   int _720 = c_bool & 8;
   bool _721 = (_720 != 0);
   float _722 = select(_721, _706, _714);
   float _723 = select(_721, _707, _715);
   float _724 = select(_721, _708, _716);
   float _725 = select(_721, _619, _717);
   SV_Target.x = _722;
   SV_Target.y = _723;
   SV_Target.z = _724;
   SV_Target.w = _725;
   return SV_Target;
}
