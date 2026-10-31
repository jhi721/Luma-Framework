// Yakuza Kiwami material r_o1ds_a(vr)2d_m4d (0xDCF21E2B, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

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
   float4 _48 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _53 = _48.x * _48.x;
   float _54 = _48.y * _48.y;
   float _55 = _48.z * _48.z;
   float _59 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _60 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _61 = s3_t.Sample(s3_s, float2(_59, _60));
   float4 _65 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _69 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _72 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _73 = rsqrt(_72);
   float _74 = _73 * TEXCOORD_2.x;
   float _75 = _73 * TEXCOORD_2.y;
   float _76 = _73 * TEXCOORD_2.z;
   float _77 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _78 = rsqrt(_77);
   float _79 = _78 * TEXCOORD_3.x;
   float _80 = _78 * TEXCOORD_3.y;
   float _81 = _78 * TEXCOORD_3.z;
   float _86 = -0.0f - (c_light_vec[0].x);
   float _87 = -0.0f - (c_light_vec[0].y);
   float _88 = -0.0f - (c_light_vec[0].z);
   float _89 = dot(float3(_74, _75, _76), float3(_86, _87, _88));
   float _93 = c_rt_size.z * SV_Position.x;
   float _94 = c_rt_size.w * SV_Position.y;
   int _97 = c_bool & 8;
   bool _98 = (_97 == 0);
   float _105;
   float _106;
   float _107;
   float _119;
   float _120;
   float _175;
   float _176;
   float _177;
   float _178;
   float _192;
   float _193;
   float _239;
   float _240;
   float _241;
   float _276;
   float _277;
   float _278;
   float _279;
   float _280;
   float _281;
   float _435;
   float _436;
   float _437;
   float _438;
   float _439;
   float _440;
   float _616;
   float _617;
   float _618;
   float _619;
   float _620;
   float _621;
   float _706;
   float _707;
   float _708;
   float _724;
   float _725;
   float _726;
   if (!_98)
   {
      float4 _100 = s13_t.Sample(s13_s, float2(_93, _94));
      _105 = _100.x;
      _106 = _100.y;
      _107 = _100.w;
   }
   else
   {
      _105 = 1.0f;
      _106 = 1.0f;
      _107 = 1.0f;
   }
   if (!_98)
   {
      float _112 = (c_sdw_param[0].x) * _105;
      float _113 = (c_sdw_param[0].z) * _106;
      float _116 = _112 + (c_sdw_param[0].y);
      float _117 = _113 + (c_sdw_param[0].w);
      _119 = _116;
      _120 = _117;
   }
   else
   {
      _119 = _105;
      _120 = _106;
   }
   float _127 = (c_sdw_param[1].x) * _69.y;
   float _129 = _127 + (c_sdw_param[1].y);
   float _130 = _129 * _69.x;
   float _131 = 1.0f - (c_light_col[5].x);
   float _132 = 1.0f - (c_light_col[5].y);
   float _133 = 1.0f - (c_light_col[5].z);
   float _134 = _130 * _131;
   float _135 = _130 * _132;
   float _136 = _130 * _133;
   float _137 = _134 + (c_light_col[5].x);
   float _138 = _135 + (c_light_col[5].y);
   float _139 = _136 + (c_light_col[5].z);
   float _143 = c_rt_size.z * SV_Position.x;
   float _144 = c_rt_size.w * SV_Position.y;
   float4 _145 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_143, _144)));
   int _152 = c_bool & 16384;
   bool _153 = (_152 == 0);
   if (!_153)
   {
      float _155 = _145.x * 2.0f;
      float _156 = _145.y * 2.0f;
      float _157 = _145.z * 2.0f;
      float _158 = _145.w * 2.0f;
      _175 = _155;
      _176 = _156;
      _177 = _157;
      _178 = _158;
   }
   else
   {
      int _160 = c_bool & 8192;
      bool _161 = (_160 == 0);
      if (!_161)
      {
         float _163 = _145.x * 2.0f;
         float _164 = _145.y * 2.0f;
         float _165 = _145.z * 2.0f;
         float _166 = _145.w * 2.0f;
         _175 = _163;
         _176 = _164;
         _177 = _165;
         _178 = _166;
      }
      else
      {
         float _170 = _145.x / c_tone_control.z;
         float _171 = _145.y / c_tone_control.z;
         float _172 = _145.z / c_tone_control.z;
         float _173 = _145.w / c_tone_control.z;
         _175 = _170;
         _176 = _171;
         _177 = _172;
         _178 = _173;
      }
   }
   int _179 = c_bool & 8;
   bool _180 = (_179 == 0);
   if (!_180)
   {
      float _185 = (c_sdw_param[0].x) * _119;
      float _186 = (c_sdw_param[0].z) * _120;
      float _189 = _185 + (c_sdw_param[0].y);
      float _190 = _186 + (c_sdw_param[0].w);
      _192 = _189;
      _193 = _190;
   }
   else
   {
      _192 = _119;
      _193 = _120;
   }
   float _194 = _193 * _192;
   float _199 = 1.0f - (c_light_col[5].x);
   float _200 = 1.0f - (c_light_col[5].y);
   float _201 = 1.0f - (c_light_col[5].z);
   float _202 = _199 * _194;
   float _203 = _200 * _194;
   float _204 = _201 * _194;
   float _205 = _202 + (c_light_col[5].x);
   float _206 = _203 + (c_light_col[5].y);
   float _207 = _204 + (c_light_col[5].z);
   float _208 = _107 * COLOR.x;
   float _209 = _208 * _137;
   float _210 = _107 * COLOR.y;
   float _211 = _210 * _138;
   float _212 = _107 * COLOR.z;
   float _213 = _212 * _139;
   float _214 = _205 * _137;
   float _215 = _206 * _138;
   float _216 = _207 * _139;
   if (!_180)
   {
      float _220 = (c_sdw_param[2].x) * _205;
      float _221 = (c_sdw_param[2].x) * _206;
      float _222 = (c_sdw_param[2].x) * _207;
      float _224 = _220 + (c_sdw_param[2].y);
      float _225 = _221 + (c_sdw_param[2].y);
      float _226 = _222 + (c_sdw_param[2].y);
      float _228 = (c_sdw_param[2].z) * _137;
      float _229 = (c_sdw_param[2].z) * _138;
      float _230 = (c_sdw_param[2].z) * _139;
      float _232 = _228 + (c_sdw_param[2].w);
      float _233 = _229 + (c_sdw_param[2].w);
      float _234 = _230 + (c_sdw_param[2].w);
      float _235 = _232 * _224;
      float _236 = _233 * _225;
      float _237 = _234 * _226;
      _239 = _235;
      _240 = _236;
      _241 = _237;
   }
   else
   {
      _239 = _214;
      _240 = _215;
      _241 = _216;
   }
   float _242 = _107 * _69.x;
   float _243 = _239 * _242;
   float _244 = _240 * _242;
   float _245 = _241 * _242;
   float _256 = c_mat_emissive.x * _53;
   float _257 = _256 * c_emissive_scale;
   float _258 = c_mat_emissive.y * _54;
   float _259 = _258 * c_emissive_scale;
   float _260 = c_mat_emissive.z * _55;
   float _261 = _260 * c_emissive_scale;
   int _262 = c_bool & 2048;
   bool _263 = (_262 == 0);
   if (_263)
   {
      float _265 = _193 * _175;
      float _266 = _193 * _176;
      float _267 = _193 * _177;
      float _268 = _265 + TEXCOORD_4.x;
      float _269 = _266 + TEXCOORD_4.y;
      float _270 = _267 + TEXCOORD_4.z;
      float _271 = _193 * _178;
      float _272 = _271 + TEXCOORD_5.x;
      float _273 = _271 + TEXCOORD_5.y;
      float _274 = _271 + TEXCOORD_5.z;
      _276 = _268;
      _277 = _269;
      _278 = _270;
      _279 = _272;
      _280 = _273;
      _281 = _274;
   }
   else
   {
      _276 = TEXCOORD_4.x;
      _277 = TEXCOORD_4.y;
      _278 = TEXCOORD_4.z;
      _279 = TEXCOORD_5.x;
      _280 = TEXCOORD_5.y;
      _281 = TEXCOORD_5.z;
   }
   float _286 = -0.0f - (c_light_vec[1].x);
   float _287 = -0.0f - (c_light_vec[1].y);
   float _288 = -0.0f - (c_light_vec[1].z);
   float _289 = dot(float3(_74, _75, _76), float3(_286, _287, _288));
   float _290 = saturate(_289);
   float _295 = saturate(_89);
   float _296 = (c_light_col[0].x) * _295;
   float _297 = (c_light_col[0].y) * _295;
   float _298 = (c_light_col[0].z) * _295;
   float _303 = (c_light_col[1].x) * _290;
   float _304 = (c_light_col[1].y) * _290;
   float _305 = (c_light_col[1].z) * _290;
   float _314 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _315 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _316 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _317 = _296 * _205;
   float _318 = _297 * _206;
   float _319 = _298 * _207;
   float _320 = _303 * _193;
   float _321 = _304 * _193;
   float _322 = _305 * _193;
   float _323 = _320 + _317;
   float _324 = _321 + _318;
   float _325 = _322 + _319;
   float _326 = _314 * _323;
   float _327 = _315 * _324;
   float _328 = _316 * _325;
   float _329 = _257 + c_scene_ambient.x;
   float _330 = _329 + _326;
   float _331 = _259 + c_scene_ambient.y;
   float _332 = _331 + _327;
   float _333 = _261 + c_scene_ambient.z;
   float _334 = _333 + _328;
   float _335 = _330 * _209;
   float _336 = _332 * _211;
   float _337 = _334 * _213;
   float _338 = _276 * _243;
   float _339 = _277 * _244;
   float _340 = _278 * _245;
   float _341 = _335 + _338;
   float _342 = _336 + _339;
   float _343 = _337 + _340;
   float _348 = (c_light_vec[0].x) + _79;
   float _349 = (c_light_vec[0].y) + _80;
   float _350 = (c_light_vec[0].z) + _81;
   float _351 = dot(float3(_348, _349, _350), float3(_348, _349, _350));
   float _352 = rsqrt(_351);
   float _353 = _348 * _352;
   float _354 = -0.0f - _353;
   float _355 = _349 * _352;
   float _356 = -0.0f - _355;
   float _357 = _350 * _352;
   float _358 = -0.0f - _357;
   float _359 = dot(float3(_354, _356, _358), float3(_74, _75, _76));
   float _360 = saturate(_359);
   float _361 = _80 + -0.30000001192092896f;
   float _362 = dot(float3(_79, _361, _81), float3(_79, _361, _81));
   float _363 = rsqrt(_362);
   float _364 = _79 * _363;
   float _365 = -0.0f - _364;
   float _366 = _361 * _363;
   float _367 = -0.0f - _366;
   float _368 = _81 * _363;
   float _369 = -0.0f - _368;
   float _370 = dot(float3(_365, _367, _369), float3(_74, _75, _76));
   float _371 = saturate(_370);
   float _374 = log2(_360);
   float _375 = log2(_371);
   float _376 = _374 * c_mat_power;
   float _377 = _375 * c_mat_power;
   float _378 = exp2(_376);
   float _379 = exp2(_377);
   float _382 = _378 * (c_light_col[0].x);
   float _383 = _378 * (c_light_col[0].y);
   float _384 = _378 * (c_light_col[0].z);
   float _385 = _379 * 2.0f;
   float _386 = _385 * (c_light_vec[5].x);
   float _387 = _386 * (c_light_col[1].x);
   float _388 = _386 * (c_light_col[1].y);
   float _389 = _386 * (c_light_col[1].z);
   float _390 = _382 * _205;
   float _391 = _383 * _206;
   float _392 = _384 * _207;
   float _393 = _387 * _290;
   float _394 = _388 * _290;
   float _395 = _389 * _290;
   float _396 = _393 + _390;
   float _397 = _394 + _391;
   float _398 = _395 + _392;
   float _405 = c_mat_specular.x * c_specular_scale.x;
   float _406 = _405 * _209;
   float _407 = _406 * _396;
   float _408 = c_mat_specular.y * c_specular_scale.y;
   float _409 = _408 * _211;
   float _410 = _409 * _397;
   float _411 = c_mat_specular.z * c_specular_scale.z;
   float _412 = _411 * _213;
   float _413 = _412 * _398;
   float _414 = _279 * _243;
   float _415 = _414 * _379;
   float _416 = _280 * _244;
   float _417 = _416 * _379;
   float _418 = _281 * _245;
   float _419 = _418 * _379;
   float _420 = _407 + _415;
   float _421 = _410 + _417;
   float _422 = _413 + _419;
   if (!_263)
   {
      float _424 = _193 * _175;
      float _425 = _193 * _176;
      float _426 = _193 * _177;
      float _427 = _341 + _424;
      float _428 = _342 + _425;
      float _429 = _343 + _426;
      float _430 = _193 * _178;
      float _431 = _420 + _430;
      float _432 = _421 + _430;
      float _433 = _422 + _430;
      _435 = _427;
      _436 = _428;
      _437 = _429;
      _438 = _431;
      _439 = _432;
      _440 = _433;
   }
   else
   {
      _435 = _341;
      _436 = _342;
      _437 = _343;
      _438 = _420;
      _439 = _421;
      _440 = _422;
   }
   int _441 = c_bool & 128;
   bool _442 = (_441 != 0);
   float _443 = select(_442, 0.0f, _438);
   float _444 = select(_442, 0.0f, _439);
   float _445 = select(_442, 0.0f, _440);
   float _446 = _435 * _53;
   float _447 = _436 * _54;
   float _448 = _437 * _55;
   float _449 = _443 * _61.x;
   float _450 = _444 * _61.y;
   float _451 = _445 * _61.z;
   int _452 = c_bool & 32768;
   bool _453 = (_452 == 0);
   if (!_453)
   {
      float _457 = (c_ps_user[11].x) * _53;
      float _458 = (c_ps_user[11].x) * _54;
      float _459 = (c_ps_user[11].x) * _55;
      float _461 = _74 + 1.0f;
      float _462 = _75 + 1.0f;
      float _463 = _76 + 1.0f;
      float _464 = _461 * 0.5f;
      float _465 = _462 * 0.5f;
      float _466 = _463 * 0.5f;
      float _467 = saturate(_464);
      float _468 = saturate(_465);
      float _469 = saturate(_466);
      float _470 = (c_ps_user[11].y) * _467;
      float _471 = _468 * (c_ps_user[11].y);
      float _472 = _469 * (c_ps_user[11].y);
      float _474 = (c_ps_user[11].z) * _61.x;
      float _475 = (c_ps_user[11].z) * _61.y;
      float _476 = (c_ps_user[11].z) * _61.z;
      float _478 = c_mat_power * 0.009999999776482582f;
      float _479 = _478 * (c_ps_user[11].w);
      float _482 = (c_ps_user[12].x) * _205;
      float _483 = (c_ps_user[12].x) * _206;
      float _484 = (c_ps_user[12].x) * _207;
      float _486 = (c_ps_user[12].y) * _209;
      float _487 = (c_ps_user[12].y) * _211;
      float _488 = (c_ps_user[12].y) * _213;
      float _490 = (c_ps_user[12].z) * _243;
      float _491 = (c_ps_user[12].z) * _244;
      float _492 = (c_ps_user[12].z) * _245;
      float _498 = -0.0f - _79;
      float _499 = -0.0f - _80;
      float _500 = -0.0f - _81;
      float _501 = dot(float3(_498, _499, _500), float3(_74, _75, _76));
      float _502 = 1.0f - _501;
      float _503 = _502 * _502;
      float _504 = _502 * 0.9722222089767456f;
      float _505 = _503 * _503;
      float _506 = _505 * _504;
      float _507 = _506 + 0.02777777425944805f;
      float _508 = _507 * (c_ps_user[13].z);
      float _510 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _511 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _512 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _515 = (c_ps_user[14].x) * _257;
      float _516 = (c_ps_user[14].x) * _259;
      float _517 = (c_ps_user[14].x) * _261;
      float _519 = (c_ps_user[14].y) * _296;
      float _520 = (c_ps_user[14].y) * _297;
      float _521 = (c_ps_user[14].y) * _298;
      float _523 = (c_ps_user[14].z) * _303;
      float _524 = (c_ps_user[14].z) * _304;
      float _525 = (c_ps_user[14].z) * _305;
      float _527 = (c_ps_user[14].w) * _276;
      float _528 = (c_ps_user[14].w) * _277;
      float _529 = (c_ps_user[14].w) * _278;
      float _532 = (c_ps_user[15].x) * _382;
      float _533 = (c_ps_user[15].x) * _383;
      float _534 = (c_ps_user[15].x) * _384;
      float _536 = _405 * _387;
      float _537 = _536 * (c_ps_user[15].y);
      float _538 = _408 * _388;
      float _539 = _538 * (c_ps_user[15].y);
      float _540 = _411 * _389;
      float _541 = _540 * (c_ps_user[15].y);
      float _543 = (c_ps_user[15].z) * _279;
      float _544 = (c_ps_user[15].z) * _280;
      float _545 = (c_ps_user[15].z) * _281;
      float _546 = _537 + _543;
      float _547 = _546 * _379;
      float _548 = _519 + (c_ps_user[13].x);
      float _549 = _548 + _523;
      float _550 = c_diffuse_scale.x * _549;
      float _551 = _550 * c_mat_diffuse.x;
      float _552 = (c_ps_user[13].y) + _474;
      float _553 = _552 + _532;
      float _554 = c_mat_specular.x * _553;
      float _555 = _554 * c_specular_scale.x;
      float _556 = _470 + _457;
      float _557 = _556 + _479;
      float _558 = _557 + (c_ps_user[12].w);
      float _559 = _558 + _482;
      float _560 = _559 + _486;
      float _561 = _560 + _490;
      float _562 = _561 + _510;
      float _563 = _562 + _515;
      float _564 = _563 + _527;
      float _565 = _564 + _508;
      float _566 = _565 + _547;
      float _567 = _566 + _551;
      float _568 = _567 + _555;
      float _569 = _539 + _544;
      float _570 = _569 * _379;
      float _571 = _520 + (c_ps_user[13].x);
      float _572 = _571 + _524;
      float _573 = c_diffuse_scale.y * _572;
      float _574 = _573 * c_mat_diffuse.y;
      float _575 = (c_ps_user[13].y) + _475;
      float _576 = _575 + _533;
      float _577 = c_mat_specular.y * _576;
      float _578 = _577 * c_specular_scale.y;
      float _579 = _471 + _458;
      float _580 = _579 + _479;
      float _581 = _580 + (c_ps_user[12].w);
      float _582 = _581 + _483;
      float _583 = _582 + _487;
      float _584 = _583 + _491;
      float _585 = _584 + _511;
      float _586 = _585 + _516;
      float _587 = _586 + _528;
      float _588 = _587 + _508;
      float _589 = _588 + _570;
      float _590 = _589 + _574;
      float _591 = _590 + _578;
      float _592 = _541 + _545;
      float _593 = _592 * _379;
      float _594 = _521 + (c_ps_user[13].x);
      float _595 = _594 + _525;
      float _596 = c_diffuse_scale.z * _595;
      float _597 = _596 * c_mat_diffuse.z;
      float _598 = (c_ps_user[13].y) + _476;
      float _599 = _598 + _534;
      float _600 = c_mat_specular.z * _599;
      float _601 = _600 * c_specular_scale.z;
      float _602 = _479 + _459;
      float _603 = _602 + _472;
      float _604 = _603 + (c_ps_user[12].w);
      float _605 = _604 + _484;
      float _606 = _605 + _488;
      float _607 = _606 + _492;
      float _608 = _607 + _512;
      float _609 = _608 + _517;
      float _610 = _609 + _529;
      float _611 = _610 + _508;
      float _612 = _611 + _593;
      float _613 = _612 + _597;
      float _614 = _613 + _601;
      _616 = _568;
      _617 = _591;
      _618 = _614;
      _619 = 0.0f;
      _620 = 0.0f;
      _621 = 0.0f;
   }
   else
   {
      _616 = _446;
      _617 = _447;
      _618 = _448;
      _619 = _449;
      _620 = _450;
      _621 = _451;
   }
   float _622 = _65.x * _65.x;
   float _623 = _622 * COLOR_1.w;
   float _624 = _65.y * _65.y;
   float _625 = _624 * COLOR_1.w;
   float _626 = _65.z * _65.z;
   float _627 = _626 * COLOR_1.w;
   float _628 = _616 + _623;
   float _629 = _628 + _619;
   float _630 = _617 + _625;
   float _631 = _630 + _620;
   float _632 = _618 + _627;
   float _633 = _632 + _621;
   float _638 = c_mat_opacity * _48.w;
   float _639 = _638 * c_global_opacity;
   float _640 = _639 * c_opacity_scale;
   int _641 = c_bool & 16;
   bool _642 = (_641 == 0);
   if (!_642)
   {
      float _646 = c_tone_control.x * _629;
      float _647 = c_tone_control.x * _631;
      float _648 = c_tone_control.x * _633;
      float _650 = log2(_646);
      float _651 = log2(_647);
      float _652 = log2(_648);
      float _653 = _650 * c_tone_control.y;
      float _654 = _651 * c_tone_control.y;
      float _655 = _652 * c_tone_control.y;
      float _656 = exp2(_653);
      float _657 = exp2(_654);
      float _658 = exp2(_655);
      _706 = _656;
      _707 = _657;
      _708 = _658;
   }
   else
   {
      if (_453)
      {
         float _663 = c_tone_control.x * _629;
         float _664 = c_tone_control.x * _631;
         float _665 = c_tone_control.x * _633;
         float _666 = _663 * 2.0f;
         float _667 = _664 * 2.0f;
         float _668 = _665 * 2.0f;
         float _669 = _663 * 0.30000001192092896f;
         float _670 = _664 * 0.30000001192092896f;
         float _671 = _665 * 0.30000001192092896f;
         float _672 = _669 + 0.05000000074505806f;
         float _673 = _670 + 0.05000000074505806f;
         float _674 = _671 + 0.05000000074505806f;
         float _675 = _672 * _666;
         float _676 = _673 * _667;
         float _677 = _674 * _668;
         float _678 = _675 + 0.004000000189989805f;
         float _679 = _676 + 0.004000000189989805f;
         float _680 = _677 + 0.004000000189989805f;
         float _681 = _669 + 0.5f;
         float _682 = _670 + 0.5f;
         float _683 = _671 + 0.5f;
         float _684 = _681 * _666;
         float _685 = _682 * _667;
         float _686 = _683 * _668;
         float _687 = _684 + 0.06000000238418579f;
         float _688 = _685 + 0.06000000238418579f;
         float _689 = _686 + 0.06000000238418579f;
         float _690 = _678 / _687;
         float _691 = _679 / _688;
         float _692 = _680 / _689;
         float _693 = _690 + -0.06666666269302368f;
         float _694 = _691 + -0.06666666269302368f;
         float _695 = _692 + -0.06666666269302368f;
         float _696 = _693 * 1.3790643215179443f;
         float _697 = _694 * 1.3790643215179443f;
         float _698 = _695 * 1.3790643215179443f;
         float _699 = YKR_MaterialCurve(_696, _663);
         float _700 = YKR_MaterialCurve(_697, _664);
         float _701 = YKR_MaterialCurve(_698, _665);
         float _702 = sqrt(_699);
         float _703 = sqrt(_700);
         float _704 = sqrt(_701);
         _706 = _702;
         _707 = _703;
         _708 = _704;
      }
      else
      {
         _706 = _629;
         _707 = _631;
         _708 = _633;
      }
   }
   int _709 = c_bool & 4;
   bool _710 = (_709 == 0);
   if (_710)
   {
      float _712 = saturate(COLOR_1.x);
      float _717 = (c_fog_param[0].x) - _706;
      float _718 = (c_fog_param[0].y) - _707;
      float _719 = (c_fog_param[0].z) - _708;
      float _720 = _717 * _712;
      float _721 = _718 * _712;
      float _722 = _719 * _712;
      _724 = _720;
      _725 = _721;
      _726 = _722;
   }
   else
   {
      _724 = COLOR_1.x;
      _725 = COLOR_1.y;
      _726 = COLOR_1.z;
   }
   float _727 = _724 + _706;
   float _728 = _725 + _707;
   float _729 = _726 + _708;
   float _735 = (c_ps_col_scl[0].x) * _727;
   float _736 = (c_ps_col_scl[0].y) * _728;
   float _737 = (c_ps_col_scl[0].z) * _729;
   float _738 = (c_ps_col_scl[0].w) * _640;
   SV_Target.x = _735;
   SV_Target.y = _736;
   SV_Target.z = _737;
   SV_Target.w = _738;
   int _741 = c_bool & 8;
   bool _742 = (_741 != 0);
   float _743 = select(_742, _727, _735);
   float _744 = select(_742, _728, _736);
   float _745 = select(_742, _729, _737);
   float _746 = select(_742, _640, _738);
   SV_Target.x = _743;
   SV_Target.y = _744;
   SV_Target.z = _745;
   SV_Target.w = _746;
   return SV_Target;
}
