// Yakuza Kiwami material r_o1ds_a(vr)2d (0x19F51CF2, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

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
   float4 _44 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _49 = _44.x * _44.x;
   float _50 = _44.y * _44.y;
   float _51 = _44.z * _44.z;
   float _55 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _56 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _57 = s3_t.Sample(s3_s, float2(_55, _56));
   float4 _61 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _65 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _66 = rsqrt(_65);
   float _67 = _66 * TEXCOORD_2.x;
   float _68 = _66 * TEXCOORD_2.y;
   float _69 = _66 * TEXCOORD_2.z;
   float _70 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _71 = rsqrt(_70);
   float _72 = _71 * TEXCOORD_3.x;
   float _73 = _71 * TEXCOORD_3.y;
   float _74 = _71 * TEXCOORD_3.z;
   float _79 = -0.0f - (c_light_vec[0].x);
   float _80 = -0.0f - (c_light_vec[0].y);
   float _81 = -0.0f - (c_light_vec[0].z);
   float _82 = dot(float3(_67, _68, _69), float3(_79, _80, _81));
   float _86 = c_rt_size.z * SV_Position.x;
   float _87 = c_rt_size.w * SV_Position.y;
   int _90 = c_bool & 8;
   bool _91 = (_90 == 0);
   float _98;
   float _99;
   float _100;
   float _112;
   float _113;
   float _149;
   float _150;
   float _151;
   float _152;
   float _166;
   float _167;
   float _202;
   float _203;
   float _204;
   float _238;
   float _239;
   float _240;
   float _241;
   float _242;
   float _243;
   float _397;
   float _398;
   float _399;
   float _400;
   float _401;
   float _402;
   float _578;
   float _579;
   float _580;
   float _581;
   float _582;
   float _583;
   float _668;
   float _669;
   float _670;
   float _686;
   float _687;
   float _688;
   if (!_91)
   {
      float4 _93 = s13_t.Sample(s13_s, float2(_86, _87));
      _98 = _93.x;
      _99 = _93.y;
      _100 = _93.w;
   }
   else
   {
      _98 = 1.0f;
      _99 = 1.0f;
      _100 = 1.0f;
   }
   if (!_91)
   {
      float _105 = (c_sdw_param[0].x) * _98;
      float _106 = (c_sdw_param[0].z) * _99;
      float _109 = _105 + (c_sdw_param[0].y);
      float _110 = _106 + (c_sdw_param[0].w);
      _112 = _109;
      _113 = _110;
   }
   else
   {
      _112 = _98;
      _113 = _99;
   }
   float _117 = c_rt_size.z * SV_Position.x;
   float _118 = c_rt_size.w * SV_Position.y;
   float4 _119 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_117, _118)));
   int _126 = c_bool & 16384;
   bool _127 = (_126 == 0);
   if (!_127)
   {
      float _129 = _119.x * 2.0f;
      float _130 = _119.y * 2.0f;
      float _131 = _119.z * 2.0f;
      float _132 = _119.w * 2.0f;
      _149 = _129;
      _150 = _130;
      _151 = _131;
      _152 = _132;
   }
   else
   {
      int _134 = c_bool & 8192;
      bool _135 = (_134 == 0);
      if (!_135)
      {
         float _137 = _119.x * 2.0f;
         float _138 = _119.y * 2.0f;
         float _139 = _119.z * 2.0f;
         float _140 = _119.w * 2.0f;
         _149 = _137;
         _150 = _138;
         _151 = _139;
         _152 = _140;
      }
      else
      {
         float _144 = _119.x / c_tone_control.z;
         float _145 = _119.y / c_tone_control.z;
         float _146 = _119.z / c_tone_control.z;
         float _147 = _119.w / c_tone_control.z;
         _149 = _144;
         _150 = _145;
         _151 = _146;
         _152 = _147;
      }
   }
   int _153 = c_bool & 8;
   bool _154 = (_153 == 0);
   if (!_154)
   {
      float _159 = (c_sdw_param[0].x) * _112;
      float _160 = (c_sdw_param[0].z) * _113;
      float _163 = _159 + (c_sdw_param[0].y);
      float _164 = _160 + (c_sdw_param[0].w);
      _166 = _163;
      _167 = _164;
   }
   else
   {
      _166 = _112;
      _167 = _113;
   }
   float _168 = _167 * _166;
   float _173 = 1.0f - (c_light_col[5].x);
   float _174 = 1.0f - (c_light_col[5].y);
   float _175 = 1.0f - (c_light_col[5].z);
   float _176 = _173 * _168;
   float _177 = _174 * _168;
   float _178 = _175 * _168;
   float _179 = _176 + (c_light_col[5].x);
   float _180 = _177 + (c_light_col[5].y);
   float _181 = _178 + (c_light_col[5].z);
   float _182 = _100 * COLOR.x;
   float _183 = _100 * COLOR.y;
   float _184 = _100 * COLOR.z;
   if (!_154)
   {
      float _188 = (c_sdw_param[2].x) * _179;
      float _189 = (c_sdw_param[2].x) * _180;
      float _190 = (c_sdw_param[2].x) * _181;
      float _192 = _188 + (c_sdw_param[2].y);
      float _193 = _189 + (c_sdw_param[2].y);
      float _194 = _190 + (c_sdw_param[2].y);
      float _197 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _198 = _192 * _197;
      float _199 = _193 * _197;
      float _200 = _194 * _197;
      _202 = _198;
      _203 = _199;
      _204 = _200;
   }
   else
   {
      _202 = _179;
      _203 = _180;
      _204 = _181;
   }
   float _205 = _202 * _100;
   float _206 = _203 * _100;
   float _207 = _204 * _100;
   float _218 = c_mat_emissive.x * _49;
   float _219 = _218 * c_emissive_scale;
   float _220 = c_mat_emissive.y * _50;
   float _221 = _220 * c_emissive_scale;
   float _222 = c_mat_emissive.z * _51;
   float _223 = _222 * c_emissive_scale;
   int _224 = c_bool & 2048;
   bool _225 = (_224 == 0);
   if (_225)
   {
      float _227 = _167 * _149;
      float _228 = _167 * _150;
      float _229 = _167 * _151;
      float _230 = _227 + TEXCOORD_4.x;
      float _231 = _228 + TEXCOORD_4.y;
      float _232 = _229 + TEXCOORD_4.z;
      float _233 = _167 * _152;
      float _234 = _233 + TEXCOORD_5.x;
      float _235 = _233 + TEXCOORD_5.y;
      float _236 = _233 + TEXCOORD_5.z;
      _238 = _230;
      _239 = _231;
      _240 = _232;
      _241 = _234;
      _242 = _235;
      _243 = _236;
   }
   else
   {
      _238 = TEXCOORD_4.x;
      _239 = TEXCOORD_4.y;
      _240 = TEXCOORD_4.z;
      _241 = TEXCOORD_5.x;
      _242 = TEXCOORD_5.y;
      _243 = TEXCOORD_5.z;
   }
   float _248 = -0.0f - (c_light_vec[1].x);
   float _249 = -0.0f - (c_light_vec[1].y);
   float _250 = -0.0f - (c_light_vec[1].z);
   float _251 = dot(float3(_67, _68, _69), float3(_248, _249, _250));
   float _252 = saturate(_251);
   float _257 = saturate(_82);
   float _258 = (c_light_col[0].x) * _257;
   float _259 = (c_light_col[0].y) * _257;
   float _260 = (c_light_col[0].z) * _257;
   float _265 = (c_light_col[1].x) * _252;
   float _266 = (c_light_col[1].y) * _252;
   float _267 = (c_light_col[1].z) * _252;
   float _276 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _277 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _278 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _279 = _258 * _179;
   float _280 = _259 * _180;
   float _281 = _260 * _181;
   float _282 = _265 * _167;
   float _283 = _266 * _167;
   float _284 = _267 * _167;
   float _285 = _282 + _279;
   float _286 = _283 + _280;
   float _287 = _284 + _281;
   float _288 = _276 * _285;
   float _289 = _277 * _286;
   float _290 = _278 * _287;
   float _291 = _219 + c_scene_ambient.x;
   float _292 = _291 + _288;
   float _293 = _221 + c_scene_ambient.y;
   float _294 = _293 + _289;
   float _295 = _223 + c_scene_ambient.z;
   float _296 = _295 + _290;
   float _297 = _292 * _182;
   float _298 = _294 * _183;
   float _299 = _296 * _184;
   float _300 = _238 * _205;
   float _301 = _239 * _206;
   float _302 = _240 * _207;
   float _303 = _297 + _300;
   float _304 = _298 + _301;
   float _305 = _299 + _302;
   float _310 = (c_light_vec[0].x) + _72;
   float _311 = (c_light_vec[0].y) + _73;
   float _312 = (c_light_vec[0].z) + _74;
   float _313 = dot(float3(_310, _311, _312), float3(_310, _311, _312));
   float _314 = rsqrt(_313);
   float _315 = _310 * _314;
   float _316 = -0.0f - _315;
   float _317 = _311 * _314;
   float _318 = -0.0f - _317;
   float _319 = _312 * _314;
   float _320 = -0.0f - _319;
   float _321 = dot(float3(_316, _318, _320), float3(_67, _68, _69));
   float _322 = saturate(_321);
   float _323 = _73 + -0.30000001192092896f;
   float _324 = dot(float3(_72, _323, _74), float3(_72, _323, _74));
   float _325 = rsqrt(_324);
   float _326 = _72 * _325;
   float _327 = -0.0f - _326;
   float _328 = _323 * _325;
   float _329 = -0.0f - _328;
   float _330 = _74 * _325;
   float _331 = -0.0f - _330;
   float _332 = dot(float3(_327, _329, _331), float3(_67, _68, _69));
   float _333 = saturate(_332);
   float _336 = log2(_322);
   float _337 = log2(_333);
   float _338 = _336 * c_mat_power;
   float _339 = _337 * c_mat_power;
   float _340 = exp2(_338);
   float _341 = exp2(_339);
   float _344 = _340 * (c_light_col[0].x);
   float _345 = _340 * (c_light_col[0].y);
   float _346 = _340 * (c_light_col[0].z);
   float _347 = _341 * 2.0f;
   float _348 = _347 * (c_light_vec[5].x);
   float _349 = _348 * (c_light_col[1].x);
   float _350 = _348 * (c_light_col[1].y);
   float _351 = _348 * (c_light_col[1].z);
   float _352 = _344 * _179;
   float _353 = _345 * _180;
   float _354 = _346 * _181;
   float _355 = _349 * _252;
   float _356 = _350 * _252;
   float _357 = _351 * _252;
   float _358 = _355 + _352;
   float _359 = _356 + _353;
   float _360 = _357 + _354;
   float _367 = c_mat_specular.x * c_specular_scale.x;
   float _368 = _367 * _182;
   float _369 = _368 * _358;
   float _370 = c_mat_specular.y * c_specular_scale.y;
   float _371 = _370 * _183;
   float _372 = _371 * _359;
   float _373 = c_mat_specular.z * c_specular_scale.z;
   float _374 = _373 * _184;
   float _375 = _374 * _360;
   float _376 = _241 * _205;
   float _377 = _376 * _341;
   float _378 = _242 * _206;
   float _379 = _378 * _341;
   float _380 = _243 * _207;
   float _381 = _380 * _341;
   float _382 = _369 + _377;
   float _383 = _372 + _379;
   float _384 = _375 + _381;
   if (!_225)
   {
      float _386 = _167 * _149;
      float _387 = _167 * _150;
      float _388 = _167 * _151;
      float _389 = _303 + _386;
      float _390 = _304 + _387;
      float _391 = _305 + _388;
      float _392 = _167 * _152;
      float _393 = _382 + _392;
      float _394 = _383 + _392;
      float _395 = _384 + _392;
      _397 = _389;
      _398 = _390;
      _399 = _391;
      _400 = _393;
      _401 = _394;
      _402 = _395;
   }
   else
   {
      _397 = _303;
      _398 = _304;
      _399 = _305;
      _400 = _382;
      _401 = _383;
      _402 = _384;
   }
   int _403 = c_bool & 128;
   bool _404 = (_403 != 0);
   float _405 = select(_404, 0.0f, _400);
   float _406 = select(_404, 0.0f, _401);
   float _407 = select(_404, 0.0f, _402);
   float _408 = _397 * _49;
   float _409 = _398 * _50;
   float _410 = _399 * _51;
   float _411 = _405 * _57.x;
   float _412 = _406 * _57.y;
   float _413 = _407 * _57.z;
   int _414 = c_bool & 32768;
   bool _415 = (_414 == 0);
   if (!_415)
   {
      float _419 = (c_ps_user[11].x) * _49;
      float _420 = (c_ps_user[11].x) * _50;
      float _421 = (c_ps_user[11].x) * _51;
      float _423 = _67 + 1.0f;
      float _424 = _68 + 1.0f;
      float _425 = _69 + 1.0f;
      float _426 = _423 * 0.5f;
      float _427 = _424 * 0.5f;
      float _428 = _425 * 0.5f;
      float _429 = saturate(_426);
      float _430 = saturate(_427);
      float _431 = saturate(_428);
      float _432 = (c_ps_user[11].y) * _429;
      float _433 = _430 * (c_ps_user[11].y);
      float _434 = _431 * (c_ps_user[11].y);
      float _436 = (c_ps_user[11].z) * _57.x;
      float _437 = (c_ps_user[11].z) * _57.y;
      float _438 = (c_ps_user[11].z) * _57.z;
      float _440 = c_mat_power * 0.009999999776482582f;
      float _441 = _440 * (c_ps_user[11].w);
      float _444 = (c_ps_user[12].x) * _179;
      float _445 = (c_ps_user[12].x) * _180;
      float _446 = (c_ps_user[12].x) * _181;
      float _448 = (c_ps_user[12].y) * _182;
      float _449 = (c_ps_user[12].y) * _183;
      float _450 = (c_ps_user[12].y) * _184;
      float _452 = (c_ps_user[12].z) * _205;
      float _453 = (c_ps_user[12].z) * _206;
      float _454 = (c_ps_user[12].z) * _207;
      float _460 = -0.0f - _72;
      float _461 = -0.0f - _73;
      float _462 = -0.0f - _74;
      float _463 = dot(float3(_460, _461, _462), float3(_67, _68, _69));
      float _464 = 1.0f - _463;
      float _465 = _464 * _464;
      float _466 = _464 * 0.9722222089767456f;
      float _467 = _465 * _465;
      float _468 = _467 * _466;
      float _469 = _468 + 0.02777777425944805f;
      float _470 = _469 * (c_ps_user[13].z);
      float _472 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _473 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _474 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _477 = (c_ps_user[14].x) * _219;
      float _478 = (c_ps_user[14].x) * _221;
      float _479 = (c_ps_user[14].x) * _223;
      float _481 = (c_ps_user[14].y) * _258;
      float _482 = (c_ps_user[14].y) * _259;
      float _483 = (c_ps_user[14].y) * _260;
      float _485 = (c_ps_user[14].z) * _265;
      float _486 = (c_ps_user[14].z) * _266;
      float _487 = (c_ps_user[14].z) * _267;
      float _489 = (c_ps_user[14].w) * _238;
      float _490 = (c_ps_user[14].w) * _239;
      float _491 = (c_ps_user[14].w) * _240;
      float _494 = (c_ps_user[15].x) * _344;
      float _495 = (c_ps_user[15].x) * _345;
      float _496 = (c_ps_user[15].x) * _346;
      float _498 = _367 * _349;
      float _499 = _498 * (c_ps_user[15].y);
      float _500 = _370 * _350;
      float _501 = _500 * (c_ps_user[15].y);
      float _502 = _373 * _351;
      float _503 = _502 * (c_ps_user[15].y);
      float _505 = (c_ps_user[15].z) * _241;
      float _506 = (c_ps_user[15].z) * _242;
      float _507 = (c_ps_user[15].z) * _243;
      float _508 = _499 + _505;
      float _509 = _508 * _341;
      float _510 = _481 + (c_ps_user[13].x);
      float _511 = _510 + _485;
      float _512 = c_diffuse_scale.x * _511;
      float _513 = _512 * c_mat_diffuse.x;
      float _514 = (c_ps_user[13].y) + _436;
      float _515 = _514 + _494;
      float _516 = c_mat_specular.x * _515;
      float _517 = _516 * c_specular_scale.x;
      float _518 = _432 + _419;
      float _519 = _518 + _441;
      float _520 = _519 + (c_ps_user[12].w);
      float _521 = _520 + _444;
      float _522 = _521 + _448;
      float _523 = _522 + _452;
      float _524 = _523 + _472;
      float _525 = _524 + _477;
      float _526 = _525 + _489;
      float _527 = _526 + _470;
      float _528 = _527 + _509;
      float _529 = _528 + _513;
      float _530 = _529 + _517;
      float _531 = _501 + _506;
      float _532 = _531 * _341;
      float _533 = _482 + (c_ps_user[13].x);
      float _534 = _533 + _486;
      float _535 = c_diffuse_scale.y * _534;
      float _536 = _535 * c_mat_diffuse.y;
      float _537 = (c_ps_user[13].y) + _437;
      float _538 = _537 + _495;
      float _539 = c_mat_specular.y * _538;
      float _540 = _539 * c_specular_scale.y;
      float _541 = _433 + _420;
      float _542 = _541 + _441;
      float _543 = _542 + (c_ps_user[12].w);
      float _544 = _543 + _445;
      float _545 = _544 + _449;
      float _546 = _545 + _453;
      float _547 = _546 + _473;
      float _548 = _547 + _478;
      float _549 = _548 + _490;
      float _550 = _549 + _470;
      float _551 = _550 + _532;
      float _552 = _551 + _536;
      float _553 = _552 + _540;
      float _554 = _503 + _507;
      float _555 = _554 * _341;
      float _556 = _483 + (c_ps_user[13].x);
      float _557 = _556 + _487;
      float _558 = c_diffuse_scale.z * _557;
      float _559 = _558 * c_mat_diffuse.z;
      float _560 = (c_ps_user[13].y) + _438;
      float _561 = _560 + _496;
      float _562 = c_mat_specular.z * _561;
      float _563 = _562 * c_specular_scale.z;
      float _564 = _441 + _421;
      float _565 = _564 + _434;
      float _566 = _565 + (c_ps_user[12].w);
      float _567 = _566 + _446;
      float _568 = _567 + _450;
      float _569 = _568 + _454;
      float _570 = _569 + _474;
      float _571 = _570 + _479;
      float _572 = _571 + _491;
      float _573 = _572 + _470;
      float _574 = _573 + _555;
      float _575 = _574 + _559;
      float _576 = _575 + _563;
      _578 = _530;
      _579 = _553;
      _580 = _576;
      _581 = 0.0f;
      _582 = 0.0f;
      _583 = 0.0f;
   }
   else
   {
      _578 = _408;
      _579 = _409;
      _580 = _410;
      _581 = _411;
      _582 = _412;
      _583 = _413;
   }
   float _584 = _61.x * _61.x;
   float _585 = _584 * COLOR_1.w;
   float _586 = _61.y * _61.y;
   float _587 = _586 * COLOR_1.w;
   float _588 = _61.z * _61.z;
   float _589 = _588 * COLOR_1.w;
   float _590 = _578 + _585;
   float _591 = _590 + _581;
   float _592 = _579 + _587;
   float _593 = _592 + _582;
   float _594 = _580 + _589;
   float _595 = _594 + _583;
   float _600 = c_mat_opacity * _44.w;
   float _601 = _600 * c_global_opacity;
   float _602 = _601 * c_opacity_scale;
   int _603 = c_bool & 16;
   bool _604 = (_603 == 0);
   if (!_604)
   {
      float _608 = c_tone_control.x * _591;
      float _609 = c_tone_control.x * _593;
      float _610 = c_tone_control.x * _595;
      float _612 = log2(_608);
      float _613 = log2(_609);
      float _614 = log2(_610);
      float _615 = _612 * c_tone_control.y;
      float _616 = _613 * c_tone_control.y;
      float _617 = _614 * c_tone_control.y;
      float _618 = exp2(_615);
      float _619 = exp2(_616);
      float _620 = exp2(_617);
      _668 = _618;
      _669 = _619;
      _670 = _620;
   }
   else
   {
      if (_415)
      {
         float _625 = c_tone_control.x * _591;
         float _626 = c_tone_control.x * _593;
         float _627 = c_tone_control.x * _595;
         float _628 = _625 * 2.0f;
         float _629 = _626 * 2.0f;
         float _630 = _627 * 2.0f;
         float _631 = _625 * 0.30000001192092896f;
         float _632 = _626 * 0.30000001192092896f;
         float _633 = _627 * 0.30000001192092896f;
         float _634 = _631 + 0.05000000074505806f;
         float _635 = _632 + 0.05000000074505806f;
         float _636 = _633 + 0.05000000074505806f;
         float _637 = _634 * _628;
         float _638 = _635 * _629;
         float _639 = _636 * _630;
         float _640 = _637 + 0.004000000189989805f;
         float _641 = _638 + 0.004000000189989805f;
         float _642 = _639 + 0.004000000189989805f;
         float _643 = _631 + 0.5f;
         float _644 = _632 + 0.5f;
         float _645 = _633 + 0.5f;
         float _646 = _643 * _628;
         float _647 = _644 * _629;
         float _648 = _645 * _630;
         float _649 = _646 + 0.06000000238418579f;
         float _650 = _647 + 0.06000000238418579f;
         float _651 = _648 + 0.06000000238418579f;
         float _652 = _640 / _649;
         float _653 = _641 / _650;
         float _654 = _642 / _651;
         float _655 = _652 + -0.06666666269302368f;
         float _656 = _653 + -0.06666666269302368f;
         float _657 = _654 + -0.06666666269302368f;
         float _658 = _655 * 1.3790643215179443f;
         float _659 = _656 * 1.3790643215179443f;
         float _660 = _657 * 1.3790643215179443f;
         float _661 = YKR_MaterialCurve(_658, _625);
         float _662 = YKR_MaterialCurve(_659, _626);
         float _663 = YKR_MaterialCurve(_660, _627);
         float _664 = sqrt(_661);
         float _665 = sqrt(_662);
         float _666 = sqrt(_663);
         _668 = _664;
         _669 = _665;
         _670 = _666;
      }
      else
      {
         _668 = _591;
         _669 = _593;
         _670 = _595;
      }
   }
   int _671 = c_bool & 4;
   bool _672 = (_671 == 0);
   if (_672)
   {
      float _674 = saturate(COLOR_1.x);
      float _679 = (c_fog_param[0].x) - _668;
      float _680 = (c_fog_param[0].y) - _669;
      float _681 = (c_fog_param[0].z) - _670;
      float _682 = _679 * _674;
      float _683 = _680 * _674;
      float _684 = _681 * _674;
      _686 = _682;
      _687 = _683;
      _688 = _684;
   }
   else
   {
      _686 = COLOR_1.x;
      _687 = COLOR_1.y;
      _688 = COLOR_1.z;
   }
   float _689 = _686 + _668;
   float _690 = _687 + _669;
   float _691 = _688 + _670;
   float _697 = (c_ps_col_scl[0].x) * _689;
   float _698 = (c_ps_col_scl[0].y) * _690;
   float _699 = (c_ps_col_scl[0].z) * _691;
   float _700 = (c_ps_col_scl[0].w) * _602;
   SV_Target.x = _697;
   SV_Target.y = _698;
   SV_Target.z = _699;
   SV_Target.w = _700;
   int _703 = c_bool & 8;
   bool _704 = (_703 != 0);
   float _705 = select(_704, _689, _697);
   float _706 = select(_704, _690, _698);
   float _707 = select(_704, _691, _699);
   float _708 = select(_704, _602, _700);
   SV_Target.x = _705;
   SV_Target.y = _706;
   SV_Target.z = _707;
   SV_Target.w = _708;
   return SV_Target;
}
