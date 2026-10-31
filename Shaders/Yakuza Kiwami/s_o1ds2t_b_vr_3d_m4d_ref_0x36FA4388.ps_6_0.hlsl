// Yakuza Kiwami material s_o1ds2t_b(vr)3d_m4d[ref] (0x36FA4388, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

TextureCube<float4> s2c_t : register(t2);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s2c_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _59 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _64 = _59.x * _59.x;
   float _65 = _59.y * _59.y;
   float _66 = _59.z * _59.z;
   float _70 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _72 = s3_t.Sample(s3_s, float2(_70, _71));
   float _80 = (c_attribute_ps[0].z) * TEXCOORD.z;
   float _81 = (c_attribute_ps[0].w) * TEXCOORD.w;
   float4 _82 = s5_t.Sample(s5_s, float2(_80, _81));
   float4 _85 = s1_t.Sample(s1_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _90 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _93 = _85.w * COLOR_1.w;
   float _94 = _85.x - _64;
   float _95 = _85.y - _65;
   float _96 = _85.z - _66;
   float _97 = _93 * _94;
   float _98 = _93 * _95;
   float _99 = _93 * _96;
   float _100 = _97 + _64;
   float _101 = _98 + _65;
   float _102 = _99 + _66;
   float _103 = _82.x * 2.0f;
   float _104 = _82.y * 2.0f;
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
   float _239;
   float _240;
   float _241;
   float _242;
   float _256;
   float _257;
   float _303;
   float _304;
   float _305;
   float _340;
   float _341;
   float _342;
   float _343;
   float _344;
   float _345;
   float _499;
   float _500;
   float _501;
   float _502;
   float _503;
   float _504;
   float _680;
   float _681;
   float _682;
   float _683;
   float _684;
   float _685;
   float _808;
   float _809;
   float _810;
   float _826;
   float _827;
   float _828;
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
   float _191 = (c_sdw_param[1].x) * _90.y;
   float _193 = _191 + (c_sdw_param[1].y);
   float _194 = _193 * _90.x;
   float _195 = 1.0f - (c_light_col[5].x);
   float _196 = 1.0f - (c_light_col[5].y);
   float _197 = 1.0f - (c_light_col[5].z);
   float _198 = _194 * _195;
   float _199 = _194 * _196;
   float _200 = _194 * _197;
   float _201 = _198 + (c_light_col[5].x);
   float _202 = _199 + (c_light_col[5].y);
   float _203 = _200 + (c_light_col[5].z);
   float _207 = c_rt_size.z * SV_Position.x;
   float _208 = c_rt_size.w * SV_Position.y;
   float4 _209 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_207, _208)));
   int _216 = c_bool & 16384;
   bool _217 = (_216 == 0);
   if (!_217)
   {
      float _219 = _209.x * 2.0f;
      float _220 = _209.y * 2.0f;
      float _221 = _209.z * 2.0f;
      float _222 = _209.w * 2.0f;
      _239 = _219;
      _240 = _220;
      _241 = _221;
      _242 = _222;
   }
   else
   {
      int _224 = c_bool & 8192;
      bool _225 = (_224 == 0);
      if (!_225)
      {
         float _227 = _209.x * 2.0f;
         float _228 = _209.y * 2.0f;
         float _229 = _209.z * 2.0f;
         float _230 = _209.w * 2.0f;
         _239 = _227;
         _240 = _228;
         _241 = _229;
         _242 = _230;
      }
      else
      {
         float _234 = _209.x / c_tone_control.z;
         float _235 = _209.y / c_tone_control.z;
         float _236 = _209.z / c_tone_control.z;
         float _237 = _209.w / c_tone_control.z;
         _239 = _234;
         _240 = _235;
         _241 = _236;
         _242 = _237;
      }
   }
   int _243 = c_bool & 8;
   bool _244 = (_243 == 0);
   if (!_244)
   {
      float _249 = (c_sdw_param[0].x) * _183;
      float _250 = (c_sdw_param[0].z) * _184;
      float _253 = _249 + (c_sdw_param[0].y);
      float _254 = _250 + (c_sdw_param[0].w);
      _256 = _253;
      _257 = _254;
   }
   else
   {
      _256 = _183;
      _257 = _184;
   }
   float _258 = _257 * _256;
   float _263 = 1.0f - (c_light_col[5].x);
   float _264 = 1.0f - (c_light_col[5].y);
   float _265 = 1.0f - (c_light_col[5].z);
   float _266 = _263 * _258;
   float _267 = _264 * _258;
   float _268 = _265 * _258;
   float _269 = _266 + (c_light_col[5].x);
   float _270 = _267 + (c_light_col[5].y);
   float _271 = _268 + (c_light_col[5].z);
   float _272 = _171 * COLOR.x;
   float _273 = _272 * _201;
   float _274 = _171 * COLOR.y;
   float _275 = _274 * _202;
   float _276 = _171 * COLOR.z;
   float _277 = _276 * _203;
   float _278 = _269 * _201;
   float _279 = _270 * _202;
   float _280 = _271 * _203;
   if (!_244)
   {
      float _284 = (c_sdw_param[2].x) * _269;
      float _285 = (c_sdw_param[2].x) * _270;
      float _286 = (c_sdw_param[2].x) * _271;
      float _288 = _284 + (c_sdw_param[2].y);
      float _289 = _285 + (c_sdw_param[2].y);
      float _290 = _286 + (c_sdw_param[2].y);
      float _292 = (c_sdw_param[2].z) * _201;
      float _293 = (c_sdw_param[2].z) * _202;
      float _294 = (c_sdw_param[2].z) * _203;
      float _296 = _292 + (c_sdw_param[2].w);
      float _297 = _293 + (c_sdw_param[2].w);
      float _298 = _294 + (c_sdw_param[2].w);
      float _299 = _296 * _288;
      float _300 = _297 * _289;
      float _301 = _298 * _290;
      _303 = _299;
      _304 = _300;
      _305 = _301;
   }
   else
   {
      _303 = _278;
      _304 = _279;
      _305 = _280;
   }
   float _306 = _171 * _90.x;
   float _307 = _303 * _306;
   float _308 = _304 * _306;
   float _309 = _305 * _306;
   float _320 = c_mat_emissive.x * _100;
   float _321 = _320 * c_emissive_scale;
   float _322 = c_mat_emissive.y * _101;
   float _323 = _322 * c_emissive_scale;
   float _324 = c_mat_emissive.z * _102;
   float _325 = _324 * c_emissive_scale;
   int _326 = c_bool & 2048;
   bool _327 = (_326 == 0);
   if (_327)
   {
      float _329 = _257 * _239;
      float _330 = _257 * _240;
      float _331 = _257 * _241;
      float _332 = _329 + TEXCOORD_4.x;
      float _333 = _330 + TEXCOORD_4.y;
      float _334 = _331 + TEXCOORD_4.z;
      float _335 = _257 * _242;
      float _336 = _335 + TEXCOORD_5.x;
      float _337 = _335 + TEXCOORD_5.y;
      float _338 = _335 + TEXCOORD_5.z;
      _340 = _332;
      _341 = _333;
      _342 = _334;
      _343 = _336;
      _344 = _337;
      _345 = _338;
   }
   else
   {
      _340 = TEXCOORD_4.x;
      _341 = TEXCOORD_4.y;
      _342 = TEXCOORD_4.z;
      _343 = TEXCOORD_5.x;
      _344 = TEXCOORD_5.y;
      _345 = TEXCOORD_5.z;
   }
   float _350 = -0.0f - (c_light_vec[1].x);
   float _351 = -0.0f - (c_light_vec[1].y);
   float _352 = -0.0f - (c_light_vec[1].z);
   float _353 = dot(float3(_138, _139, _140), float3(_350, _351, _352));
   float _354 = saturate(_353);
   float _359 = saturate(_153);
   float _360 = (c_light_col[0].x) * _359;
   float _361 = (c_light_col[0].y) * _359;
   float _362 = (c_light_col[0].z) * _359;
   float _367 = (c_light_col[1].x) * _354;
   float _368 = (c_light_col[1].y) * _354;
   float _369 = (c_light_col[1].z) * _354;
   float _378 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _379 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _380 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _381 = _360 * _269;
   float _382 = _361 * _270;
   float _383 = _362 * _271;
   float _384 = _367 * _257;
   float _385 = _368 * _257;
   float _386 = _369 * _257;
   float _387 = _384 + _381;
   float _388 = _385 + _382;
   float _389 = _386 + _383;
   float _390 = _378 * _387;
   float _391 = _379 * _388;
   float _392 = _380 * _389;
   float _393 = _321 + c_scene_ambient.x;
   float _394 = _393 + _390;
   float _395 = _323 + c_scene_ambient.y;
   float _396 = _395 + _391;
   float _397 = _325 + c_scene_ambient.z;
   float _398 = _397 + _392;
   float _399 = _394 * _273;
   float _400 = _396 * _275;
   float _401 = _398 * _277;
   float _402 = _340 * _307;
   float _403 = _341 * _308;
   float _404 = _342 * _309;
   float _405 = _399 + _402;
   float _406 = _400 + _403;
   float _407 = _401 + _404;
   float _412 = (c_light_vec[0].x) + _143;
   float _413 = (c_light_vec[0].y) + _144;
   float _414 = (c_light_vec[0].z) + _145;
   float _415 = dot(float3(_412, _413, _414), float3(_412, _413, _414));
   float _416 = rsqrt(_415);
   float _417 = _412 * _416;
   float _418 = -0.0f - _417;
   float _419 = _413 * _416;
   float _420 = -0.0f - _419;
   float _421 = _414 * _416;
   float _422 = -0.0f - _421;
   float _423 = dot(float3(_418, _420, _422), float3(_138, _139, _140));
   float _424 = saturate(_423);
   float _425 = _144 + -0.30000001192092896f;
   float _426 = dot(float3(_143, _425, _145), float3(_143, _425, _145));
   float _427 = rsqrt(_426);
   float _428 = _143 * _427;
   float _429 = -0.0f - _428;
   float _430 = _425 * _427;
   float _431 = -0.0f - _430;
   float _432 = _145 * _427;
   float _433 = -0.0f - _432;
   float _434 = dot(float3(_429, _431, _433), float3(_138, _139, _140));
   float _435 = saturate(_434);
   float _438 = log2(_424);
   float _439 = log2(_435);
   float _440 = _438 * c_mat_power;
   float _441 = _439 * c_mat_power;
   float _442 = exp2(_440);
   float _443 = exp2(_441);
   float _446 = _442 * (c_light_col[0].x);
   float _447 = _442 * (c_light_col[0].y);
   float _448 = _442 * (c_light_col[0].z);
   float _449 = _443 * 2.0f;
   float _450 = _449 * (c_light_vec[5].x);
   float _451 = _450 * (c_light_col[1].x);
   float _452 = _450 * (c_light_col[1].y);
   float _453 = _450 * (c_light_col[1].z);
   float _454 = _446 * _269;
   float _455 = _447 * _270;
   float _456 = _448 * _271;
   float _457 = _451 * _354;
   float _458 = _452 * _354;
   float _459 = _453 * _354;
   float _460 = _457 + _454;
   float _461 = _458 + _455;
   float _462 = _459 + _456;
   float _469 = c_mat_specular.x * c_specular_scale.x;
   float _470 = _469 * _273;
   float _471 = _470 * _460;
   float _472 = c_mat_specular.y * c_specular_scale.y;
   float _473 = _472 * _275;
   float _474 = _473 * _461;
   float _475 = c_mat_specular.z * c_specular_scale.z;
   float _476 = _475 * _277;
   float _477 = _476 * _462;
   float _478 = _343 * _307;
   float _479 = _478 * _443;
   float _480 = _344 * _308;
   float _481 = _480 * _443;
   float _482 = _345 * _309;
   float _483 = _482 * _443;
   float _484 = _471 + _479;
   float _485 = _474 + _481;
   float _486 = _477 + _483;
   if (!_327)
   {
      float _488 = _257 * _239;
      float _489 = _257 * _240;
      float _490 = _257 * _241;
      float _491 = _405 + _488;
      float _492 = _406 + _489;
      float _493 = _407 + _490;
      float _494 = _257 * _242;
      float _495 = _484 + _494;
      float _496 = _485 + _494;
      float _497 = _486 + _494;
      _499 = _491;
      _500 = _492;
      _501 = _493;
      _502 = _495;
      _503 = _496;
      _504 = _497;
   }
   else
   {
      _499 = _405;
      _500 = _406;
      _501 = _407;
      _502 = _484;
      _503 = _485;
      _504 = _486;
   }
   int _505 = c_bool & 128;
   bool _506 = (_505 != 0);
   float _507 = select(_506, 0.0f, _502);
   float _508 = select(_506, 0.0f, _503);
   float _509 = select(_506, 0.0f, _504);
   float _510 = _499 * _100;
   float _511 = _500 * _101;
   float _512 = _501 * _102;
   float _513 = _507 * _72.x;
   float _514 = _508 * _72.y;
   float _515 = _509 * _72.z;
   int _516 = c_bool & 32768;
   bool _517 = (_516 == 0);
   if (!_517)
   {
      float _521 = (c_ps_user[11].x) * _100;
      float _522 = (c_ps_user[11].x) * _101;
      float _523 = (c_ps_user[11].x) * _102;
      float _525 = _138 + 1.0f;
      float _526 = _139 + 1.0f;
      float _527 = _140 + 1.0f;
      float _528 = _525 * 0.5f;
      float _529 = _526 * 0.5f;
      float _530 = _527 * 0.5f;
      float _531 = saturate(_528);
      float _532 = saturate(_529);
      float _533 = saturate(_530);
      float _534 = (c_ps_user[11].y) * _531;
      float _535 = _532 * (c_ps_user[11].y);
      float _536 = _533 * (c_ps_user[11].y);
      float _538 = (c_ps_user[11].z) * _72.x;
      float _539 = (c_ps_user[11].z) * _72.y;
      float _540 = (c_ps_user[11].z) * _72.z;
      float _542 = c_mat_power * 0.009999999776482582f;
      float _543 = _542 * (c_ps_user[11].w);
      float _546 = (c_ps_user[12].x) * _269;
      float _547 = (c_ps_user[12].x) * _270;
      float _548 = (c_ps_user[12].x) * _271;
      float _550 = (c_ps_user[12].y) * _273;
      float _551 = (c_ps_user[12].y) * _275;
      float _552 = (c_ps_user[12].y) * _277;
      float _554 = (c_ps_user[12].z) * _307;
      float _555 = (c_ps_user[12].z) * _308;
      float _556 = (c_ps_user[12].z) * _309;
      float _562 = -0.0f - _143;
      float _563 = -0.0f - _144;
      float _564 = -0.0f - _145;
      float _565 = dot(float3(_562, _563, _564), float3(_138, _139, _140));
      float _566 = 1.0f - _565;
      float _567 = _566 * _566;
      float _568 = _566 * 0.9722222089767456f;
      float _569 = _567 * _567;
      float _570 = _569 * _568;
      float _571 = _570 + 0.02777777425944805f;
      float _572 = _571 * (c_ps_user[13].z);
      float _574 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _575 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _576 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _579 = (c_ps_user[14].x) * _321;
      float _580 = (c_ps_user[14].x) * _323;
      float _581 = (c_ps_user[14].x) * _325;
      float _583 = (c_ps_user[14].y) * _360;
      float _584 = (c_ps_user[14].y) * _361;
      float _585 = (c_ps_user[14].y) * _362;
      float _587 = (c_ps_user[14].z) * _367;
      float _588 = (c_ps_user[14].z) * _368;
      float _589 = (c_ps_user[14].z) * _369;
      float _591 = (c_ps_user[14].w) * _340;
      float _592 = (c_ps_user[14].w) * _341;
      float _593 = (c_ps_user[14].w) * _342;
      float _596 = (c_ps_user[15].x) * _446;
      float _597 = (c_ps_user[15].x) * _447;
      float _598 = (c_ps_user[15].x) * _448;
      float _600 = _469 * _451;
      float _601 = _600 * (c_ps_user[15].y);
      float _602 = _472 * _452;
      float _603 = _602 * (c_ps_user[15].y);
      float _604 = _475 * _453;
      float _605 = _604 * (c_ps_user[15].y);
      float _607 = (c_ps_user[15].z) * _343;
      float _608 = (c_ps_user[15].z) * _344;
      float _609 = (c_ps_user[15].z) * _345;
      float _610 = _601 + _607;
      float _611 = _610 * _443;
      float _612 = _583 + (c_ps_user[13].x);
      float _613 = _612 + _587;
      float _614 = c_diffuse_scale.x * _613;
      float _615 = _614 * c_mat_diffuse.x;
      float _616 = (c_ps_user[13].y) + _538;
      float _617 = _616 + _596;
      float _618 = c_mat_specular.x * _617;
      float _619 = _618 * c_specular_scale.x;
      float _620 = _534 + _521;
      float _621 = _620 + _543;
      float _622 = _621 + (c_ps_user[12].w);
      float _623 = _622 + _546;
      float _624 = _623 + _550;
      float _625 = _624 + _554;
      float _626 = _625 + _574;
      float _627 = _626 + _579;
      float _628 = _627 + _591;
      float _629 = _628 + _572;
      float _630 = _629 + _611;
      float _631 = _630 + _615;
      float _632 = _631 + _619;
      float _633 = _603 + _608;
      float _634 = _633 * _443;
      float _635 = _584 + (c_ps_user[13].x);
      float _636 = _635 + _588;
      float _637 = c_diffuse_scale.y * _636;
      float _638 = _637 * c_mat_diffuse.y;
      float _639 = (c_ps_user[13].y) + _539;
      float _640 = _639 + _597;
      float _641 = c_mat_specular.y * _640;
      float _642 = _641 * c_specular_scale.y;
      float _643 = _535 + _522;
      float _644 = _643 + _543;
      float _645 = _644 + (c_ps_user[12].w);
      float _646 = _645 + _547;
      float _647 = _646 + _551;
      float _648 = _647 + _555;
      float _649 = _648 + _575;
      float _650 = _649 + _580;
      float _651 = _650 + _592;
      float _652 = _651 + _572;
      float _653 = _652 + _634;
      float _654 = _653 + _638;
      float _655 = _654 + _642;
      float _656 = _605 + _609;
      float _657 = _656 * _443;
      float _658 = _585 + (c_ps_user[13].x);
      float _659 = _658 + _589;
      float _660 = c_diffuse_scale.z * _659;
      float _661 = _660 * c_mat_diffuse.z;
      float _662 = (c_ps_user[13].y) + _540;
      float _663 = _662 + _598;
      float _664 = c_mat_specular.z * _663;
      float _665 = _664 * c_specular_scale.z;
      float _666 = _543 + _523;
      float _667 = _666 + _536;
      float _668 = _667 + (c_ps_user[12].w);
      float _669 = _668 + _548;
      float _670 = _669 + _552;
      float _671 = _670 + _556;
      float _672 = _671 + _576;
      float _673 = _672 + _581;
      float _674 = _673 + _593;
      float _675 = _674 + _572;
      float _676 = _675 + _657;
      float _677 = _676 + _661;
      float _678 = _677 + _665;
      _680 = _632;
      _681 = _655;
      _682 = _678;
      _683 = 0.0f;
      _684 = 0.0f;
      _685 = 0.0f;
   }
   else
   {
      _680 = _510;
      _681 = _511;
      _682 = _512;
      _683 = _513;
      _684 = _514;
      _685 = _515;
   }
   float _686 = _683 + _680;
   float _687 = _684 + _681;
   float _688 = _685 + _682;
   float _689 = dot(float3(_143, _144, _145), float3(_138, _139, _140));
   float _690 = _689 * 2.0f;
   float _691 = _690 * _138;
   float _692 = _690 * _139;
   float _693 = _690 * _140;
   float _694 = _143 - _691;
   float _695 = _144 - _692;
   float _696 = _145 - _693;
   float _697 = -0.0f - _694;
   float4 _698 = s2c_t.Sample(s2c_s, float3(_697, _695, _696));
   float _705 = _184 * _183;
   float _706 = _153 + 0.75f;
   float _707 = saturate(_706);
   float _708 = _705 * _707;
   float _709 = max(_708, (c_light_vec[5].y));
   float _710 = max(_698.w, 0.003921568859368563f);
   float _711 = 0.25f / _710;
   float _712 = _711 * _698.x;
   float _713 = _711 * _698.y;
   float _714 = _711 * _698.z;
   float _715 = _72.w * TEXCOORD_2.w;
   float _716 = _715 * _90.x;
   float _717 = _709 * _716;
   float _718 = _717 * _201;
   float _719 = _712 * _712;
   float _720 = _719 * _718;
   float _721 = _717 * _202;
   float _722 = _713 * _713;
   float _723 = _722 * _721;
   float _724 = _717 * _203;
   float _725 = _714 * _714;
   float _726 = _725 * _724;
   float _727 = _686 + _720;
   float _728 = _687 + _723;
   float _729 = _688 + _726;
   float _736 = c_mat_opacity * _59.w;
   float _737 = _736 * c_opacity_scale;
   float _738 = _737 * c_global_opacity;
   int _741 = c_bool & 16;
   bool _742 = (_741 == 0);
   if (!_742)
   {
      float _746 = c_tone_control.x * _727;
      float _747 = c_tone_control.x * _728;
      float _748 = c_tone_control.x * _729;
      float _750 = log2(_746);
      float _751 = log2(_747);
      float _752 = log2(_748);
      float _753 = _750 * c_tone_control.y;
      float _754 = _751 * c_tone_control.y;
      float _755 = _752 * c_tone_control.y;
      float _756 = exp2(_753);
      float _757 = exp2(_754);
      float _758 = exp2(_755);
      _808 = _756;
      _809 = _757;
      _810 = _758;
   }
   else
   {
      int _760 = c_bool & 32768;
      bool _761 = (_760 == 0);
      if (_761)
      {
         float _765 = c_tone_control.x * _727;
         float _766 = c_tone_control.x * _728;
         float _767 = c_tone_control.x * _729;
         float _768 = _765 * 2.0f;
         float _769 = _766 * 2.0f;
         float _770 = _767 * 2.0f;
         float _771 = _765 * 0.30000001192092896f;
         float _772 = _766 * 0.30000001192092896f;
         float _773 = _767 * 0.30000001192092896f;
         float _774 = _771 + 0.05000000074505806f;
         float _775 = _772 + 0.05000000074505806f;
         float _776 = _773 + 0.05000000074505806f;
         float _777 = _774 * _768;
         float _778 = _775 * _769;
         float _779 = _776 * _770;
         float _780 = _777 + 0.004000000189989805f;
         float _781 = _778 + 0.004000000189989805f;
         float _782 = _779 + 0.004000000189989805f;
         float _783 = _771 + 0.5f;
         float _784 = _772 + 0.5f;
         float _785 = _773 + 0.5f;
         float _786 = _783 * _768;
         float _787 = _784 * _769;
         float _788 = _785 * _770;
         float _789 = _786 + 0.06000000238418579f;
         float _790 = _787 + 0.06000000238418579f;
         float _791 = _788 + 0.06000000238418579f;
         float _792 = _780 / _789;
         float _793 = _781 / _790;
         float _794 = _782 / _791;
         float _795 = _792 + -0.06666666269302368f;
         float _796 = _793 + -0.06666666269302368f;
         float _797 = _794 + -0.06666666269302368f;
         float _798 = _795 * 1.3790643215179443f;
         float _799 = _796 * 1.3790643215179443f;
         float _800 = _797 * 1.3790643215179443f;
         float _801 = YKR_MaterialCurve(_798, _765);
         float _802 = YKR_MaterialCurve(_799, _766);
         float _803 = YKR_MaterialCurve(_800, _767);
         float _804 = sqrt(_801);
         float _805 = sqrt(_802);
         float _806 = sqrt(_803);
         _808 = _804;
         _809 = _805;
         _810 = _806;
      }
      else
      {
         _808 = _727;
         _809 = _728;
         _810 = _729;
      }
   }
   int _811 = c_bool & 4;
   bool _812 = (_811 == 0);
   if (_812)
   {
      float _814 = saturate(COLOR_1.x);
      float _819 = (c_fog_param[0].x) - _808;
      float _820 = (c_fog_param[0].y) - _809;
      float _821 = (c_fog_param[0].z) - _810;
      float _822 = _819 * _814;
      float _823 = _820 * _814;
      float _824 = _821 * _814;
      _826 = _822;
      _827 = _823;
      _828 = _824;
   }
   else
   {
      _826 = COLOR_1.x;
      _827 = COLOR_1.y;
      _828 = COLOR_1.z;
   }
   float _829 = _826 + _808;
   float _830 = _827 + _809;
   float _831 = _828 + _810;
   float _837 = (c_ps_col_scl[0].x) * _829;
   float _838 = (c_ps_col_scl[0].y) * _830;
   float _839 = (c_ps_col_scl[0].z) * _831;
   float _840 = (c_ps_col_scl[0].w) * _738;
   SV_Target.x = _837;
   SV_Target.y = _838;
   SV_Target.z = _839;
   SV_Target.w = _840;
   int _843 = c_bool & 8;
   bool _844 = (_843 != 0);
   float _845 = select(_844, _829, _837);
   float _846 = select(_844, _830, _838);
   float _847 = select(_844, _831, _839);
   float _848 = select(_844, _738, _840);
   SV_Target.x = _845;
   SV_Target.y = _846;
   SV_Target.z = _847;
   SV_Target.w = _848;
   return SV_Target;
}
