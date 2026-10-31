// Yakuza Kiwami material r_o1mt (0x2D6B073F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
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
   float4 _43 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _48 = _43.x * _43.x;
   float _49 = _43.y * _43.y;
   float _50 = _43.z * _43.z;
   float _54 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _55 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _56 = s5_t.Sample(s5_s, float2(_54, _55));
   float _59 = _56.x * 2.0f;
   float _60 = _56.y * 2.0f;
   float _61 = _59 + -1.0f;
   float _62 = _60 + -1.0f;
   float _63 = dot(float2(_61, _62), float2(_61, _62));
   float _64 = 1.0f - _63;
   float _65 = saturate(_64);
   float _66 = sqrt(_65);
   float _67 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _68 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _69 = _67 - _68;
   float _70 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _71 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _72 = _70 - _71;
   float _73 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _74 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _75 = _73 - _74;
   float _76 = _61 * TEXCOORD_6.x;
   float _77 = _61 * TEXCOORD_6.y;
   float _78 = _61 * TEXCOORD_6.z;
   float _79 = _62 * TEXCOORD_6.w;
   float _80 = _79 * _69;
   float _81 = _79 * _72;
   float _82 = _79 * _75;
   float _83 = _66 * TEXCOORD_2.x;
   float _84 = _66 * TEXCOORD_2.y;
   float _85 = _66 * TEXCOORD_2.z;
   float _86 = _83 + _76;
   float _87 = _86 + _80;
   float _88 = _84 + _77;
   float _89 = _88 + _81;
   float _90 = _85 + _78;
   float _91 = _90 + _82;
   float _92 = dot(float3(_87, _89, _91), float3(_87, _89, _91));
   float _93 = rsqrt(_92);
   float _94 = _87 * _93;
   float _95 = _89 * _93;
   float _96 = _91 * _93;
   float _97 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _98 = rsqrt(_97);
   float _99 = _98 * TEXCOORD_3.x;
   float _100 = _98 * TEXCOORD_3.y;
   float _101 = _98 * TEXCOORD_3.z;
   float _106 = -0.0f - (c_light_vec[0].x);
   float _107 = -0.0f - (c_light_vec[0].y);
   float _108 = -0.0f - (c_light_vec[0].z);
   float _109 = dot(float3(_94, _95, _96), float3(_106, _107, _108));
   float _113 = c_rt_size.z * SV_Position.x;
   float _114 = c_rt_size.w * SV_Position.y;
   int _117 = c_bool & 8;
   bool _118 = (_117 == 0);
   float _125;
   float _126;
   float _127;
   float _139;
   float _140;
   float _176;
   float _177;
   float _178;
   float _179;
   float _193;
   float _194;
   float _229;
   float _230;
   float _231;
   float _265;
   float _266;
   float _267;
   float _268;
   float _269;
   float _270;
   float _424;
   float _425;
   float _426;
   float _427;
   float _428;
   float _429;
   float _601;
   float _602;
   float _603;
   float _604;
   float _605;
   float _606;
   float _681;
   float _682;
   float _683;
   float _699;
   float _700;
   float _701;
   if (!_118)
   {
      float4 _120 = s13_t.Sample(s13_s, float2(_113, _114));
      _125 = _120.x;
      _126 = _120.y;
      _127 = _120.w;
   }
   else
   {
      _125 = 1.0f;
      _126 = 1.0f;
      _127 = 1.0f;
   }
   if (!_118)
   {
      float _132 = (c_sdw_param[0].x) * _125;
      float _133 = (c_sdw_param[0].z) * _126;
      float _136 = _132 + (c_sdw_param[0].y);
      float _137 = _133 + (c_sdw_param[0].w);
      _139 = _136;
      _140 = _137;
   }
   else
   {
      _139 = _125;
      _140 = _126;
   }
   float _144 = c_rt_size.z * SV_Position.x;
   float _145 = c_rt_size.w * SV_Position.y;
   float4 _146 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_144, _145)));
   int _153 = c_bool & 16384;
   bool _154 = (_153 == 0);
   if (!_154)
   {
      float _156 = _146.x * 2.0f;
      float _157 = _146.y * 2.0f;
      float _158 = _146.z * 2.0f;
      float _159 = _146.w * 2.0f;
      _176 = _156;
      _177 = _157;
      _178 = _158;
      _179 = _159;
   }
   else
   {
      int _161 = c_bool & 8192;
      bool _162 = (_161 == 0);
      if (!_162)
      {
         float _164 = _146.x * 2.0f;
         float _165 = _146.y * 2.0f;
         float _166 = _146.z * 2.0f;
         float _167 = _146.w * 2.0f;
         _176 = _164;
         _177 = _165;
         _178 = _166;
         _179 = _167;
      }
      else
      {
         float _171 = _146.x / c_tone_control.z;
         float _172 = _146.y / c_tone_control.z;
         float _173 = _146.z / c_tone_control.z;
         float _174 = _146.w / c_tone_control.z;
         _176 = _171;
         _177 = _172;
         _178 = _173;
         _179 = _174;
      }
   }
   int _180 = c_bool & 8;
   bool _181 = (_180 == 0);
   if (!_181)
   {
      float _186 = (c_sdw_param[0].x) * _139;
      float _187 = (c_sdw_param[0].z) * _140;
      float _190 = _186 + (c_sdw_param[0].y);
      float _191 = _187 + (c_sdw_param[0].w);
      _193 = _190;
      _194 = _191;
   }
   else
   {
      _193 = _139;
      _194 = _140;
   }
   float _195 = _194 * _193;
   float _200 = 1.0f - (c_light_col[5].x);
   float _201 = 1.0f - (c_light_col[5].y);
   float _202 = 1.0f - (c_light_col[5].z);
   float _203 = _200 * _195;
   float _204 = _201 * _195;
   float _205 = _202 * _195;
   float _206 = _203 + (c_light_col[5].x);
   float _207 = _204 + (c_light_col[5].y);
   float _208 = _205 + (c_light_col[5].z);
   float _209 = _127 * COLOR.x;
   float _210 = _127 * COLOR.y;
   float _211 = _127 * COLOR.z;
   if (!_181)
   {
      float _215 = (c_sdw_param[2].x) * _206;
      float _216 = (c_sdw_param[2].x) * _207;
      float _217 = (c_sdw_param[2].x) * _208;
      float _219 = _215 + (c_sdw_param[2].y);
      float _220 = _216 + (c_sdw_param[2].y);
      float _221 = _217 + (c_sdw_param[2].y);
      float _224 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _225 = _219 * _224;
      float _226 = _220 * _224;
      float _227 = _221 * _224;
      _229 = _225;
      _230 = _226;
      _231 = _227;
   }
   else
   {
      _229 = _206;
      _230 = _207;
      _231 = _208;
   }
   float _232 = _229 * _127;
   float _233 = _230 * _127;
   float _234 = _231 * _127;
   float _245 = c_mat_emissive.x * _48;
   float _246 = _245 * c_emissive_scale;
   float _247 = c_mat_emissive.y * _49;
   float _248 = _247 * c_emissive_scale;
   float _249 = c_mat_emissive.z * _50;
   float _250 = _249 * c_emissive_scale;
   int _251 = c_bool & 2048;
   bool _252 = (_251 == 0);
   if (_252)
   {
      float _254 = _194 * _176;
      float _255 = _194 * _177;
      float _256 = _194 * _178;
      float _257 = _254 + TEXCOORD_4.x;
      float _258 = _255 + TEXCOORD_4.y;
      float _259 = _256 + TEXCOORD_4.z;
      float _260 = _194 * _179;
      float _261 = _260 + TEXCOORD_5.x;
      float _262 = _260 + TEXCOORD_5.y;
      float _263 = _260 + TEXCOORD_5.z;
      _265 = _261;
      _266 = _262;
      _267 = _263;
      _268 = _257;
      _269 = _258;
      _270 = _259;
   }
   else
   {
      _265 = TEXCOORD_5.x;
      _266 = TEXCOORD_5.y;
      _267 = TEXCOORD_5.z;
      _268 = TEXCOORD_4.x;
      _269 = TEXCOORD_4.y;
      _270 = TEXCOORD_4.z;
   }
   float _275 = -0.0f - (c_light_vec[1].x);
   float _276 = -0.0f - (c_light_vec[1].y);
   float _277 = -0.0f - (c_light_vec[1].z);
   float _278 = dot(float3(_94, _95, _96), float3(_275, _276, _277));
   float _279 = saturate(_278);
   float _284 = saturate(_109);
   float _285 = (c_light_col[0].x) * _284;
   float _286 = (c_light_col[0].y) * _284;
   float _287 = (c_light_col[0].z) * _284;
   float _292 = (c_light_col[1].x) * _279;
   float _293 = (c_light_col[1].y) * _279;
   float _294 = (c_light_col[1].z) * _279;
   float _303 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _304 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _305 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _306 = _285 * _206;
   float _307 = _286 * _207;
   float _308 = _287 * _208;
   float _309 = _292 * _194;
   float _310 = _293 * _194;
   float _311 = _294 * _194;
   float _312 = _309 + _306;
   float _313 = _310 + _307;
   float _314 = _311 + _308;
   float _315 = _303 * _312;
   float _316 = _304 * _313;
   float _317 = _305 * _314;
   float _318 = _246 + c_scene_ambient.x;
   float _319 = _318 + _315;
   float _320 = _248 + c_scene_ambient.y;
   float _321 = _320 + _316;
   float _322 = _250 + c_scene_ambient.z;
   float _323 = _322 + _317;
   float _324 = _319 * _209;
   float _325 = _321 * _210;
   float _326 = _323 * _211;
   float _327 = _268 * _232;
   float _328 = _269 * _233;
   float _329 = _270 * _234;
   float _330 = _324 + _327;
   float _331 = _325 + _328;
   float _332 = _326 + _329;
   float _337 = (c_light_vec[0].x) + _99;
   float _338 = (c_light_vec[0].y) + _100;
   float _339 = (c_light_vec[0].z) + _101;
   float _340 = dot(float3(_337, _338, _339), float3(_337, _338, _339));
   float _341 = rsqrt(_340);
   float _342 = _337 * _341;
   float _343 = -0.0f - _342;
   float _344 = _338 * _341;
   float _345 = -0.0f - _344;
   float _346 = _339 * _341;
   float _347 = -0.0f - _346;
   float _348 = dot(float3(_343, _345, _347), float3(_94, _95, _96));
   float _349 = saturate(_348);
   float _350 = _100 + -0.30000001192092896f;
   float _351 = dot(float3(_99, _350, _101), float3(_99, _350, _101));
   float _352 = rsqrt(_351);
   float _353 = _99 * _352;
   float _354 = -0.0f - _353;
   float _355 = _350 * _352;
   float _356 = -0.0f - _355;
   float _357 = _101 * _352;
   float _358 = -0.0f - _357;
   float _359 = dot(float3(_354, _356, _358), float3(_94, _95, _96));
   float _360 = saturate(_359);
   float _363 = log2(_349);
   float _364 = log2(_360);
   float _365 = _363 * c_mat_power;
   float _366 = _364 * c_mat_power;
   float _367 = exp2(_365);
   float _368 = exp2(_366);
   float _371 = _367 * (c_light_col[0].x);
   float _372 = _367 * (c_light_col[0].y);
   float _373 = _367 * (c_light_col[0].z);
   float _374 = _368 * 2.0f;
   float _375 = _374 * (c_light_vec[5].x);
   float _376 = _375 * (c_light_col[1].x);
   float _377 = _375 * (c_light_col[1].y);
   float _378 = _375 * (c_light_col[1].z);
   float _379 = _371 * _206;
   float _380 = _372 * _207;
   float _381 = _373 * _208;
   float _382 = _376 * _279;
   float _383 = _377 * _279;
   float _384 = _378 * _279;
   float _385 = _382 + _379;
   float _386 = _383 + _380;
   float _387 = _384 + _381;
   float _394 = c_mat_specular.x * c_specular_scale.x;
   float _395 = _394 * _209;
   float _396 = _395 * _385;
   float _397 = c_mat_specular.y * c_specular_scale.y;
   float _398 = _397 * _210;
   float _399 = _398 * _386;
   float _400 = c_mat_specular.z * c_specular_scale.z;
   float _401 = _400 * _211;
   float _402 = _401 * _387;
   float _403 = _265 * _232;
   float _404 = _403 * _368;
   float _405 = _266 * _233;
   float _406 = _405 * _368;
   float _407 = _267 * _234;
   float _408 = _407 * _368;
   float _409 = _396 + _404;
   float _410 = _399 + _406;
   float _411 = _402 + _408;
   if (!_252)
   {
      float _413 = _194 * _176;
      float _414 = _194 * _177;
      float _415 = _194 * _178;
      float _416 = _330 + _413;
      float _417 = _331 + _414;
      float _418 = _332 + _415;
      float _419 = _194 * _179;
      float _420 = _409 + _419;
      float _421 = _410 + _419;
      float _422 = _411 + _419;
      _424 = _416;
      _425 = _417;
      _426 = _418;
      _427 = _420;
      _428 = _421;
      _429 = _422;
   }
   else
   {
      _424 = _330;
      _425 = _331;
      _426 = _332;
      _427 = _409;
      _428 = _410;
      _429 = _411;
   }
   int _430 = c_bool & 128;
   bool _431 = (_430 != 0);
   float _432 = select(_431, 0.0f, _427);
   float _433 = select(_431, 0.0f, _428);
   float _434 = select(_431, 0.0f, _429);
   float _435 = _424 * _48;
   float _436 = _425 * _49;
   float _437 = _426 * _50;
   float _438 = _432 * _43.w;
   float _439 = _433 * _43.w;
   float _440 = _434 * _43.w;
   int _441 = c_bool & 32768;
   bool _442 = (_441 == 0);
   if (!_442)
   {
      float _446 = (c_ps_user[11].x) * _48;
      float _447 = (c_ps_user[11].x) * _49;
      float _448 = (c_ps_user[11].x) * _50;
      float _450 = _94 + 1.0f;
      float _451 = _95 + 1.0f;
      float _452 = _96 + 1.0f;
      float _453 = _450 * 0.5f;
      float _454 = _451 * 0.5f;
      float _455 = _452 * 0.5f;
      float _456 = saturate(_453);
      float _457 = saturate(_454);
      float _458 = saturate(_455);
      float _459 = (c_ps_user[11].y) * _456;
      float _460 = _457 * (c_ps_user[11].y);
      float _461 = _458 * (c_ps_user[11].y);
      float _463 = (c_ps_user[11].z) * _43.w;
      float _465 = c_mat_power * 0.009999999776482582f;
      float _466 = _465 * (c_ps_user[11].w);
      float _469 = (c_ps_user[12].x) * _206;
      float _470 = (c_ps_user[12].x) * _207;
      float _471 = (c_ps_user[12].x) * _208;
      float _473 = (c_ps_user[12].y) * _209;
      float _474 = (c_ps_user[12].y) * _210;
      float _475 = (c_ps_user[12].y) * _211;
      float _477 = (c_ps_user[12].z) * _232;
      float _478 = (c_ps_user[12].z) * _233;
      float _479 = (c_ps_user[12].z) * _234;
      float _485 = -0.0f - _99;
      float _486 = -0.0f - _100;
      float _487 = -0.0f - _101;
      float _488 = dot(float3(_485, _486, _487), float3(_94, _95, _96));
      float _489 = 1.0f - _488;
      float _490 = _489 * _489;
      float _491 = _489 * 0.9722222089767456f;
      float _492 = _490 * _490;
      float _493 = _492 * _491;
      float _494 = _493 + 0.02777777425944805f;
      float _495 = _494 * (c_ps_user[13].z);
      float _497 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _498 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _499 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _502 = (c_ps_user[14].x) * _246;
      float _503 = (c_ps_user[14].x) * _248;
      float _504 = (c_ps_user[14].x) * _250;
      float _506 = (c_ps_user[14].y) * _285;
      float _507 = (c_ps_user[14].y) * _286;
      float _508 = (c_ps_user[14].y) * _287;
      float _510 = (c_ps_user[14].z) * _292;
      float _511 = (c_ps_user[14].z) * _293;
      float _512 = (c_ps_user[14].z) * _294;
      float _514 = (c_ps_user[14].w) * _268;
      float _515 = (c_ps_user[14].w) * _269;
      float _516 = (c_ps_user[14].w) * _270;
      float _519 = (c_ps_user[15].x) * _371;
      float _520 = (c_ps_user[15].x) * _372;
      float _521 = (c_ps_user[15].x) * _373;
      float _523 = _394 * _376;
      float _524 = _523 * (c_ps_user[15].y);
      float _525 = _397 * _377;
      float _526 = _525 * (c_ps_user[15].y);
      float _527 = _400 * _378;
      float _528 = _527 * (c_ps_user[15].y);
      float _530 = (c_ps_user[15].z) * _265;
      float _531 = (c_ps_user[15].z) * _266;
      float _532 = (c_ps_user[15].z) * _267;
      float _533 = _524 + _530;
      float _534 = _533 * _368;
      float _535 = _506 + (c_ps_user[13].x);
      float _536 = _535 + _510;
      float _537 = c_diffuse_scale.x * _536;
      float _538 = _537 * c_mat_diffuse.x;
      float _539 = (c_ps_user[13].y) + _463;
      float _540 = _539 + _519;
      float _541 = c_mat_specular.x * _540;
      float _542 = _541 * c_specular_scale.x;
      float _543 = _459 + _446;
      float _544 = _543 + _466;
      float _545 = _544 + (c_ps_user[12].w);
      float _546 = _545 + _469;
      float _547 = _546 + _473;
      float _548 = _547 + _477;
      float _549 = _548 + _497;
      float _550 = _549 + _502;
      float _551 = _550 + _514;
      float _552 = _551 + _495;
      float _553 = _552 + _534;
      float _554 = _553 + _538;
      float _555 = _554 + _542;
      float _556 = _526 + _531;
      float _557 = _556 * _368;
      float _558 = _507 + (c_ps_user[13].x);
      float _559 = _558 + _511;
      float _560 = c_diffuse_scale.y * _559;
      float _561 = _560 * c_mat_diffuse.y;
      float _562 = _539 + _520;
      float _563 = c_mat_specular.y * _562;
      float _564 = _563 * c_specular_scale.y;
      float _565 = _460 + _447;
      float _566 = _565 + _466;
      float _567 = _566 + (c_ps_user[12].w);
      float _568 = _567 + _470;
      float _569 = _568 + _474;
      float _570 = _569 + _478;
      float _571 = _570 + _498;
      float _572 = _571 + _503;
      float _573 = _572 + _515;
      float _574 = _573 + _495;
      float _575 = _574 + _557;
      float _576 = _575 + _561;
      float _577 = _576 + _564;
      float _578 = _528 + _532;
      float _579 = _578 * _368;
      float _580 = _508 + (c_ps_user[13].x);
      float _581 = _580 + _512;
      float _582 = c_diffuse_scale.z * _581;
      float _583 = _582 * c_mat_diffuse.z;
      float _584 = _539 + _521;
      float _585 = c_mat_specular.z * _584;
      float _586 = _585 * c_specular_scale.z;
      float _587 = _466 + _448;
      float _588 = _587 + _461;
      float _589 = _588 + (c_ps_user[12].w);
      float _590 = _589 + _471;
      float _591 = _590 + _475;
      float _592 = _591 + _479;
      float _593 = _592 + _499;
      float _594 = _593 + _504;
      float _595 = _594 + _516;
      float _596 = _595 + _495;
      float _597 = _596 + _579;
      float _598 = _597 + _583;
      float _599 = _598 + _586;
      _601 = 0.0f;
      _602 = 0.0f;
      _603 = 0.0f;
      _604 = _555;
      _605 = _577;
      _606 = _599;
   }
   else
   {
      _601 = _438;
      _602 = _439;
      _603 = _440;
      _604 = _435;
      _605 = _436;
      _606 = _437;
   }
   float _607 = _604 + _601;
   float _608 = _605 + _602;
   float _609 = _606 + _603;
   float _614 = c_global_opacity * c_mat_opacity;
   float _615 = _614 * c_opacity_scale;
   int _616 = c_bool & 16;
   bool _617 = (_616 == 0);
   if (!_617)
   {
      float _621 = c_tone_control.x * _607;
      float _622 = c_tone_control.x * _608;
      float _623 = c_tone_control.x * _609;
      float _625 = log2(_621);
      float _626 = log2(_622);
      float _627 = log2(_623);
      float _628 = _625 * c_tone_control.y;
      float _629 = _626 * c_tone_control.y;
      float _630 = _627 * c_tone_control.y;
      float _631 = exp2(_628);
      float _632 = exp2(_629);
      float _633 = exp2(_630);
      _681 = _631;
      _682 = _632;
      _683 = _633;
   }
   else
   {
      if (_442)
      {
         float _638 = c_tone_control.x * _607;
         float _639 = c_tone_control.x * _608;
         float _640 = c_tone_control.x * _609;
         float _641 = _638 * 2.0f;
         float _642 = _639 * 2.0f;
         float _643 = _640 * 2.0f;
         float _644 = _638 * 0.30000001192092896f;
         float _645 = _639 * 0.30000001192092896f;
         float _646 = _640 * 0.30000001192092896f;
         float _647 = _644 + 0.05000000074505806f;
         float _648 = _645 + 0.05000000074505806f;
         float _649 = _646 + 0.05000000074505806f;
         float _650 = _647 * _641;
         float _651 = _648 * _642;
         float _652 = _649 * _643;
         float _653 = _650 + 0.004000000189989805f;
         float _654 = _651 + 0.004000000189989805f;
         float _655 = _652 + 0.004000000189989805f;
         float _656 = _644 + 0.5f;
         float _657 = _645 + 0.5f;
         float _658 = _646 + 0.5f;
         float _659 = _656 * _641;
         float _660 = _657 * _642;
         float _661 = _658 * _643;
         float _662 = _659 + 0.06000000238418579f;
         float _663 = _660 + 0.06000000238418579f;
         float _664 = _661 + 0.06000000238418579f;
         float _665 = _653 / _662;
         float _666 = _654 / _663;
         float _667 = _655 / _664;
         float _668 = _665 + -0.06666666269302368f;
         float _669 = _666 + -0.06666666269302368f;
         float _670 = _667 + -0.06666666269302368f;
         float _671 = _668 * 1.3790643215179443f;
         float _672 = _669 * 1.3790643215179443f;
         float _673 = _670 * 1.3790643215179443f;
         float _674 = YKR_MaterialCurve(_671, _638);
         float _675 = YKR_MaterialCurve(_672, _639);
         float _676 = YKR_MaterialCurve(_673, _640);
         float _677 = sqrt(_674);
         float _678 = sqrt(_675);
         float _679 = sqrt(_676);
         _681 = _677;
         _682 = _678;
         _683 = _679;
      }
      else
      {
         _681 = _607;
         _682 = _608;
         _683 = _609;
      }
   }
   int _684 = c_bool & 4;
   bool _685 = (_684 == 0);
   if (_685)
   {
      float _687 = saturate(COLOR_1.x);
      float _692 = (c_fog_param[0].x) - _681;
      float _693 = (c_fog_param[0].y) - _682;
      float _694 = (c_fog_param[0].z) - _683;
      float _695 = _692 * _687;
      float _696 = _693 * _687;
      float _697 = _694 * _687;
      _699 = _695;
      _700 = _696;
      _701 = _697;
   }
   else
   {
      _699 = COLOR_1.x;
      _700 = COLOR_1.y;
      _701 = COLOR_1.z;
   }
   float _702 = _699 + _681;
   float _703 = _700 + _682;
   float _704 = _701 + _683;
   float _710 = (c_ps_col_scl[0].x) * _702;
   float _711 = (c_ps_col_scl[0].y) * _703;
   float _712 = (c_ps_col_scl[0].z) * _704;
   float _713 = (c_ps_col_scl[0].w) * _615;
   SV_Target.x = _710;
   SV_Target.y = _711;
   SV_Target.z = _712;
   SV_Target.w = _713;
   int _716 = c_bool & 8;
   bool _717 = (_716 != 0);
   float _718 = select(_717, _702, _710);
   float _719 = select(_717, _703, _711);
   float _720 = select(_717, _704, _712);
   float _721 = select(_717, _615, _713);
   SV_Target.x = _718;
   SV_Target.y = _719;
   SV_Target.z = _720;
   SV_Target.w = _721;
   return SV_Target;
}
