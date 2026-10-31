// Yakuza Kiwami material r_o1du_b(vr)2du_m4d (0xD9F3B7DB, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _65 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _70 = _65.x * _65.x;
   float _71 = _65.y * _65.y;
   float _72 = _65.z * _65.z;
   float _76 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _77 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _78 = s4_t.Sample(s4_s, float2(_76, _77));
   float4 _80 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _83 = _65.w * COLOR_1.w;
   float _84 = _78.y - _63.y;
   float _85 = _84 * _83;
   float _86 = _85 + _63.y;
   float _87 = _70 - _55;
   float _88 = _71 - _56;
   float _89 = _72 - _57;
   float _90 = _87 * _83;
   float _91 = _88 * _83;
   float _92 = _89 * _83;
   float _93 = _90 + _55;
   float _94 = _91 + _56;
   float _95 = _92 + _57;
   float _96 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _97 = rsqrt(_96);
   float _98 = _97 * TEXCOORD_2.x;
   float _99 = _97 * TEXCOORD_2.y;
   float _100 = _97 * TEXCOORD_2.z;
   float _101 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _102 = rsqrt(_101);
   float _103 = _102 * TEXCOORD_3.x;
   float _104 = _102 * TEXCOORD_3.y;
   float _105 = _102 * TEXCOORD_3.z;
   float _110 = -0.0f - (c_light_vec[0].x);
   float _111 = -0.0f - (c_light_vec[0].y);
   float _112 = -0.0f - (c_light_vec[0].z);
   float _113 = dot(float3(_98, _99, _100), float3(_110, _111, _112));
   float _117 = c_rt_size.z * SV_Position.x;
   float _118 = c_rt_size.w * SV_Position.y;
   int _121 = c_bool & 8;
   bool _122 = (_121 == 0);
   float _129;
   float _130;
   float _131;
   float _143;
   float _144;
   float _199;
   float _200;
   float _201;
   float _202;
   float _216;
   float _217;
   float _263;
   float _264;
   float _265;
   float _300;
   float _301;
   float _302;
   float _303;
   float _304;
   float _305;
   float _459;
   float _460;
   float _461;
   float _462;
   float _463;
   float _464;
   float _636;
   float _637;
   float _638;
   float _639;
   float _640;
   float _641;
   float _717;
   float _718;
   float _719;
   float _735;
   float _736;
   float _737;
   if (!_122)
   {
      float4 _124 = s13_t.Sample(s13_s, float2(_117, _118));
      _129 = _124.x;
      _130 = _124.y;
      _131 = _124.w;
   }
   else
   {
      _129 = 1.0f;
      _130 = 1.0f;
      _131 = 1.0f;
   }
   if (!_122)
   {
      float _136 = (c_sdw_param[0].x) * _129;
      float _137 = (c_sdw_param[0].z) * _130;
      float _140 = _136 + (c_sdw_param[0].y);
      float _141 = _137 + (c_sdw_param[0].w);
      _143 = _140;
      _144 = _141;
   }
   else
   {
      _143 = _129;
      _144 = _130;
   }
   float _151 = (c_sdw_param[1].x) * _80.y;
   float _153 = _151 + (c_sdw_param[1].y);
   float _154 = _153 * _80.x;
   float _155 = 1.0f - (c_light_col[5].x);
   float _156 = 1.0f - (c_light_col[5].y);
   float _157 = 1.0f - (c_light_col[5].z);
   float _158 = _154 * _155;
   float _159 = _154 * _156;
   float _160 = _154 * _157;
   float _161 = _158 + (c_light_col[5].x);
   float _162 = _159 + (c_light_col[5].y);
   float _163 = _160 + (c_light_col[5].z);
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
      float _209 = (c_sdw_param[0].x) * _143;
      float _210 = (c_sdw_param[0].z) * _144;
      float _213 = _209 + (c_sdw_param[0].y);
      float _214 = _210 + (c_sdw_param[0].w);
      _216 = _213;
      _217 = _214;
   }
   else
   {
      _216 = _143;
      _217 = _144;
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
   float _232 = _131 * COLOR.x;
   float _233 = _232 * _161;
   float _234 = _131 * COLOR.y;
   float _235 = _234 * _162;
   float _236 = _131 * COLOR.z;
   float _237 = _236 * _163;
   float _238 = _229 * _161;
   float _239 = _230 * _162;
   float _240 = _231 * _163;
   if (!_204)
   {
      float _244 = (c_sdw_param[2].x) * _229;
      float _245 = (c_sdw_param[2].x) * _230;
      float _246 = (c_sdw_param[2].x) * _231;
      float _248 = _244 + (c_sdw_param[2].y);
      float _249 = _245 + (c_sdw_param[2].y);
      float _250 = _246 + (c_sdw_param[2].y);
      float _252 = (c_sdw_param[2].z) * _161;
      float _253 = (c_sdw_param[2].z) * _162;
      float _254 = (c_sdw_param[2].z) * _163;
      float _256 = _252 + (c_sdw_param[2].w);
      float _257 = _253 + (c_sdw_param[2].w);
      float _258 = _254 + (c_sdw_param[2].w);
      float _259 = _256 * _248;
      float _260 = _257 * _249;
      float _261 = _258 * _250;
      _263 = _259;
      _264 = _260;
      _265 = _261;
   }
   else
   {
      _263 = _238;
      _264 = _239;
      _265 = _240;
   }
   float _266 = _131 * _80.x;
   float _267 = _263 * _266;
   float _268 = _264 * _266;
   float _269 = _265 * _266;
   float _280 = c_mat_emissive.x * _93;
   float _281 = _280 * c_emissive_scale;
   float _282 = c_mat_emissive.y * _94;
   float _283 = _282 * c_emissive_scale;
   float _284 = c_mat_emissive.z * _95;
   float _285 = _284 * c_emissive_scale;
   int _286 = c_bool & 2048;
   bool _287 = (_286 == 0);
   if (_287)
   {
      float _289 = _217 * _199;
      float _290 = _217 * _200;
      float _291 = _217 * _201;
      float _292 = _289 + TEXCOORD_4.x;
      float _293 = _290 + TEXCOORD_4.y;
      float _294 = _291 + TEXCOORD_4.z;
      float _295 = _217 * _202;
      float _296 = _295 + TEXCOORD_5.x;
      float _297 = _295 + TEXCOORD_5.y;
      float _298 = _295 + TEXCOORD_5.z;
      _300 = _292;
      _301 = _293;
      _302 = _294;
      _303 = _296;
      _304 = _297;
      _305 = _298;
   }
   else
   {
      _300 = TEXCOORD_4.x;
      _301 = TEXCOORD_4.y;
      _302 = TEXCOORD_4.z;
      _303 = TEXCOORD_5.x;
      _304 = TEXCOORD_5.y;
      _305 = TEXCOORD_5.z;
   }
   float _310 = -0.0f - (c_light_vec[1].x);
   float _311 = -0.0f - (c_light_vec[1].y);
   float _312 = -0.0f - (c_light_vec[1].z);
   float _313 = dot(float3(_98, _99, _100), float3(_310, _311, _312));
   float _314 = saturate(_313);
   float _319 = saturate(_113);
   float _320 = (c_light_col[0].x) * _319;
   float _321 = (c_light_col[0].y) * _319;
   float _322 = (c_light_col[0].z) * _319;
   float _327 = (c_light_col[1].x) * _314;
   float _328 = (c_light_col[1].y) * _314;
   float _329 = (c_light_col[1].z) * _314;
   float _338 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _339 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _340 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _341 = _320 * _229;
   float _342 = _321 * _230;
   float _343 = _322 * _231;
   float _344 = _327 * _217;
   float _345 = _328 * _217;
   float _346 = _329 * _217;
   float _347 = _344 + _341;
   float _348 = _345 + _342;
   float _349 = _346 + _343;
   float _350 = _338 * _347;
   float _351 = _339 * _348;
   float _352 = _340 * _349;
   float _353 = _281 + c_scene_ambient.x;
   float _354 = _353 + _350;
   float _355 = _283 + c_scene_ambient.y;
   float _356 = _355 + _351;
   float _357 = _285 + c_scene_ambient.z;
   float _358 = _357 + _352;
   float _359 = _354 * _233;
   float _360 = _356 * _235;
   float _361 = _358 * _237;
   float _362 = _300 * _267;
   float _363 = _301 * _268;
   float _364 = _302 * _269;
   float _365 = _359 + _362;
   float _366 = _360 + _363;
   float _367 = _361 + _364;
   float _372 = (c_light_vec[0].x) + _103;
   float _373 = (c_light_vec[0].y) + _104;
   float _374 = (c_light_vec[0].z) + _105;
   float _375 = dot(float3(_372, _373, _374), float3(_372, _373, _374));
   float _376 = rsqrt(_375);
   float _377 = _372 * _376;
   float _378 = -0.0f - _377;
   float _379 = _373 * _376;
   float _380 = -0.0f - _379;
   float _381 = _374 * _376;
   float _382 = -0.0f - _381;
   float _383 = dot(float3(_378, _380, _382), float3(_98, _99, _100));
   float _384 = saturate(_383);
   float _385 = _104 + -0.30000001192092896f;
   float _386 = dot(float3(_103, _385, _105), float3(_103, _385, _105));
   float _387 = rsqrt(_386);
   float _388 = _103 * _387;
   float _389 = -0.0f - _388;
   float _390 = _385 * _387;
   float _391 = -0.0f - _390;
   float _392 = _105 * _387;
   float _393 = -0.0f - _392;
   float _394 = dot(float3(_389, _391, _393), float3(_98, _99, _100));
   float _395 = saturate(_394);
   float _398 = log2(_384);
   float _399 = log2(_395);
   float _400 = _398 * c_mat_power;
   float _401 = _399 * c_mat_power;
   float _402 = exp2(_400);
   float _403 = exp2(_401);
   float _406 = _402 * (c_light_col[0].x);
   float _407 = _402 * (c_light_col[0].y);
   float _408 = _402 * (c_light_col[0].z);
   float _409 = _403 * 2.0f;
   float _410 = _409 * (c_light_vec[5].x);
   float _411 = _410 * (c_light_col[1].x);
   float _412 = _410 * (c_light_col[1].y);
   float _413 = _410 * (c_light_col[1].z);
   float _414 = _406 * _229;
   float _415 = _407 * _230;
   float _416 = _408 * _231;
   float _417 = _411 * _314;
   float _418 = _412 * _314;
   float _419 = _413 * _314;
   float _420 = _417 + _414;
   float _421 = _418 + _415;
   float _422 = _419 + _416;
   float _429 = c_mat_specular.x * c_specular_scale.x;
   float _430 = _429 * _233;
   float _431 = _430 * _420;
   float _432 = c_mat_specular.y * c_specular_scale.y;
   float _433 = _432 * _235;
   float _434 = _433 * _421;
   float _435 = c_mat_specular.z * c_specular_scale.z;
   float _436 = _435 * _237;
   float _437 = _436 * _422;
   float _438 = _303 * _267;
   float _439 = _438 * _403;
   float _440 = _304 * _268;
   float _441 = _440 * _403;
   float _442 = _305 * _269;
   float _443 = _442 * _403;
   float _444 = _431 + _439;
   float _445 = _434 + _441;
   float _446 = _437 + _443;
   if (!_287)
   {
      float _448 = _217 * _199;
      float _449 = _217 * _200;
      float _450 = _217 * _201;
      float _451 = _365 + _448;
      float _452 = _366 + _449;
      float _453 = _367 + _450;
      float _454 = _217 * _202;
      float _455 = _444 + _454;
      float _456 = _445 + _454;
      float _457 = _446 + _454;
      _459 = _451;
      _460 = _452;
      _461 = _453;
      _462 = _455;
      _463 = _456;
      _464 = _457;
   }
   else
   {
      _459 = _365;
      _460 = _366;
      _461 = _367;
      _462 = _444;
      _463 = _445;
      _464 = _446;
   }
   int _465 = c_bool & 128;
   bool _466 = (_465 != 0);
   float _467 = select(_466, 0.0f, _462);
   float _468 = select(_466, 0.0f, _463);
   float _469 = select(_466, 0.0f, _464);
   float _470 = _459 * _93;
   float _471 = _460 * _94;
   float _472 = _461 * _95;
   float _473 = _467 * _86;
   float _474 = _468 * _86;
   float _475 = _469 * _86;
   int _476 = c_bool & 32768;
   bool _477 = (_476 == 0);
   if (!_477)
   {
      float _481 = (c_ps_user[11].x) * _93;
      float _482 = (c_ps_user[11].x) * _94;
      float _483 = (c_ps_user[11].x) * _95;
      float _485 = _98 + 1.0f;
      float _486 = _99 + 1.0f;
      float _487 = _100 + 1.0f;
      float _488 = _485 * 0.5f;
      float _489 = _486 * 0.5f;
      float _490 = _487 * 0.5f;
      float _491 = saturate(_488);
      float _492 = saturate(_489);
      float _493 = saturate(_490);
      float _494 = (c_ps_user[11].y) * _491;
      float _495 = _492 * (c_ps_user[11].y);
      float _496 = _493 * (c_ps_user[11].y);
      float _498 = (c_ps_user[11].z) * _86;
      float _500 = c_mat_power * 0.009999999776482582f;
      float _501 = _500 * (c_ps_user[11].w);
      float _504 = (c_ps_user[12].x) * _229;
      float _505 = (c_ps_user[12].x) * _230;
      float _506 = (c_ps_user[12].x) * _231;
      float _508 = (c_ps_user[12].y) * _233;
      float _509 = (c_ps_user[12].y) * _235;
      float _510 = (c_ps_user[12].y) * _237;
      float _512 = (c_ps_user[12].z) * _267;
      float _513 = (c_ps_user[12].z) * _268;
      float _514 = (c_ps_user[12].z) * _269;
      float _520 = -0.0f - _103;
      float _521 = -0.0f - _104;
      float _522 = -0.0f - _105;
      float _523 = dot(float3(_520, _521, _522), float3(_98, _99, _100));
      float _524 = 1.0f - _523;
      float _525 = _524 * _524;
      float _526 = _524 * 0.9722222089767456f;
      float _527 = _525 * _525;
      float _528 = _527 * _526;
      float _529 = _528 + 0.02777777425944805f;
      float _530 = _529 * (c_ps_user[13].z);
      float _532 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _533 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _534 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _537 = (c_ps_user[14].x) * _281;
      float _538 = (c_ps_user[14].x) * _283;
      float _539 = (c_ps_user[14].x) * _285;
      float _541 = (c_ps_user[14].y) * _320;
      float _542 = (c_ps_user[14].y) * _321;
      float _543 = (c_ps_user[14].y) * _322;
      float _545 = (c_ps_user[14].z) * _327;
      float _546 = (c_ps_user[14].z) * _328;
      float _547 = (c_ps_user[14].z) * _329;
      float _549 = (c_ps_user[14].w) * _300;
      float _550 = (c_ps_user[14].w) * _301;
      float _551 = (c_ps_user[14].w) * _302;
      float _554 = (c_ps_user[15].x) * _406;
      float _555 = (c_ps_user[15].x) * _407;
      float _556 = (c_ps_user[15].x) * _408;
      float _558 = _429 * _411;
      float _559 = _558 * (c_ps_user[15].y);
      float _560 = _432 * _412;
      float _561 = _560 * (c_ps_user[15].y);
      float _562 = _435 * _413;
      float _563 = _562 * (c_ps_user[15].y);
      float _565 = (c_ps_user[15].z) * _303;
      float _566 = (c_ps_user[15].z) * _304;
      float _567 = (c_ps_user[15].z) * _305;
      float _568 = _559 + _565;
      float _569 = _568 * _403;
      float _570 = _541 + (c_ps_user[13].x);
      float _571 = _570 + _545;
      float _572 = c_diffuse_scale.x * _571;
      float _573 = _572 * c_mat_diffuse.x;
      float _574 = (c_ps_user[13].y) + _498;
      float _575 = _574 + _554;
      float _576 = c_mat_specular.x * _575;
      float _577 = _576 * c_specular_scale.x;
      float _578 = _494 + _481;
      float _579 = _578 + _501;
      float _580 = _579 + (c_ps_user[12].w);
      float _581 = _580 + _504;
      float _582 = _581 + _508;
      float _583 = _582 + _512;
      float _584 = _583 + _532;
      float _585 = _584 + _537;
      float _586 = _585 + _549;
      float _587 = _586 + _530;
      float _588 = _587 + _569;
      float _589 = _588 + _573;
      float _590 = _589 + _577;
      float _591 = _561 + _566;
      float _592 = _591 * _403;
      float _593 = _542 + (c_ps_user[13].x);
      float _594 = _593 + _546;
      float _595 = c_diffuse_scale.y * _594;
      float _596 = _595 * c_mat_diffuse.y;
      float _597 = _574 + _555;
      float _598 = c_mat_specular.y * _597;
      float _599 = _598 * c_specular_scale.y;
      float _600 = _495 + _482;
      float _601 = _600 + _501;
      float _602 = _601 + (c_ps_user[12].w);
      float _603 = _602 + _505;
      float _604 = _603 + _509;
      float _605 = _604 + _513;
      float _606 = _605 + _533;
      float _607 = _606 + _538;
      float _608 = _607 + _550;
      float _609 = _608 + _530;
      float _610 = _609 + _592;
      float _611 = _610 + _596;
      float _612 = _611 + _599;
      float _613 = _563 + _567;
      float _614 = _613 * _403;
      float _615 = _543 + (c_ps_user[13].x);
      float _616 = _615 + _547;
      float _617 = c_diffuse_scale.z * _616;
      float _618 = _617 * c_mat_diffuse.z;
      float _619 = _574 + _556;
      float _620 = c_mat_specular.z * _619;
      float _621 = _620 * c_specular_scale.z;
      float _622 = _501 + _483;
      float _623 = _622 + _496;
      float _624 = _623 + (c_ps_user[12].w);
      float _625 = _624 + _506;
      float _626 = _625 + _510;
      float _627 = _626 + _514;
      float _628 = _627 + _534;
      float _629 = _628 + _539;
      float _630 = _629 + _551;
      float _631 = _630 + _530;
      float _632 = _631 + _614;
      float _633 = _632 + _618;
      float _634 = _633 + _621;
      _636 = _590;
      _637 = _612;
      _638 = _634;
      _639 = 0.0f;
      _640 = 0.0f;
      _641 = 0.0f;
   }
   else
   {
      _636 = _470;
      _637 = _471;
      _638 = _472;
      _639 = _473;
      _640 = _474;
      _641 = _475;
   }
   float _642 = _639 + _636;
   float _643 = _640 + _637;
   float _644 = _641 + _638;
   float _649 = c_mat_opacity * _50.w;
   float _650 = _649 * c_global_opacity;
   float _651 = _650 * c_opacity_scale;
   int _652 = c_bool & 16;
   bool _653 = (_652 == 0);
   if (!_653)
   {
      float _657 = c_tone_control.x * _642;
      float _658 = c_tone_control.x * _643;
      float _659 = c_tone_control.x * _644;
      float _661 = log2(_657);
      float _662 = log2(_658);
      float _663 = log2(_659);
      float _664 = _661 * c_tone_control.y;
      float _665 = _662 * c_tone_control.y;
      float _666 = _663 * c_tone_control.y;
      float _667 = exp2(_664);
      float _668 = exp2(_665);
      float _669 = exp2(_666);
      _717 = _667;
      _718 = _668;
      _719 = _669;
   }
   else
   {
      if (_477)
      {
         float _674 = c_tone_control.x * _642;
         float _675 = c_tone_control.x * _643;
         float _676 = c_tone_control.x * _644;
         float _677 = _674 * 2.0f;
         float _678 = _675 * 2.0f;
         float _679 = _676 * 2.0f;
         float _680 = _674 * 0.30000001192092896f;
         float _681 = _675 * 0.30000001192092896f;
         float _682 = _676 * 0.30000001192092896f;
         float _683 = _680 + 0.05000000074505806f;
         float _684 = _681 + 0.05000000074505806f;
         float _685 = _682 + 0.05000000074505806f;
         float _686 = _683 * _677;
         float _687 = _684 * _678;
         float _688 = _685 * _679;
         float _689 = _686 + 0.004000000189989805f;
         float _690 = _687 + 0.004000000189989805f;
         float _691 = _688 + 0.004000000189989805f;
         float _692 = _680 + 0.5f;
         float _693 = _681 + 0.5f;
         float _694 = _682 + 0.5f;
         float _695 = _692 * _677;
         float _696 = _693 * _678;
         float _697 = _694 * _679;
         float _698 = _695 + 0.06000000238418579f;
         float _699 = _696 + 0.06000000238418579f;
         float _700 = _697 + 0.06000000238418579f;
         float _701 = _689 / _698;
         float _702 = _690 / _699;
         float _703 = _691 / _700;
         float _704 = _701 + -0.06666666269302368f;
         float _705 = _702 + -0.06666666269302368f;
         float _706 = _703 + -0.06666666269302368f;
         float _707 = _704 * 1.3790643215179443f;
         float _708 = _705 * 1.3790643215179443f;
         float _709 = _706 * 1.3790643215179443f;
         float _710 = YKR_MaterialCurve(_707, _674);
         float _711 = YKR_MaterialCurve(_708, _675);
         float _712 = YKR_MaterialCurve(_709, _676);
         float _713 = sqrt(_710);
         float _714 = sqrt(_711);
         float _715 = sqrt(_712);
         _717 = _713;
         _718 = _714;
         _719 = _715;
      }
      else
      {
         _717 = _642;
         _718 = _643;
         _719 = _644;
      }
   }
   int _720 = c_bool & 4;
   bool _721 = (_720 == 0);
   if (_721)
   {
      float _723 = saturate(COLOR_1.x);
      float _728 = (c_fog_param[0].x) - _717;
      float _729 = (c_fog_param[0].y) - _718;
      float _730 = (c_fog_param[0].z) - _719;
      float _731 = _728 * _723;
      float _732 = _729 * _723;
      float _733 = _730 * _723;
      _735 = _731;
      _736 = _732;
      _737 = _733;
   }
   else
   {
      _735 = COLOR_1.x;
      _736 = COLOR_1.y;
      _737 = COLOR_1.z;
   }
   float _738 = _735 + _717;
   float _739 = _736 + _718;
   float _740 = _737 + _719;
   float _746 = (c_ps_col_scl[0].x) * _738;
   float _747 = (c_ps_col_scl[0].y) * _739;
   float _748 = (c_ps_col_scl[0].z) * _740;
   float _749 = (c_ps_col_scl[0].w) * _651;
   SV_Target.x = _746;
   SV_Target.y = _747;
   SV_Target.z = _748;
   SV_Target.w = _749;
   int _752 = c_bool & 8;
   bool _753 = (_752 != 0);
   float _754 = select(_753, _738, _746);
   float _755 = select(_753, _739, _747);
   float _756 = select(_753, _740, _748);
   float _757 = select(_753, _651, _749);
   SV_Target.x = _754;
   SV_Target.y = _755;
   SV_Target.z = _756;
   SV_Target.w = _757;
   return SV_Target;
}
