// Yakuza Kiwami material r_o1ds_b(vr)2d_m4d (0x591161BE, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _70 = _65.x * _65.x;
   float _71 = _65.y * _65.y;
   float _72 = _65.z * _65.z;
   float4 _73 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _76 = _65.w * COLOR_1.w;
   float _77 = _70 - _53;
   float _78 = _71 - _54;
   float _79 = _72 - _55;
   float _80 = _77 * _76;
   float _81 = _78 * _76;
   float _82 = _79 * _76;
   float _83 = _80 + _53;
   float _84 = _81 + _54;
   float _85 = _82 + _55;
   float _86 = 1.0f - _61.x;
   float _87 = 1.0f - _61.y;
   float _88 = 1.0f - _61.z;
   float _89 = _76 * _86;
   float _90 = _76 * _87;
   float _91 = _76 * _88;
   float _92 = _89 + _61.x;
   float _93 = _90 + _61.y;
   float _94 = _91 + _61.z;
   float _95 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _96 = rsqrt(_95);
   float _97 = _96 * TEXCOORD_2.x;
   float _98 = _96 * TEXCOORD_2.y;
   float _99 = _96 * TEXCOORD_2.z;
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
   float _198;
   float _199;
   float _200;
   float _201;
   float _215;
   float _216;
   float _262;
   float _263;
   float _264;
   float _299;
   float _300;
   float _301;
   float _302;
   float _303;
   float _304;
   float _458;
   float _459;
   float _460;
   float _461;
   float _462;
   float _463;
   float _639;
   float _640;
   float _641;
   float _642;
   float _643;
   float _644;
   float _720;
   float _721;
   float _722;
   float _738;
   float _739;
   float _740;
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
   float _150 = (c_sdw_param[1].x) * _73.y;
   float _152 = _150 + (c_sdw_param[1].y);
   float _153 = _152 * _73.x;
   float _154 = 1.0f - (c_light_col[5].x);
   float _155 = 1.0f - (c_light_col[5].y);
   float _156 = 1.0f - (c_light_col[5].z);
   float _157 = _153 * _154;
   float _158 = _153 * _155;
   float _159 = _153 * _156;
   float _160 = _157 + (c_light_col[5].x);
   float _161 = _158 + (c_light_col[5].y);
   float _162 = _159 + (c_light_col[5].z);
   float _166 = c_rt_size.z * SV_Position.x;
   float _167 = c_rt_size.w * SV_Position.y;
   float4 _168 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_166, _167)));
   int _175 = c_bool & 16384;
   bool _176 = (_175 == 0);
   if (!_176)
   {
      float _178 = _168.x * 2.0f;
      float _179 = _168.y * 2.0f;
      float _180 = _168.z * 2.0f;
      float _181 = _168.w * 2.0f;
      _198 = _178;
      _199 = _179;
      _200 = _180;
      _201 = _181;
   }
   else
   {
      int _183 = c_bool & 8192;
      bool _184 = (_183 == 0);
      if (!_184)
      {
         float _186 = _168.x * 2.0f;
         float _187 = _168.y * 2.0f;
         float _188 = _168.z * 2.0f;
         float _189 = _168.w * 2.0f;
         _198 = _186;
         _199 = _187;
         _200 = _188;
         _201 = _189;
      }
      else
      {
         float _193 = _168.x / c_tone_control.z;
         float _194 = _168.y / c_tone_control.z;
         float _195 = _168.z / c_tone_control.z;
         float _196 = _168.w / c_tone_control.z;
         _198 = _193;
         _199 = _194;
         _200 = _195;
         _201 = _196;
      }
   }
   int _202 = c_bool & 8;
   bool _203 = (_202 == 0);
   if (!_203)
   {
      float _208 = (c_sdw_param[0].x) * _142;
      float _209 = (c_sdw_param[0].z) * _143;
      float _212 = _208 + (c_sdw_param[0].y);
      float _213 = _209 + (c_sdw_param[0].w);
      _215 = _212;
      _216 = _213;
   }
   else
   {
      _215 = _142;
      _216 = _143;
   }
   float _217 = _216 * _215;
   float _222 = 1.0f - (c_light_col[5].x);
   float _223 = 1.0f - (c_light_col[5].y);
   float _224 = 1.0f - (c_light_col[5].z);
   float _225 = _222 * _217;
   float _226 = _223 * _217;
   float _227 = _224 * _217;
   float _228 = _225 + (c_light_col[5].x);
   float _229 = _226 + (c_light_col[5].y);
   float _230 = _227 + (c_light_col[5].z);
   float _231 = _130 * COLOR.x;
   float _232 = _231 * _160;
   float _233 = _130 * COLOR.y;
   float _234 = _233 * _161;
   float _235 = _130 * COLOR.z;
   float _236 = _235 * _162;
   float _237 = _228 * _160;
   float _238 = _229 * _161;
   float _239 = _230 * _162;
   if (!_203)
   {
      float _243 = (c_sdw_param[2].x) * _228;
      float _244 = (c_sdw_param[2].x) * _229;
      float _245 = (c_sdw_param[2].x) * _230;
      float _247 = _243 + (c_sdw_param[2].y);
      float _248 = _244 + (c_sdw_param[2].y);
      float _249 = _245 + (c_sdw_param[2].y);
      float _251 = (c_sdw_param[2].z) * _160;
      float _252 = (c_sdw_param[2].z) * _161;
      float _253 = (c_sdw_param[2].z) * _162;
      float _255 = _251 + (c_sdw_param[2].w);
      float _256 = _252 + (c_sdw_param[2].w);
      float _257 = _253 + (c_sdw_param[2].w);
      float _258 = _255 * _247;
      float _259 = _256 * _248;
      float _260 = _257 * _249;
      _262 = _258;
      _263 = _259;
      _264 = _260;
   }
   else
   {
      _262 = _237;
      _263 = _238;
      _264 = _239;
   }
   float _265 = _130 * _73.x;
   float _266 = _262 * _265;
   float _267 = _263 * _265;
   float _268 = _264 * _265;
   float _279 = c_mat_emissive.x * _83;
   float _280 = _279 * c_emissive_scale;
   float _281 = c_mat_emissive.y * _84;
   float _282 = _281 * c_emissive_scale;
   float _283 = c_mat_emissive.z * _85;
   float _284 = _283 * c_emissive_scale;
   int _285 = c_bool & 2048;
   bool _286 = (_285 == 0);
   if (_286)
   {
      float _288 = _216 * _198;
      float _289 = _216 * _199;
      float _290 = _216 * _200;
      float _291 = _288 + TEXCOORD_4.x;
      float _292 = _289 + TEXCOORD_4.y;
      float _293 = _290 + TEXCOORD_4.z;
      float _294 = _216 * _201;
      float _295 = _294 + TEXCOORD_5.x;
      float _296 = _294 + TEXCOORD_5.y;
      float _297 = _294 + TEXCOORD_5.z;
      _299 = _291;
      _300 = _292;
      _301 = _293;
      _302 = _295;
      _303 = _296;
      _304 = _297;
   }
   else
   {
      _299 = TEXCOORD_4.x;
      _300 = TEXCOORD_4.y;
      _301 = TEXCOORD_4.z;
      _302 = TEXCOORD_5.x;
      _303 = TEXCOORD_5.y;
      _304 = TEXCOORD_5.z;
   }
   float _309 = -0.0f - (c_light_vec[1].x);
   float _310 = -0.0f - (c_light_vec[1].y);
   float _311 = -0.0f - (c_light_vec[1].z);
   float _312 = dot(float3(_97, _98, _99), float3(_309, _310, _311));
   float _313 = saturate(_312);
   float _318 = saturate(_112);
   float _319 = (c_light_col[0].x) * _318;
   float _320 = (c_light_col[0].y) * _318;
   float _321 = (c_light_col[0].z) * _318;
   float _326 = (c_light_col[1].x) * _313;
   float _327 = (c_light_col[1].y) * _313;
   float _328 = (c_light_col[1].z) * _313;
   float _337 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _338 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _339 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _340 = _319 * _228;
   float _341 = _320 * _229;
   float _342 = _321 * _230;
   float _343 = _326 * _216;
   float _344 = _327 * _216;
   float _345 = _328 * _216;
   float _346 = _343 + _340;
   float _347 = _344 + _341;
   float _348 = _345 + _342;
   float _349 = _337 * _346;
   float _350 = _338 * _347;
   float _351 = _339 * _348;
   float _352 = _280 + c_scene_ambient.x;
   float _353 = _352 + _349;
   float _354 = _282 + c_scene_ambient.y;
   float _355 = _354 + _350;
   float _356 = _284 + c_scene_ambient.z;
   float _357 = _356 + _351;
   float _358 = _353 * _232;
   float _359 = _355 * _234;
   float _360 = _357 * _236;
   float _361 = _299 * _266;
   float _362 = _300 * _267;
   float _363 = _301 * _268;
   float _364 = _358 + _361;
   float _365 = _359 + _362;
   float _366 = _360 + _363;
   float _371 = (c_light_vec[0].x) + _102;
   float _372 = (c_light_vec[0].y) + _103;
   float _373 = (c_light_vec[0].z) + _104;
   float _374 = dot(float3(_371, _372, _373), float3(_371, _372, _373));
   float _375 = rsqrt(_374);
   float _376 = _371 * _375;
   float _377 = -0.0f - _376;
   float _378 = _372 * _375;
   float _379 = -0.0f - _378;
   float _380 = _373 * _375;
   float _381 = -0.0f - _380;
   float _382 = dot(float3(_377, _379, _381), float3(_97, _98, _99));
   float _383 = saturate(_382);
   float _384 = _103 + -0.30000001192092896f;
   float _385 = dot(float3(_102, _384, _104), float3(_102, _384, _104));
   float _386 = rsqrt(_385);
   float _387 = _102 * _386;
   float _388 = -0.0f - _387;
   float _389 = _384 * _386;
   float _390 = -0.0f - _389;
   float _391 = _104 * _386;
   float _392 = -0.0f - _391;
   float _393 = dot(float3(_388, _390, _392), float3(_97, _98, _99));
   float _394 = saturate(_393);
   float _397 = log2(_383);
   float _398 = log2(_394);
   float _399 = _397 * c_mat_power;
   float _400 = _398 * c_mat_power;
   float _401 = exp2(_399);
   float _402 = exp2(_400);
   float _405 = _401 * (c_light_col[0].x);
   float _406 = _401 * (c_light_col[0].y);
   float _407 = _401 * (c_light_col[0].z);
   float _408 = _402 * 2.0f;
   float _409 = _408 * (c_light_vec[5].x);
   float _410 = _409 * (c_light_col[1].x);
   float _411 = _409 * (c_light_col[1].y);
   float _412 = _409 * (c_light_col[1].z);
   float _413 = _405 * _228;
   float _414 = _406 * _229;
   float _415 = _407 * _230;
   float _416 = _410 * _313;
   float _417 = _411 * _313;
   float _418 = _412 * _313;
   float _419 = _416 + _413;
   float _420 = _417 + _414;
   float _421 = _418 + _415;
   float _428 = c_mat_specular.x * c_specular_scale.x;
   float _429 = _428 * _232;
   float _430 = _429 * _419;
   float _431 = c_mat_specular.y * c_specular_scale.y;
   float _432 = _431 * _234;
   float _433 = _432 * _420;
   float _434 = c_mat_specular.z * c_specular_scale.z;
   float _435 = _434 * _236;
   float _436 = _435 * _421;
   float _437 = _302 * _266;
   float _438 = _437 * _402;
   float _439 = _303 * _267;
   float _440 = _439 * _402;
   float _441 = _304 * _268;
   float _442 = _441 * _402;
   float _443 = _430 + _438;
   float _444 = _433 + _440;
   float _445 = _436 + _442;
   if (!_286)
   {
      float _447 = _216 * _198;
      float _448 = _216 * _199;
      float _449 = _216 * _200;
      float _450 = _364 + _447;
      float _451 = _365 + _448;
      float _452 = _366 + _449;
      float _453 = _216 * _201;
      float _454 = _443 + _453;
      float _455 = _444 + _453;
      float _456 = _445 + _453;
      _458 = _450;
      _459 = _451;
      _460 = _452;
      _461 = _454;
      _462 = _455;
      _463 = _456;
   }
   else
   {
      _458 = _364;
      _459 = _365;
      _460 = _366;
      _461 = _443;
      _462 = _444;
      _463 = _445;
   }
   int _464 = c_bool & 128;
   bool _465 = (_464 != 0);
   float _466 = select(_465, 0.0f, _461);
   float _467 = select(_465, 0.0f, _462);
   float _468 = select(_465, 0.0f, _463);
   float _469 = _458 * _83;
   float _470 = _459 * _84;
   float _471 = _460 * _85;
   float _472 = _466 * _92;
   float _473 = _467 * _93;
   float _474 = _468 * _94;
   int _475 = c_bool & 32768;
   bool _476 = (_475 == 0);
   if (!_476)
   {
      float _480 = (c_ps_user[11].x) * _83;
      float _481 = (c_ps_user[11].x) * _84;
      float _482 = (c_ps_user[11].x) * _85;
      float _484 = _97 + 1.0f;
      float _485 = _98 + 1.0f;
      float _486 = _99 + 1.0f;
      float _487 = _484 * 0.5f;
      float _488 = _485 * 0.5f;
      float _489 = _486 * 0.5f;
      float _490 = saturate(_487);
      float _491 = saturate(_488);
      float _492 = saturate(_489);
      float _493 = (c_ps_user[11].y) * _490;
      float _494 = _491 * (c_ps_user[11].y);
      float _495 = _492 * (c_ps_user[11].y);
      float _497 = (c_ps_user[11].z) * _92;
      float _498 = (c_ps_user[11].z) * _93;
      float _499 = (c_ps_user[11].z) * _94;
      float _501 = c_mat_power * 0.009999999776482582f;
      float _502 = _501 * (c_ps_user[11].w);
      float _505 = (c_ps_user[12].x) * _228;
      float _506 = (c_ps_user[12].x) * _229;
      float _507 = (c_ps_user[12].x) * _230;
      float _509 = (c_ps_user[12].y) * _232;
      float _510 = (c_ps_user[12].y) * _234;
      float _511 = (c_ps_user[12].y) * _236;
      float _513 = (c_ps_user[12].z) * _266;
      float _514 = (c_ps_user[12].z) * _267;
      float _515 = (c_ps_user[12].z) * _268;
      float _521 = -0.0f - _102;
      float _522 = -0.0f - _103;
      float _523 = -0.0f - _104;
      float _524 = dot(float3(_521, _522, _523), float3(_97, _98, _99));
      float _525 = 1.0f - _524;
      float _526 = _525 * _525;
      float _527 = _525 * 0.9722222089767456f;
      float _528 = _526 * _526;
      float _529 = _528 * _527;
      float _530 = _529 + 0.02777777425944805f;
      float _531 = _530 * (c_ps_user[13].z);
      float _533 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _534 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _535 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _538 = (c_ps_user[14].x) * _280;
      float _539 = (c_ps_user[14].x) * _282;
      float _540 = (c_ps_user[14].x) * _284;
      float _542 = (c_ps_user[14].y) * _319;
      float _543 = (c_ps_user[14].y) * _320;
      float _544 = (c_ps_user[14].y) * _321;
      float _546 = (c_ps_user[14].z) * _326;
      float _547 = (c_ps_user[14].z) * _327;
      float _548 = (c_ps_user[14].z) * _328;
      float _550 = (c_ps_user[14].w) * _299;
      float _551 = (c_ps_user[14].w) * _300;
      float _552 = (c_ps_user[14].w) * _301;
      float _555 = (c_ps_user[15].x) * _405;
      float _556 = (c_ps_user[15].x) * _406;
      float _557 = (c_ps_user[15].x) * _407;
      float _559 = _428 * _410;
      float _560 = _559 * (c_ps_user[15].y);
      float _561 = _431 * _411;
      float _562 = _561 * (c_ps_user[15].y);
      float _563 = _434 * _412;
      float _564 = _563 * (c_ps_user[15].y);
      float _566 = (c_ps_user[15].z) * _302;
      float _567 = (c_ps_user[15].z) * _303;
      float _568 = (c_ps_user[15].z) * _304;
      float _569 = _560 + _566;
      float _570 = _569 * _402;
      float _571 = _542 + (c_ps_user[13].x);
      float _572 = _571 + _546;
      float _573 = c_diffuse_scale.x * _572;
      float _574 = _573 * c_mat_diffuse.x;
      float _575 = (c_ps_user[13].y) + _497;
      float _576 = _575 + _555;
      float _577 = c_mat_specular.x * _576;
      float _578 = _577 * c_specular_scale.x;
      float _579 = _493 + _480;
      float _580 = _579 + _502;
      float _581 = _580 + (c_ps_user[12].w);
      float _582 = _581 + _505;
      float _583 = _582 + _509;
      float _584 = _583 + _513;
      float _585 = _584 + _533;
      float _586 = _585 + _538;
      float _587 = _586 + _550;
      float _588 = _587 + _531;
      float _589 = _588 + _570;
      float _590 = _589 + _574;
      float _591 = _590 + _578;
      float _592 = _562 + _567;
      float _593 = _592 * _402;
      float _594 = _543 + (c_ps_user[13].x);
      float _595 = _594 + _547;
      float _596 = c_diffuse_scale.y * _595;
      float _597 = _596 * c_mat_diffuse.y;
      float _598 = (c_ps_user[13].y) + _498;
      float _599 = _598 + _556;
      float _600 = c_mat_specular.y * _599;
      float _601 = _600 * c_specular_scale.y;
      float _602 = _494 + _481;
      float _603 = _602 + _502;
      float _604 = _603 + (c_ps_user[12].w);
      float _605 = _604 + _506;
      float _606 = _605 + _510;
      float _607 = _606 + _514;
      float _608 = _607 + _534;
      float _609 = _608 + _539;
      float _610 = _609 + _551;
      float _611 = _610 + _531;
      float _612 = _611 + _593;
      float _613 = _612 + _597;
      float _614 = _613 + _601;
      float _615 = _564 + _568;
      float _616 = _615 * _402;
      float _617 = _544 + (c_ps_user[13].x);
      float _618 = _617 + _548;
      float _619 = c_diffuse_scale.z * _618;
      float _620 = _619 * c_mat_diffuse.z;
      float _621 = (c_ps_user[13].y) + _499;
      float _622 = _621 + _557;
      float _623 = c_mat_specular.z * _622;
      float _624 = _623 * c_specular_scale.z;
      float _625 = _502 + _482;
      float _626 = _625 + _495;
      float _627 = _626 + (c_ps_user[12].w);
      float _628 = _627 + _507;
      float _629 = _628 + _511;
      float _630 = _629 + _515;
      float _631 = _630 + _535;
      float _632 = _631 + _540;
      float _633 = _632 + _552;
      float _634 = _633 + _531;
      float _635 = _634 + _616;
      float _636 = _635 + _620;
      float _637 = _636 + _624;
      _639 = _591;
      _640 = _614;
      _641 = _637;
      _642 = 0.0f;
      _643 = 0.0f;
      _644 = 0.0f;
   }
   else
   {
      _639 = _469;
      _640 = _470;
      _641 = _471;
      _642 = _472;
      _643 = _473;
      _644 = _474;
   }
   float _645 = _642 + _639;
   float _646 = _643 + _640;
   float _647 = _644 + _641;
   float _652 = c_mat_opacity * _48.w;
   float _653 = _652 * c_global_opacity;
   float _654 = _653 * c_opacity_scale;
   int _655 = c_bool & 16;
   bool _656 = (_655 == 0);
   if (!_656)
   {
      float _660 = c_tone_control.x * _645;
      float _661 = c_tone_control.x * _646;
      float _662 = c_tone_control.x * _647;
      float _664 = log2(_660);
      float _665 = log2(_661);
      float _666 = log2(_662);
      float _667 = _664 * c_tone_control.y;
      float _668 = _665 * c_tone_control.y;
      float _669 = _666 * c_tone_control.y;
      float _670 = exp2(_667);
      float _671 = exp2(_668);
      float _672 = exp2(_669);
      _720 = _670;
      _721 = _671;
      _722 = _672;
   }
   else
   {
      if (_476)
      {
         float _677 = c_tone_control.x * _645;
         float _678 = c_tone_control.x * _646;
         float _679 = c_tone_control.x * _647;
         float _680 = _677 * 2.0f;
         float _681 = _678 * 2.0f;
         float _682 = _679 * 2.0f;
         float _683 = _677 * 0.30000001192092896f;
         float _684 = _678 * 0.30000001192092896f;
         float _685 = _679 * 0.30000001192092896f;
         float _686 = _683 + 0.05000000074505806f;
         float _687 = _684 + 0.05000000074505806f;
         float _688 = _685 + 0.05000000074505806f;
         float _689 = _686 * _680;
         float _690 = _687 * _681;
         float _691 = _688 * _682;
         float _692 = _689 + 0.004000000189989805f;
         float _693 = _690 + 0.004000000189989805f;
         float _694 = _691 + 0.004000000189989805f;
         float _695 = _683 + 0.5f;
         float _696 = _684 + 0.5f;
         float _697 = _685 + 0.5f;
         float _698 = _695 * _680;
         float _699 = _696 * _681;
         float _700 = _697 * _682;
         float _701 = _698 + 0.06000000238418579f;
         float _702 = _699 + 0.06000000238418579f;
         float _703 = _700 + 0.06000000238418579f;
         float _704 = _692 / _701;
         float _705 = _693 / _702;
         float _706 = _694 / _703;
         float _707 = _704 + -0.06666666269302368f;
         float _708 = _705 + -0.06666666269302368f;
         float _709 = _706 + -0.06666666269302368f;
         float _710 = _707 * 1.3790643215179443f;
         float _711 = _708 * 1.3790643215179443f;
         float _712 = _709 * 1.3790643215179443f;
         float _713 = YKR_MaterialCurve(_710, _677);
         float _714 = YKR_MaterialCurve(_711, _678);
         float _715 = YKR_MaterialCurve(_712, _679);
         float _716 = sqrt(_713);
         float _717 = sqrt(_714);
         float _718 = sqrt(_715);
         _720 = _716;
         _721 = _717;
         _722 = _718;
      }
      else
      {
         _720 = _645;
         _721 = _646;
         _722 = _647;
      }
   }
   int _723 = c_bool & 4;
   bool _724 = (_723 == 0);
   if (_724)
   {
      float _726 = saturate(COLOR_1.x);
      float _731 = (c_fog_param[0].x) - _720;
      float _732 = (c_fog_param[0].y) - _721;
      float _733 = (c_fog_param[0].z) - _722;
      float _734 = _731 * _726;
      float _735 = _732 * _726;
      float _736 = _733 * _726;
      _738 = _734;
      _739 = _735;
      _740 = _736;
   }
   else
   {
      _738 = COLOR_1.x;
      _739 = COLOR_1.y;
      _740 = COLOR_1.z;
   }
   float _741 = _738 + _720;
   float _742 = _739 + _721;
   float _743 = _740 + _722;
   float _749 = (c_ps_col_scl[0].x) * _741;
   float _750 = (c_ps_col_scl[0].y) * _742;
   float _751 = (c_ps_col_scl[0].z) * _743;
   float _752 = (c_ps_col_scl[0].w) * _654;
   SV_Target.x = _749;
   SV_Target.y = _750;
   SV_Target.z = _751;
   SV_Target.w = _752;
   int _755 = c_bool & 8;
   bool _756 = (_755 != 0);
   float _757 = select(_756, _741, _749);
   float _758 = select(_756, _742, _750);
   float _759 = select(_756, _743, _751);
   float _760 = select(_756, _654, _752);
   SV_Target.x = _757;
   SV_Target.y = _758;
   SV_Target.z = _759;
   SV_Target.w = _760;
   return SV_Target;
}
