// Yakuza Kiwami material r_b1d[ag] (0xE9EDF565, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _35 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _40 = _35.x * _35.x;
   float _41 = _35.y * _35.y;
   float _42 = _35.z * _35.z;
   float _43 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _44 = rsqrt(_43);
   float _45 = _44 * TEXCOORD_2.x;
   float _46 = _44 * TEXCOORD_2.y;
   float _47 = _44 * TEXCOORD_2.z;
   float _48 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _49 = rsqrt(_48);
   float _54 = -0.0f - (c_light_vec[0].x);
   float _55 = -0.0f - (c_light_vec[0].y);
   float _56 = -0.0f - (c_light_vec[0].z);
   float _57 = dot(float3(_45, _46, _47), float3(_54, _55, _56));
   float _61 = c_rt_size.z * SV_Position.x;
   float _62 = c_rt_size.w * SV_Position.y;
   int _65 = c_bool & 8;
   bool _66 = (_65 == 0);
   float _73;
   float _74;
   float _75;
   float _87;
   float _88;
   float _120;
   float _121;
   float _122;
   float _136;
   float _137;
   float _172;
   float _173;
   float _174;
   float _204;
   float _205;
   float _206;
   float _277;
   float _278;
   float _279;
   float _399;
   float _400;
   float _401;
   float _475;
   float _476;
   float _477;
   float _493;
   float _494;
   float _495;
   if (!_66)
   {
      float4 _68 = s13_t.Sample(s13_s, float2(_61, _62));
      _73 = _68.x;
      _74 = _68.y;
      _75 = _68.w;
   }
   else
   {
      _73 = 1.0f;
      _74 = 1.0f;
      _75 = 1.0f;
   }
   if (!_66)
   {
      float _80 = (c_sdw_param[0].x) * _73;
      float _81 = (c_sdw_param[0].z) * _74;
      float _84 = _80 + (c_sdw_param[0].y);
      float _85 = _81 + (c_sdw_param[0].w);
      _87 = _84;
      _88 = _85;
   }
   else
   {
      _87 = _73;
      _88 = _74;
   }
   float _92 = c_rt_size.z * SV_Position.x;
   float _93 = c_rt_size.w * SV_Position.y;
   float4 _94 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_92, _93)));
   int _100 = c_bool & 16384;
   bool _101 = (_100 == 0);
   if (!_101)
   {
      float _103 = _94.x * 2.0f;
      float _104 = _94.y * 2.0f;
      float _105 = _94.z * 2.0f;
      _120 = _103;
      _121 = _104;
      _122 = _105;
   }
   else
   {
      int _107 = c_bool & 8192;
      bool _108 = (_107 == 0);
      if (!_108)
      {
         float _110 = _94.x * 2.0f;
         float _111 = _94.y * 2.0f;
         float _112 = _94.z * 2.0f;
         _120 = _110;
         _121 = _111;
         _122 = _112;
      }
      else
      {
         float _116 = _94.x / c_tone_control.z;
         float _117 = _94.y / c_tone_control.z;
         float _118 = _94.z / c_tone_control.z;
         _120 = _116;
         _121 = _117;
         _122 = _118;
      }
   }
   int _123 = c_bool & 8;
   bool _124 = (_123 == 0);
   if (!_124)
   {
      float _129 = (c_sdw_param[0].x) * _87;
      float _130 = (c_sdw_param[0].z) * _88;
      float _133 = _129 + (c_sdw_param[0].y);
      float _134 = _130 + (c_sdw_param[0].w);
      _136 = _133;
      _137 = _134;
   }
   else
   {
      _136 = _87;
      _137 = _88;
   }
   float _138 = _137 * _136;
   float _143 = 1.0f - (c_light_col[5].x);
   float _144 = 1.0f - (c_light_col[5].y);
   float _145 = 1.0f - (c_light_col[5].z);
   float _146 = _143 * _138;
   float _147 = _144 * _138;
   float _148 = _145 * _138;
   float _149 = _146 + (c_light_col[5].x);
   float _150 = _147 + (c_light_col[5].y);
   float _151 = _148 + (c_light_col[5].z);
   float _152 = _75 * COLOR.x;
   float _153 = _75 * COLOR.y;
   float _154 = _75 * COLOR.z;
   if (!_124)
   {
      float _158 = (c_sdw_param[2].x) * _149;
      float _159 = (c_sdw_param[2].x) * _150;
      float _160 = (c_sdw_param[2].x) * _151;
      float _162 = _158 + (c_sdw_param[2].y);
      float _163 = _159 + (c_sdw_param[2].y);
      float _164 = _160 + (c_sdw_param[2].y);
      float _167 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _168 = _162 * _167;
      float _169 = _163 * _167;
      float _170 = _164 * _167;
      _172 = _168;
      _173 = _169;
      _174 = _170;
   }
   else
   {
      _172 = _149;
      _173 = _150;
      _174 = _151;
   }
   float _175 = _172 * _75;
   float _176 = _173 * _75;
   float _177 = _174 * _75;
   float _188 = c_mat_emissive.x * _40;
   float _189 = _188 * c_emissive_scale;
   float _190 = c_mat_emissive.y * _41;
   float _191 = _190 * c_emissive_scale;
   float _192 = c_mat_emissive.z * _42;
   float _193 = _192 * c_emissive_scale;
   int _194 = c_bool & 2048;
   bool _195 = (_194 == 0);
   if (_195)
   {
      float _197 = _137 * _120;
      float _198 = _137 * _121;
      float _199 = _137 * _122;
      float _200 = _197 + TEXCOORD_4.x;
      float _201 = _198 + TEXCOORD_4.y;
      float _202 = _199 + TEXCOORD_4.z;
      _204 = _200;
      _205 = _201;
      _206 = _202;
   }
   else
   {
      _204 = TEXCOORD_4.x;
      _205 = TEXCOORD_4.y;
      _206 = TEXCOORD_4.z;
   }
   float _211 = -0.0f - (c_light_vec[1].x);
   float _212 = -0.0f - (c_light_vec[1].y);
   float _213 = -0.0f - (c_light_vec[1].z);
   float _214 = dot(float3(_45, _46, _47), float3(_211, _212, _213));
   float _215 = saturate(_214);
   float _220 = saturate(_57);
   float _221 = (c_light_col[0].x) * _220;
   float _222 = (c_light_col[0].y) * _220;
   float _223 = (c_light_col[0].z) * _220;
   float _228 = (c_light_col[1].x) * _215;
   float _229 = (c_light_col[1].y) * _215;
   float _230 = (c_light_col[1].z) * _215;
   float _239 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _240 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _241 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _242 = _221 * _149;
   float _243 = _222 * _150;
   float _244 = _223 * _151;
   float _245 = _228 * _137;
   float _246 = _229 * _137;
   float _247 = _230 * _137;
   float _248 = _245 + _242;
   float _249 = _246 + _243;
   float _250 = _247 + _244;
   float _251 = _239 * _248;
   float _252 = _240 * _249;
   float _253 = _241 * _250;
   float _254 = _189 + c_scene_ambient.x;
   float _255 = _254 + _251;
   float _256 = _191 + c_scene_ambient.y;
   float _257 = _256 + _252;
   float _258 = _193 + c_scene_ambient.z;
   float _259 = _258 + _253;
   float _260 = _255 * _152;
   float _261 = _257 * _153;
   float _262 = _259 * _154;
   float _263 = _204 * _175;
   float _264 = _205 * _176;
   float _265 = _206 * _177;
   float _266 = _260 + _263;
   float _267 = _261 + _264;
   float _268 = _262 + _265;
   if (!_195)
   {
      float _270 = _137 * _120;
      float _271 = _137 * _121;
      float _272 = _137 * _122;
      float _273 = _266 + _270;
      float _274 = _267 + _271;
      float _275 = _268 + _272;
      _277 = _273;
      _278 = _274;
      _279 = _275;
   }
   else
   {
      _277 = _266;
      _278 = _267;
      _279 = _268;
   }
   float _280 = _277 * _40;
   float _281 = _278 * _41;
   float _282 = _279 * _42;
   int _283 = c_bool & 32768;
   bool _284 = (_283 == 0);
   if (!_284)
   {
      float _288 = (c_ps_user[11].x) * _40;
      float _289 = (c_ps_user[11].x) * _41;
      float _290 = (c_ps_user[11].x) * _42;
      float _292 = _45 + 1.0f;
      float _293 = _46 + 1.0f;
      float _294 = _47 + 1.0f;
      float _295 = _292 * 0.5f;
      float _296 = _293 * 0.5f;
      float _297 = _294 * 0.5f;
      float _298 = saturate(_295);
      float _299 = saturate(_296);
      float _300 = saturate(_297);
      float _301 = (c_ps_user[11].y) * _298;
      float _302 = _299 * (c_ps_user[11].y);
      float _303 = _300 * (c_ps_user[11].y);
      float _306 = (c_ps_user[12].x) * _149;
      float _307 = (c_ps_user[12].x) * _150;
      float _308 = (c_ps_user[12].x) * _151;
      float _310 = (c_ps_user[12].y) * _152;
      float _311 = (c_ps_user[12].y) * _153;
      float _312 = (c_ps_user[12].y) * _154;
      float _314 = (c_ps_user[12].z) * _175;
      float _315 = (c_ps_user[12].z) * _176;
      float _316 = (c_ps_user[12].z) * _177;
      float _321 = TEXCOORD_3.x * _49;
      float _322 = -0.0f - _321;
      float _323 = TEXCOORD_3.y * _49;
      float _324 = -0.0f - _323;
      float _325 = TEXCOORD_3.z * _49;
      float _326 = -0.0f - _325;
      float _327 = dot(float3(_322, _324, _326), float3(_45, _46, _47));
      float _328 = 1.0f - _327;
      float _329 = _328 * _328;
      float _330 = _328 * 0.9722222089767456f;
      float _331 = _329 * _329;
      float _332 = _331 * _330;
      float _333 = _332 + 0.02777777425944805f;
      float _334 = _333 * (c_ps_user[13].z);
      float _336 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _337 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _338 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _341 = (c_ps_user[14].x) * _189;
      float _342 = (c_ps_user[14].x) * _191;
      float _343 = (c_ps_user[14].x) * _193;
      float _345 = (c_ps_user[14].y) * _221;
      float _346 = (c_ps_user[14].y) * _222;
      float _347 = (c_ps_user[14].y) * _223;
      float _349 = (c_ps_user[14].z) * _228;
      float _350 = (c_ps_user[14].z) * _229;
      float _351 = (c_ps_user[14].z) * _230;
      float _353 = (c_ps_user[14].w) * _204;
      float _354 = (c_ps_user[14].w) * _205;
      float _355 = (c_ps_user[14].w) * _206;
      float _356 = _345 + (c_ps_user[13].x);
      float _357 = _356 + _349;
      float _358 = c_diffuse_scale.x * _357;
      float _359 = _358 * c_mat_diffuse.x;
      float _360 = _301 + _288;
      float _361 = _360 + (c_ps_user[12].w);
      float _362 = _361 + _306;
      float _363 = _362 + _310;
      float _364 = _363 + _314;
      float _365 = _364 + _336;
      float _366 = _365 + _341;
      float _367 = _366 + _353;
      float _368 = _367 + _334;
      float _369 = _368 + _359;
      float _370 = _346 + (c_ps_user[13].x);
      float _371 = _370 + _350;
      float _372 = c_diffuse_scale.y * _371;
      float _373 = _372 * c_mat_diffuse.y;
      float _374 = _302 + _289;
      float _375 = _374 + (c_ps_user[12].w);
      float _376 = _375 + _307;
      float _377 = _376 + _311;
      float _378 = _377 + _315;
      float _379 = _378 + _337;
      float _380 = _379 + _342;
      float _381 = _380 + _354;
      float _382 = _381 + _334;
      float _383 = _382 + _373;
      float _384 = _347 + (c_ps_user[13].x);
      float _385 = _384 + _351;
      float _386 = c_diffuse_scale.z * _385;
      float _387 = _386 * c_mat_diffuse.z;
      float _388 = _303 + _290;
      float _389 = _388 + (c_ps_user[12].w);
      float _390 = _389 + _308;
      float _391 = _390 + _312;
      float _392 = _391 + _316;
      float _393 = _392 + _338;
      float _394 = _393 + _343;
      float _395 = _394 + _355;
      float _396 = _395 + _334;
      float _397 = _396 + _387;
      _399 = _369;
      _400 = _383;
      _401 = _397;
   }
   else
   {
      _399 = _280;
      _400 = _281;
      _401 = _282;
   }
   float _406 = _35.w * COLOR.w;
   float _407 = _406 * c_mat_opacity;
   float _408 = _407 * c_global_opacity;
   float _409 = _408 * c_opacity_scale;
   int _410 = c_bool & 16;
   bool _411 = (_410 == 0);
   if (!_411)
   {
      float _415 = c_tone_control.x * _399;
      float _416 = c_tone_control.x * _400;
      float _417 = c_tone_control.x * _401;
      float _419 = log2(_415);
      float _420 = log2(_416);
      float _421 = log2(_417);
      float _422 = _419 * c_tone_control.y;
      float _423 = _420 * c_tone_control.y;
      float _424 = _421 * c_tone_control.y;
      float _425 = exp2(_422);
      float _426 = exp2(_423);
      float _427 = exp2(_424);
      _475 = _425;
      _476 = _426;
      _477 = _427;
   }
   else
   {
      if (_284)
      {
         float _432 = c_tone_control.x * _399;
         float _433 = c_tone_control.x * _400;
         float _434 = c_tone_control.x * _401;
         float _435 = _432 * 2.0f;
         float _436 = _433 * 2.0f;
         float _437 = _434 * 2.0f;
         float _438 = _432 * 0.30000001192092896f;
         float _439 = _433 * 0.30000001192092896f;
         float _440 = _434 * 0.30000001192092896f;
         float _441 = _438 + 0.05000000074505806f;
         float _442 = _439 + 0.05000000074505806f;
         float _443 = _440 + 0.05000000074505806f;
         float _444 = _441 * _435;
         float _445 = _442 * _436;
         float _446 = _443 * _437;
         float _447 = _444 + 0.004000000189989805f;
         float _448 = _445 + 0.004000000189989805f;
         float _449 = _446 + 0.004000000189989805f;
         float _450 = _438 + 0.5f;
         float _451 = _439 + 0.5f;
         float _452 = _440 + 0.5f;
         float _453 = _450 * _435;
         float _454 = _451 * _436;
         float _455 = _452 * _437;
         float _456 = _453 + 0.06000000238418579f;
         float _457 = _454 + 0.06000000238418579f;
         float _458 = _455 + 0.06000000238418579f;
         float _459 = _447 / _456;
         float _460 = _448 / _457;
         float _461 = _449 / _458;
         float _462 = _459 + -0.06666666269302368f;
         float _463 = _460 + -0.06666666269302368f;
         float _464 = _461 + -0.06666666269302368f;
         float _465 = _462 * 1.3790643215179443f;
         float _466 = _463 * 1.3790643215179443f;
         float _467 = _464 * 1.3790643215179443f;
         float _468 = YKR_MaterialCurve(_465, _432);
         float _469 = YKR_MaterialCurve(_466, _433);
         float _470 = YKR_MaterialCurve(_467, _434);
         float _471 = sqrt(_468);
         float _472 = sqrt(_469);
         float _473 = sqrt(_470);
         _475 = _471;
         _476 = _472;
         _477 = _473;
      }
      else
      {
         _475 = _399;
         _476 = _400;
         _477 = _401;
      }
   }
   int _478 = c_bool & 4;
   bool _479 = (_478 == 0);
   if (_479)
   {
      float _481 = saturate(COLOR_1.x);
      float _486 = (c_fog_param[0].x) - _475;
      float _487 = (c_fog_param[0].y) - _476;
      float _488 = (c_fog_param[0].z) - _477;
      float _489 = _486 * _481;
      float _490 = _487 * _481;
      float _491 = _488 * _481;
      _493 = _489;
      _494 = _490;
      _495 = _491;
   }
   else
   {
      _493 = COLOR_1.x;
      _494 = COLOR_1.y;
      _495 = COLOR_1.z;
   }
   float _496 = _493 + _475;
   float _497 = _494 + _476;
   float _498 = _495 + _477;
   float _501 = _409 - c_mat_punchthrough_ref;
   bool _502 = (_501 < 0.0f);
   if (_502)
      discard;
   float _508 = (c_ps_col_scl[0].x) * _496;
   float _509 = (c_ps_col_scl[0].y) * _497;
   float _510 = (c_ps_col_scl[0].z) * _498;
   float _511 = (c_ps_col_scl[0].w) * _409;
   SV_Target.x = _508;
   SV_Target.y = _509;
   SV_Target.z = _510;
   SV_Target.w = _511;
   int _514 = c_bool & 8;
   bool _515 = (_514 != 0);
   float _516 = select(_515, _496, _508);
   float _517 = select(_515, _497, _509);
   float _518 = select(_515, _498, _510);
   float _519 = select(_515, _409, _511);
   SV_Target.x = _516;
   SV_Target.y = _517;
   SV_Target.z = _518;
   SV_Target.w = _519;
   return SV_Target;
}
