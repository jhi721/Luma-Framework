// Yakuza Kiwami material r_o1d2u[ref][sign] (0x83EC3ABD, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _44 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _49 = _44.x * _44.x;
   float _50 = _44.y * _44.y;
   float _51 = _44.z * _44.z;
   float _55 = (c_attribute_ps[0].x) * TEXCOORD.z;
   float _56 = (c_attribute_ps[0].y) * TEXCOORD.w;
   float4 _57 = s3_t.Sample(s3_s, float2(_55, _56));
   float _60 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _61 = rsqrt(_60);
   float _62 = _61 * TEXCOORD_2.x;
   float _63 = _61 * TEXCOORD_2.y;
   float _64 = _61 * TEXCOORD_2.z;
   float _65 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _66 = rsqrt(_65);
   float _67 = _66 * TEXCOORD_3.x;
   float _68 = _66 * TEXCOORD_3.y;
   float _69 = _66 * TEXCOORD_3.z;
   float _74 = -0.0f - (c_light_vec[0].x);
   float _75 = -0.0f - (c_light_vec[0].y);
   float _76 = -0.0f - (c_light_vec[0].z);
   float _77 = dot(float3(_62, _63, _64), float3(_74, _75, _76));
   float _81 = c_rt_size.z * SV_Position.x;
   float _82 = c_rt_size.w * SV_Position.y;
   int _85 = c_bool & 8;
   bool _86 = (_85 == 0);
   float _93;
   float _94;
   float _95;
   float _107;
   float _108;
   float _144;
   float _145;
   float _146;
   float _147;
   float _161;
   float _162;
   float _197;
   float _198;
   float _199;
   float _240;
   float _241;
   float _242;
   float _243;
   float _244;
   float _245;
   float _399;
   float _400;
   float _401;
   float _402;
   float _403;
   float _404;
   float _576;
   float _577;
   float _578;
   float _579;
   float _580;
   float _581;
   float _700;
   float _701;
   float _702;
   float _718;
   float _719;
   float _720;
   if (!_86)
   {
      float4 _88 = s13_t.Sample(s13_s, float2(_81, _82));
      _93 = _88.x;
      _94 = _88.y;
      _95 = _88.w;
   }
   else
   {
      _93 = 1.0f;
      _94 = 1.0f;
      _95 = 1.0f;
   }
   if (!_86)
   {
      float _100 = (c_sdw_param[0].x) * _93;
      float _101 = (c_sdw_param[0].z) * _94;
      float _104 = _100 + (c_sdw_param[0].y);
      float _105 = _101 + (c_sdw_param[0].w);
      _107 = _104;
      _108 = _105;
   }
   else
   {
      _107 = _93;
      _108 = _94;
   }
   float _112 = c_rt_size.z * SV_Position.x;
   float _113 = c_rt_size.w * SV_Position.y;
   float4 _114 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_112, _113)));
   int _121 = c_bool & 16384;
   bool _122 = (_121 == 0);
   if (!_122)
   {
      float _124 = _114.x * 2.0f;
      float _125 = _114.y * 2.0f;
      float _126 = _114.z * 2.0f;
      float _127 = _114.w * 2.0f;
      _144 = _124;
      _145 = _125;
      _146 = _126;
      _147 = _127;
   }
   else
   {
      int _129 = c_bool & 8192;
      bool _130 = (_129 == 0);
      if (!_130)
      {
         float _132 = _114.x * 2.0f;
         float _133 = _114.y * 2.0f;
         float _134 = _114.z * 2.0f;
         float _135 = _114.w * 2.0f;
         _144 = _132;
         _145 = _133;
         _146 = _134;
         _147 = _135;
      }
      else
      {
         float _139 = _114.x / c_tone_control.z;
         float _140 = _114.y / c_tone_control.z;
         float _141 = _114.z / c_tone_control.z;
         float _142 = _114.w / c_tone_control.z;
         _144 = _139;
         _145 = _140;
         _146 = _141;
         _147 = _142;
      }
   }
   int _148 = c_bool & 8;
   bool _149 = (_148 == 0);
   if (!_149)
   {
      float _154 = (c_sdw_param[0].x) * _107;
      float _155 = (c_sdw_param[0].z) * _108;
      float _158 = _154 + (c_sdw_param[0].y);
      float _159 = _155 + (c_sdw_param[0].w);
      _161 = _158;
      _162 = _159;
   }
   else
   {
      _161 = _107;
      _162 = _108;
   }
   float _163 = _162 * _161;
   float _168 = 1.0f - (c_light_col[5].x);
   float _169 = 1.0f - (c_light_col[5].y);
   float _170 = 1.0f - (c_light_col[5].z);
   float _171 = _168 * _163;
   float _172 = _169 * _163;
   float _173 = _170 * _163;
   float _174 = _171 + (c_light_col[5].x);
   float _175 = _172 + (c_light_col[5].y);
   float _176 = _173 + (c_light_col[5].z);
   float _177 = _95 * COLOR.x;
   float _178 = _95 * COLOR.y;
   float _179 = _95 * COLOR.z;
   if (!_149)
   {
      float _183 = (c_sdw_param[2].x) * _174;
      float _184 = (c_sdw_param[2].x) * _175;
      float _185 = (c_sdw_param[2].x) * _176;
      float _187 = _183 + (c_sdw_param[2].y);
      float _188 = _184 + (c_sdw_param[2].y);
      float _189 = _185 + (c_sdw_param[2].y);
      float _192 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _193 = _187 * _192;
      float _194 = _188 * _192;
      float _195 = _189 * _192;
      _197 = _193;
      _198 = _194;
      _199 = _195;
   }
   else
   {
      _197 = _174;
      _198 = _175;
      _199 = _176;
   }
   float _200 = _197 * _95;
   float _201 = _198 * _95;
   float _202 = _199 * _95;
   float _215 = (c_ps_user[0].x) * c_emissive_scale;
   float _216 = _215 * c_mat_emissive.x;
   float _217 = _215 * c_mat_emissive.y;
   float _218 = _215 * c_mat_emissive.z;
   float _220 = _216 + (c_ps_user[0].y);
   float _221 = _217 + (c_ps_user[0].y);
   float _222 = _218 + (c_ps_user[0].y);
   float _223 = _220 * _49;
   float _224 = _221 * _50;
   float _225 = _222 * _51;
   int _226 = c_bool & 2048;
   bool _227 = (_226 == 0);
   if (_227)
   {
      float _229 = _162 * _144;
      float _230 = _162 * _145;
      float _231 = _162 * _146;
      float _232 = _229 + TEXCOORD_4.x;
      float _233 = _230 + TEXCOORD_4.y;
      float _234 = _231 + TEXCOORD_4.z;
      float _235 = _162 * _147;
      float _236 = _235 + TEXCOORD_5.x;
      float _237 = _235 + TEXCOORD_5.y;
      float _238 = _235 + TEXCOORD_5.z;
      _240 = _232;
      _241 = _233;
      _242 = _234;
      _243 = _236;
      _244 = _237;
      _245 = _238;
   }
   else
   {
      _240 = TEXCOORD_4.x;
      _241 = TEXCOORD_4.y;
      _242 = TEXCOORD_4.z;
      _243 = TEXCOORD_5.x;
      _244 = TEXCOORD_5.y;
      _245 = TEXCOORD_5.z;
   }
   float _250 = -0.0f - (c_light_vec[1].x);
   float _251 = -0.0f - (c_light_vec[1].y);
   float _252 = -0.0f - (c_light_vec[1].z);
   float _253 = dot(float3(_62, _63, _64), float3(_250, _251, _252));
   float _254 = saturate(_253);
   float _259 = saturate(_77);
   float _260 = (c_light_col[0].x) * _259;
   float _261 = (c_light_col[0].y) * _259;
   float _262 = (c_light_col[0].z) * _259;
   float _267 = (c_light_col[1].x) * _254;
   float _268 = (c_light_col[1].y) * _254;
   float _269 = (c_light_col[1].z) * _254;
   float _278 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _279 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _280 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _281 = _260 * _174;
   float _282 = _261 * _175;
   float _283 = _262 * _176;
   float _284 = _267 * _162;
   float _285 = _268 * _162;
   float _286 = _269 * _162;
   float _287 = _284 + _281;
   float _288 = _285 + _282;
   float _289 = _286 + _283;
   float _290 = _278 * _287;
   float _291 = _279 * _288;
   float _292 = _280 * _289;
   float _293 = _223 + c_scene_ambient.x;
   float _294 = _293 + _290;
   float _295 = _224 + c_scene_ambient.y;
   float _296 = _295 + _291;
   float _297 = _225 + c_scene_ambient.z;
   float _298 = _297 + _292;
   float _299 = _294 * _177;
   float _300 = _296 * _178;
   float _301 = _298 * _179;
   float _302 = _240 * _200;
   float _303 = _241 * _201;
   float _304 = _242 * _202;
   float _305 = _299 + _302;
   float _306 = _300 + _303;
   float _307 = _301 + _304;
   float _312 = (c_light_vec[0].x) + _67;
   float _313 = (c_light_vec[0].y) + _68;
   float _314 = (c_light_vec[0].z) + _69;
   float _315 = dot(float3(_312, _313, _314), float3(_312, _313, _314));
   float _316 = rsqrt(_315);
   float _317 = _312 * _316;
   float _318 = -0.0f - _317;
   float _319 = _313 * _316;
   float _320 = -0.0f - _319;
   float _321 = _314 * _316;
   float _322 = -0.0f - _321;
   float _323 = dot(float3(_318, _320, _322), float3(_62, _63, _64));
   float _324 = saturate(_323);
   float _325 = _68 + -0.30000001192092896f;
   float _326 = dot(float3(_67, _325, _69), float3(_67, _325, _69));
   float _327 = rsqrt(_326);
   float _328 = _67 * _327;
   float _329 = -0.0f - _328;
   float _330 = _325 * _327;
   float _331 = -0.0f - _330;
   float _332 = _69 * _327;
   float _333 = -0.0f - _332;
   float _334 = dot(float3(_329, _331, _333), float3(_62, _63, _64));
   float _335 = saturate(_334);
   float _338 = log2(_324);
   float _339 = log2(_335);
   float _340 = _338 * c_mat_power;
   float _341 = _339 * c_mat_power;
   float _342 = exp2(_340);
   float _343 = exp2(_341);
   float _346 = _342 * (c_light_col[0].x);
   float _347 = _342 * (c_light_col[0].y);
   float _348 = _342 * (c_light_col[0].z);
   float _349 = _343 * 2.0f;
   float _350 = _349 * (c_light_vec[5].x);
   float _351 = _350 * (c_light_col[1].x);
   float _352 = _350 * (c_light_col[1].y);
   float _353 = _350 * (c_light_col[1].z);
   float _354 = _346 * _174;
   float _355 = _347 * _175;
   float _356 = _348 * _176;
   float _357 = _351 * _254;
   float _358 = _352 * _254;
   float _359 = _353 * _254;
   float _360 = _357 + _354;
   float _361 = _358 + _355;
   float _362 = _359 + _356;
   float _369 = c_mat_specular.x * c_specular_scale.x;
   float _370 = _369 * _177;
   float _371 = _370 * _360;
   float _372 = c_mat_specular.y * c_specular_scale.y;
   float _373 = _372 * _178;
   float _374 = _373 * _361;
   float _375 = c_mat_specular.z * c_specular_scale.z;
   float _376 = _375 * _179;
   float _377 = _376 * _362;
   float _378 = _243 * _200;
   float _379 = _378 * _343;
   float _380 = _244 * _201;
   float _381 = _380 * _343;
   float _382 = _245 * _202;
   float _383 = _382 * _343;
   float _384 = _371 + _379;
   float _385 = _374 + _381;
   float _386 = _377 + _383;
   if (!_227)
   {
      float _388 = _162 * _144;
      float _389 = _162 * _145;
      float _390 = _162 * _146;
      float _391 = _305 + _388;
      float _392 = _306 + _389;
      float _393 = _307 + _390;
      float _394 = _162 * _147;
      float _395 = _384 + _394;
      float _396 = _385 + _394;
      float _397 = _386 + _394;
      _399 = _391;
      _400 = _392;
      _401 = _393;
      _402 = _395;
      _403 = _396;
      _404 = _397;
   }
   else
   {
      _399 = _305;
      _400 = _306;
      _401 = _307;
      _402 = _384;
      _403 = _385;
      _404 = _386;
   }
   int _405 = c_bool & 128;
   bool _406 = (_405 != 0);
   float _407 = select(_406, 0.0f, _402);
   float _408 = select(_406, 0.0f, _403);
   float _409 = select(_406, 0.0f, _404);
   float _410 = _399 * _49;
   float _411 = _400 * _50;
   float _412 = _401 * _51;
   float _413 = _407 * _57.y;
   float _414 = _408 * _57.y;
   float _415 = _409 * _57.y;
   int _416 = c_bool & 32768;
   bool _417 = (_416 == 0);
   if (!_417)
   {
      float _421 = (c_ps_user[11].x) * _49;
      float _422 = (c_ps_user[11].x) * _50;
      float _423 = (c_ps_user[11].x) * _51;
      float _425 = _62 + 1.0f;
      float _426 = _63 + 1.0f;
      float _427 = _64 + 1.0f;
      float _428 = _425 * 0.5f;
      float _429 = _426 * 0.5f;
      float _430 = _427 * 0.5f;
      float _431 = saturate(_428);
      float _432 = saturate(_429);
      float _433 = saturate(_430);
      float _434 = (c_ps_user[11].y) * _431;
      float _435 = _432 * (c_ps_user[11].y);
      float _436 = _433 * (c_ps_user[11].y);
      float _438 = (c_ps_user[11].z) * _57.y;
      float _440 = c_mat_power * 0.009999999776482582f;
      float _441 = _440 * (c_ps_user[11].w);
      float _444 = (c_ps_user[12].x) * _174;
      float _445 = (c_ps_user[12].x) * _175;
      float _446 = (c_ps_user[12].x) * _176;
      float _448 = (c_ps_user[12].y) * _177;
      float _449 = (c_ps_user[12].y) * _178;
      float _450 = (c_ps_user[12].y) * _179;
      float _452 = (c_ps_user[12].z) * _200;
      float _453 = (c_ps_user[12].z) * _201;
      float _454 = (c_ps_user[12].z) * _202;
      float _460 = -0.0f - _67;
      float _461 = -0.0f - _68;
      float _462 = -0.0f - _69;
      float _463 = dot(float3(_460, _461, _462), float3(_62, _63, _64));
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
      float _477 = (c_ps_user[14].x) * _223;
      float _478 = (c_ps_user[14].x) * _224;
      float _479 = (c_ps_user[14].x) * _225;
      float _481 = (c_ps_user[14].y) * _260;
      float _482 = (c_ps_user[14].y) * _261;
      float _483 = (c_ps_user[14].y) * _262;
      float _485 = (c_ps_user[14].z) * _267;
      float _486 = (c_ps_user[14].z) * _268;
      float _487 = (c_ps_user[14].z) * _269;
      float _489 = (c_ps_user[14].w) * _240;
      float _490 = (c_ps_user[14].w) * _241;
      float _491 = (c_ps_user[14].w) * _242;
      float _494 = (c_ps_user[15].x) * _346;
      float _495 = (c_ps_user[15].x) * _347;
      float _496 = (c_ps_user[15].x) * _348;
      float _498 = _369 * _351;
      float _499 = _498 * (c_ps_user[15].y);
      float _500 = _372 * _352;
      float _501 = _500 * (c_ps_user[15].y);
      float _502 = _375 * _353;
      float _503 = _502 * (c_ps_user[15].y);
      float _505 = (c_ps_user[15].z) * _243;
      float _506 = (c_ps_user[15].z) * _244;
      float _507 = (c_ps_user[15].z) * _245;
      float _508 = _499 + _505;
      float _509 = _508 * _343;
      float _510 = _481 + (c_ps_user[13].x);
      float _511 = _510 + _485;
      float _512 = c_diffuse_scale.x * _511;
      float _513 = _512 * c_mat_diffuse.x;
      float _514 = (c_ps_user[13].y) + _438;
      float _515 = _514 + _494;
      float _516 = c_mat_specular.x * _515;
      float _517 = _516 * c_specular_scale.x;
      float _518 = _434 + _421;
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
      float _532 = _531 * _343;
      float _533 = _482 + (c_ps_user[13].x);
      float _534 = _533 + _486;
      float _535 = c_diffuse_scale.y * _534;
      float _536 = _535 * c_mat_diffuse.y;
      float _537 = _514 + _495;
      float _538 = c_mat_specular.y * _537;
      float _539 = _538 * c_specular_scale.y;
      float _540 = _435 + _422;
      float _541 = _540 + _441;
      float _542 = _541 + (c_ps_user[12].w);
      float _543 = _542 + _445;
      float _544 = _543 + _449;
      float _545 = _544 + _453;
      float _546 = _545 + _473;
      float _547 = _546 + _478;
      float _548 = _547 + _490;
      float _549 = _548 + _470;
      float _550 = _549 + _532;
      float _551 = _550 + _536;
      float _552 = _551 + _539;
      float _553 = _503 + _507;
      float _554 = _553 * _343;
      float _555 = _483 + (c_ps_user[13].x);
      float _556 = _555 + _487;
      float _557 = c_diffuse_scale.z * _556;
      float _558 = _557 * c_mat_diffuse.z;
      float _559 = _514 + _496;
      float _560 = c_mat_specular.z * _559;
      float _561 = _560 * c_specular_scale.z;
      float _562 = _441 + _423;
      float _563 = _562 + _436;
      float _564 = _563 + (c_ps_user[12].w);
      float _565 = _564 + _446;
      float _566 = _565 + _450;
      float _567 = _566 + _454;
      float _568 = _567 + _474;
      float _569 = _568 + _479;
      float _570 = _569 + _491;
      float _571 = _570 + _470;
      float _572 = _571 + _554;
      float _573 = _572 + _558;
      float _574 = _573 + _561;
      _576 = _530;
      _577 = _552;
      _578 = _574;
      _579 = 0.0f;
      _580 = 0.0f;
      _581 = 0.0f;
   }
   else
   {
      _576 = _410;
      _577 = _411;
      _578 = _412;
      _579 = _413;
      _580 = _414;
      _581 = _415;
   }
   float _582 = _579 + _576;
   float _583 = _580 + _577;
   float _584 = _581 + _578;
   float _585 = dot(float3(_67, _68, _69), float3(_62, _63, _64));
   float _586 = _585 * 2.0f;
   float _587 = _586 * _62;
   float _588 = _586 * _63;
   float _589 = _586 * _64;
   float _590 = _67 - _587;
   float _591 = _68 - _588;
   float _592 = _69 - _589;
   float _593 = -0.0f - _590;
   float4 _594 = s6c_t.Sample(s6c_s, float3(_593, _591, _592));
   float _601 = _108 * _107;
   float _602 = _77 + 0.75f;
   float _603 = saturate(_602);
   float _604 = _601 * _603;
   float _605 = max(_604, (c_light_vec[5].y));
   float _606 = max(_594.w, 0.003921568859368563f);
   float _607 = 0.25f / _606;
   float _608 = _607 * _594.x;
   float _609 = _607 * _594.y;
   float _610 = _607 * _594.z;
   float _611 = _57.x * TEXCOORD_2.w;
   float _612 = _611 * _605;
   float _613 = _608 * _608;
   float _614 = _613 * _612;
   float _615 = _609 * _609;
   float _616 = _615 * _612;
   float _617 = _610 * _610;
   float _618 = _617 * _612;
   float _619 = _582 + _614;
   float _620 = _583 + _616;
   float _621 = _584 + _618;
   float _628 = c_mat_opacity * _44.w;
   float _629 = _628 * c_opacity_scale;
   float _630 = _629 * c_global_opacity;
   int _633 = c_bool & 16;
   bool _634 = (_633 == 0);
   if (!_634)
   {
      float _638 = c_tone_control.x * _619;
      float _639 = c_tone_control.x * _620;
      float _640 = c_tone_control.x * _621;
      float _642 = log2(_638);
      float _643 = log2(_639);
      float _644 = log2(_640);
      float _645 = _642 * c_tone_control.y;
      float _646 = _643 * c_tone_control.y;
      float _647 = _644 * c_tone_control.y;
      float _648 = exp2(_645);
      float _649 = exp2(_646);
      float _650 = exp2(_647);
      _700 = _648;
      _701 = _649;
      _702 = _650;
   }
   else
   {
      int _652 = c_bool & 32768;
      bool _653 = (_652 == 0);
      if (_653)
      {
         float _657 = c_tone_control.x * _619;
         float _658 = c_tone_control.x * _620;
         float _659 = c_tone_control.x * _621;
         float _660 = _657 * 2.0f;
         float _661 = _658 * 2.0f;
         float _662 = _659 * 2.0f;
         float _663 = _657 * 0.30000001192092896f;
         float _664 = _658 * 0.30000001192092896f;
         float _665 = _659 * 0.30000001192092896f;
         float _666 = _663 + 0.05000000074505806f;
         float _667 = _664 + 0.05000000074505806f;
         float _668 = _665 + 0.05000000074505806f;
         float _669 = _666 * _660;
         float _670 = _667 * _661;
         float _671 = _668 * _662;
         float _672 = _669 + 0.004000000189989805f;
         float _673 = _670 + 0.004000000189989805f;
         float _674 = _671 + 0.004000000189989805f;
         float _675 = _663 + 0.5f;
         float _676 = _664 + 0.5f;
         float _677 = _665 + 0.5f;
         float _678 = _675 * _660;
         float _679 = _676 * _661;
         float _680 = _677 * _662;
         float _681 = _678 + 0.06000000238418579f;
         float _682 = _679 + 0.06000000238418579f;
         float _683 = _680 + 0.06000000238418579f;
         float _684 = _672 / _681;
         float _685 = _673 / _682;
         float _686 = _674 / _683;
         float _687 = _684 + -0.06666666269302368f;
         float _688 = _685 + -0.06666666269302368f;
         float _689 = _686 + -0.06666666269302368f;
         float _690 = _687 * 1.3790643215179443f;
         float _691 = _688 * 1.3790643215179443f;
         float _692 = _689 * 1.3790643215179443f;
         float _693 = YKR_MaterialCurve(_690, _657);
         float _694 = YKR_MaterialCurve(_691, _658);
         float _695 = YKR_MaterialCurve(_692, _659);
         float _696 = sqrt(_693);
         float _697 = sqrt(_694);
         float _698 = sqrt(_695);
         _700 = _696;
         _701 = _697;
         _702 = _698;
      }
      else
      {
         _700 = _619;
         _701 = _620;
         _702 = _621;
      }
   }
   int _703 = c_bool & 4;
   bool _704 = (_703 == 0);
   if (_704)
   {
      float _706 = saturate(COLOR_1.x);
      float _711 = (c_fog_param[0].x) - _700;
      float _712 = (c_fog_param[0].y) - _701;
      float _713 = (c_fog_param[0].z) - _702;
      float _714 = _711 * _706;
      float _715 = _712 * _706;
      float _716 = _713 * _706;
      _718 = _714;
      _719 = _715;
      _720 = _716;
   }
   else
   {
      _718 = COLOR_1.x;
      _719 = COLOR_1.y;
      _720 = COLOR_1.z;
   }
   float _721 = _718 + _700;
   float _722 = _719 + _701;
   float _723 = _720 + _702;
   float _729 = (c_ps_col_scl[0].x) * _721;
   float _730 = (c_ps_col_scl[0].y) * _722;
   float _731 = (c_ps_col_scl[0].z) * _723;
   float _732 = (c_ps_col_scl[0].w) * _630;
   SV_Target.x = _729;
   SV_Target.y = _730;
   SV_Target.z = _731;
   SV_Target.w = _732;
   int _735 = c_bool & 8;
   bool _736 = (_735 != 0);
   float _737 = select(_736, _721, _729);
   float _738 = select(_736, _722, _730);
   float _739 = select(_736, _723, _731);
   float _740 = select(_736, _630, _732);
   SV_Target.x = _737;
   SV_Target.y = _738;
   SV_Target.z = _739;
   SV_Target.w = _740;
   return SV_Target;
}
