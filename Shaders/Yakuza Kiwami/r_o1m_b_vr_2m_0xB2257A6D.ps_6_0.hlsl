// Yakuza Kiwami material r_o1m_b(vr)2m (0xB2257A6D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _42 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _47 = _42.x * _42.x;
   float _48 = _42.y * _42.y;
   float _49 = _42.z * _42.z;
   float4 _50 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _58 = _55 - _47;
   float _59 = _56 - _48;
   float _60 = _57 - _49;
   float _61 = _58 * COLOR_1.w;
   float _62 = _59 * COLOR_1.w;
   float _63 = _60 * COLOR_1.w;
   float _64 = _61 + _47;
   float _65 = _62 + _48;
   float _66 = _63 + _49;
   float _67 = _50.w - _42.w;
   float _68 = _67 * COLOR_1.w;
   float _69 = _68 + _42.w;
   float _70 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _71 = rsqrt(_70);
   float _72 = _71 * TEXCOORD_2.x;
   float _73 = _71 * TEXCOORD_2.y;
   float _74 = _71 * TEXCOORD_2.z;
   float _75 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _76 = rsqrt(_75);
   float _77 = _76 * TEXCOORD_3.x;
   float _78 = _76 * TEXCOORD_3.y;
   float _79 = _76 * TEXCOORD_3.z;
   float _84 = -0.0f - (c_light_vec[0].x);
   float _85 = -0.0f - (c_light_vec[0].y);
   float _86 = -0.0f - (c_light_vec[0].z);
   float _87 = dot(float3(_72, _73, _74), float3(_84, _85, _86));
   float _91 = c_rt_size.z * SV_Position.x;
   float _92 = c_rt_size.w * SV_Position.y;
   int _95 = c_bool & 8;
   bool _96 = (_95 == 0);
   float _103;
   float _104;
   float _105;
   float _117;
   float _118;
   float _154;
   float _155;
   float _156;
   float _157;
   float _171;
   float _172;
   float _207;
   float _208;
   float _209;
   float _243;
   float _244;
   float _245;
   float _246;
   float _247;
   float _248;
   float _402;
   float _403;
   float _404;
   float _405;
   float _406;
   float _407;
   float _579;
   float _580;
   float _581;
   float _582;
   float _583;
   float _584;
   float _659;
   float _660;
   float _661;
   float _677;
   float _678;
   float _679;
   if (!_96)
   {
      float4 _98 = s13_t.Sample(s13_s, float2(_91, _92));
      _103 = _98.x;
      _104 = _98.y;
      _105 = _98.w;
   }
   else
   {
      _103 = 1.0f;
      _104 = 1.0f;
      _105 = 1.0f;
   }
   if (!_96)
   {
      float _110 = (c_sdw_param[0].x) * _103;
      float _111 = (c_sdw_param[0].z) * _104;
      float _114 = _110 + (c_sdw_param[0].y);
      float _115 = _111 + (c_sdw_param[0].w);
      _117 = _114;
      _118 = _115;
   }
   else
   {
      _117 = _103;
      _118 = _104;
   }
   float _122 = c_rt_size.z * SV_Position.x;
   float _123 = c_rt_size.w * SV_Position.y;
   float4 _124 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_122, _123)));
   int _131 = c_bool & 16384;
   bool _132 = (_131 == 0);
   if (!_132)
   {
      float _134 = _124.x * 2.0f;
      float _135 = _124.y * 2.0f;
      float _136 = _124.z * 2.0f;
      float _137 = _124.w * 2.0f;
      _154 = _134;
      _155 = _135;
      _156 = _136;
      _157 = _137;
   }
   else
   {
      int _139 = c_bool & 8192;
      bool _140 = (_139 == 0);
      if (!_140)
      {
         float _142 = _124.x * 2.0f;
         float _143 = _124.y * 2.0f;
         float _144 = _124.z * 2.0f;
         float _145 = _124.w * 2.0f;
         _154 = _142;
         _155 = _143;
         _156 = _144;
         _157 = _145;
      }
      else
      {
         float _149 = _124.x / c_tone_control.z;
         float _150 = _124.y / c_tone_control.z;
         float _151 = _124.z / c_tone_control.z;
         float _152 = _124.w / c_tone_control.z;
         _154 = _149;
         _155 = _150;
         _156 = _151;
         _157 = _152;
      }
   }
   int _158 = c_bool & 8;
   bool _159 = (_158 == 0);
   if (!_159)
   {
      float _164 = (c_sdw_param[0].x) * _117;
      float _165 = (c_sdw_param[0].z) * _118;
      float _168 = _164 + (c_sdw_param[0].y);
      float _169 = _165 + (c_sdw_param[0].w);
      _171 = _168;
      _172 = _169;
   }
   else
   {
      _171 = _117;
      _172 = _118;
   }
   float _173 = _172 * _171;
   float _178 = 1.0f - (c_light_col[5].x);
   float _179 = 1.0f - (c_light_col[5].y);
   float _180 = 1.0f - (c_light_col[5].z);
   float _181 = _178 * _173;
   float _182 = _179 * _173;
   float _183 = _180 * _173;
   float _184 = _181 + (c_light_col[5].x);
   float _185 = _182 + (c_light_col[5].y);
   float _186 = _183 + (c_light_col[5].z);
   float _187 = _105 * COLOR.x;
   float _188 = _105 * COLOR.y;
   float _189 = _105 * COLOR.z;
   if (!_159)
   {
      float _193 = (c_sdw_param[2].x) * _184;
      float _194 = (c_sdw_param[2].x) * _185;
      float _195 = (c_sdw_param[2].x) * _186;
      float _197 = _193 + (c_sdw_param[2].y);
      float _198 = _194 + (c_sdw_param[2].y);
      float _199 = _195 + (c_sdw_param[2].y);
      float _202 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _203 = _197 * _202;
      float _204 = _198 * _202;
      float _205 = _199 * _202;
      _207 = _203;
      _208 = _204;
      _209 = _205;
   }
   else
   {
      _207 = _184;
      _208 = _185;
      _209 = _186;
   }
   float _210 = _207 * _105;
   float _211 = _208 * _105;
   float _212 = _209 * _105;
   float _223 = c_mat_emissive.x * _64;
   float _224 = _223 * c_emissive_scale;
   float _225 = c_mat_emissive.y * _65;
   float _226 = _225 * c_emissive_scale;
   float _227 = c_mat_emissive.z * _66;
   float _228 = _227 * c_emissive_scale;
   int _229 = c_bool & 2048;
   bool _230 = (_229 == 0);
   if (_230)
   {
      float _232 = _172 * _154;
      float _233 = _172 * _155;
      float _234 = _172 * _156;
      float _235 = _232 + TEXCOORD_4.x;
      float _236 = _233 + TEXCOORD_4.y;
      float _237 = _234 + TEXCOORD_4.z;
      float _238 = _172 * _157;
      float _239 = _238 + TEXCOORD_5.x;
      float _240 = _238 + TEXCOORD_5.y;
      float _241 = _238 + TEXCOORD_5.z;
      _243 = _239;
      _244 = _240;
      _245 = _241;
      _246 = _235;
      _247 = _236;
      _248 = _237;
   }
   else
   {
      _243 = TEXCOORD_5.x;
      _244 = TEXCOORD_5.y;
      _245 = TEXCOORD_5.z;
      _246 = TEXCOORD_4.x;
      _247 = TEXCOORD_4.y;
      _248 = TEXCOORD_4.z;
   }
   float _253 = -0.0f - (c_light_vec[1].x);
   float _254 = -0.0f - (c_light_vec[1].y);
   float _255 = -0.0f - (c_light_vec[1].z);
   float _256 = dot(float3(_72, _73, _74), float3(_253, _254, _255));
   float _257 = saturate(_256);
   float _262 = saturate(_87);
   float _263 = (c_light_col[0].x) * _262;
   float _264 = (c_light_col[0].y) * _262;
   float _265 = (c_light_col[0].z) * _262;
   float _270 = (c_light_col[1].x) * _257;
   float _271 = (c_light_col[1].y) * _257;
   float _272 = (c_light_col[1].z) * _257;
   float _281 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _282 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _283 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _284 = _263 * _184;
   float _285 = _264 * _185;
   float _286 = _265 * _186;
   float _287 = _270 * _172;
   float _288 = _271 * _172;
   float _289 = _272 * _172;
   float _290 = _287 + _284;
   float _291 = _288 + _285;
   float _292 = _289 + _286;
   float _293 = _281 * _290;
   float _294 = _282 * _291;
   float _295 = _283 * _292;
   float _296 = _224 + c_scene_ambient.x;
   float _297 = _296 + _293;
   float _298 = _226 + c_scene_ambient.y;
   float _299 = _298 + _294;
   float _300 = _228 + c_scene_ambient.z;
   float _301 = _300 + _295;
   float _302 = _297 * _187;
   float _303 = _299 * _188;
   float _304 = _301 * _189;
   float _305 = _246 * _210;
   float _306 = _247 * _211;
   float _307 = _248 * _212;
   float _308 = _302 + _305;
   float _309 = _303 + _306;
   float _310 = _304 + _307;
   float _315 = (c_light_vec[0].x) + _77;
   float _316 = (c_light_vec[0].y) + _78;
   float _317 = (c_light_vec[0].z) + _79;
   float _318 = dot(float3(_315, _316, _317), float3(_315, _316, _317));
   float _319 = rsqrt(_318);
   float _320 = _315 * _319;
   float _321 = -0.0f - _320;
   float _322 = _316 * _319;
   float _323 = -0.0f - _322;
   float _324 = _317 * _319;
   float _325 = -0.0f - _324;
   float _326 = dot(float3(_321, _323, _325), float3(_72, _73, _74));
   float _327 = saturate(_326);
   float _328 = _78 + -0.30000001192092896f;
   float _329 = dot(float3(_77, _328, _79), float3(_77, _328, _79));
   float _330 = rsqrt(_329);
   float _331 = _77 * _330;
   float _332 = -0.0f - _331;
   float _333 = _328 * _330;
   float _334 = -0.0f - _333;
   float _335 = _79 * _330;
   float _336 = -0.0f - _335;
   float _337 = dot(float3(_332, _334, _336), float3(_72, _73, _74));
   float _338 = saturate(_337);
   float _341 = log2(_327);
   float _342 = log2(_338);
   float _343 = _341 * c_mat_power;
   float _344 = _342 * c_mat_power;
   float _345 = exp2(_343);
   float _346 = exp2(_344);
   float _349 = _345 * (c_light_col[0].x);
   float _350 = _345 * (c_light_col[0].y);
   float _351 = _345 * (c_light_col[0].z);
   float _352 = _346 * 2.0f;
   float _353 = _352 * (c_light_vec[5].x);
   float _354 = _353 * (c_light_col[1].x);
   float _355 = _353 * (c_light_col[1].y);
   float _356 = _353 * (c_light_col[1].z);
   float _357 = _349 * _184;
   float _358 = _350 * _185;
   float _359 = _351 * _186;
   float _360 = _354 * _257;
   float _361 = _355 * _257;
   float _362 = _356 * _257;
   float _363 = _360 + _357;
   float _364 = _361 + _358;
   float _365 = _362 + _359;
   float _372 = c_mat_specular.x * c_specular_scale.x;
   float _373 = _372 * _187;
   float _374 = _373 * _363;
   float _375 = c_mat_specular.y * c_specular_scale.y;
   float _376 = _375 * _188;
   float _377 = _376 * _364;
   float _378 = c_mat_specular.z * c_specular_scale.z;
   float _379 = _378 * _189;
   float _380 = _379 * _365;
   float _381 = _243 * _210;
   float _382 = _381 * _346;
   float _383 = _244 * _211;
   float _384 = _383 * _346;
   float _385 = _245 * _212;
   float _386 = _385 * _346;
   float _387 = _374 + _382;
   float _388 = _377 + _384;
   float _389 = _380 + _386;
   if (!_230)
   {
      float _391 = _172 * _154;
      float _392 = _172 * _155;
      float _393 = _172 * _156;
      float _394 = _308 + _391;
      float _395 = _309 + _392;
      float _396 = _310 + _393;
      float _397 = _172 * _157;
      float _398 = _387 + _397;
      float _399 = _388 + _397;
      float _400 = _389 + _397;
      _402 = _394;
      _403 = _395;
      _404 = _396;
      _405 = _398;
      _406 = _399;
      _407 = _400;
   }
   else
   {
      _402 = _308;
      _403 = _309;
      _404 = _310;
      _405 = _387;
      _406 = _388;
      _407 = _389;
   }
   int _408 = c_bool & 128;
   bool _409 = (_408 != 0);
   float _410 = select(_409, 0.0f, _405);
   float _411 = select(_409, 0.0f, _406);
   float _412 = select(_409, 0.0f, _407);
   float _413 = _402 * _64;
   float _414 = _403 * _65;
   float _415 = _404 * _66;
   float _416 = _410 * _69;
   float _417 = _411 * _69;
   float _418 = _412 * _69;
   int _419 = c_bool & 32768;
   bool _420 = (_419 == 0);
   if (!_420)
   {
      float _424 = (c_ps_user[11].x) * _64;
      float _425 = (c_ps_user[11].x) * _65;
      float _426 = (c_ps_user[11].x) * _66;
      float _428 = _72 + 1.0f;
      float _429 = _73 + 1.0f;
      float _430 = _74 + 1.0f;
      float _431 = _428 * 0.5f;
      float _432 = _429 * 0.5f;
      float _433 = _430 * 0.5f;
      float _434 = saturate(_431);
      float _435 = saturate(_432);
      float _436 = saturate(_433);
      float _437 = (c_ps_user[11].y) * _434;
      float _438 = _435 * (c_ps_user[11].y);
      float _439 = _436 * (c_ps_user[11].y);
      float _441 = (c_ps_user[11].z) * _69;
      float _443 = c_mat_power * 0.009999999776482582f;
      float _444 = _443 * (c_ps_user[11].w);
      float _447 = (c_ps_user[12].x) * _184;
      float _448 = (c_ps_user[12].x) * _185;
      float _449 = (c_ps_user[12].x) * _186;
      float _451 = (c_ps_user[12].y) * _187;
      float _452 = (c_ps_user[12].y) * _188;
      float _453 = (c_ps_user[12].y) * _189;
      float _455 = (c_ps_user[12].z) * _210;
      float _456 = (c_ps_user[12].z) * _211;
      float _457 = (c_ps_user[12].z) * _212;
      float _463 = -0.0f - _77;
      float _464 = -0.0f - _78;
      float _465 = -0.0f - _79;
      float _466 = dot(float3(_463, _464, _465), float3(_72, _73, _74));
      float _467 = 1.0f - _466;
      float _468 = _467 * _467;
      float _469 = _467 * 0.9722222089767456f;
      float _470 = _468 * _468;
      float _471 = _470 * _469;
      float _472 = _471 + 0.02777777425944805f;
      float _473 = _472 * (c_ps_user[13].z);
      float _475 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _476 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _477 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _480 = (c_ps_user[14].x) * _224;
      float _481 = (c_ps_user[14].x) * _226;
      float _482 = (c_ps_user[14].x) * _228;
      float _484 = (c_ps_user[14].y) * _263;
      float _485 = (c_ps_user[14].y) * _264;
      float _486 = (c_ps_user[14].y) * _265;
      float _488 = (c_ps_user[14].z) * _270;
      float _489 = (c_ps_user[14].z) * _271;
      float _490 = (c_ps_user[14].z) * _272;
      float _492 = (c_ps_user[14].w) * _246;
      float _493 = (c_ps_user[14].w) * _247;
      float _494 = (c_ps_user[14].w) * _248;
      float _497 = (c_ps_user[15].x) * _349;
      float _498 = (c_ps_user[15].x) * _350;
      float _499 = (c_ps_user[15].x) * _351;
      float _501 = _372 * _354;
      float _502 = _501 * (c_ps_user[15].y);
      float _503 = _375 * _355;
      float _504 = _503 * (c_ps_user[15].y);
      float _505 = _378 * _356;
      float _506 = _505 * (c_ps_user[15].y);
      float _508 = (c_ps_user[15].z) * _243;
      float _509 = (c_ps_user[15].z) * _244;
      float _510 = (c_ps_user[15].z) * _245;
      float _511 = _502 + _508;
      float _512 = _511 * _346;
      float _513 = _484 + (c_ps_user[13].x);
      float _514 = _513 + _488;
      float _515 = c_diffuse_scale.x * _514;
      float _516 = _515 * c_mat_diffuse.x;
      float _517 = (c_ps_user[13].y) + _441;
      float _518 = _517 + _497;
      float _519 = c_mat_specular.x * _518;
      float _520 = _519 * c_specular_scale.x;
      float _521 = _437 + _424;
      float _522 = _521 + _444;
      float _523 = _522 + (c_ps_user[12].w);
      float _524 = _523 + _447;
      float _525 = _524 + _451;
      float _526 = _525 + _455;
      float _527 = _526 + _475;
      float _528 = _527 + _480;
      float _529 = _528 + _492;
      float _530 = _529 + _473;
      float _531 = _530 + _512;
      float _532 = _531 + _516;
      float _533 = _532 + _520;
      float _534 = _504 + _509;
      float _535 = _534 * _346;
      float _536 = _485 + (c_ps_user[13].x);
      float _537 = _536 + _489;
      float _538 = c_diffuse_scale.y * _537;
      float _539 = _538 * c_mat_diffuse.y;
      float _540 = _517 + _498;
      float _541 = c_mat_specular.y * _540;
      float _542 = _541 * c_specular_scale.y;
      float _543 = _438 + _425;
      float _544 = _543 + _444;
      float _545 = _544 + (c_ps_user[12].w);
      float _546 = _545 + _448;
      float _547 = _546 + _452;
      float _548 = _547 + _456;
      float _549 = _548 + _476;
      float _550 = _549 + _481;
      float _551 = _550 + _493;
      float _552 = _551 + _473;
      float _553 = _552 + _535;
      float _554 = _553 + _539;
      float _555 = _554 + _542;
      float _556 = _506 + _510;
      float _557 = _556 * _346;
      float _558 = _486 + (c_ps_user[13].x);
      float _559 = _558 + _490;
      float _560 = c_diffuse_scale.z * _559;
      float _561 = _560 * c_mat_diffuse.z;
      float _562 = _517 + _499;
      float _563 = c_mat_specular.z * _562;
      float _564 = _563 * c_specular_scale.z;
      float _565 = _444 + _426;
      float _566 = _565 + _439;
      float _567 = _566 + (c_ps_user[12].w);
      float _568 = _567 + _449;
      float _569 = _568 + _453;
      float _570 = _569 + _457;
      float _571 = _570 + _477;
      float _572 = _571 + _482;
      float _573 = _572 + _494;
      float _574 = _573 + _473;
      float _575 = _574 + _557;
      float _576 = _575 + _561;
      float _577 = _576 + _564;
      _579 = _533;
      _580 = _555;
      _581 = _577;
      _582 = 0.0f;
      _583 = 0.0f;
      _584 = 0.0f;
   }
   else
   {
      _579 = _413;
      _580 = _414;
      _581 = _415;
      _582 = _416;
      _583 = _417;
      _584 = _418;
   }
   float _585 = _582 + _579;
   float _586 = _583 + _580;
   float _587 = _584 + _581;
   float _592 = c_global_opacity * c_mat_opacity;
   float _593 = _592 * c_opacity_scale;
   int _594 = c_bool & 16;
   bool _595 = (_594 == 0);
   if (!_595)
   {
      float _599 = c_tone_control.x * _585;
      float _600 = c_tone_control.x * _586;
      float _601 = c_tone_control.x * _587;
      float _603 = log2(_599);
      float _604 = log2(_600);
      float _605 = log2(_601);
      float _606 = _603 * c_tone_control.y;
      float _607 = _604 * c_tone_control.y;
      float _608 = _605 * c_tone_control.y;
      float _609 = exp2(_606);
      float _610 = exp2(_607);
      float _611 = exp2(_608);
      _659 = _609;
      _660 = _610;
      _661 = _611;
   }
   else
   {
      if (_420)
      {
         float _616 = c_tone_control.x * _585;
         float _617 = c_tone_control.x * _586;
         float _618 = c_tone_control.x * _587;
         float _619 = _616 * 2.0f;
         float _620 = _617 * 2.0f;
         float _621 = _618 * 2.0f;
         float _622 = _616 * 0.30000001192092896f;
         float _623 = _617 * 0.30000001192092896f;
         float _624 = _618 * 0.30000001192092896f;
         float _625 = _622 + 0.05000000074505806f;
         float _626 = _623 + 0.05000000074505806f;
         float _627 = _624 + 0.05000000074505806f;
         float _628 = _625 * _619;
         float _629 = _626 * _620;
         float _630 = _627 * _621;
         float _631 = _628 + 0.004000000189989805f;
         float _632 = _629 + 0.004000000189989805f;
         float _633 = _630 + 0.004000000189989805f;
         float _634 = _622 + 0.5f;
         float _635 = _623 + 0.5f;
         float _636 = _624 + 0.5f;
         float _637 = _634 * _619;
         float _638 = _635 * _620;
         float _639 = _636 * _621;
         float _640 = _637 + 0.06000000238418579f;
         float _641 = _638 + 0.06000000238418579f;
         float _642 = _639 + 0.06000000238418579f;
         float _643 = _631 / _640;
         float _644 = _632 / _641;
         float _645 = _633 / _642;
         float _646 = _643 + -0.06666666269302368f;
         float _647 = _644 + -0.06666666269302368f;
         float _648 = _645 + -0.06666666269302368f;
         float _649 = _646 * 1.3790643215179443f;
         float _650 = _647 * 1.3790643215179443f;
         float _651 = _648 * 1.3790643215179443f;
         float _652 = YKR_MaterialCurve(_649, _616);
         float _653 = YKR_MaterialCurve(_650, _617);
         float _654 = YKR_MaterialCurve(_651, _618);
         float _655 = sqrt(_652);
         float _656 = sqrt(_653);
         float _657 = sqrt(_654);
         _659 = _655;
         _660 = _656;
         _661 = _657;
      }
      else
      {
         _659 = _585;
         _660 = _586;
         _661 = _587;
      }
   }
   int _662 = c_bool & 4;
   bool _663 = (_662 == 0);
   if (_663)
   {
      float _665 = saturate(COLOR_1.x);
      float _670 = (c_fog_param[0].x) - _659;
      float _671 = (c_fog_param[0].y) - _660;
      float _672 = (c_fog_param[0].z) - _661;
      float _673 = _670 * _665;
      float _674 = _671 * _665;
      float _675 = _672 * _665;
      _677 = _673;
      _678 = _674;
      _679 = _675;
   }
   else
   {
      _677 = COLOR_1.x;
      _678 = COLOR_1.y;
      _679 = COLOR_1.z;
   }
   float _680 = _677 + _659;
   float _681 = _678 + _660;
   float _682 = _679 + _661;
   float _688 = (c_ps_col_scl[0].x) * _680;
   float _689 = (c_ps_col_scl[0].y) * _681;
   float _690 = (c_ps_col_scl[0].z) * _682;
   float _691 = (c_ps_col_scl[0].w) * _593;
   SV_Target.x = _688;
   SV_Target.y = _689;
   SV_Target.z = _690;
   SV_Target.w = _691;
   int _694 = c_bool & 8;
   bool _695 = (_694 != 0);
   float _696 = select(_695, _680, _688);
   float _697 = select(_695, _681, _689);
   float _698 = select(_695, _682, _690);
   float _699 = select(_695, _593, _691);
   SV_Target.x = _696;
   SV_Target.y = _697;
   SV_Target.z = _698;
   SV_Target.w = _699;
   return SV_Target;
}
