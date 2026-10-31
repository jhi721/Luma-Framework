// Yakuza Kiwami material r_b1dst_m(vr)2dst (0x6E8745C0, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

SamplerState s6_s : register(s6);

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
   float4 _55 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _60 = _55.x * _55.x;
   float _61 = _55.y * _55.y;
   float _62 = _55.z * _55.z;
   float _66 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _67 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _68 = s3_t.Sample(s3_s, float2(_66, _67));
   float _75 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _76 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _77 = s5_t.Sample(s5_s, float2(_75, _76));
   float4 _80 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _84 = _80.x * _80.x;
   float _85 = _80.y * _80.y;
   float _86 = _80.z * _80.z;
   float _90 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _91 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _92 = s4_t.Sample(s4_s, float2(_90, _91));
   float _99 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _100 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _101 = s6_t.Sample(s6_s, float2(_99, _100));
   float _104 = 1.0f - _84;
   float _105 = 1.0f - _85;
   float _106 = 1.0f - _86;
   float _107 = _104 * COLOR_1.w;
   float _108 = _105 * COLOR_1.w;
   float _109 = _106 * COLOR_1.w;
   float _110 = 1.0f - _107;
   float _111 = 1.0f - _108;
   float _112 = 1.0f - _109;
   float _113 = _60 * _110;
   float _114 = _61 * _111;
   float _115 = _62 * _112;
   float _116 = 1.0f - _92.x;
   float _117 = 1.0f - _92.y;
   float _118 = 1.0f - _92.z;
   float _119 = _116 * COLOR_1.w;
   float _120 = _117 * COLOR_1.w;
   float _121 = _118 * COLOR_1.w;
   float _122 = 1.0f - _119;
   float _123 = 1.0f - _120;
   float _124 = 1.0f - _121;
   float _125 = _122 * _68.x;
   float _126 = _123 * _68.y;
   float _127 = _124 * _68.z;
   float _128 = _101.x - _77.x;
   float _129 = _101.y - _77.y;
   float _130 = _128 * COLOR_1.w;
   float _131 = _129 * COLOR_1.w;
   float _132 = _130 + _77.x;
   float _133 = _131 + _77.y;
   float _134 = _132 * 2.0f;
   float _135 = _133 * 2.0f;
   float _136 = _134 + -1.0f;
   float _137 = _135 + -1.0f;
   float _138 = dot(float2(_136, _137), float2(_136, _137));
   float _139 = 1.0f - _138;
   float _140 = saturate(_139);
   float _141 = sqrt(_140);
   float _142 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _143 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _144 = _142 - _143;
   float _145 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _146 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _147 = _145 - _146;
   float _148 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _149 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _150 = _148 - _149;
   float _151 = _136 * TEXCOORD_6.x;
   float _152 = _136 * TEXCOORD_6.y;
   float _153 = _136 * TEXCOORD_6.z;
   float _154 = _137 * TEXCOORD_6.w;
   float _155 = _154 * _144;
   float _156 = _154 * _147;
   float _157 = _154 * _150;
   float _158 = _141 * TEXCOORD_2.x;
   float _159 = _141 * TEXCOORD_2.y;
   float _160 = _141 * TEXCOORD_2.z;
   float _161 = _151 + _158;
   float _162 = _161 + _155;
   float _163 = _152 + _159;
   float _164 = _163 + _156;
   float _165 = _153 + _160;
   float _166 = _165 + _157;
   float _167 = dot(float3(_162, _164, _166), float3(_162, _164, _166));
   float _168 = rsqrt(_167);
   float _169 = _162 * _168;
   float _170 = _164 * _168;
   float _171 = _166 * _168;
   float _172 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _173 = rsqrt(_172);
   float _174 = _173 * TEXCOORD_3.x;
   float _175 = _173 * TEXCOORD_3.y;
   float _176 = _173 * TEXCOORD_3.z;
   float _181 = -0.0f - (c_light_vec[0].x);
   float _182 = -0.0f - (c_light_vec[0].y);
   float _183 = -0.0f - (c_light_vec[0].z);
   float _184 = dot(float3(_169, _170, _171), float3(_181, _182, _183));
   float _188 = c_rt_size.z * SV_Position.x;
   float _189 = c_rt_size.w * SV_Position.y;
   int _192 = c_bool & 8;
   bool _193 = (_192 == 0);
   float _200;
   float _201;
   float _202;
   float _214;
   float _215;
   float _251;
   float _252;
   float _253;
   float _254;
   float _268;
   float _269;
   float _304;
   float _305;
   float _306;
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
   float _762;
   float _763;
   float _764;
   float _780;
   float _781;
   float _782;
   if (!_193)
   {
      float4 _195 = s13_t.Sample(s13_s, float2(_188, _189));
      _200 = _195.x;
      _201 = _195.y;
      _202 = _195.w;
   }
   else
   {
      _200 = 1.0f;
      _201 = 1.0f;
      _202 = 1.0f;
   }
   if (!_193)
   {
      float _207 = (c_sdw_param[0].x) * _200;
      float _208 = (c_sdw_param[0].z) * _201;
      float _211 = _207 + (c_sdw_param[0].y);
      float _212 = _208 + (c_sdw_param[0].w);
      _214 = _211;
      _215 = _212;
   }
   else
   {
      _214 = _200;
      _215 = _201;
   }
   float _219 = c_rt_size.z * SV_Position.x;
   float _220 = c_rt_size.w * SV_Position.y;
   float4 _221 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_219, _220)));
   int _228 = c_bool & 16384;
   bool _229 = (_228 == 0);
   if (!_229)
   {
      float _231 = _221.x * 2.0f;
      float _232 = _221.y * 2.0f;
      float _233 = _221.z * 2.0f;
      float _234 = _221.w * 2.0f;
      _251 = _231;
      _252 = _232;
      _253 = _233;
      _254 = _234;
   }
   else
   {
      int _236 = c_bool & 8192;
      bool _237 = (_236 == 0);
      if (!_237)
      {
         float _239 = _221.x * 2.0f;
         float _240 = _221.y * 2.0f;
         float _241 = _221.z * 2.0f;
         float _242 = _221.w * 2.0f;
         _251 = _239;
         _252 = _240;
         _253 = _241;
         _254 = _242;
      }
      else
      {
         float _246 = _221.x / c_tone_control.z;
         float _247 = _221.y / c_tone_control.z;
         float _248 = _221.z / c_tone_control.z;
         float _249 = _221.w / c_tone_control.z;
         _251 = _246;
         _252 = _247;
         _253 = _248;
         _254 = _249;
      }
   }
   int _255 = c_bool & 8;
   bool _256 = (_255 == 0);
   if (!_256)
   {
      float _261 = (c_sdw_param[0].x) * _214;
      float _262 = (c_sdw_param[0].z) * _215;
      float _265 = _261 + (c_sdw_param[0].y);
      float _266 = _262 + (c_sdw_param[0].w);
      _268 = _265;
      _269 = _266;
   }
   else
   {
      _268 = _214;
      _269 = _215;
   }
   float _270 = _269 * _268;
   float _275 = 1.0f - (c_light_col[5].x);
   float _276 = 1.0f - (c_light_col[5].y);
   float _277 = 1.0f - (c_light_col[5].z);
   float _278 = _275 * _270;
   float _279 = _276 * _270;
   float _280 = _277 * _270;
   float _281 = _278 + (c_light_col[5].x);
   float _282 = _279 + (c_light_col[5].y);
   float _283 = _280 + (c_light_col[5].z);
   float _284 = _202 * COLOR.x;
   float _285 = _202 * COLOR.y;
   float _286 = _202 * COLOR.z;
   if (!_256)
   {
      float _290 = (c_sdw_param[2].x) * _281;
      float _291 = (c_sdw_param[2].x) * _282;
      float _292 = (c_sdw_param[2].x) * _283;
      float _294 = _290 + (c_sdw_param[2].y);
      float _295 = _291 + (c_sdw_param[2].y);
      float _296 = _292 + (c_sdw_param[2].y);
      float _299 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _300 = _294 * _299;
      float _301 = _295 * _299;
      float _302 = _296 * _299;
      _304 = _300;
      _305 = _301;
      _306 = _302;
   }
   else
   {
      _304 = _281;
      _305 = _282;
      _306 = _283;
   }
   float _307 = _304 * _202;
   float _308 = _305 * _202;
   float _309 = _306 * _202;
   float _320 = c_mat_emissive.x * _113;
   float _321 = _320 * c_emissive_scale;
   float _322 = c_mat_emissive.y * _114;
   float _323 = _322 * c_emissive_scale;
   float _324 = c_mat_emissive.z * _115;
   float _325 = _324 * c_emissive_scale;
   int _326 = c_bool & 2048;
   bool _327 = (_326 == 0);
   if (_327)
   {
      float _329 = _269 * _251;
      float _330 = _269 * _252;
      float _331 = _269 * _253;
      float _332 = _329 + TEXCOORD_4.x;
      float _333 = _330 + TEXCOORD_4.y;
      float _334 = _331 + TEXCOORD_4.z;
      float _335 = _269 * _254;
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
   float _353 = dot(float3(_169, _170, _171), float3(_350, _351, _352));
   float _354 = saturate(_353);
   float _359 = saturate(_184);
   float _360 = (c_light_col[0].x) * _359;
   float _361 = (c_light_col[0].y) * _359;
   float _362 = (c_light_col[0].z) * _359;
   float _367 = (c_light_col[1].x) * _354;
   float _368 = (c_light_col[1].y) * _354;
   float _369 = (c_light_col[1].z) * _354;
   float _378 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _379 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _380 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _381 = _360 * _281;
   float _382 = _361 * _282;
   float _383 = _362 * _283;
   float _384 = _367 * _269;
   float _385 = _368 * _269;
   float _386 = _369 * _269;
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
   float _399 = _394 * _284;
   float _400 = _396 * _285;
   float _401 = _398 * _286;
   float _402 = _340 * _307;
   float _403 = _341 * _308;
   float _404 = _342 * _309;
   float _405 = _399 + _402;
   float _406 = _400 + _403;
   float _407 = _401 + _404;
   float _412 = (c_light_vec[0].x) + _174;
   float _413 = (c_light_vec[0].y) + _175;
   float _414 = (c_light_vec[0].z) + _176;
   float _415 = dot(float3(_412, _413, _414), float3(_412, _413, _414));
   float _416 = rsqrt(_415);
   float _417 = _412 * _416;
   float _418 = -0.0f - _417;
   float _419 = _413 * _416;
   float _420 = -0.0f - _419;
   float _421 = _414 * _416;
   float _422 = -0.0f - _421;
   float _423 = dot(float3(_418, _420, _422), float3(_169, _170, _171));
   float _424 = saturate(_423);
   float _425 = _175 + -0.30000001192092896f;
   float _426 = dot(float3(_174, _425, _176), float3(_174, _425, _176));
   float _427 = rsqrt(_426);
   float _428 = _174 * _427;
   float _429 = -0.0f - _428;
   float _430 = _425 * _427;
   float _431 = -0.0f - _430;
   float _432 = _176 * _427;
   float _433 = -0.0f - _432;
   float _434 = dot(float3(_429, _431, _433), float3(_169, _170, _171));
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
   float _454 = _446 * _281;
   float _455 = _447 * _282;
   float _456 = _448 * _283;
   float _457 = _451 * _354;
   float _458 = _452 * _354;
   float _459 = _453 * _354;
   float _460 = _457 + _454;
   float _461 = _458 + _455;
   float _462 = _459 + _456;
   float _469 = c_mat_specular.x * c_specular_scale.x;
   float _470 = _469 * _284;
   float _471 = _470 * _460;
   float _472 = c_mat_specular.y * c_specular_scale.y;
   float _473 = _472 * _285;
   float _474 = _473 * _461;
   float _475 = c_mat_specular.z * c_specular_scale.z;
   float _476 = _475 * _286;
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
      float _488 = _269 * _251;
      float _489 = _269 * _252;
      float _490 = _269 * _253;
      float _491 = _405 + _488;
      float _492 = _406 + _489;
      float _493 = _407 + _490;
      float _494 = _269 * _254;
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
   float _510 = _499 * _113;
   float _511 = _500 * _114;
   float _512 = _501 * _115;
   float _513 = _507 * _125;
   float _514 = _508 * _126;
   float _515 = _509 * _127;
   int _516 = c_bool & 32768;
   bool _517 = (_516 == 0);
   if (!_517)
   {
      float _521 = (c_ps_user[11].x) * _113;
      float _522 = (c_ps_user[11].x) * _114;
      float _523 = (c_ps_user[11].x) * _115;
      float _525 = _169 + 1.0f;
      float _526 = _170 + 1.0f;
      float _527 = _171 + 1.0f;
      float _528 = _525 * 0.5f;
      float _529 = _526 * 0.5f;
      float _530 = _527 * 0.5f;
      float _531 = saturate(_528);
      float _532 = saturate(_529);
      float _533 = saturate(_530);
      float _534 = (c_ps_user[11].y) * _531;
      float _535 = _532 * (c_ps_user[11].y);
      float _536 = _533 * (c_ps_user[11].y);
      float _538 = (c_ps_user[11].z) * _125;
      float _539 = (c_ps_user[11].z) * _126;
      float _540 = (c_ps_user[11].z) * _127;
      float _542 = c_mat_power * 0.009999999776482582f;
      float _543 = _542 * (c_ps_user[11].w);
      float _546 = (c_ps_user[12].x) * _281;
      float _547 = (c_ps_user[12].x) * _282;
      float _548 = (c_ps_user[12].x) * _283;
      float _550 = (c_ps_user[12].y) * _284;
      float _551 = (c_ps_user[12].y) * _285;
      float _552 = (c_ps_user[12].y) * _286;
      float _554 = (c_ps_user[12].z) * _307;
      float _555 = (c_ps_user[12].z) * _308;
      float _556 = (c_ps_user[12].z) * _309;
      float _562 = -0.0f - _174;
      float _563 = -0.0f - _175;
      float _564 = -0.0f - _176;
      float _565 = dot(float3(_562, _563, _564), float3(_169, _170, _171));
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
   float _693 = _55.w * COLOR.w;
   float _694 = _693 * c_mat_opacity;
   float _695 = _694 * c_global_opacity;
   float _696 = _695 * c_opacity_scale;
   int _697 = c_bool & 16;
   bool _698 = (_697 == 0);
   if (!_698)
   {
      float _702 = c_tone_control.x * _686;
      float _703 = c_tone_control.x * _687;
      float _704 = c_tone_control.x * _688;
      float _706 = log2(_702);
      float _707 = log2(_703);
      float _708 = log2(_704);
      float _709 = _706 * c_tone_control.y;
      float _710 = _707 * c_tone_control.y;
      float _711 = _708 * c_tone_control.y;
      float _712 = exp2(_709);
      float _713 = exp2(_710);
      float _714 = exp2(_711);
      _762 = _712;
      _763 = _713;
      _764 = _714;
   }
   else
   {
      if (_517)
      {
         float _719 = c_tone_control.x * _686;
         float _720 = c_tone_control.x * _687;
         float _721 = c_tone_control.x * _688;
         float _722 = _719 * 2.0f;
         float _723 = _720 * 2.0f;
         float _724 = _721 * 2.0f;
         float _725 = _719 * 0.30000001192092896f;
         float _726 = _720 * 0.30000001192092896f;
         float _727 = _721 * 0.30000001192092896f;
         float _728 = _725 + 0.05000000074505806f;
         float _729 = _726 + 0.05000000074505806f;
         float _730 = _727 + 0.05000000074505806f;
         float _731 = _728 * _722;
         float _732 = _729 * _723;
         float _733 = _730 * _724;
         float _734 = _731 + 0.004000000189989805f;
         float _735 = _732 + 0.004000000189989805f;
         float _736 = _733 + 0.004000000189989805f;
         float _737 = _725 + 0.5f;
         float _738 = _726 + 0.5f;
         float _739 = _727 + 0.5f;
         float _740 = _737 * _722;
         float _741 = _738 * _723;
         float _742 = _739 * _724;
         float _743 = _740 + 0.06000000238418579f;
         float _744 = _741 + 0.06000000238418579f;
         float _745 = _742 + 0.06000000238418579f;
         float _746 = _734 / _743;
         float _747 = _735 / _744;
         float _748 = _736 / _745;
         float _749 = _746 + -0.06666666269302368f;
         float _750 = _747 + -0.06666666269302368f;
         float _751 = _748 + -0.06666666269302368f;
         float _752 = _749 * 1.3790643215179443f;
         float _753 = _750 * 1.3790643215179443f;
         float _754 = _751 * 1.3790643215179443f;
         float _755 = YKR_MaterialCurve(_752, _719);
         float _756 = YKR_MaterialCurve(_753, _720);
         float _757 = YKR_MaterialCurve(_754, _721);
         float _758 = sqrt(_755);
         float _759 = sqrt(_756);
         float _760 = sqrt(_757);
         _762 = _758;
         _763 = _759;
         _764 = _760;
      }
      else
      {
         _762 = _686;
         _763 = _687;
         _764 = _688;
      }
   }
   int _765 = c_bool & 4;
   bool _766 = (_765 == 0);
   if (_766)
   {
      float _768 = saturate(COLOR_1.x);
      float _773 = (c_fog_param[0].x) - _762;
      float _774 = (c_fog_param[0].y) - _763;
      float _775 = (c_fog_param[0].z) - _764;
      float _776 = _773 * _768;
      float _777 = _774 * _768;
      float _778 = _775 * _768;
      _780 = _776;
      _781 = _777;
      _782 = _778;
   }
   else
   {
      _780 = COLOR_1.x;
      _781 = COLOR_1.y;
      _782 = COLOR_1.z;
   }
   float _783 = _780 + _762;
   float _784 = _781 + _763;
   float _785 = _782 + _764;
   float _788 = _696 - c_mat_punchthrough_ref;
   bool _789 = (_788 < 0.0f);
   if (_789)
      discard;
   float _795 = (c_ps_col_scl[0].x) * _783;
   float _796 = (c_ps_col_scl[0].y) * _784;
   float _797 = (c_ps_col_scl[0].z) * _785;
   float _798 = (c_ps_col_scl[0].w) * _696;
   SV_Target.x = _795;
   SV_Target.y = _796;
   SV_Target.z = _797;
   SV_Target.w = _798;
   int _801 = c_bool & 8;
   bool _802 = (_801 != 0);
   float _803 = select(_802, _783, _795);
   float _804 = select(_802, _784, _796);
   float _805 = select(_802, _785, _797);
   float _806 = select(_802, _696, _798);
   SV_Target.x = _803;
   SV_Target.y = _804;
   SV_Target.z = _805;
   SV_Target.w = _806;
   return SV_Target;
}
