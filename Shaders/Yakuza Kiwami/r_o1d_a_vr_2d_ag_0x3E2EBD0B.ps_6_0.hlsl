// Yakuza Kiwami material r_o1d_a(vr)2d[ag] (0x3E2EBD0B, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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
   float4 _39 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _44 = _39.x * _39.x;
   float _45 = _39.y * _39.y;
   float _46 = _39.z * _39.z;
   float4 _47 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _52 = _47.x * _47.x;
   float _53 = _47.y * _47.y;
   float _54 = _47.z * _47.z;
   float _55 = _47.w * COLOR_1.w;
   float _56 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _57 = rsqrt(_56);
   float _58 = _57 * TEXCOORD_2.x;
   float _59 = _57 * TEXCOORD_2.y;
   float _60 = _57 * TEXCOORD_2.z;
   float _61 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _62 = rsqrt(_61);
   float _67 = -0.0f - (c_light_vec[0].x);
   float _68 = -0.0f - (c_light_vec[0].y);
   float _69 = -0.0f - (c_light_vec[0].z);
   float _70 = dot(float3(_58, _59, _60), float3(_67, _68, _69));
   float _74 = c_rt_size.z * SV_Position.x;
   float _75 = c_rt_size.w * SV_Position.y;
   int _78 = c_bool & 8;
   bool _79 = (_78 == 0);
   float _86;
   float _87;
   float _88;
   float _100;
   float _101;
   float _133;
   float _134;
   float _135;
   float _149;
   float _150;
   float _185;
   float _186;
   float _187;
   float _217;
   float _218;
   float _219;
   float _290;
   float _291;
   float _292;
   float _412;
   float _413;
   float _414;
   float _493;
   float _494;
   float _495;
   float _511;
   float _512;
   float _513;
   if (!_79)
   {
      float4 _81 = s13_t.Sample(s13_s, float2(_74, _75));
      _86 = _81.x;
      _87 = _81.y;
      _88 = _81.w;
   }
   else
   {
      _86 = 1.0f;
      _87 = 1.0f;
      _88 = 1.0f;
   }
   if (!_79)
   {
      float _93 = (c_sdw_param[0].x) * _86;
      float _94 = (c_sdw_param[0].z) * _87;
      float _97 = _93 + (c_sdw_param[0].y);
      float _98 = _94 + (c_sdw_param[0].w);
      _100 = _97;
      _101 = _98;
   }
   else
   {
      _100 = _86;
      _101 = _87;
   }
   float _105 = c_rt_size.z * SV_Position.x;
   float _106 = c_rt_size.w * SV_Position.y;
   float4 _107 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_105, _106)));
   int _113 = c_bool & 16384;
   bool _114 = (_113 == 0);
   if (!_114)
   {
      float _116 = _107.x * 2.0f;
      float _117 = _107.y * 2.0f;
      float _118 = _107.z * 2.0f;
      _133 = _116;
      _134 = _117;
      _135 = _118;
   }
   else
   {
      int _120 = c_bool & 8192;
      bool _121 = (_120 == 0);
      if (!_121)
      {
         float _123 = _107.x * 2.0f;
         float _124 = _107.y * 2.0f;
         float _125 = _107.z * 2.0f;
         _133 = _123;
         _134 = _124;
         _135 = _125;
      }
      else
      {
         float _129 = _107.x / c_tone_control.z;
         float _130 = _107.y / c_tone_control.z;
         float _131 = _107.z / c_tone_control.z;
         _133 = _129;
         _134 = _130;
         _135 = _131;
      }
   }
   int _136 = c_bool & 8;
   bool _137 = (_136 == 0);
   if (!_137)
   {
      float _142 = (c_sdw_param[0].x) * _100;
      float _143 = (c_sdw_param[0].z) * _101;
      float _146 = _142 + (c_sdw_param[0].y);
      float _147 = _143 + (c_sdw_param[0].w);
      _149 = _146;
      _150 = _147;
   }
   else
   {
      _149 = _100;
      _150 = _101;
   }
   float _151 = _150 * _149;
   float _156 = 1.0f - (c_light_col[5].x);
   float _157 = 1.0f - (c_light_col[5].y);
   float _158 = 1.0f - (c_light_col[5].z);
   float _159 = _156 * _151;
   float _160 = _157 * _151;
   float _161 = _158 * _151;
   float _162 = _159 + (c_light_col[5].x);
   float _163 = _160 + (c_light_col[5].y);
   float _164 = _161 + (c_light_col[5].z);
   float _165 = _88 * COLOR.x;
   float _166 = _88 * COLOR.y;
   float _167 = _88 * COLOR.z;
   if (!_137)
   {
      float _171 = (c_sdw_param[2].x) * _162;
      float _172 = (c_sdw_param[2].x) * _163;
      float _173 = (c_sdw_param[2].x) * _164;
      float _175 = _171 + (c_sdw_param[2].y);
      float _176 = _172 + (c_sdw_param[2].y);
      float _177 = _173 + (c_sdw_param[2].y);
      float _180 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _181 = _175 * _180;
      float _182 = _176 * _180;
      float _183 = _177 * _180;
      _185 = _181;
      _186 = _182;
      _187 = _183;
   }
   else
   {
      _185 = _162;
      _186 = _163;
      _187 = _164;
   }
   float _188 = _185 * _88;
   float _189 = _186 * _88;
   float _190 = _187 * _88;
   float _201 = c_mat_emissive.x * _44;
   float _202 = _201 * c_emissive_scale;
   float _203 = c_mat_emissive.y * _45;
   float _204 = _203 * c_emissive_scale;
   float _205 = c_mat_emissive.z * _46;
   float _206 = _205 * c_emissive_scale;
   int _207 = c_bool & 2048;
   bool _208 = (_207 == 0);
   if (_208)
   {
      float _210 = _150 * _133;
      float _211 = _150 * _134;
      float _212 = _150 * _135;
      float _213 = _210 + TEXCOORD_4.x;
      float _214 = _211 + TEXCOORD_4.y;
      float _215 = _212 + TEXCOORD_4.z;
      _217 = _213;
      _218 = _214;
      _219 = _215;
   }
   else
   {
      _217 = TEXCOORD_4.x;
      _218 = TEXCOORD_4.y;
      _219 = TEXCOORD_4.z;
   }
   float _224 = -0.0f - (c_light_vec[1].x);
   float _225 = -0.0f - (c_light_vec[1].y);
   float _226 = -0.0f - (c_light_vec[1].z);
   float _227 = dot(float3(_58, _59, _60), float3(_224, _225, _226));
   float _228 = saturate(_227);
   float _233 = saturate(_70);
   float _234 = (c_light_col[0].x) * _233;
   float _235 = (c_light_col[0].y) * _233;
   float _236 = (c_light_col[0].z) * _233;
   float _241 = (c_light_col[1].x) * _228;
   float _242 = (c_light_col[1].y) * _228;
   float _243 = (c_light_col[1].z) * _228;
   float _252 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _253 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _254 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _255 = _234 * _162;
   float _256 = _235 * _163;
   float _257 = _236 * _164;
   float _258 = _241 * _150;
   float _259 = _242 * _150;
   float _260 = _243 * _150;
   float _261 = _258 + _255;
   float _262 = _259 + _256;
   float _263 = _260 + _257;
   float _264 = _252 * _261;
   float _265 = _253 * _262;
   float _266 = _254 * _263;
   float _267 = _202 + c_scene_ambient.x;
   float _268 = _267 + _264;
   float _269 = _204 + c_scene_ambient.y;
   float _270 = _269 + _265;
   float _271 = _206 + c_scene_ambient.z;
   float _272 = _271 + _266;
   float _273 = _268 * _165;
   float _274 = _270 * _166;
   float _275 = _272 * _167;
   float _276 = _217 * _188;
   float _277 = _218 * _189;
   float _278 = _219 * _190;
   float _279 = _273 + _276;
   float _280 = _274 + _277;
   float _281 = _275 + _278;
   if (!_208)
   {
      float _283 = _150 * _133;
      float _284 = _150 * _134;
      float _285 = _150 * _135;
      float _286 = _279 + _283;
      float _287 = _280 + _284;
      float _288 = _281 + _285;
      _290 = _286;
      _291 = _287;
      _292 = _288;
   }
   else
   {
      _290 = _279;
      _291 = _280;
      _292 = _281;
   }
   float _293 = _290 * _44;
   float _294 = _291 * _45;
   float _295 = _292 * _46;
   int _296 = c_bool & 32768;
   bool _297 = (_296 == 0);
   if (!_297)
   {
      float _301 = (c_ps_user[11].x) * _44;
      float _302 = (c_ps_user[11].x) * _45;
      float _303 = (c_ps_user[11].x) * _46;
      float _305 = _58 + 1.0f;
      float _306 = _59 + 1.0f;
      float _307 = _60 + 1.0f;
      float _308 = _305 * 0.5f;
      float _309 = _306 * 0.5f;
      float _310 = _307 * 0.5f;
      float _311 = saturate(_308);
      float _312 = saturate(_309);
      float _313 = saturate(_310);
      float _314 = (c_ps_user[11].y) * _311;
      float _315 = _312 * (c_ps_user[11].y);
      float _316 = _313 * (c_ps_user[11].y);
      float _319 = (c_ps_user[12].x) * _162;
      float _320 = (c_ps_user[12].x) * _163;
      float _321 = (c_ps_user[12].x) * _164;
      float _323 = (c_ps_user[12].y) * _165;
      float _324 = (c_ps_user[12].y) * _166;
      float _325 = (c_ps_user[12].y) * _167;
      float _327 = (c_ps_user[12].z) * _188;
      float _328 = (c_ps_user[12].z) * _189;
      float _329 = (c_ps_user[12].z) * _190;
      float _334 = TEXCOORD_3.x * _62;
      float _335 = -0.0f - _334;
      float _336 = TEXCOORD_3.y * _62;
      float _337 = -0.0f - _336;
      float _338 = TEXCOORD_3.z * _62;
      float _339 = -0.0f - _338;
      float _340 = dot(float3(_335, _337, _339), float3(_58, _59, _60));
      float _341 = 1.0f - _340;
      float _342 = _341 * _341;
      float _343 = _341 * 0.9722222089767456f;
      float _344 = _342 * _342;
      float _345 = _344 * _343;
      float _346 = _345 + 0.02777777425944805f;
      float _347 = _346 * (c_ps_user[13].z);
      float _349 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _350 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _351 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _354 = (c_ps_user[14].x) * _202;
      float _355 = (c_ps_user[14].x) * _204;
      float _356 = (c_ps_user[14].x) * _206;
      float _358 = (c_ps_user[14].y) * _234;
      float _359 = (c_ps_user[14].y) * _235;
      float _360 = (c_ps_user[14].y) * _236;
      float _362 = (c_ps_user[14].z) * _241;
      float _363 = (c_ps_user[14].z) * _242;
      float _364 = (c_ps_user[14].z) * _243;
      float _366 = (c_ps_user[14].w) * _217;
      float _367 = (c_ps_user[14].w) * _218;
      float _368 = (c_ps_user[14].w) * _219;
      float _369 = _358 + (c_ps_user[13].x);
      float _370 = _369 + _362;
      float _371 = c_diffuse_scale.x * _370;
      float _372 = _371 * c_mat_diffuse.x;
      float _373 = _314 + _301;
      float _374 = _373 + (c_ps_user[12].w);
      float _375 = _374 + _319;
      float _376 = _375 + _323;
      float _377 = _376 + _327;
      float _378 = _377 + _349;
      float _379 = _378 + _354;
      float _380 = _379 + _366;
      float _381 = _380 + _347;
      float _382 = _381 + _372;
      float _383 = _359 + (c_ps_user[13].x);
      float _384 = _383 + _363;
      float _385 = c_diffuse_scale.y * _384;
      float _386 = _385 * c_mat_diffuse.y;
      float _387 = _315 + _302;
      float _388 = _387 + (c_ps_user[12].w);
      float _389 = _388 + _320;
      float _390 = _389 + _324;
      float _391 = _390 + _328;
      float _392 = _391 + _350;
      float _393 = _392 + _355;
      float _394 = _393 + _367;
      float _395 = _394 + _347;
      float _396 = _395 + _386;
      float _397 = _360 + (c_ps_user[13].x);
      float _398 = _397 + _364;
      float _399 = c_diffuse_scale.z * _398;
      float _400 = _399 * c_mat_diffuse.z;
      float _401 = _316 + _303;
      float _402 = _401 + (c_ps_user[12].w);
      float _403 = _402 + _321;
      float _404 = _403 + _325;
      float _405 = _404 + _329;
      float _406 = _405 + _351;
      float _407 = _406 + _356;
      float _408 = _407 + _368;
      float _409 = _408 + _347;
      float _410 = _409 + _400;
      _412 = _382;
      _413 = _396;
      _414 = _410;
   }
   else
   {
      _412 = _293;
      _413 = _294;
      _414 = _295;
   }
   float _415 = _52 * _55;
   float _416 = _53 * _55;
   float _417 = _54 * _55;
   float _418 = _412 + _415;
   float _419 = _413 + _416;
   float _420 = _414 + _417;
   float _425 = c_mat_opacity * _39.w;
   float _426 = _425 * c_global_opacity;
   float _427 = _426 * c_opacity_scale;
   int _428 = c_bool & 16;
   bool _429 = (_428 == 0);
   if (!_429)
   {
      float _433 = c_tone_control.x * _418;
      float _434 = c_tone_control.x * _419;
      float _435 = c_tone_control.x * _420;
      float _437 = log2(_433);
      float _438 = log2(_434);
      float _439 = log2(_435);
      float _440 = _437 * c_tone_control.y;
      float _441 = _438 * c_tone_control.y;
      float _442 = _439 * c_tone_control.y;
      float _443 = exp2(_440);
      float _444 = exp2(_441);
      float _445 = exp2(_442);
      _493 = _443;
      _494 = _444;
      _495 = _445;
   }
   else
   {
      if (_297)
      {
         float _450 = c_tone_control.x * _418;
         float _451 = c_tone_control.x * _419;
         float _452 = c_tone_control.x * _420;
         float _453 = _450 * 2.0f;
         float _454 = _451 * 2.0f;
         float _455 = _452 * 2.0f;
         float _456 = _450 * 0.30000001192092896f;
         float _457 = _451 * 0.30000001192092896f;
         float _458 = _452 * 0.30000001192092896f;
         float _459 = _456 + 0.05000000074505806f;
         float _460 = _457 + 0.05000000074505806f;
         float _461 = _458 + 0.05000000074505806f;
         float _462 = _459 * _453;
         float _463 = _460 * _454;
         float _464 = _461 * _455;
         float _465 = _462 + 0.004000000189989805f;
         float _466 = _463 + 0.004000000189989805f;
         float _467 = _464 + 0.004000000189989805f;
         float _468 = _456 + 0.5f;
         float _469 = _457 + 0.5f;
         float _470 = _458 + 0.5f;
         float _471 = _468 * _453;
         float _472 = _469 * _454;
         float _473 = _470 * _455;
         float _474 = _471 + 0.06000000238418579f;
         float _475 = _472 + 0.06000000238418579f;
         float _476 = _473 + 0.06000000238418579f;
         float _477 = _465 / _474;
         float _478 = _466 / _475;
         float _479 = _467 / _476;
         float _480 = _477 + -0.06666666269302368f;
         float _481 = _478 + -0.06666666269302368f;
         float _482 = _479 + -0.06666666269302368f;
         float _483 = _480 * 1.3790643215179443f;
         float _484 = _481 * 1.3790643215179443f;
         float _485 = _482 * 1.3790643215179443f;
         float _486 = YKR_MaterialCurve(_483, _450);
         float _487 = YKR_MaterialCurve(_484, _451);
         float _488 = YKR_MaterialCurve(_485, _452);
         float _489 = sqrt(_486);
         float _490 = sqrt(_487);
         float _491 = sqrt(_488);
         _493 = _489;
         _494 = _490;
         _495 = _491;
      }
      else
      {
         _493 = _418;
         _494 = _419;
         _495 = _420;
      }
   }
   int _496 = c_bool & 4;
   bool _497 = (_496 == 0);
   if (_497)
   {
      float _499 = saturate(COLOR_1.x);
      float _504 = (c_fog_param[0].x) - _493;
      float _505 = (c_fog_param[0].y) - _494;
      float _506 = (c_fog_param[0].z) - _495;
      float _507 = _504 * _499;
      float _508 = _505 * _499;
      float _509 = _506 * _499;
      _511 = _507;
      _512 = _508;
      _513 = _509;
   }
   else
   {
      _511 = COLOR_1.x;
      _512 = COLOR_1.y;
      _513 = COLOR_1.z;
   }
   float _514 = _511 + _493;
   float _515 = _512 + _494;
   float _516 = _513 + _495;
   float _522 = (c_ps_col_scl[0].x) * _514;
   float _523 = (c_ps_col_scl[0].y) * _515;
   float _524 = (c_ps_col_scl[0].z) * _516;
   float _525 = (c_ps_col_scl[0].w) * _427;
   SV_Target.x = _522;
   SV_Target.y = _523;
   SV_Target.z = _524;
   SV_Target.w = _525;
   int _528 = c_bool & 8;
   bool _529 = (_528 != 0);
   float _530 = select(_529, _514, _522);
   float _531 = select(_529, _515, _523);
   float _532 = select(_529, _516, _524);
   float _533 = select(_529, _427, _525);
   SV_Target.x = _530;
   SV_Target.y = _531;
   SV_Target.z = _532;
   SV_Target.w = _533;
   return SV_Target;
}
