// Yakuza Kiwami material r_o1dut_b(vr)2du (0x1C967C1E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s4_s : register(s4);

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
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float _63 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _64 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _65 = s3_t.Sample(s3_s, float2(_63, _64));
   float _70 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _72 = s5_t.Sample(s5_s, float2(_70, _71));
   float4 _75 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _80 = _75.x * _75.x;
   float _81 = _75.y * _75.y;
   float _82 = _75.z * _75.z;
   float _86 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _87 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _88 = s4_t.Sample(s4_s, float2(_86, _87));
   float _90 = _75.w * COLOR_1.w;
   float _91 = _88.y - _65.y;
   float _92 = _91 * _90;
   float _93 = _92 + _65.y;
   float _94 = _80 - _57;
   float _95 = _81 - _58;
   float _96 = _82 - _59;
   float _97 = _94 * _90;
   float _98 = _95 * _90;
   float _99 = _96 * _90;
   float _100 = _97 + _57;
   float _101 = _98 + _58;
   float _102 = _99 + _59;
   float _103 = _72.x * 2.0f;
   float _104 = _72.y * 2.0f;
   float _105 = _103 + -1.0f;
   float _106 = _104 + -1.0f;
   float _107 = dot(float2(_105, _106), float2(_105, _106));
   float _108 = 1.0f - _107;
   float _109 = saturate(_108);
   float _110 = sqrt(_109);
   float _111 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _112 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _113 = _111 - _112;
   float _114 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _115 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _116 = _114 - _115;
   float _117 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _118 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _119 = _117 - _118;
   float _120 = _105 * TEXCOORD_6.x;
   float _121 = _105 * TEXCOORD_6.y;
   float _122 = _105 * TEXCOORD_6.z;
   float _123 = _106 * TEXCOORD_6.w;
   float _124 = _123 * _113;
   float _125 = _123 * _116;
   float _126 = _123 * _119;
   float _127 = _124 + _120;
   float _128 = _125 + _121;
   float _129 = _126 + _122;
   float _130 = _110 * TEXCOORD_2.x;
   float _131 = _110 * TEXCOORD_2.y;
   float _132 = _110 * TEXCOORD_2.z;
   float _133 = _127 + _130;
   float _134 = _128 + _131;
   float _135 = _129 + _132;
   float _136 = dot(float3(_133, _134, _135), float3(_133, _134, _135));
   float _137 = rsqrt(_136);
   float _138 = _133 * _137;
   float _139 = _134 * _137;
   float _140 = _135 * _137;
   float _141 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _142 = rsqrt(_141);
   float _143 = _142 * TEXCOORD_3.x;
   float _144 = _142 * TEXCOORD_3.y;
   float _145 = _142 * TEXCOORD_3.z;
   float _150 = -0.0f - (c_light_vec[0].x);
   float _151 = -0.0f - (c_light_vec[0].y);
   float _152 = -0.0f - (c_light_vec[0].z);
   float _153 = dot(float3(_138, _139, _140), float3(_150, _151, _152));
   float _157 = c_rt_size.z * SV_Position.x;
   float _158 = c_rt_size.w * SV_Position.y;
   int _161 = c_bool & 8;
   bool _162 = (_161 == 0);
   float _169;
   float _170;
   float _171;
   float _183;
   float _184;
   float _220;
   float _221;
   float _222;
   float _223;
   float _237;
   float _238;
   float _273;
   float _274;
   float _275;
   float _309;
   float _310;
   float _311;
   float _312;
   float _313;
   float _314;
   float _468;
   float _469;
   float _470;
   float _471;
   float _472;
   float _473;
   float _645;
   float _646;
   float _647;
   float _648;
   float _649;
   float _650;
   float _726;
   float _727;
   float _728;
   float _744;
   float _745;
   float _746;
   if (!_162)
   {
      float4 _164 = s13_t.Sample(s13_s, float2(_157, _158));
      _169 = _164.x;
      _170 = _164.y;
      _171 = _164.w;
   }
   else
   {
      _169 = 1.0f;
      _170 = 1.0f;
      _171 = 1.0f;
   }
   if (!_162)
   {
      float _176 = (c_sdw_param[0].x) * _169;
      float _177 = (c_sdw_param[0].z) * _170;
      float _180 = _176 + (c_sdw_param[0].y);
      float _181 = _177 + (c_sdw_param[0].w);
      _183 = _180;
      _184 = _181;
   }
   else
   {
      _183 = _169;
      _184 = _170;
   }
   float _188 = c_rt_size.z * SV_Position.x;
   float _189 = c_rt_size.w * SV_Position.y;
   float4 _190 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_188, _189)));
   int _197 = c_bool & 16384;
   bool _198 = (_197 == 0);
   if (!_198)
   {
      float _200 = _190.x * 2.0f;
      float _201 = _190.y * 2.0f;
      float _202 = _190.z * 2.0f;
      float _203 = _190.w * 2.0f;
      _220 = _200;
      _221 = _201;
      _222 = _202;
      _223 = _203;
   }
   else
   {
      int _205 = c_bool & 8192;
      bool _206 = (_205 == 0);
      if (!_206)
      {
         float _208 = _190.x * 2.0f;
         float _209 = _190.y * 2.0f;
         float _210 = _190.z * 2.0f;
         float _211 = _190.w * 2.0f;
         _220 = _208;
         _221 = _209;
         _222 = _210;
         _223 = _211;
      }
      else
      {
         float _215 = _190.x / c_tone_control.z;
         float _216 = _190.y / c_tone_control.z;
         float _217 = _190.z / c_tone_control.z;
         float _218 = _190.w / c_tone_control.z;
         _220 = _215;
         _221 = _216;
         _222 = _217;
         _223 = _218;
      }
   }
   int _224 = c_bool & 8;
   bool _225 = (_224 == 0);
   if (!_225)
   {
      float _230 = (c_sdw_param[0].x) * _183;
      float _231 = (c_sdw_param[0].z) * _184;
      float _234 = _230 + (c_sdw_param[0].y);
      float _235 = _231 + (c_sdw_param[0].w);
      _237 = _234;
      _238 = _235;
   }
   else
   {
      _237 = _183;
      _238 = _184;
   }
   float _239 = _238 * _237;
   float _244 = 1.0f - (c_light_col[5].x);
   float _245 = 1.0f - (c_light_col[5].y);
   float _246 = 1.0f - (c_light_col[5].z);
   float _247 = _244 * _239;
   float _248 = _245 * _239;
   float _249 = _246 * _239;
   float _250 = _247 + (c_light_col[5].x);
   float _251 = _248 + (c_light_col[5].y);
   float _252 = _249 + (c_light_col[5].z);
   float _253 = _171 * COLOR.x;
   float _254 = _171 * COLOR.y;
   float _255 = _171 * COLOR.z;
   if (!_225)
   {
      float _259 = (c_sdw_param[2].x) * _250;
      float _260 = (c_sdw_param[2].x) * _251;
      float _261 = (c_sdw_param[2].x) * _252;
      float _263 = _259 + (c_sdw_param[2].y);
      float _264 = _260 + (c_sdw_param[2].y);
      float _265 = _261 + (c_sdw_param[2].y);
      float _268 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _269 = _263 * _268;
      float _270 = _264 * _268;
      float _271 = _265 * _268;
      _273 = _269;
      _274 = _270;
      _275 = _271;
   }
   else
   {
      _273 = _250;
      _274 = _251;
      _275 = _252;
   }
   float _276 = _273 * _171;
   float _277 = _274 * _171;
   float _278 = _275 * _171;
   float _289 = c_mat_emissive.x * _100;
   float _290 = _289 * c_emissive_scale;
   float _291 = c_mat_emissive.y * _101;
   float _292 = _291 * c_emissive_scale;
   float _293 = c_mat_emissive.z * _102;
   float _294 = _293 * c_emissive_scale;
   int _295 = c_bool & 2048;
   bool _296 = (_295 == 0);
   if (_296)
   {
      float _298 = _238 * _220;
      float _299 = _238 * _221;
      float _300 = _238 * _222;
      float _301 = _298 + TEXCOORD_4.x;
      float _302 = _299 + TEXCOORD_4.y;
      float _303 = _300 + TEXCOORD_4.z;
      float _304 = _238 * _223;
      float _305 = _304 + TEXCOORD_5.x;
      float _306 = _304 + TEXCOORD_5.y;
      float _307 = _304 + TEXCOORD_5.z;
      _309 = _301;
      _310 = _302;
      _311 = _303;
      _312 = _305;
      _313 = _306;
      _314 = _307;
   }
   else
   {
      _309 = TEXCOORD_4.x;
      _310 = TEXCOORD_4.y;
      _311 = TEXCOORD_4.z;
      _312 = TEXCOORD_5.x;
      _313 = TEXCOORD_5.y;
      _314 = TEXCOORD_5.z;
   }
   float _319 = -0.0f - (c_light_vec[1].x);
   float _320 = -0.0f - (c_light_vec[1].y);
   float _321 = -0.0f - (c_light_vec[1].z);
   float _322 = dot(float3(_138, _139, _140), float3(_319, _320, _321));
   float _323 = saturate(_322);
   float _328 = saturate(_153);
   float _329 = (c_light_col[0].x) * _328;
   float _330 = (c_light_col[0].y) * _328;
   float _331 = (c_light_col[0].z) * _328;
   float _336 = (c_light_col[1].x) * _323;
   float _337 = (c_light_col[1].y) * _323;
   float _338 = (c_light_col[1].z) * _323;
   float _347 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _348 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _349 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _350 = _329 * _250;
   float _351 = _330 * _251;
   float _352 = _331 * _252;
   float _353 = _336 * _238;
   float _354 = _337 * _238;
   float _355 = _338 * _238;
   float _356 = _353 + _350;
   float _357 = _354 + _351;
   float _358 = _355 + _352;
   float _359 = _347 * _356;
   float _360 = _348 * _357;
   float _361 = _349 * _358;
   float _362 = _290 + c_scene_ambient.x;
   float _363 = _362 + _359;
   float _364 = _292 + c_scene_ambient.y;
   float _365 = _364 + _360;
   float _366 = _294 + c_scene_ambient.z;
   float _367 = _366 + _361;
   float _368 = _363 * _253;
   float _369 = _365 * _254;
   float _370 = _367 * _255;
   float _371 = _309 * _276;
   float _372 = _310 * _277;
   float _373 = _311 * _278;
   float _374 = _368 + _371;
   float _375 = _369 + _372;
   float _376 = _370 + _373;
   float _381 = (c_light_vec[0].x) + _143;
   float _382 = (c_light_vec[0].y) + _144;
   float _383 = (c_light_vec[0].z) + _145;
   float _384 = dot(float3(_381, _382, _383), float3(_381, _382, _383));
   float _385 = rsqrt(_384);
   float _386 = _381 * _385;
   float _387 = -0.0f - _386;
   float _388 = _382 * _385;
   float _389 = -0.0f - _388;
   float _390 = _383 * _385;
   float _391 = -0.0f - _390;
   float _392 = dot(float3(_387, _389, _391), float3(_138, _139, _140));
   float _393 = saturate(_392);
   float _394 = _144 + -0.30000001192092896f;
   float _395 = dot(float3(_143, _394, _145), float3(_143, _394, _145));
   float _396 = rsqrt(_395);
   float _397 = _143 * _396;
   float _398 = -0.0f - _397;
   float _399 = _394 * _396;
   float _400 = -0.0f - _399;
   float _401 = _145 * _396;
   float _402 = -0.0f - _401;
   float _403 = dot(float3(_398, _400, _402), float3(_138, _139, _140));
   float _404 = saturate(_403);
   float _407 = log2(_393);
   float _408 = log2(_404);
   float _409 = _407 * c_mat_power;
   float _410 = _408 * c_mat_power;
   float _411 = exp2(_409);
   float _412 = exp2(_410);
   float _415 = _411 * (c_light_col[0].x);
   float _416 = _411 * (c_light_col[0].y);
   float _417 = _411 * (c_light_col[0].z);
   float _418 = _412 * 2.0f;
   float _419 = _418 * (c_light_vec[5].x);
   float _420 = _419 * (c_light_col[1].x);
   float _421 = _419 * (c_light_col[1].y);
   float _422 = _419 * (c_light_col[1].z);
   float _423 = _415 * _250;
   float _424 = _416 * _251;
   float _425 = _417 * _252;
   float _426 = _420 * _323;
   float _427 = _421 * _323;
   float _428 = _422 * _323;
   float _429 = _426 + _423;
   float _430 = _427 + _424;
   float _431 = _428 + _425;
   float _438 = c_mat_specular.x * c_specular_scale.x;
   float _439 = _438 * _253;
   float _440 = _439 * _429;
   float _441 = c_mat_specular.y * c_specular_scale.y;
   float _442 = _441 * _254;
   float _443 = _442 * _430;
   float _444 = c_mat_specular.z * c_specular_scale.z;
   float _445 = _444 * _255;
   float _446 = _445 * _431;
   float _447 = _312 * _276;
   float _448 = _447 * _412;
   float _449 = _313 * _277;
   float _450 = _449 * _412;
   float _451 = _314 * _278;
   float _452 = _451 * _412;
   float _453 = _440 + _448;
   float _454 = _443 + _450;
   float _455 = _446 + _452;
   if (!_296)
   {
      float _457 = _238 * _220;
      float _458 = _238 * _221;
      float _459 = _238 * _222;
      float _460 = _374 + _457;
      float _461 = _375 + _458;
      float _462 = _376 + _459;
      float _463 = _238 * _223;
      float _464 = _453 + _463;
      float _465 = _454 + _463;
      float _466 = _455 + _463;
      _468 = _460;
      _469 = _461;
      _470 = _462;
      _471 = _464;
      _472 = _465;
      _473 = _466;
   }
   else
   {
      _468 = _374;
      _469 = _375;
      _470 = _376;
      _471 = _453;
      _472 = _454;
      _473 = _455;
   }
   int _474 = c_bool & 128;
   bool _475 = (_474 != 0);
   float _476 = select(_475, 0.0f, _471);
   float _477 = select(_475, 0.0f, _472);
   float _478 = select(_475, 0.0f, _473);
   float _479 = _468 * _100;
   float _480 = _469 * _101;
   float _481 = _470 * _102;
   float _482 = _476 * _93;
   float _483 = _477 * _93;
   float _484 = _478 * _93;
   int _485 = c_bool & 32768;
   bool _486 = (_485 == 0);
   if (!_486)
   {
      float _490 = (c_ps_user[11].x) * _100;
      float _491 = (c_ps_user[11].x) * _101;
      float _492 = (c_ps_user[11].x) * _102;
      float _494 = _138 + 1.0f;
      float _495 = _139 + 1.0f;
      float _496 = _140 + 1.0f;
      float _497 = _494 * 0.5f;
      float _498 = _495 * 0.5f;
      float _499 = _496 * 0.5f;
      float _500 = saturate(_497);
      float _501 = saturate(_498);
      float _502 = saturate(_499);
      float _503 = (c_ps_user[11].y) * _500;
      float _504 = _501 * (c_ps_user[11].y);
      float _505 = _502 * (c_ps_user[11].y);
      float _507 = (c_ps_user[11].z) * _93;
      float _509 = c_mat_power * 0.009999999776482582f;
      float _510 = _509 * (c_ps_user[11].w);
      float _513 = (c_ps_user[12].x) * _250;
      float _514 = (c_ps_user[12].x) * _251;
      float _515 = (c_ps_user[12].x) * _252;
      float _517 = (c_ps_user[12].y) * _253;
      float _518 = (c_ps_user[12].y) * _254;
      float _519 = (c_ps_user[12].y) * _255;
      float _521 = (c_ps_user[12].z) * _276;
      float _522 = (c_ps_user[12].z) * _277;
      float _523 = (c_ps_user[12].z) * _278;
      float _529 = -0.0f - _143;
      float _530 = -0.0f - _144;
      float _531 = -0.0f - _145;
      float _532 = dot(float3(_529, _530, _531), float3(_138, _139, _140));
      float _533 = 1.0f - _532;
      float _534 = _533 * _533;
      float _535 = _533 * 0.9722222089767456f;
      float _536 = _534 * _534;
      float _537 = _536 * _535;
      float _538 = _537 + 0.02777777425944805f;
      float _539 = _538 * (c_ps_user[13].z);
      float _541 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _542 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _543 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _546 = (c_ps_user[14].x) * _290;
      float _547 = (c_ps_user[14].x) * _292;
      float _548 = (c_ps_user[14].x) * _294;
      float _550 = (c_ps_user[14].y) * _329;
      float _551 = (c_ps_user[14].y) * _330;
      float _552 = (c_ps_user[14].y) * _331;
      float _554 = (c_ps_user[14].z) * _336;
      float _555 = (c_ps_user[14].z) * _337;
      float _556 = (c_ps_user[14].z) * _338;
      float _558 = (c_ps_user[14].w) * _309;
      float _559 = (c_ps_user[14].w) * _310;
      float _560 = (c_ps_user[14].w) * _311;
      float _563 = (c_ps_user[15].x) * _415;
      float _564 = (c_ps_user[15].x) * _416;
      float _565 = (c_ps_user[15].x) * _417;
      float _567 = _438 * _420;
      float _568 = _567 * (c_ps_user[15].y);
      float _569 = _441 * _421;
      float _570 = _569 * (c_ps_user[15].y);
      float _571 = _444 * _422;
      float _572 = _571 * (c_ps_user[15].y);
      float _574 = (c_ps_user[15].z) * _312;
      float _575 = (c_ps_user[15].z) * _313;
      float _576 = (c_ps_user[15].z) * _314;
      float _577 = _568 + _574;
      float _578 = _577 * _412;
      float _579 = _550 + (c_ps_user[13].x);
      float _580 = _579 + _554;
      float _581 = c_diffuse_scale.x * _580;
      float _582 = _581 * c_mat_diffuse.x;
      float _583 = (c_ps_user[13].y) + _507;
      float _584 = _583 + _563;
      float _585 = c_mat_specular.x * _584;
      float _586 = _585 * c_specular_scale.x;
      float _587 = _503 + _490;
      float _588 = _587 + _510;
      float _589 = _588 + (c_ps_user[12].w);
      float _590 = _589 + _513;
      float _591 = _590 + _517;
      float _592 = _591 + _521;
      float _593 = _592 + _541;
      float _594 = _593 + _546;
      float _595 = _594 + _558;
      float _596 = _595 + _539;
      float _597 = _596 + _578;
      float _598 = _597 + _582;
      float _599 = _598 + _586;
      float _600 = _570 + _575;
      float _601 = _600 * _412;
      float _602 = _551 + (c_ps_user[13].x);
      float _603 = _602 + _555;
      float _604 = c_diffuse_scale.y * _603;
      float _605 = _604 * c_mat_diffuse.y;
      float _606 = _583 + _564;
      float _607 = c_mat_specular.y * _606;
      float _608 = _607 * c_specular_scale.y;
      float _609 = _504 + _491;
      float _610 = _609 + _510;
      float _611 = _610 + (c_ps_user[12].w);
      float _612 = _611 + _514;
      float _613 = _612 + _518;
      float _614 = _613 + _522;
      float _615 = _614 + _542;
      float _616 = _615 + _547;
      float _617 = _616 + _559;
      float _618 = _617 + _539;
      float _619 = _618 + _601;
      float _620 = _619 + _605;
      float _621 = _620 + _608;
      float _622 = _572 + _576;
      float _623 = _622 * _412;
      float _624 = _552 + (c_ps_user[13].x);
      float _625 = _624 + _556;
      float _626 = c_diffuse_scale.z * _625;
      float _627 = _626 * c_mat_diffuse.z;
      float _628 = _583 + _565;
      float _629 = c_mat_specular.z * _628;
      float _630 = _629 * c_specular_scale.z;
      float _631 = _510 + _492;
      float _632 = _631 + _505;
      float _633 = _632 + (c_ps_user[12].w);
      float _634 = _633 + _515;
      float _635 = _634 + _519;
      float _636 = _635 + _523;
      float _637 = _636 + _543;
      float _638 = _637 + _548;
      float _639 = _638 + _560;
      float _640 = _639 + _539;
      float _641 = _640 + _623;
      float _642 = _641 + _627;
      float _643 = _642 + _630;
      _645 = _599;
      _646 = _621;
      _647 = _643;
      _648 = 0.0f;
      _649 = 0.0f;
      _650 = 0.0f;
   }
   else
   {
      _645 = _479;
      _646 = _480;
      _647 = _481;
      _648 = _482;
      _649 = _483;
      _650 = _484;
   }
   float _651 = _648 + _645;
   float _652 = _649 + _646;
   float _653 = _650 + _647;
   float _658 = c_mat_opacity * _52.w;
   float _659 = _658 * c_global_opacity;
   float _660 = _659 * c_opacity_scale;
   int _661 = c_bool & 16;
   bool _662 = (_661 == 0);
   if (!_662)
   {
      float _666 = c_tone_control.x * _651;
      float _667 = c_tone_control.x * _652;
      float _668 = c_tone_control.x * _653;
      float _670 = log2(_666);
      float _671 = log2(_667);
      float _672 = log2(_668);
      float _673 = _670 * c_tone_control.y;
      float _674 = _671 * c_tone_control.y;
      float _675 = _672 * c_tone_control.y;
      float _676 = exp2(_673);
      float _677 = exp2(_674);
      float _678 = exp2(_675);
      _726 = _676;
      _727 = _677;
      _728 = _678;
   }
   else
   {
      if (_486)
      {
         float _683 = c_tone_control.x * _651;
         float _684 = c_tone_control.x * _652;
         float _685 = c_tone_control.x * _653;
         float _686 = _683 * 2.0f;
         float _687 = _684 * 2.0f;
         float _688 = _685 * 2.0f;
         float _689 = _683 * 0.30000001192092896f;
         float _690 = _684 * 0.30000001192092896f;
         float _691 = _685 * 0.30000001192092896f;
         float _692 = _689 + 0.05000000074505806f;
         float _693 = _690 + 0.05000000074505806f;
         float _694 = _691 + 0.05000000074505806f;
         float _695 = _692 * _686;
         float _696 = _693 * _687;
         float _697 = _694 * _688;
         float _698 = _695 + 0.004000000189989805f;
         float _699 = _696 + 0.004000000189989805f;
         float _700 = _697 + 0.004000000189989805f;
         float _701 = _689 + 0.5f;
         float _702 = _690 + 0.5f;
         float _703 = _691 + 0.5f;
         float _704 = _701 * _686;
         float _705 = _702 * _687;
         float _706 = _703 * _688;
         float _707 = _704 + 0.06000000238418579f;
         float _708 = _705 + 0.06000000238418579f;
         float _709 = _706 + 0.06000000238418579f;
         float _710 = _698 / _707;
         float _711 = _699 / _708;
         float _712 = _700 / _709;
         float _713 = _710 + -0.06666666269302368f;
         float _714 = _711 + -0.06666666269302368f;
         float _715 = _712 + -0.06666666269302368f;
         float _716 = _713 * 1.3790643215179443f;
         float _717 = _714 * 1.3790643215179443f;
         float _718 = _715 * 1.3790643215179443f;
         float _719 = YKR_MaterialCurve(_716, _683);
         float _720 = YKR_MaterialCurve(_717, _684);
         float _721 = YKR_MaterialCurve(_718, _685);
         float _722 = sqrt(_719);
         float _723 = sqrt(_720);
         float _724 = sqrt(_721);
         _726 = _722;
         _727 = _723;
         _728 = _724;
      }
      else
      {
         _726 = _651;
         _727 = _652;
         _728 = _653;
      }
   }
   int _729 = c_bool & 4;
   bool _730 = (_729 == 0);
   if (_730)
   {
      float _732 = saturate(COLOR_1.x);
      float _737 = (c_fog_param[0].x) - _726;
      float _738 = (c_fog_param[0].y) - _727;
      float _739 = (c_fog_param[0].z) - _728;
      float _740 = _737 * _732;
      float _741 = _738 * _732;
      float _742 = _739 * _732;
      _744 = _740;
      _745 = _741;
      _746 = _742;
   }
   else
   {
      _744 = COLOR_1.x;
      _745 = COLOR_1.y;
      _746 = COLOR_1.z;
   }
   float _747 = _744 + _726;
   float _748 = _745 + _727;
   float _749 = _746 + _728;
   float _755 = (c_ps_col_scl[0].x) * _747;
   float _756 = (c_ps_col_scl[0].y) * _748;
   float _757 = (c_ps_col_scl[0].z) * _749;
   float _758 = (c_ps_col_scl[0].w) * _660;
   SV_Target.x = _755;
   SV_Target.y = _756;
   SV_Target.z = _757;
   SV_Target.w = _758;
   int _761 = c_bool & 8;
   bool _762 = (_761 != 0);
   float _763 = select(_762, _747, _755);
   float _764 = select(_762, _748, _756);
   float _765 = select(_762, _749, _757);
   float _766 = select(_762, _660, _758);
   SV_Target.x = _763;
   SV_Target.y = _764;
   SV_Target.z = _765;
   SV_Target.w = _766;
   return SV_Target;
}
