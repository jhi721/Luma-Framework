// Yakuza Kiwami material r_o1du_m(vr)2d_b3d_m4d[ba] (0x2489DD2C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float _64 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _65 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _66 = s3_t.Sample(s3_s, float2(_64, _65));
   float4 _69 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _74 = _69.x * _69.x;
   float _75 = _69.y * _69.y;
   float _76 = _69.z * _69.z;
   float4 _77 = s4_t.Sample(s4_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _82 = _77.x * _77.x;
   float _83 = _77.y * _77.y;
   float _84 = _77.z * _77.z;
   float4 _85 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _88 = _69.w * COLOR_1.w;
   float _89 = _77.w * COLOR.w;
   float _90 = 1.0f - _74;
   float _91 = 1.0f - _75;
   float _92 = 1.0f - _76;
   float _93 = _90 * _88;
   float _94 = _91 * _88;
   float _95 = _92 * _88;
   float _96 = 1.0f - _93;
   float _97 = 1.0f - _94;
   float _98 = 1.0f - _95;
   float _99 = _58 * _96;
   float _100 = _59 * _97;
   float _101 = _60 * _98;
   float _102 = _82 - _99;
   float _103 = _83 - _100;
   float _104 = _84 - _101;
   float _105 = _102 * _89;
   float _106 = _103 * _89;
   float _107 = _104 * _89;
   float _108 = _105 + _99;
   float _109 = _106 + _100;
   float _110 = _107 + _101;
   float _111 = _88 * _66.y;
   float _112 = _66.y - _111;
   float _113 = _89 * _112;
   float _114 = _112 - _113;
   float _115 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _116 = rsqrt(_115);
   float _117 = _116 * TEXCOORD_2.x;
   float _118 = _116 * TEXCOORD_2.y;
   float _119 = _116 * TEXCOORD_2.z;
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
   float _218;
   float _219;
   float _220;
   float _221;
   float _235;
   float _236;
   float _282;
   float _283;
   float _284;
   float _319;
   float _320;
   float _321;
   float _322;
   float _323;
   float _324;
   float _478;
   float _479;
   float _480;
   float _481;
   float _482;
   float _483;
   float _655;
   float _656;
   float _657;
   float _658;
   float _659;
   float _660;
   float _737;
   float _738;
   float _739;
   float _755;
   float _756;
   float _757;
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
   float _170 = (c_sdw_param[1].x) * _85.y;
   float _172 = _170 + (c_sdw_param[1].y);
   float _173 = _172 * _85.x;
   float _174 = 1.0f - (c_light_col[5].x);
   float _175 = 1.0f - (c_light_col[5].y);
   float _176 = 1.0f - (c_light_col[5].z);
   float _177 = _173 * _174;
   float _178 = _173 * _175;
   float _179 = _173 * _176;
   float _180 = _177 + (c_light_col[5].x);
   float _181 = _178 + (c_light_col[5].y);
   float _182 = _179 + (c_light_col[5].z);
   float _186 = c_rt_size.z * SV_Position.x;
   float _187 = c_rt_size.w * SV_Position.y;
   float4 _188 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_186, _187)));
   int _195 = c_bool & 16384;
   bool _196 = (_195 == 0);
   if (!_196)
   {
      float _198 = _188.x * 2.0f;
      float _199 = _188.y * 2.0f;
      float _200 = _188.z * 2.0f;
      float _201 = _188.w * 2.0f;
      _218 = _198;
      _219 = _199;
      _220 = _200;
      _221 = _201;
   }
   else
   {
      int _203 = c_bool & 8192;
      bool _204 = (_203 == 0);
      if (!_204)
      {
         float _206 = _188.x * 2.0f;
         float _207 = _188.y * 2.0f;
         float _208 = _188.z * 2.0f;
         float _209 = _188.w * 2.0f;
         _218 = _206;
         _219 = _207;
         _220 = _208;
         _221 = _209;
      }
      else
      {
         float _213 = _188.x / c_tone_control.z;
         float _214 = _188.y / c_tone_control.z;
         float _215 = _188.z / c_tone_control.z;
         float _216 = _188.w / c_tone_control.z;
         _218 = _213;
         _219 = _214;
         _220 = _215;
         _221 = _216;
      }
   }
   int _222 = c_bool & 8;
   bool _223 = (_222 == 0);
   if (!_223)
   {
      float _228 = (c_sdw_param[0].x) * _162;
      float _229 = (c_sdw_param[0].z) * _163;
      float _232 = _228 + (c_sdw_param[0].y);
      float _233 = _229 + (c_sdw_param[0].w);
      _235 = _232;
      _236 = _233;
   }
   else
   {
      _235 = _162;
      _236 = _163;
   }
   float _237 = _236 * _235;
   float _242 = 1.0f - (c_light_col[5].x);
   float _243 = 1.0f - (c_light_col[5].y);
   float _244 = 1.0f - (c_light_col[5].z);
   float _245 = _242 * _237;
   float _246 = _243 * _237;
   float _247 = _244 * _237;
   float _248 = _245 + (c_light_col[5].x);
   float _249 = _246 + (c_light_col[5].y);
   float _250 = _247 + (c_light_col[5].z);
   float _251 = _150 * COLOR.x;
   float _252 = _251 * _180;
   float _253 = _150 * COLOR.y;
   float _254 = _253 * _181;
   float _255 = _150 * COLOR.z;
   float _256 = _255 * _182;
   float _257 = _248 * _180;
   float _258 = _249 * _181;
   float _259 = _250 * _182;
   if (!_223)
   {
      float _263 = (c_sdw_param[2].x) * _248;
      float _264 = (c_sdw_param[2].x) * _249;
      float _265 = (c_sdw_param[2].x) * _250;
      float _267 = _263 + (c_sdw_param[2].y);
      float _268 = _264 + (c_sdw_param[2].y);
      float _269 = _265 + (c_sdw_param[2].y);
      float _271 = (c_sdw_param[2].z) * _180;
      float _272 = (c_sdw_param[2].z) * _181;
      float _273 = (c_sdw_param[2].z) * _182;
      float _275 = _271 + (c_sdw_param[2].w);
      float _276 = _272 + (c_sdw_param[2].w);
      float _277 = _273 + (c_sdw_param[2].w);
      float _278 = _275 * _267;
      float _279 = _276 * _268;
      float _280 = _277 * _269;
      _282 = _278;
      _283 = _279;
      _284 = _280;
   }
   else
   {
      _282 = _257;
      _283 = _258;
      _284 = _259;
   }
   float _285 = _150 * _85.x;
   float _286 = _282 * _285;
   float _287 = _283 * _285;
   float _288 = _284 * _285;
   float _299 = c_mat_emissive.x * _108;
   float _300 = _299 * c_emissive_scale;
   float _301 = c_mat_emissive.y * _109;
   float _302 = _301 * c_emissive_scale;
   float _303 = c_mat_emissive.z * _110;
   float _304 = _303 * c_emissive_scale;
   int _305 = c_bool & 2048;
   bool _306 = (_305 == 0);
   if (_306)
   {
      float _308 = _236 * _218;
      float _309 = _236 * _219;
      float _310 = _236 * _220;
      float _311 = _308 + TEXCOORD_4.x;
      float _312 = _309 + TEXCOORD_4.y;
      float _313 = _310 + TEXCOORD_4.z;
      float _314 = _236 * _221;
      float _315 = _314 + TEXCOORD_5.x;
      float _316 = _314 + TEXCOORD_5.y;
      float _317 = _314 + TEXCOORD_5.z;
      _319 = _311;
      _320 = _312;
      _321 = _313;
      _322 = _315;
      _323 = _316;
      _324 = _317;
   }
   else
   {
      _319 = TEXCOORD_4.x;
      _320 = TEXCOORD_4.y;
      _321 = TEXCOORD_4.z;
      _322 = TEXCOORD_5.x;
      _323 = TEXCOORD_5.y;
      _324 = TEXCOORD_5.z;
   }
   float _329 = -0.0f - (c_light_vec[1].x);
   float _330 = -0.0f - (c_light_vec[1].y);
   float _331 = -0.0f - (c_light_vec[1].z);
   float _332 = dot(float3(_117, _118, _119), float3(_329, _330, _331));
   float _333 = saturate(_332);
   float _338 = saturate(_132);
   float _339 = (c_light_col[0].x) * _338;
   float _340 = (c_light_col[0].y) * _338;
   float _341 = (c_light_col[0].z) * _338;
   float _346 = (c_light_col[1].x) * _333;
   float _347 = (c_light_col[1].y) * _333;
   float _348 = (c_light_col[1].z) * _333;
   float _357 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _358 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _359 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _360 = _339 * _248;
   float _361 = _340 * _249;
   float _362 = _341 * _250;
   float _363 = _346 * _236;
   float _364 = _347 * _236;
   float _365 = _348 * _236;
   float _366 = _363 + _360;
   float _367 = _364 + _361;
   float _368 = _365 + _362;
   float _369 = _357 * _366;
   float _370 = _358 * _367;
   float _371 = _359 * _368;
   float _372 = _300 + c_scene_ambient.x;
   float _373 = _372 + _369;
   float _374 = _302 + c_scene_ambient.y;
   float _375 = _374 + _370;
   float _376 = _304 + c_scene_ambient.z;
   float _377 = _376 + _371;
   float _378 = _373 * _252;
   float _379 = _375 * _254;
   float _380 = _377 * _256;
   float _381 = _319 * _286;
   float _382 = _320 * _287;
   float _383 = _321 * _288;
   float _384 = _378 + _381;
   float _385 = _379 + _382;
   float _386 = _380 + _383;
   float _391 = (c_light_vec[0].x) + _122;
   float _392 = (c_light_vec[0].y) + _123;
   float _393 = (c_light_vec[0].z) + _124;
   float _394 = dot(float3(_391, _392, _393), float3(_391, _392, _393));
   float _395 = rsqrt(_394);
   float _396 = _391 * _395;
   float _397 = -0.0f - _396;
   float _398 = _392 * _395;
   float _399 = -0.0f - _398;
   float _400 = _393 * _395;
   float _401 = -0.0f - _400;
   float _402 = dot(float3(_397, _399, _401), float3(_117, _118, _119));
   float _403 = saturate(_402);
   float _404 = _123 + -0.30000001192092896f;
   float _405 = dot(float3(_122, _404, _124), float3(_122, _404, _124));
   float _406 = rsqrt(_405);
   float _407 = _122 * _406;
   float _408 = -0.0f - _407;
   float _409 = _404 * _406;
   float _410 = -0.0f - _409;
   float _411 = _124 * _406;
   float _412 = -0.0f - _411;
   float _413 = dot(float3(_408, _410, _412), float3(_117, _118, _119));
   float _414 = saturate(_413);
   float _417 = log2(_403);
   float _418 = log2(_414);
   float _419 = _417 * c_mat_power;
   float _420 = _418 * c_mat_power;
   float _421 = exp2(_419);
   float _422 = exp2(_420);
   float _425 = _421 * (c_light_col[0].x);
   float _426 = _421 * (c_light_col[0].y);
   float _427 = _421 * (c_light_col[0].z);
   float _428 = _422 * 2.0f;
   float _429 = _428 * (c_light_vec[5].x);
   float _430 = _429 * (c_light_col[1].x);
   float _431 = _429 * (c_light_col[1].y);
   float _432 = _429 * (c_light_col[1].z);
   float _433 = _425 * _248;
   float _434 = _426 * _249;
   float _435 = _427 * _250;
   float _436 = _430 * _333;
   float _437 = _431 * _333;
   float _438 = _432 * _333;
   float _439 = _436 + _433;
   float _440 = _437 + _434;
   float _441 = _438 + _435;
   float _448 = c_mat_specular.x * c_specular_scale.x;
   float _449 = _448 * _252;
   float _450 = _449 * _439;
   float _451 = c_mat_specular.y * c_specular_scale.y;
   float _452 = _451 * _254;
   float _453 = _452 * _440;
   float _454 = c_mat_specular.z * c_specular_scale.z;
   float _455 = _454 * _256;
   float _456 = _455 * _441;
   float _457 = _322 * _286;
   float _458 = _457 * _422;
   float _459 = _323 * _287;
   float _460 = _459 * _422;
   float _461 = _324 * _288;
   float _462 = _461 * _422;
   float _463 = _450 + _458;
   float _464 = _453 + _460;
   float _465 = _456 + _462;
   if (!_306)
   {
      float _467 = _236 * _218;
      float _468 = _236 * _219;
      float _469 = _236 * _220;
      float _470 = _384 + _467;
      float _471 = _385 + _468;
      float _472 = _386 + _469;
      float _473 = _236 * _221;
      float _474 = _463 + _473;
      float _475 = _464 + _473;
      float _476 = _465 + _473;
      _478 = _470;
      _479 = _471;
      _480 = _472;
      _481 = _474;
      _482 = _475;
      _483 = _476;
   }
   else
   {
      _478 = _384;
      _479 = _385;
      _480 = _386;
      _481 = _463;
      _482 = _464;
      _483 = _465;
   }
   int _484 = c_bool & 128;
   bool _485 = (_484 != 0);
   float _486 = select(_485, 0.0f, _481);
   float _487 = select(_485, 0.0f, _482);
   float _488 = select(_485, 0.0f, _483);
   float _489 = _478 * _108;
   float _490 = _479 * _109;
   float _491 = _480 * _110;
   float _492 = _486 * _114;
   float _493 = _487 * _114;
   float _494 = _488 * _114;
   int _495 = c_bool & 32768;
   bool _496 = (_495 == 0);
   if (!_496)
   {
      float _500 = (c_ps_user[11].x) * _108;
      float _501 = (c_ps_user[11].x) * _109;
      float _502 = (c_ps_user[11].x) * _110;
      float _504 = _117 + 1.0f;
      float _505 = _118 + 1.0f;
      float _506 = _119 + 1.0f;
      float _507 = _504 * 0.5f;
      float _508 = _505 * 0.5f;
      float _509 = _506 * 0.5f;
      float _510 = saturate(_507);
      float _511 = saturate(_508);
      float _512 = saturate(_509);
      float _513 = (c_ps_user[11].y) * _510;
      float _514 = _511 * (c_ps_user[11].y);
      float _515 = _512 * (c_ps_user[11].y);
      float _517 = (c_ps_user[11].z) * _114;
      float _519 = c_mat_power * 0.009999999776482582f;
      float _520 = _519 * (c_ps_user[11].w);
      float _523 = (c_ps_user[12].x) * _248;
      float _524 = (c_ps_user[12].x) * _249;
      float _525 = (c_ps_user[12].x) * _250;
      float _527 = (c_ps_user[12].y) * _252;
      float _528 = (c_ps_user[12].y) * _254;
      float _529 = (c_ps_user[12].y) * _256;
      float _531 = (c_ps_user[12].z) * _286;
      float _532 = (c_ps_user[12].z) * _287;
      float _533 = (c_ps_user[12].z) * _288;
      float _539 = -0.0f - _122;
      float _540 = -0.0f - _123;
      float _541 = -0.0f - _124;
      float _542 = dot(float3(_539, _540, _541), float3(_117, _118, _119));
      float _543 = 1.0f - _542;
      float _544 = _543 * _543;
      float _545 = _543 * 0.9722222089767456f;
      float _546 = _544 * _544;
      float _547 = _546 * _545;
      float _548 = _547 + 0.02777777425944805f;
      float _549 = _548 * (c_ps_user[13].z);
      float _551 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _552 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _553 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _556 = (c_ps_user[14].x) * _300;
      float _557 = (c_ps_user[14].x) * _302;
      float _558 = (c_ps_user[14].x) * _304;
      float _560 = (c_ps_user[14].y) * _339;
      float _561 = (c_ps_user[14].y) * _340;
      float _562 = (c_ps_user[14].y) * _341;
      float _564 = (c_ps_user[14].z) * _346;
      float _565 = (c_ps_user[14].z) * _347;
      float _566 = (c_ps_user[14].z) * _348;
      float _568 = (c_ps_user[14].w) * _319;
      float _569 = (c_ps_user[14].w) * _320;
      float _570 = (c_ps_user[14].w) * _321;
      float _573 = (c_ps_user[15].x) * _425;
      float _574 = (c_ps_user[15].x) * _426;
      float _575 = (c_ps_user[15].x) * _427;
      float _577 = _448 * _430;
      float _578 = _577 * (c_ps_user[15].y);
      float _579 = _451 * _431;
      float _580 = _579 * (c_ps_user[15].y);
      float _581 = _454 * _432;
      float _582 = _581 * (c_ps_user[15].y);
      float _584 = (c_ps_user[15].z) * _322;
      float _585 = (c_ps_user[15].z) * _323;
      float _586 = (c_ps_user[15].z) * _324;
      float _587 = _578 + _584;
      float _588 = _587 * _422;
      float _589 = _560 + (c_ps_user[13].x);
      float _590 = _589 + _564;
      float _591 = c_diffuse_scale.x * _590;
      float _592 = _591 * c_mat_diffuse.x;
      float _593 = (c_ps_user[13].y) + _517;
      float _594 = _593 + _573;
      float _595 = c_mat_specular.x * _594;
      float _596 = _595 * c_specular_scale.x;
      float _597 = _513 + _500;
      float _598 = _597 + _520;
      float _599 = _598 + (c_ps_user[12].w);
      float _600 = _599 + _523;
      float _601 = _600 + _527;
      float _602 = _601 + _531;
      float _603 = _602 + _551;
      float _604 = _603 + _556;
      float _605 = _604 + _568;
      float _606 = _605 + _549;
      float _607 = _606 + _588;
      float _608 = _607 + _592;
      float _609 = _608 + _596;
      float _610 = _580 + _585;
      float _611 = _610 * _422;
      float _612 = _561 + (c_ps_user[13].x);
      float _613 = _612 + _565;
      float _614 = c_diffuse_scale.y * _613;
      float _615 = _614 * c_mat_diffuse.y;
      float _616 = _593 + _574;
      float _617 = c_mat_specular.y * _616;
      float _618 = _617 * c_specular_scale.y;
      float _619 = _514 + _501;
      float _620 = _619 + _520;
      float _621 = _620 + (c_ps_user[12].w);
      float _622 = _621 + _524;
      float _623 = _622 + _528;
      float _624 = _623 + _532;
      float _625 = _624 + _552;
      float _626 = _625 + _557;
      float _627 = _626 + _569;
      float _628 = _627 + _549;
      float _629 = _628 + _611;
      float _630 = _629 + _615;
      float _631 = _630 + _618;
      float _632 = _582 + _586;
      float _633 = _632 * _422;
      float _634 = _562 + (c_ps_user[13].x);
      float _635 = _634 + _566;
      float _636 = c_diffuse_scale.z * _635;
      float _637 = _636 * c_mat_diffuse.z;
      float _638 = _593 + _575;
      float _639 = c_mat_specular.z * _638;
      float _640 = _639 * c_specular_scale.z;
      float _641 = _520 + _502;
      float _642 = _641 + _515;
      float _643 = _642 + (c_ps_user[12].w);
      float _644 = _643 + _525;
      float _645 = _644 + _529;
      float _646 = _645 + _533;
      float _647 = _646 + _553;
      float _648 = _647 + _558;
      float _649 = _648 + _570;
      float _650 = _649 + _549;
      float _651 = _650 + _633;
      float _652 = _651 + _637;
      float _653 = _652 + _640;
      _655 = _609;
      _656 = _631;
      _657 = _653;
      _658 = 0.0f;
      _659 = 0.0f;
      _660 = 0.0f;
   }
   else
   {
      _655 = _489;
      _656 = _490;
      _657 = _491;
      _658 = _492;
      _659 = _493;
      _660 = _494;
   }
   float _661 = _658 + _655;
   float _662 = _659 + _656;
   float _663 = _660 + _657;
   float _668 = _66.z * _53.w;
   float _669 = _668 * c_mat_opacity;
   float _670 = _669 * c_global_opacity;
   float _671 = _670 * c_opacity_scale;
   int _672 = c_bool & 16;
   bool _673 = (_672 == 0);
   if (!_673)
   {
      float _677 = c_tone_control.x * _661;
      float _678 = c_tone_control.x * _662;
      float _679 = c_tone_control.x * _663;
      float _681 = log2(_677);
      float _682 = log2(_678);
      float _683 = log2(_679);
      float _684 = _681 * c_tone_control.y;
      float _685 = _682 * c_tone_control.y;
      float _686 = _683 * c_tone_control.y;
      float _687 = exp2(_684);
      float _688 = exp2(_685);
      float _689 = exp2(_686);
      _737 = _687;
      _738 = _688;
      _739 = _689;
   }
   else
   {
      if (_496)
      {
         float _694 = c_tone_control.x * _661;
         float _695 = c_tone_control.x * _662;
         float _696 = c_tone_control.x * _663;
         float _697 = _694 * 2.0f;
         float _698 = _695 * 2.0f;
         float _699 = _696 * 2.0f;
         float _700 = _694 * 0.30000001192092896f;
         float _701 = _695 * 0.30000001192092896f;
         float _702 = _696 * 0.30000001192092896f;
         float _703 = _700 + 0.05000000074505806f;
         float _704 = _701 + 0.05000000074505806f;
         float _705 = _702 + 0.05000000074505806f;
         float _706 = _703 * _697;
         float _707 = _704 * _698;
         float _708 = _705 * _699;
         float _709 = _706 + 0.004000000189989805f;
         float _710 = _707 + 0.004000000189989805f;
         float _711 = _708 + 0.004000000189989805f;
         float _712 = _700 + 0.5f;
         float _713 = _701 + 0.5f;
         float _714 = _702 + 0.5f;
         float _715 = _712 * _697;
         float _716 = _713 * _698;
         float _717 = _714 * _699;
         float _718 = _715 + 0.06000000238418579f;
         float _719 = _716 + 0.06000000238418579f;
         float _720 = _717 + 0.06000000238418579f;
         float _721 = _709 / _718;
         float _722 = _710 / _719;
         float _723 = _711 / _720;
         float _724 = _721 + -0.06666666269302368f;
         float _725 = _722 + -0.06666666269302368f;
         float _726 = _723 + -0.06666666269302368f;
         float _727 = _724 * 1.3790643215179443f;
         float _728 = _725 * 1.3790643215179443f;
         float _729 = _726 * 1.3790643215179443f;
         float _730 = YKR_MaterialCurve(_727, _694);
         float _731 = YKR_MaterialCurve(_728, _695);
         float _732 = YKR_MaterialCurve(_729, _696);
         float _733 = sqrt(_730);
         float _734 = sqrt(_731);
         float _735 = sqrt(_732);
         _737 = _733;
         _738 = _734;
         _739 = _735;
      }
      else
      {
         _737 = _661;
         _738 = _662;
         _739 = _663;
      }
   }
   int _740 = c_bool & 4;
   bool _741 = (_740 == 0);
   if (_741)
   {
      float _743 = saturate(COLOR_1.x);
      float _748 = (c_fog_param[0].x) - _737;
      float _749 = (c_fog_param[0].y) - _738;
      float _750 = (c_fog_param[0].z) - _739;
      float _751 = _748 * _743;
      float _752 = _749 * _743;
      float _753 = _750 * _743;
      _755 = _751;
      _756 = _752;
      _757 = _753;
   }
   else
   {
      _755 = COLOR_1.x;
      _756 = COLOR_1.y;
      _757 = COLOR_1.z;
   }
   float _758 = _755 + _737;
   float _759 = _756 + _738;
   float _760 = _757 + _739;
   float _766 = (c_ps_col_scl[0].x) * _758;
   float _767 = (c_ps_col_scl[0].y) * _759;
   float _768 = (c_ps_col_scl[0].z) * _760;
   float _769 = (c_ps_col_scl[0].w) * _671;
   SV_Target.x = _766;
   SV_Target.y = _767;
   SV_Target.z = _768;
   SV_Target.w = _769;
   int _772 = c_bool & 8;
   bool _773 = (_772 != 0);
   float _774 = select(_773, _758, _766);
   float _775 = select(_773, _759, _767);
   float _776 = select(_773, _760, _768);
   float _777 = select(_773, _671, _769);
   SV_Target.x = _774;
   SV_Target.y = _775;
   SV_Target.z = _776;
   SV_Target.w = _777;
   return SV_Target;
}
