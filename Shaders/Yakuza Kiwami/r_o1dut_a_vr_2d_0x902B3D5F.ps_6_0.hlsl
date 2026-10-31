// Yakuza Kiwami material r_o1dut_a(vr)2d (0x902B3D5F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
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
   float _68 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _69 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _70 = s5_t.Sample(s5_s, float2(_68, _69));
   float4 _73 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _78 = _73.x * _73.x;
   float _79 = _73.y * _73.y;
   float _80 = _73.z * _73.z;
   float _81 = _73.w * COLOR_1.w;
   float _82 = _70.x * 2.0f;
   float _83 = _70.y * 2.0f;
   float _84 = _82 + -1.0f;
   float _85 = _83 + -1.0f;
   float _86 = dot(float2(_84, _85), float2(_84, _85));
   float _87 = 1.0f - _86;
   float _88 = saturate(_87);
   float _89 = sqrt(_88);
   float _90 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _91 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _92 = _90 - _91;
   float _93 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _94 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _95 = _93 - _94;
   float _96 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _97 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _98 = _96 - _97;
   float _99 = _84 * TEXCOORD_6.x;
   float _100 = _84 * TEXCOORD_6.y;
   float _101 = _84 * TEXCOORD_6.z;
   float _102 = _85 * TEXCOORD_6.w;
   float _103 = _102 * _92;
   float _104 = _102 * _95;
   float _105 = _102 * _98;
   float _106 = _103 + _99;
   float _107 = _104 + _100;
   float _108 = _105 + _101;
   float _109 = _89 * TEXCOORD_2.x;
   float _110 = _89 * TEXCOORD_2.y;
   float _111 = _89 * TEXCOORD_2.z;
   float _112 = _106 + _109;
   float _113 = _107 + _110;
   float _114 = _108 + _111;
   float _115 = dot(float3(_112, _113, _114), float3(_112, _113, _114));
   float _116 = rsqrt(_115);
   float _117 = _112 * _116;
   float _118 = _113 * _116;
   float _119 = _114 * _116;
   float _120 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _121 = rsqrt(_120);
   float _122 = _121 * TEXCOORD_3.x;
   float _123 = _121 * TEXCOORD_3.y;
   float _124 = _121 * TEXCOORD_3.z;
   float _129 = -0.0f - (c_light_vec[0].x);
   float _130 = -0.0f - (c_light_vec[0].y);
   float _131 = -0.0f - (c_light_vec[0].z);
   float _132 = dot(float3(_117, _118, _119), float3(_129, _130, _131));
   float _136 = c_rt_size.z * SV_Position.x;
   float _137 = c_rt_size.w * SV_Position.y;
   int _140 = c_bool & 8;
   bool _141 = (_140 == 0);
   float _148;
   float _149;
   float _150;
   float _162;
   float _163;
   float _199;
   float _200;
   float _201;
   float _202;
   float _216;
   float _217;
   float _252;
   float _253;
   float _254;
   float _288;
   float _289;
   float _290;
   float _291;
   float _292;
   float _293;
   float _447;
   float _448;
   float _449;
   float _450;
   float _451;
   float _452;
   float _624;
   float _625;
   float _626;
   float _627;
   float _628;
   float _629;
   float _711;
   float _712;
   float _713;
   float _729;
   float _730;
   float _731;
   if (!_141)
   {
      float4 _143 = s13_t.Sample(s13_s, float2(_136, _137));
      _148 = _143.x;
      _149 = _143.y;
      _150 = _143.w;
   }
   else
   {
      _148 = 1.0f;
      _149 = 1.0f;
      _150 = 1.0f;
   }
   if (!_141)
   {
      float _155 = (c_sdw_param[0].x) * _148;
      float _156 = (c_sdw_param[0].z) * _149;
      float _159 = _155 + (c_sdw_param[0].y);
      float _160 = _156 + (c_sdw_param[0].w);
      _162 = _159;
      _163 = _160;
   }
   else
   {
      _162 = _148;
      _163 = _149;
   }
   float _167 = c_rt_size.z * SV_Position.x;
   float _168 = c_rt_size.w * SV_Position.y;
   float4 _169 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_167, _168)));
   int _176 = c_bool & 16384;
   bool _177 = (_176 == 0);
   if (!_177)
   {
      float _179 = _169.x * 2.0f;
      float _180 = _169.y * 2.0f;
      float _181 = _169.z * 2.0f;
      float _182 = _169.w * 2.0f;
      _199 = _179;
      _200 = _180;
      _201 = _181;
      _202 = _182;
   }
   else
   {
      int _184 = c_bool & 8192;
      bool _185 = (_184 == 0);
      if (!_185)
      {
         float _187 = _169.x * 2.0f;
         float _188 = _169.y * 2.0f;
         float _189 = _169.z * 2.0f;
         float _190 = _169.w * 2.0f;
         _199 = _187;
         _200 = _188;
         _201 = _189;
         _202 = _190;
      }
      else
      {
         float _194 = _169.x / c_tone_control.z;
         float _195 = _169.y / c_tone_control.z;
         float _196 = _169.z / c_tone_control.z;
         float _197 = _169.w / c_tone_control.z;
         _199 = _194;
         _200 = _195;
         _201 = _196;
         _202 = _197;
      }
   }
   int _203 = c_bool & 8;
   bool _204 = (_203 == 0);
   if (!_204)
   {
      float _209 = (c_sdw_param[0].x) * _162;
      float _210 = (c_sdw_param[0].z) * _163;
      float _213 = _209 + (c_sdw_param[0].y);
      float _214 = _210 + (c_sdw_param[0].w);
      _216 = _213;
      _217 = _214;
   }
   else
   {
      _216 = _162;
      _217 = _163;
   }
   float _218 = _217 * _216;
   float _223 = 1.0f - (c_light_col[5].x);
   float _224 = 1.0f - (c_light_col[5].y);
   float _225 = 1.0f - (c_light_col[5].z);
   float _226 = _223 * _218;
   float _227 = _224 * _218;
   float _228 = _225 * _218;
   float _229 = _226 + (c_light_col[5].x);
   float _230 = _227 + (c_light_col[5].y);
   float _231 = _228 + (c_light_col[5].z);
   float _232 = _150 * COLOR.x;
   float _233 = _150 * COLOR.y;
   float _234 = _150 * COLOR.z;
   if (!_204)
   {
      float _238 = (c_sdw_param[2].x) * _229;
      float _239 = (c_sdw_param[2].x) * _230;
      float _240 = (c_sdw_param[2].x) * _231;
      float _242 = _238 + (c_sdw_param[2].y);
      float _243 = _239 + (c_sdw_param[2].y);
      float _244 = _240 + (c_sdw_param[2].y);
      float _247 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _248 = _242 * _247;
      float _249 = _243 * _247;
      float _250 = _244 * _247;
      _252 = _248;
      _253 = _249;
      _254 = _250;
   }
   else
   {
      _252 = _229;
      _253 = _230;
      _254 = _231;
   }
   float _255 = _252 * _150;
   float _256 = _253 * _150;
   float _257 = _254 * _150;
   float _268 = c_mat_emissive.x * _55;
   float _269 = _268 * c_emissive_scale;
   float _270 = c_mat_emissive.y * _56;
   float _271 = _270 * c_emissive_scale;
   float _272 = c_mat_emissive.z * _57;
   float _273 = _272 * c_emissive_scale;
   int _274 = c_bool & 2048;
   bool _275 = (_274 == 0);
   if (_275)
   {
      float _277 = _217 * _199;
      float _278 = _217 * _200;
      float _279 = _217 * _201;
      float _280 = _277 + TEXCOORD_4.x;
      float _281 = _278 + TEXCOORD_4.y;
      float _282 = _279 + TEXCOORD_4.z;
      float _283 = _217 * _202;
      float _284 = _283 + TEXCOORD_5.x;
      float _285 = _283 + TEXCOORD_5.y;
      float _286 = _283 + TEXCOORD_5.z;
      _288 = _280;
      _289 = _281;
      _290 = _282;
      _291 = _284;
      _292 = _285;
      _293 = _286;
   }
   else
   {
      _288 = TEXCOORD_4.x;
      _289 = TEXCOORD_4.y;
      _290 = TEXCOORD_4.z;
      _291 = TEXCOORD_5.x;
      _292 = TEXCOORD_5.y;
      _293 = TEXCOORD_5.z;
   }
   float _298 = -0.0f - (c_light_vec[1].x);
   float _299 = -0.0f - (c_light_vec[1].y);
   float _300 = -0.0f - (c_light_vec[1].z);
   float _301 = dot(float3(_117, _118, _119), float3(_298, _299, _300));
   float _302 = saturate(_301);
   float _307 = saturate(_132);
   float _308 = (c_light_col[0].x) * _307;
   float _309 = (c_light_col[0].y) * _307;
   float _310 = (c_light_col[0].z) * _307;
   float _315 = (c_light_col[1].x) * _302;
   float _316 = (c_light_col[1].y) * _302;
   float _317 = (c_light_col[1].z) * _302;
   float _326 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _327 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _328 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _329 = _308 * _229;
   float _330 = _309 * _230;
   float _331 = _310 * _231;
   float _332 = _315 * _217;
   float _333 = _316 * _217;
   float _334 = _317 * _217;
   float _335 = _332 + _329;
   float _336 = _333 + _330;
   float _337 = _334 + _331;
   float _338 = _326 * _335;
   float _339 = _327 * _336;
   float _340 = _328 * _337;
   float _341 = _269 + c_scene_ambient.x;
   float _342 = _341 + _338;
   float _343 = _271 + c_scene_ambient.y;
   float _344 = _343 + _339;
   float _345 = _273 + c_scene_ambient.z;
   float _346 = _345 + _340;
   float _347 = _342 * _232;
   float _348 = _344 * _233;
   float _349 = _346 * _234;
   float _350 = _288 * _255;
   float _351 = _289 * _256;
   float _352 = _290 * _257;
   float _353 = _347 + _350;
   float _354 = _348 + _351;
   float _355 = _349 + _352;
   float _360 = (c_light_vec[0].x) + _122;
   float _361 = (c_light_vec[0].y) + _123;
   float _362 = (c_light_vec[0].z) + _124;
   float _363 = dot(float3(_360, _361, _362), float3(_360, _361, _362));
   float _364 = rsqrt(_363);
   float _365 = _360 * _364;
   float _366 = -0.0f - _365;
   float _367 = _361 * _364;
   float _368 = -0.0f - _367;
   float _369 = _362 * _364;
   float _370 = -0.0f - _369;
   float _371 = dot(float3(_366, _368, _370), float3(_117, _118, _119));
   float _372 = saturate(_371);
   float _373 = _123 + -0.30000001192092896f;
   float _374 = dot(float3(_122, _373, _124), float3(_122, _373, _124));
   float _375 = rsqrt(_374);
   float _376 = _122 * _375;
   float _377 = -0.0f - _376;
   float _378 = _373 * _375;
   float _379 = -0.0f - _378;
   float _380 = _124 * _375;
   float _381 = -0.0f - _380;
   float _382 = dot(float3(_377, _379, _381), float3(_117, _118, _119));
   float _383 = saturate(_382);
   float _386 = log2(_372);
   float _387 = log2(_383);
   float _388 = _386 * c_mat_power;
   float _389 = _387 * c_mat_power;
   float _390 = exp2(_388);
   float _391 = exp2(_389);
   float _394 = _390 * (c_light_col[0].x);
   float _395 = _390 * (c_light_col[0].y);
   float _396 = _390 * (c_light_col[0].z);
   float _397 = _391 * 2.0f;
   float _398 = _397 * (c_light_vec[5].x);
   float _399 = _398 * (c_light_col[1].x);
   float _400 = _398 * (c_light_col[1].y);
   float _401 = _398 * (c_light_col[1].z);
   float _402 = _394 * _229;
   float _403 = _395 * _230;
   float _404 = _396 * _231;
   float _405 = _399 * _302;
   float _406 = _400 * _302;
   float _407 = _401 * _302;
   float _408 = _405 + _402;
   float _409 = _406 + _403;
   float _410 = _407 + _404;
   float _417 = c_mat_specular.x * c_specular_scale.x;
   float _418 = _417 * _232;
   float _419 = _418 * _408;
   float _420 = c_mat_specular.y * c_specular_scale.y;
   float _421 = _420 * _233;
   float _422 = _421 * _409;
   float _423 = c_mat_specular.z * c_specular_scale.z;
   float _424 = _423 * _234;
   float _425 = _424 * _410;
   float _426 = _291 * _255;
   float _427 = _426 * _391;
   float _428 = _292 * _256;
   float _429 = _428 * _391;
   float _430 = _293 * _257;
   float _431 = _430 * _391;
   float _432 = _419 + _427;
   float _433 = _422 + _429;
   float _434 = _425 + _431;
   if (!_275)
   {
      float _436 = _217 * _199;
      float _437 = _217 * _200;
      float _438 = _217 * _201;
      float _439 = _353 + _436;
      float _440 = _354 + _437;
      float _441 = _355 + _438;
      float _442 = _217 * _202;
      float _443 = _432 + _442;
      float _444 = _433 + _442;
      float _445 = _434 + _442;
      _447 = _439;
      _448 = _440;
      _449 = _441;
      _450 = _443;
      _451 = _444;
      _452 = _445;
   }
   else
   {
      _447 = _353;
      _448 = _354;
      _449 = _355;
      _450 = _432;
      _451 = _433;
      _452 = _434;
   }
   int _453 = c_bool & 128;
   bool _454 = (_453 != 0);
   float _455 = select(_454, 0.0f, _450);
   float _456 = select(_454, 0.0f, _451);
   float _457 = select(_454, 0.0f, _452);
   float _458 = _447 * _55;
   float _459 = _448 * _56;
   float _460 = _449 * _57;
   float _461 = _455 * _63.y;
   float _462 = _456 * _63.y;
   float _463 = _457 * _63.y;
   int _464 = c_bool & 32768;
   bool _465 = (_464 == 0);
   if (!_465)
   {
      float _469 = (c_ps_user[11].x) * _55;
      float _470 = (c_ps_user[11].x) * _56;
      float _471 = (c_ps_user[11].x) * _57;
      float _473 = _117 + 1.0f;
      float _474 = _118 + 1.0f;
      float _475 = _119 + 1.0f;
      float _476 = _473 * 0.5f;
      float _477 = _474 * 0.5f;
      float _478 = _475 * 0.5f;
      float _479 = saturate(_476);
      float _480 = saturate(_477);
      float _481 = saturate(_478);
      float _482 = (c_ps_user[11].y) * _479;
      float _483 = _480 * (c_ps_user[11].y);
      float _484 = _481 * (c_ps_user[11].y);
      float _486 = (c_ps_user[11].z) * _63.y;
      float _488 = c_mat_power * 0.009999999776482582f;
      float _489 = _488 * (c_ps_user[11].w);
      float _492 = (c_ps_user[12].x) * _229;
      float _493 = (c_ps_user[12].x) * _230;
      float _494 = (c_ps_user[12].x) * _231;
      float _496 = (c_ps_user[12].y) * _232;
      float _497 = (c_ps_user[12].y) * _233;
      float _498 = (c_ps_user[12].y) * _234;
      float _500 = (c_ps_user[12].z) * _255;
      float _501 = (c_ps_user[12].z) * _256;
      float _502 = (c_ps_user[12].z) * _257;
      float _508 = -0.0f - _122;
      float _509 = -0.0f - _123;
      float _510 = -0.0f - _124;
      float _511 = dot(float3(_508, _509, _510), float3(_117, _118, _119));
      float _512 = 1.0f - _511;
      float _513 = _512 * _512;
      float _514 = _512 * 0.9722222089767456f;
      float _515 = _513 * _513;
      float _516 = _515 * _514;
      float _517 = _516 + 0.02777777425944805f;
      float _518 = _517 * (c_ps_user[13].z);
      float _520 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _521 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _522 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _525 = (c_ps_user[14].x) * _269;
      float _526 = (c_ps_user[14].x) * _271;
      float _527 = (c_ps_user[14].x) * _273;
      float _529 = (c_ps_user[14].y) * _308;
      float _530 = (c_ps_user[14].y) * _309;
      float _531 = (c_ps_user[14].y) * _310;
      float _533 = (c_ps_user[14].z) * _315;
      float _534 = (c_ps_user[14].z) * _316;
      float _535 = (c_ps_user[14].z) * _317;
      float _537 = (c_ps_user[14].w) * _288;
      float _538 = (c_ps_user[14].w) * _289;
      float _539 = (c_ps_user[14].w) * _290;
      float _542 = (c_ps_user[15].x) * _394;
      float _543 = (c_ps_user[15].x) * _395;
      float _544 = (c_ps_user[15].x) * _396;
      float _546 = _417 * _399;
      float _547 = _546 * (c_ps_user[15].y);
      float _548 = _420 * _400;
      float _549 = _548 * (c_ps_user[15].y);
      float _550 = _423 * _401;
      float _551 = _550 * (c_ps_user[15].y);
      float _553 = (c_ps_user[15].z) * _291;
      float _554 = (c_ps_user[15].z) * _292;
      float _555 = (c_ps_user[15].z) * _293;
      float _556 = _547 + _553;
      float _557 = _556 * _391;
      float _558 = _529 + (c_ps_user[13].x);
      float _559 = _558 + _533;
      float _560 = c_diffuse_scale.x * _559;
      float _561 = _560 * c_mat_diffuse.x;
      float _562 = (c_ps_user[13].y) + _486;
      float _563 = _562 + _542;
      float _564 = c_mat_specular.x * _563;
      float _565 = _564 * c_specular_scale.x;
      float _566 = _482 + _469;
      float _567 = _566 + _489;
      float _568 = _567 + (c_ps_user[12].w);
      float _569 = _568 + _492;
      float _570 = _569 + _496;
      float _571 = _570 + _500;
      float _572 = _571 + _520;
      float _573 = _572 + _525;
      float _574 = _573 + _537;
      float _575 = _574 + _518;
      float _576 = _575 + _557;
      float _577 = _576 + _561;
      float _578 = _577 + _565;
      float _579 = _549 + _554;
      float _580 = _579 * _391;
      float _581 = _530 + (c_ps_user[13].x);
      float _582 = _581 + _534;
      float _583 = c_diffuse_scale.y * _582;
      float _584 = _583 * c_mat_diffuse.y;
      float _585 = _562 + _543;
      float _586 = c_mat_specular.y * _585;
      float _587 = _586 * c_specular_scale.y;
      float _588 = _483 + _470;
      float _589 = _588 + _489;
      float _590 = _589 + (c_ps_user[12].w);
      float _591 = _590 + _493;
      float _592 = _591 + _497;
      float _593 = _592 + _501;
      float _594 = _593 + _521;
      float _595 = _594 + _526;
      float _596 = _595 + _538;
      float _597 = _596 + _518;
      float _598 = _597 + _580;
      float _599 = _598 + _584;
      float _600 = _599 + _587;
      float _601 = _551 + _555;
      float _602 = _601 * _391;
      float _603 = _531 + (c_ps_user[13].x);
      float _604 = _603 + _535;
      float _605 = c_diffuse_scale.z * _604;
      float _606 = _605 * c_mat_diffuse.z;
      float _607 = _562 + _544;
      float _608 = c_mat_specular.z * _607;
      float _609 = _608 * c_specular_scale.z;
      float _610 = _489 + _471;
      float _611 = _610 + _484;
      float _612 = _611 + (c_ps_user[12].w);
      float _613 = _612 + _494;
      float _614 = _613 + _498;
      float _615 = _614 + _502;
      float _616 = _615 + _522;
      float _617 = _616 + _527;
      float _618 = _617 + _539;
      float _619 = _618 + _518;
      float _620 = _619 + _602;
      float _621 = _620 + _606;
      float _622 = _621 + _609;
      _624 = _578;
      _625 = _600;
      _626 = _622;
      _627 = 0.0f;
      _628 = 0.0f;
      _629 = 0.0f;
   }
   else
   {
      _624 = _458;
      _625 = _459;
      _626 = _460;
      _627 = _461;
      _628 = _462;
      _629 = _463;
   }
   float _630 = _78 * _81;
   float _631 = _79 * _81;
   float _632 = _80 * _81;
   float _633 = _624 + _630;
   float _634 = _633 + _627;
   float _635 = _625 + _631;
   float _636 = _635 + _628;
   float _637 = _626 + _632;
   float _638 = _637 + _629;
   float _643 = c_mat_opacity * _50.w;
   float _644 = _643 * c_global_opacity;
   float _645 = _644 * c_opacity_scale;
   int _646 = c_bool & 16;
   bool _647 = (_646 == 0);
   if (!_647)
   {
      float _651 = c_tone_control.x * _634;
      float _652 = c_tone_control.x * _636;
      float _653 = c_tone_control.x * _638;
      float _655 = log2(_651);
      float _656 = log2(_652);
      float _657 = log2(_653);
      float _658 = _655 * c_tone_control.y;
      float _659 = _656 * c_tone_control.y;
      float _660 = _657 * c_tone_control.y;
      float _661 = exp2(_658);
      float _662 = exp2(_659);
      float _663 = exp2(_660);
      _711 = _661;
      _712 = _662;
      _713 = _663;
   }
   else
   {
      if (_465)
      {
         float _668 = c_tone_control.x * _634;
         float _669 = c_tone_control.x * _636;
         float _670 = c_tone_control.x * _638;
         float _671 = _668 * 2.0f;
         float _672 = _669 * 2.0f;
         float _673 = _670 * 2.0f;
         float _674 = _668 * 0.30000001192092896f;
         float _675 = _669 * 0.30000001192092896f;
         float _676 = _670 * 0.30000001192092896f;
         float _677 = _674 + 0.05000000074505806f;
         float _678 = _675 + 0.05000000074505806f;
         float _679 = _676 + 0.05000000074505806f;
         float _680 = _677 * _671;
         float _681 = _678 * _672;
         float _682 = _679 * _673;
         float _683 = _680 + 0.004000000189989805f;
         float _684 = _681 + 0.004000000189989805f;
         float _685 = _682 + 0.004000000189989805f;
         float _686 = _674 + 0.5f;
         float _687 = _675 + 0.5f;
         float _688 = _676 + 0.5f;
         float _689 = _686 * _671;
         float _690 = _687 * _672;
         float _691 = _688 * _673;
         float _692 = _689 + 0.06000000238418579f;
         float _693 = _690 + 0.06000000238418579f;
         float _694 = _691 + 0.06000000238418579f;
         float _695 = _683 / _692;
         float _696 = _684 / _693;
         float _697 = _685 / _694;
         float _698 = _695 + -0.06666666269302368f;
         float _699 = _696 + -0.06666666269302368f;
         float _700 = _697 + -0.06666666269302368f;
         float _701 = _698 * 1.3790643215179443f;
         float _702 = _699 * 1.3790643215179443f;
         float _703 = _700 * 1.3790643215179443f;
         float _704 = YKR_MaterialCurve(_701, _668);
         float _705 = YKR_MaterialCurve(_702, _669);
         float _706 = YKR_MaterialCurve(_703, _670);
         float _707 = sqrt(_704);
         float _708 = sqrt(_705);
         float _709 = sqrt(_706);
         _711 = _707;
         _712 = _708;
         _713 = _709;
      }
      else
      {
         _711 = _634;
         _712 = _636;
         _713 = _638;
      }
   }
   int _714 = c_bool & 4;
   bool _715 = (_714 == 0);
   if (_715)
   {
      float _717 = saturate(COLOR_1.x);
      float _722 = (c_fog_param[0].x) - _711;
      float _723 = (c_fog_param[0].y) - _712;
      float _724 = (c_fog_param[0].z) - _713;
      float _725 = _722 * _717;
      float _726 = _723 * _717;
      float _727 = _724 * _717;
      _729 = _725;
      _730 = _726;
      _731 = _727;
   }
   else
   {
      _729 = COLOR_1.x;
      _730 = COLOR_1.y;
      _731 = COLOR_1.z;
   }
   float _732 = _729 + _711;
   float _733 = _730 + _712;
   float _734 = _731 + _713;
   float _740 = (c_ps_col_scl[0].x) * _732;
   float _741 = (c_ps_col_scl[0].y) * _733;
   float _742 = (c_ps_col_scl[0].z) * _734;
   float _743 = (c_ps_col_scl[0].w) * _645;
   SV_Target.x = _740;
   SV_Target.y = _741;
   SV_Target.z = _742;
   SV_Target.w = _743;
   int _746 = c_bool & 8;
   bool _747 = (_746 != 0);
   float _748 = select(_747, _732, _740);
   float _749 = select(_747, _733, _741);
   float _750 = select(_747, _734, _742);
   float _751 = select(_747, _645, _743);
   SV_Target.x = _748;
   SV_Target.y = _749;
   SV_Target.z = _750;
   SV_Target.w = _751;
   return SV_Target;
}
