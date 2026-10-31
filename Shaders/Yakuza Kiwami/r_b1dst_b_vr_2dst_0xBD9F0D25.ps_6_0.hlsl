// Yakuza Kiwami material r_b1dst_b(vr)2dst (0xBD9F0D25, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _85 = _80.x * _80.x;
   float _86 = _80.y * _80.y;
   float _87 = _80.z * _80.z;
   float _91 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _92 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _93 = s4_t.Sample(s4_s, float2(_91, _92));
   float _100 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _101 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _102 = s6_t.Sample(s6_s, float2(_100, _101));
   float _105 = _80.w * COLOR_1.w;
   float _106 = _85 - _60;
   float _107 = _86 - _61;
   float _108 = _87 - _62;
   float _109 = _106 * _105;
   float _110 = _107 * _105;
   float _111 = _108 * _105;
   float _112 = _109 + _60;
   float _113 = _110 + _61;
   float _114 = _111 + _62;
   float _115 = _93.x - _68.x;
   float _116 = _93.y - _68.y;
   float _117 = _93.z - _68.z;
   float _118 = _115 * _105;
   float _119 = _116 * _105;
   float _120 = _117 * _105;
   float _121 = _118 + _68.x;
   float _122 = _119 + _68.y;
   float _123 = _120 + _68.z;
   float _124 = _102.x - _77.x;
   float _125 = _102.y - _77.y;
   float _126 = _124 * _105;
   float _127 = _125 * _105;
   float _128 = _126 + _77.x;
   float _129 = _127 + _77.y;
   float _130 = _128 * 2.0f;
   float _131 = _129 * 2.0f;
   float _132 = _130 + -1.0f;
   float _133 = _131 + -1.0f;
   float _134 = dot(float2(_132, _133), float2(_132, _133));
   float _135 = 1.0f - _134;
   float _136 = saturate(_135);
   float _137 = sqrt(_136);
   float _138 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _139 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _140 = _138 - _139;
   float _141 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _142 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _143 = _141 - _142;
   float _144 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _145 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _146 = _144 - _145;
   float _147 = _132 * TEXCOORD_6.x;
   float _148 = _132 * TEXCOORD_6.y;
   float _149 = _132 * TEXCOORD_6.z;
   float _150 = _133 * TEXCOORD_6.w;
   float _151 = _150 * _140;
   float _152 = _150 * _143;
   float _153 = _150 * _146;
   float _154 = _137 * TEXCOORD_2.x;
   float _155 = _137 * TEXCOORD_2.y;
   float _156 = _137 * TEXCOORD_2.z;
   float _157 = _147 + _154;
   float _158 = _157 + _151;
   float _159 = _148 + _155;
   float _160 = _159 + _152;
   float _161 = _149 + _156;
   float _162 = _161 + _153;
   float _163 = dot(float3(_158, _160, _162), float3(_158, _160, _162));
   float _164 = rsqrt(_163);
   float _165 = _158 * _164;
   float _166 = _160 * _164;
   float _167 = _162 * _164;
   float _168 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _169 = rsqrt(_168);
   float _170 = _169 * TEXCOORD_3.x;
   float _171 = _169 * TEXCOORD_3.y;
   float _172 = _169 * TEXCOORD_3.z;
   float _177 = -0.0f - (c_light_vec[0].x);
   float _178 = -0.0f - (c_light_vec[0].y);
   float _179 = -0.0f - (c_light_vec[0].z);
   float _180 = dot(float3(_165, _166, _167), float3(_177, _178, _179));
   float _184 = c_rt_size.z * SV_Position.x;
   float _185 = c_rt_size.w * SV_Position.y;
   int _188 = c_bool & 8;
   bool _189 = (_188 == 0);
   float _196;
   float _197;
   float _198;
   float _210;
   float _211;
   float _247;
   float _248;
   float _249;
   float _250;
   float _264;
   float _265;
   float _300;
   float _301;
   float _302;
   float _336;
   float _337;
   float _338;
   float _339;
   float _340;
   float _341;
   float _495;
   float _496;
   float _497;
   float _498;
   float _499;
   float _500;
   float _676;
   float _677;
   float _678;
   float _679;
   float _680;
   float _681;
   float _758;
   float _759;
   float _760;
   float _776;
   float _777;
   float _778;
   if (!_189)
   {
      float4 _191 = s13_t.Sample(s13_s, float2(_184, _185));
      _196 = _191.x;
      _197 = _191.y;
      _198 = _191.w;
   }
   else
   {
      _196 = 1.0f;
      _197 = 1.0f;
      _198 = 1.0f;
   }
   if (!_189)
   {
      float _203 = (c_sdw_param[0].x) * _196;
      float _204 = (c_sdw_param[0].z) * _197;
      float _207 = _203 + (c_sdw_param[0].y);
      float _208 = _204 + (c_sdw_param[0].w);
      _210 = _207;
      _211 = _208;
   }
   else
   {
      _210 = _196;
      _211 = _197;
   }
   float _215 = c_rt_size.z * SV_Position.x;
   float _216 = c_rt_size.w * SV_Position.y;
   float4 _217 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_215, _216)));
   int _224 = c_bool & 16384;
   bool _225 = (_224 == 0);
   if (!_225)
   {
      float _227 = _217.x * 2.0f;
      float _228 = _217.y * 2.0f;
      float _229 = _217.z * 2.0f;
      float _230 = _217.w * 2.0f;
      _247 = _227;
      _248 = _228;
      _249 = _229;
      _250 = _230;
   }
   else
   {
      int _232 = c_bool & 8192;
      bool _233 = (_232 == 0);
      if (!_233)
      {
         float _235 = _217.x * 2.0f;
         float _236 = _217.y * 2.0f;
         float _237 = _217.z * 2.0f;
         float _238 = _217.w * 2.0f;
         _247 = _235;
         _248 = _236;
         _249 = _237;
         _250 = _238;
      }
      else
      {
         float _242 = _217.x / c_tone_control.z;
         float _243 = _217.y / c_tone_control.z;
         float _244 = _217.z / c_tone_control.z;
         float _245 = _217.w / c_tone_control.z;
         _247 = _242;
         _248 = _243;
         _249 = _244;
         _250 = _245;
      }
   }
   int _251 = c_bool & 8;
   bool _252 = (_251 == 0);
   if (!_252)
   {
      float _257 = (c_sdw_param[0].x) * _210;
      float _258 = (c_sdw_param[0].z) * _211;
      float _261 = _257 + (c_sdw_param[0].y);
      float _262 = _258 + (c_sdw_param[0].w);
      _264 = _261;
      _265 = _262;
   }
   else
   {
      _264 = _210;
      _265 = _211;
   }
   float _266 = _265 * _264;
   float _271 = 1.0f - (c_light_col[5].x);
   float _272 = 1.0f - (c_light_col[5].y);
   float _273 = 1.0f - (c_light_col[5].z);
   float _274 = _271 * _266;
   float _275 = _272 * _266;
   float _276 = _273 * _266;
   float _277 = _274 + (c_light_col[5].x);
   float _278 = _275 + (c_light_col[5].y);
   float _279 = _276 + (c_light_col[5].z);
   float _280 = _198 * COLOR.x;
   float _281 = _198 * COLOR.y;
   float _282 = _198 * COLOR.z;
   if (!_252)
   {
      float _286 = (c_sdw_param[2].x) * _277;
      float _287 = (c_sdw_param[2].x) * _278;
      float _288 = (c_sdw_param[2].x) * _279;
      float _290 = _286 + (c_sdw_param[2].y);
      float _291 = _287 + (c_sdw_param[2].y);
      float _292 = _288 + (c_sdw_param[2].y);
      float _295 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _296 = _290 * _295;
      float _297 = _291 * _295;
      float _298 = _292 * _295;
      _300 = _296;
      _301 = _297;
      _302 = _298;
   }
   else
   {
      _300 = _277;
      _301 = _278;
      _302 = _279;
   }
   float _303 = _300 * _198;
   float _304 = _301 * _198;
   float _305 = _302 * _198;
   float _316 = c_mat_emissive.x * _112;
   float _317 = _316 * c_emissive_scale;
   float _318 = c_mat_emissive.y * _113;
   float _319 = _318 * c_emissive_scale;
   float _320 = c_mat_emissive.z * _114;
   float _321 = _320 * c_emissive_scale;
   int _322 = c_bool & 2048;
   bool _323 = (_322 == 0);
   if (_323)
   {
      float _325 = _265 * _247;
      float _326 = _265 * _248;
      float _327 = _265 * _249;
      float _328 = _325 + TEXCOORD_4.x;
      float _329 = _326 + TEXCOORD_4.y;
      float _330 = _327 + TEXCOORD_4.z;
      float _331 = _265 * _250;
      float _332 = _331 + TEXCOORD_5.x;
      float _333 = _331 + TEXCOORD_5.y;
      float _334 = _331 + TEXCOORD_5.z;
      _336 = _328;
      _337 = _329;
      _338 = _330;
      _339 = _332;
      _340 = _333;
      _341 = _334;
   }
   else
   {
      _336 = TEXCOORD_4.x;
      _337 = TEXCOORD_4.y;
      _338 = TEXCOORD_4.z;
      _339 = TEXCOORD_5.x;
      _340 = TEXCOORD_5.y;
      _341 = TEXCOORD_5.z;
   }
   float _346 = -0.0f - (c_light_vec[1].x);
   float _347 = -0.0f - (c_light_vec[1].y);
   float _348 = -0.0f - (c_light_vec[1].z);
   float _349 = dot(float3(_165, _166, _167), float3(_346, _347, _348));
   float _350 = saturate(_349);
   float _355 = saturate(_180);
   float _356 = (c_light_col[0].x) * _355;
   float _357 = (c_light_col[0].y) * _355;
   float _358 = (c_light_col[0].z) * _355;
   float _363 = (c_light_col[1].x) * _350;
   float _364 = (c_light_col[1].y) * _350;
   float _365 = (c_light_col[1].z) * _350;
   float _374 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _375 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _376 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _377 = _356 * _277;
   float _378 = _357 * _278;
   float _379 = _358 * _279;
   float _380 = _363 * _265;
   float _381 = _364 * _265;
   float _382 = _365 * _265;
   float _383 = _380 + _377;
   float _384 = _381 + _378;
   float _385 = _382 + _379;
   float _386 = _374 * _383;
   float _387 = _375 * _384;
   float _388 = _376 * _385;
   float _389 = _317 + c_scene_ambient.x;
   float _390 = _389 + _386;
   float _391 = _319 + c_scene_ambient.y;
   float _392 = _391 + _387;
   float _393 = _321 + c_scene_ambient.z;
   float _394 = _393 + _388;
   float _395 = _390 * _280;
   float _396 = _392 * _281;
   float _397 = _394 * _282;
   float _398 = _336 * _303;
   float _399 = _337 * _304;
   float _400 = _338 * _305;
   float _401 = _395 + _398;
   float _402 = _396 + _399;
   float _403 = _397 + _400;
   float _408 = (c_light_vec[0].x) + _170;
   float _409 = (c_light_vec[0].y) + _171;
   float _410 = (c_light_vec[0].z) + _172;
   float _411 = dot(float3(_408, _409, _410), float3(_408, _409, _410));
   float _412 = rsqrt(_411);
   float _413 = _408 * _412;
   float _414 = -0.0f - _413;
   float _415 = _409 * _412;
   float _416 = -0.0f - _415;
   float _417 = _410 * _412;
   float _418 = -0.0f - _417;
   float _419 = dot(float3(_414, _416, _418), float3(_165, _166, _167));
   float _420 = saturate(_419);
   float _421 = _171 + -0.30000001192092896f;
   float _422 = dot(float3(_170, _421, _172), float3(_170, _421, _172));
   float _423 = rsqrt(_422);
   float _424 = _170 * _423;
   float _425 = -0.0f - _424;
   float _426 = _421 * _423;
   float _427 = -0.0f - _426;
   float _428 = _172 * _423;
   float _429 = -0.0f - _428;
   float _430 = dot(float3(_425, _427, _429), float3(_165, _166, _167));
   float _431 = saturate(_430);
   float _434 = log2(_420);
   float _435 = log2(_431);
   float _436 = _434 * c_mat_power;
   float _437 = _435 * c_mat_power;
   float _438 = exp2(_436);
   float _439 = exp2(_437);
   float _442 = _438 * (c_light_col[0].x);
   float _443 = _438 * (c_light_col[0].y);
   float _444 = _438 * (c_light_col[0].z);
   float _445 = _439 * 2.0f;
   float _446 = _445 * (c_light_vec[5].x);
   float _447 = _446 * (c_light_col[1].x);
   float _448 = _446 * (c_light_col[1].y);
   float _449 = _446 * (c_light_col[1].z);
   float _450 = _442 * _277;
   float _451 = _443 * _278;
   float _452 = _444 * _279;
   float _453 = _447 * _350;
   float _454 = _448 * _350;
   float _455 = _449 * _350;
   float _456 = _453 + _450;
   float _457 = _454 + _451;
   float _458 = _455 + _452;
   float _465 = c_mat_specular.x * c_specular_scale.x;
   float _466 = _465 * _280;
   float _467 = _466 * _456;
   float _468 = c_mat_specular.y * c_specular_scale.y;
   float _469 = _468 * _281;
   float _470 = _469 * _457;
   float _471 = c_mat_specular.z * c_specular_scale.z;
   float _472 = _471 * _282;
   float _473 = _472 * _458;
   float _474 = _339 * _303;
   float _475 = _474 * _439;
   float _476 = _340 * _304;
   float _477 = _476 * _439;
   float _478 = _341 * _305;
   float _479 = _478 * _439;
   float _480 = _467 + _475;
   float _481 = _470 + _477;
   float _482 = _473 + _479;
   if (!_323)
   {
      float _484 = _265 * _247;
      float _485 = _265 * _248;
      float _486 = _265 * _249;
      float _487 = _401 + _484;
      float _488 = _402 + _485;
      float _489 = _403 + _486;
      float _490 = _265 * _250;
      float _491 = _480 + _490;
      float _492 = _481 + _490;
      float _493 = _482 + _490;
      _495 = _487;
      _496 = _488;
      _497 = _489;
      _498 = _491;
      _499 = _492;
      _500 = _493;
   }
   else
   {
      _495 = _401;
      _496 = _402;
      _497 = _403;
      _498 = _480;
      _499 = _481;
      _500 = _482;
   }
   int _501 = c_bool & 128;
   bool _502 = (_501 != 0);
   float _503 = select(_502, 0.0f, _498);
   float _504 = select(_502, 0.0f, _499);
   float _505 = select(_502, 0.0f, _500);
   float _506 = _495 * _112;
   float _507 = _496 * _113;
   float _508 = _497 * _114;
   float _509 = _503 * _121;
   float _510 = _504 * _122;
   float _511 = _505 * _123;
   int _512 = c_bool & 32768;
   bool _513 = (_512 == 0);
   if (!_513)
   {
      float _517 = (c_ps_user[11].x) * _112;
      float _518 = (c_ps_user[11].x) * _113;
      float _519 = (c_ps_user[11].x) * _114;
      float _521 = _165 + 1.0f;
      float _522 = _166 + 1.0f;
      float _523 = _167 + 1.0f;
      float _524 = _521 * 0.5f;
      float _525 = _522 * 0.5f;
      float _526 = _523 * 0.5f;
      float _527 = saturate(_524);
      float _528 = saturate(_525);
      float _529 = saturate(_526);
      float _530 = (c_ps_user[11].y) * _527;
      float _531 = _528 * (c_ps_user[11].y);
      float _532 = _529 * (c_ps_user[11].y);
      float _534 = (c_ps_user[11].z) * _121;
      float _535 = (c_ps_user[11].z) * _122;
      float _536 = (c_ps_user[11].z) * _123;
      float _538 = c_mat_power * 0.009999999776482582f;
      float _539 = _538 * (c_ps_user[11].w);
      float _542 = (c_ps_user[12].x) * _277;
      float _543 = (c_ps_user[12].x) * _278;
      float _544 = (c_ps_user[12].x) * _279;
      float _546 = (c_ps_user[12].y) * _280;
      float _547 = (c_ps_user[12].y) * _281;
      float _548 = (c_ps_user[12].y) * _282;
      float _550 = (c_ps_user[12].z) * _303;
      float _551 = (c_ps_user[12].z) * _304;
      float _552 = (c_ps_user[12].z) * _305;
      float _558 = -0.0f - _170;
      float _559 = -0.0f - _171;
      float _560 = -0.0f - _172;
      float _561 = dot(float3(_558, _559, _560), float3(_165, _166, _167));
      float _562 = 1.0f - _561;
      float _563 = _562 * _562;
      float _564 = _562 * 0.9722222089767456f;
      float _565 = _563 * _563;
      float _566 = _565 * _564;
      float _567 = _566 + 0.02777777425944805f;
      float _568 = _567 * (c_ps_user[13].z);
      float _570 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _571 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _572 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _575 = (c_ps_user[14].x) * _317;
      float _576 = (c_ps_user[14].x) * _319;
      float _577 = (c_ps_user[14].x) * _321;
      float _579 = (c_ps_user[14].y) * _356;
      float _580 = (c_ps_user[14].y) * _357;
      float _581 = (c_ps_user[14].y) * _358;
      float _583 = (c_ps_user[14].z) * _363;
      float _584 = (c_ps_user[14].z) * _364;
      float _585 = (c_ps_user[14].z) * _365;
      float _587 = (c_ps_user[14].w) * _336;
      float _588 = (c_ps_user[14].w) * _337;
      float _589 = (c_ps_user[14].w) * _338;
      float _592 = (c_ps_user[15].x) * _442;
      float _593 = (c_ps_user[15].x) * _443;
      float _594 = (c_ps_user[15].x) * _444;
      float _596 = _465 * _447;
      float _597 = _596 * (c_ps_user[15].y);
      float _598 = _468 * _448;
      float _599 = _598 * (c_ps_user[15].y);
      float _600 = _471 * _449;
      float _601 = _600 * (c_ps_user[15].y);
      float _603 = (c_ps_user[15].z) * _339;
      float _604 = (c_ps_user[15].z) * _340;
      float _605 = (c_ps_user[15].z) * _341;
      float _606 = _597 + _603;
      float _607 = _606 * _439;
      float _608 = _579 + (c_ps_user[13].x);
      float _609 = _608 + _583;
      float _610 = c_diffuse_scale.x * _609;
      float _611 = _610 * c_mat_diffuse.x;
      float _612 = (c_ps_user[13].y) + _534;
      float _613 = _612 + _592;
      float _614 = c_mat_specular.x * _613;
      float _615 = _614 * c_specular_scale.x;
      float _616 = _530 + _517;
      float _617 = _616 + _539;
      float _618 = _617 + (c_ps_user[12].w);
      float _619 = _618 + _542;
      float _620 = _619 + _546;
      float _621 = _620 + _550;
      float _622 = _621 + _570;
      float _623 = _622 + _575;
      float _624 = _623 + _587;
      float _625 = _624 + _568;
      float _626 = _625 + _607;
      float _627 = _626 + _611;
      float _628 = _627 + _615;
      float _629 = _599 + _604;
      float _630 = _629 * _439;
      float _631 = _580 + (c_ps_user[13].x);
      float _632 = _631 + _584;
      float _633 = c_diffuse_scale.y * _632;
      float _634 = _633 * c_mat_diffuse.y;
      float _635 = (c_ps_user[13].y) + _535;
      float _636 = _635 + _593;
      float _637 = c_mat_specular.y * _636;
      float _638 = _637 * c_specular_scale.y;
      float _639 = _531 + _518;
      float _640 = _639 + _539;
      float _641 = _640 + (c_ps_user[12].w);
      float _642 = _641 + _543;
      float _643 = _642 + _547;
      float _644 = _643 + _551;
      float _645 = _644 + _571;
      float _646 = _645 + _576;
      float _647 = _646 + _588;
      float _648 = _647 + _568;
      float _649 = _648 + _630;
      float _650 = _649 + _634;
      float _651 = _650 + _638;
      float _652 = _601 + _605;
      float _653 = _652 * _439;
      float _654 = _581 + (c_ps_user[13].x);
      float _655 = _654 + _585;
      float _656 = c_diffuse_scale.z * _655;
      float _657 = _656 * c_mat_diffuse.z;
      float _658 = (c_ps_user[13].y) + _536;
      float _659 = _658 + _594;
      float _660 = c_mat_specular.z * _659;
      float _661 = _660 * c_specular_scale.z;
      float _662 = _539 + _519;
      float _663 = _662 + _532;
      float _664 = _663 + (c_ps_user[12].w);
      float _665 = _664 + _544;
      float _666 = _665 + _548;
      float _667 = _666 + _552;
      float _668 = _667 + _572;
      float _669 = _668 + _577;
      float _670 = _669 + _589;
      float _671 = _670 + _568;
      float _672 = _671 + _653;
      float _673 = _672 + _657;
      float _674 = _673 + _661;
      _676 = _628;
      _677 = _651;
      _678 = _674;
      _679 = 0.0f;
      _680 = 0.0f;
      _681 = 0.0f;
   }
   else
   {
      _676 = _506;
      _677 = _507;
      _678 = _508;
      _679 = _509;
      _680 = _510;
      _681 = _511;
   }
   float _682 = _679 + _676;
   float _683 = _680 + _677;
   float _684 = _681 + _678;
   float _689 = _55.w * COLOR.w;
   float _690 = _689 * c_mat_opacity;
   float _691 = _690 * c_global_opacity;
   float _692 = _691 * c_opacity_scale;
   int _693 = c_bool & 16;
   bool _694 = (_693 == 0);
   if (!_694)
   {
      float _698 = c_tone_control.x * _682;
      float _699 = c_tone_control.x * _683;
      float _700 = c_tone_control.x * _684;
      float _702 = log2(_698);
      float _703 = log2(_699);
      float _704 = log2(_700);
      float _705 = _702 * c_tone_control.y;
      float _706 = _703 * c_tone_control.y;
      float _707 = _704 * c_tone_control.y;
      float _708 = exp2(_705);
      float _709 = exp2(_706);
      float _710 = exp2(_707);
      _758 = _708;
      _759 = _709;
      _760 = _710;
   }
   else
   {
      if (_513)
      {
         float _715 = c_tone_control.x * _682;
         float _716 = c_tone_control.x * _683;
         float _717 = c_tone_control.x * _684;
         float _718 = _715 * 2.0f;
         float _719 = _716 * 2.0f;
         float _720 = _717 * 2.0f;
         float _721 = _715 * 0.30000001192092896f;
         float _722 = _716 * 0.30000001192092896f;
         float _723 = _717 * 0.30000001192092896f;
         float _724 = _721 + 0.05000000074505806f;
         float _725 = _722 + 0.05000000074505806f;
         float _726 = _723 + 0.05000000074505806f;
         float _727 = _724 * _718;
         float _728 = _725 * _719;
         float _729 = _726 * _720;
         float _730 = _727 + 0.004000000189989805f;
         float _731 = _728 + 0.004000000189989805f;
         float _732 = _729 + 0.004000000189989805f;
         float _733 = _721 + 0.5f;
         float _734 = _722 + 0.5f;
         float _735 = _723 + 0.5f;
         float _736 = _733 * _718;
         float _737 = _734 * _719;
         float _738 = _735 * _720;
         float _739 = _736 + 0.06000000238418579f;
         float _740 = _737 + 0.06000000238418579f;
         float _741 = _738 + 0.06000000238418579f;
         float _742 = _730 / _739;
         float _743 = _731 / _740;
         float _744 = _732 / _741;
         float _745 = _742 + -0.06666666269302368f;
         float _746 = _743 + -0.06666666269302368f;
         float _747 = _744 + -0.06666666269302368f;
         float _748 = _745 * 1.3790643215179443f;
         float _749 = _746 * 1.3790643215179443f;
         float _750 = _747 * 1.3790643215179443f;
         float _751 = YKR_MaterialCurve(_748, _715);
         float _752 = YKR_MaterialCurve(_749, _716);
         float _753 = YKR_MaterialCurve(_750, _717);
         float _754 = sqrt(_751);
         float _755 = sqrt(_752);
         float _756 = sqrt(_753);
         _758 = _754;
         _759 = _755;
         _760 = _756;
      }
      else
      {
         _758 = _682;
         _759 = _683;
         _760 = _684;
      }
   }
   int _761 = c_bool & 4;
   bool _762 = (_761 == 0);
   if (_762)
   {
      float _764 = saturate(COLOR_1.x);
      float _769 = (c_fog_param[0].x) - _758;
      float _770 = (c_fog_param[0].y) - _759;
      float _771 = (c_fog_param[0].z) - _760;
      float _772 = _769 * _764;
      float _773 = _770 * _764;
      float _774 = _771 * _764;
      _776 = _772;
      _777 = _773;
      _778 = _774;
   }
   else
   {
      _776 = COLOR_1.x;
      _777 = COLOR_1.y;
      _778 = COLOR_1.z;
   }
   float _779 = _776 + _758;
   float _780 = _777 + _759;
   float _781 = _778 + _760;
   float _787 = (c_ps_col_scl[0].x) * _779;
   float _788 = (c_ps_col_scl[0].y) * _780;
   float _789 = (c_ps_col_scl[0].z) * _781;
   float _790 = (c_ps_col_scl[0].w) * _692;
   SV_Target.x = _787;
   SV_Target.y = _788;
   SV_Target.z = _789;
   SV_Target.w = _790;
   int _793 = c_bool & 8;
   bool _794 = (_793 != 0);
   float _795 = select(_794, _779, _787);
   float _796 = select(_794, _780, _788);
   float _797 = select(_794, _781, _789);
   float _798 = select(_794, _692, _790);
   SV_Target.x = _795;
   SV_Target.y = _796;
   SV_Target.z = _797;
   SV_Target.w = _798;
   return SV_Target;
}
