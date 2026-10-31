// Yakuza Kiwami material r_o1d_b(vr)2d[ref] (0xD7339D22, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s6c_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _45 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _50 = _45.x * _45.x;
   float _51 = _45.y * _45.y;
   float _52 = _45.z * _45.z;
   float4 _53 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float _61 = _53.w * COLOR_1.w;
   float _62 = _58 - _50;
   float _63 = _59 - _51;
   float _64 = _60 - _52;
   float _65 = _62 * _61;
   float _66 = _63 * _61;
   float _67 = _64 * _61;
   float _68 = _65 + _50;
   float _69 = _66 + _51;
   float _70 = _67 + _52;
   float _71 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _72 = rsqrt(_71);
   float _73 = _72 * TEXCOORD_2.x;
   float _74 = _72 * TEXCOORD_2.y;
   float _75 = _72 * TEXCOORD_2.z;
   float _76 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _77 = rsqrt(_76);
   float _78 = _77 * TEXCOORD_3.x;
   float _79 = _77 * TEXCOORD_3.y;
   float _80 = _77 * TEXCOORD_3.z;
   float _85 = -0.0f - (c_light_vec[0].x);
   float _86 = -0.0f - (c_light_vec[0].y);
   float _87 = -0.0f - (c_light_vec[0].z);
   float _88 = dot(float3(_73, _74, _75), float3(_85, _86, _87));
   float _92 = c_rt_size.z * SV_Position.x;
   float _93 = c_rt_size.w * SV_Position.y;
   int _96 = c_bool & 8;
   bool _97 = (_96 == 0);
   float _104;
   float _105;
   float _106;
   float _118;
   float _119;
   float _155;
   float _156;
   float _157;
   float _158;
   float _172;
   float _173;
   float _208;
   float _209;
   float _210;
   float _244;
   float _245;
   float _246;
   float _247;
   float _248;
   float _249;
   float _403;
   float _404;
   float _405;
   float _406;
   float _407;
   float _408;
   float _576;
   float _577;
   float _578;
   float _579;
   float _580;
   float _581;
   float _699;
   float _700;
   float _701;
   float _717;
   float _718;
   float _719;
   if (!_97)
   {
      float4 _99 = s13_t.Sample(s13_s, float2(_92, _93));
      _104 = _99.x;
      _105 = _99.y;
      _106 = _99.w;
   }
   else
   {
      _104 = 1.0f;
      _105 = 1.0f;
      _106 = 1.0f;
   }
   if (!_97)
   {
      float _111 = (c_sdw_param[0].x) * _104;
      float _112 = (c_sdw_param[0].z) * _105;
      float _115 = _111 + (c_sdw_param[0].y);
      float _116 = _112 + (c_sdw_param[0].w);
      _118 = _115;
      _119 = _116;
   }
   else
   {
      _118 = _104;
      _119 = _105;
   }
   float _123 = c_rt_size.z * SV_Position.x;
   float _124 = c_rt_size.w * SV_Position.y;
   float4 _125 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_123, _124)));
   int _132 = c_bool & 16384;
   bool _133 = (_132 == 0);
   if (!_133)
   {
      float _135 = _125.x * 2.0f;
      float _136 = _125.y * 2.0f;
      float _137 = _125.z * 2.0f;
      float _138 = _125.w * 2.0f;
      _155 = _135;
      _156 = _136;
      _157 = _137;
      _158 = _138;
   }
   else
   {
      int _140 = c_bool & 8192;
      bool _141 = (_140 == 0);
      if (!_141)
      {
         float _143 = _125.x * 2.0f;
         float _144 = _125.y * 2.0f;
         float _145 = _125.z * 2.0f;
         float _146 = _125.w * 2.0f;
         _155 = _143;
         _156 = _144;
         _157 = _145;
         _158 = _146;
      }
      else
      {
         float _150 = _125.x / c_tone_control.z;
         float _151 = _125.y / c_tone_control.z;
         float _152 = _125.z / c_tone_control.z;
         float _153 = _125.w / c_tone_control.z;
         _155 = _150;
         _156 = _151;
         _157 = _152;
         _158 = _153;
      }
   }
   int _159 = c_bool & 8;
   bool _160 = (_159 == 0);
   if (!_160)
   {
      float _165 = (c_sdw_param[0].x) * _118;
      float _166 = (c_sdw_param[0].z) * _119;
      float _169 = _165 + (c_sdw_param[0].y);
      float _170 = _166 + (c_sdw_param[0].w);
      _172 = _169;
      _173 = _170;
   }
   else
   {
      _172 = _118;
      _173 = _119;
   }
   float _174 = _173 * _172;
   float _179 = 1.0f - (c_light_col[5].x);
   float _180 = 1.0f - (c_light_col[5].y);
   float _181 = 1.0f - (c_light_col[5].z);
   float _182 = _179 * _174;
   float _183 = _180 * _174;
   float _184 = _181 * _174;
   float _185 = _182 + (c_light_col[5].x);
   float _186 = _183 + (c_light_col[5].y);
   float _187 = _184 + (c_light_col[5].z);
   float _188 = _106 * COLOR.x;
   float _189 = _106 * COLOR.y;
   float _190 = _106 * COLOR.z;
   if (!_160)
   {
      float _194 = (c_sdw_param[2].x) * _185;
      float _195 = (c_sdw_param[2].x) * _186;
      float _196 = (c_sdw_param[2].x) * _187;
      float _198 = _194 + (c_sdw_param[2].y);
      float _199 = _195 + (c_sdw_param[2].y);
      float _200 = _196 + (c_sdw_param[2].y);
      float _203 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _204 = _198 * _203;
      float _205 = _199 * _203;
      float _206 = _200 * _203;
      _208 = _204;
      _209 = _205;
      _210 = _206;
   }
   else
   {
      _208 = _185;
      _209 = _186;
      _210 = _187;
   }
   float _211 = _208 * _106;
   float _212 = _209 * _106;
   float _213 = _210 * _106;
   float _224 = c_mat_emissive.x * _68;
   float _225 = _224 * c_emissive_scale;
   float _226 = c_mat_emissive.y * _69;
   float _227 = _226 * c_emissive_scale;
   float _228 = c_mat_emissive.z * _70;
   float _229 = _228 * c_emissive_scale;
   int _230 = c_bool & 2048;
   bool _231 = (_230 == 0);
   if (_231)
   {
      float _233 = _173 * _155;
      float _234 = _173 * _156;
      float _235 = _173 * _157;
      float _236 = _233 + TEXCOORD_4.x;
      float _237 = _234 + TEXCOORD_4.y;
      float _238 = _235 + TEXCOORD_4.z;
      float _239 = _173 * _158;
      float _240 = _239 + TEXCOORD_5.x;
      float _241 = _239 + TEXCOORD_5.y;
      float _242 = _239 + TEXCOORD_5.z;
      _244 = _236;
      _245 = _237;
      _246 = _238;
      _247 = _240;
      _248 = _241;
      _249 = _242;
   }
   else
   {
      _244 = TEXCOORD_4.x;
      _245 = TEXCOORD_4.y;
      _246 = TEXCOORD_4.z;
      _247 = TEXCOORD_5.x;
      _248 = TEXCOORD_5.y;
      _249 = TEXCOORD_5.z;
   }
   float _254 = -0.0f - (c_light_vec[1].x);
   float _255 = -0.0f - (c_light_vec[1].y);
   float _256 = -0.0f - (c_light_vec[1].z);
   float _257 = dot(float3(_73, _74, _75), float3(_254, _255, _256));
   float _258 = saturate(_257);
   float _263 = saturate(_88);
   float _264 = (c_light_col[0].x) * _263;
   float _265 = (c_light_col[0].y) * _263;
   float _266 = (c_light_col[0].z) * _263;
   float _271 = (c_light_col[1].x) * _258;
   float _272 = (c_light_col[1].y) * _258;
   float _273 = (c_light_col[1].z) * _258;
   float _282 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _283 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _284 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _285 = _264 * _185;
   float _286 = _265 * _186;
   float _287 = _266 * _187;
   float _288 = _271 * _173;
   float _289 = _272 * _173;
   float _290 = _273 * _173;
   float _291 = _288 + _285;
   float _292 = _289 + _286;
   float _293 = _290 + _287;
   float _294 = _282 * _291;
   float _295 = _283 * _292;
   float _296 = _284 * _293;
   float _297 = _225 + c_scene_ambient.x;
   float _298 = _297 + _294;
   float _299 = _227 + c_scene_ambient.y;
   float _300 = _299 + _295;
   float _301 = _229 + c_scene_ambient.z;
   float _302 = _301 + _296;
   float _303 = _298 * _188;
   float _304 = _300 * _189;
   float _305 = _302 * _190;
   float _306 = _244 * _211;
   float _307 = _245 * _212;
   float _308 = _246 * _213;
   float _309 = _303 + _306;
   float _310 = _304 + _307;
   float _311 = _305 + _308;
   float _316 = (c_light_vec[0].x) + _78;
   float _317 = (c_light_vec[0].y) + _79;
   float _318 = (c_light_vec[0].z) + _80;
   float _319 = dot(float3(_316, _317, _318), float3(_316, _317, _318));
   float _320 = rsqrt(_319);
   float _321 = _316 * _320;
   float _322 = -0.0f - _321;
   float _323 = _317 * _320;
   float _324 = -0.0f - _323;
   float _325 = _318 * _320;
   float _326 = -0.0f - _325;
   float _327 = dot(float3(_322, _324, _326), float3(_73, _74, _75));
   float _328 = saturate(_327);
   float _329 = _79 + -0.30000001192092896f;
   float _330 = dot(float3(_78, _329, _80), float3(_78, _329, _80));
   float _331 = rsqrt(_330);
   float _332 = _78 * _331;
   float _333 = -0.0f - _332;
   float _334 = _329 * _331;
   float _335 = -0.0f - _334;
   float _336 = _80 * _331;
   float _337 = -0.0f - _336;
   float _338 = dot(float3(_333, _335, _337), float3(_73, _74, _75));
   float _339 = saturate(_338);
   float _342 = log2(_328);
   float _343 = log2(_339);
   float _344 = _342 * c_mat_power;
   float _345 = _343 * c_mat_power;
   float _346 = exp2(_344);
   float _347 = exp2(_345);
   float _350 = _346 * (c_light_col[0].x);
   float _351 = _346 * (c_light_col[0].y);
   float _352 = _346 * (c_light_col[0].z);
   float _353 = _347 * 2.0f;
   float _354 = _353 * (c_light_vec[5].x);
   float _355 = _354 * (c_light_col[1].x);
   float _356 = _354 * (c_light_col[1].y);
   float _357 = _354 * (c_light_col[1].z);
   float _358 = _350 * _185;
   float _359 = _351 * _186;
   float _360 = _352 * _187;
   float _361 = _355 * _258;
   float _362 = _356 * _258;
   float _363 = _357 * _258;
   float _364 = _361 + _358;
   float _365 = _362 + _359;
   float _366 = _363 + _360;
   float _373 = c_mat_specular.x * c_specular_scale.x;
   float _374 = _373 * _188;
   float _375 = _374 * _364;
   float _376 = c_mat_specular.y * c_specular_scale.y;
   float _377 = _376 * _189;
   float _378 = _377 * _365;
   float _379 = c_mat_specular.z * c_specular_scale.z;
   float _380 = _379 * _190;
   float _381 = _380 * _366;
   float _382 = _247 * _211;
   float _383 = _382 * _347;
   float _384 = _248 * _212;
   float _385 = _384 * _347;
   float _386 = _249 * _213;
   float _387 = _386 * _347;
   float _388 = _375 + _383;
   float _389 = _378 + _385;
   float _390 = _381 + _387;
   if (!_231)
   {
      float _392 = _173 * _155;
      float _393 = _173 * _156;
      float _394 = _173 * _157;
      float _395 = _309 + _392;
      float _396 = _310 + _393;
      float _397 = _311 + _394;
      float _398 = _173 * _158;
      float _399 = _388 + _398;
      float _400 = _389 + _398;
      float _401 = _390 + _398;
      _403 = _395;
      _404 = _396;
      _405 = _397;
      _406 = _399;
      _407 = _400;
      _408 = _401;
   }
   else
   {
      _403 = _309;
      _404 = _310;
      _405 = _311;
      _406 = _388;
      _407 = _389;
      _408 = _390;
   }
   int _409 = c_bool & 128;
   bool _410 = (_409 != 0);
   float _411 = select(_410, 0.0f, _406);
   float _412 = select(_410, 0.0f, _407);
   float _413 = select(_410, 0.0f, _408);
   float _414 = _403 * _68;
   float _415 = _404 * _69;
   float _416 = _405 * _70;
   int _417 = c_bool & 32768;
   bool _418 = (_417 == 0);
   if (!_418)
   {
      float _422 = (c_ps_user[11].x) * _68;
      float _423 = (c_ps_user[11].x) * _69;
      float _424 = (c_ps_user[11].x) * _70;
      float _426 = _73 + 1.0f;
      float _427 = _74 + 1.0f;
      float _428 = _75 + 1.0f;
      float _429 = _426 * 0.5f;
      float _430 = _427 * 0.5f;
      float _431 = _428 * 0.5f;
      float _432 = saturate(_429);
      float _433 = saturate(_430);
      float _434 = saturate(_431);
      float _435 = (c_ps_user[11].y) * _432;
      float _436 = _433 * (c_ps_user[11].y);
      float _437 = _434 * (c_ps_user[11].y);
      float _440 = c_mat_power * 0.009999999776482582f;
      float _441 = _440 * (c_ps_user[11].w);
      float _444 = (c_ps_user[12].x) * _185;
      float _445 = (c_ps_user[12].x) * _186;
      float _446 = (c_ps_user[12].x) * _187;
      float _448 = (c_ps_user[12].y) * _188;
      float _449 = (c_ps_user[12].y) * _189;
      float _450 = (c_ps_user[12].y) * _190;
      float _452 = (c_ps_user[12].z) * _211;
      float _453 = (c_ps_user[12].z) * _212;
      float _454 = (c_ps_user[12].z) * _213;
      float _460 = -0.0f - _78;
      float _461 = -0.0f - _79;
      float _462 = -0.0f - _80;
      float _463 = dot(float3(_460, _461, _462), float3(_73, _74, _75));
      float _464 = 1.0f - _463;
      float _465 = _464 * _464;
      float _466 = _464 * 0.9722222089767456f;
      float _467 = _465 * _465;
      float _468 = _467 * _466;
      float _469 = _468 + 0.02777777425944805f;
      float _470 = _469 * (c_ps_user[13].z);
      float _472 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _473 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _474 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _477 = (c_ps_user[14].x) * _225;
      float _478 = (c_ps_user[14].x) * _227;
      float _479 = (c_ps_user[14].x) * _229;
      float _481 = (c_ps_user[14].y) * _264;
      float _482 = (c_ps_user[14].y) * _265;
      float _483 = (c_ps_user[14].y) * _266;
      float _485 = (c_ps_user[14].z) * _271;
      float _486 = (c_ps_user[14].z) * _272;
      float _487 = (c_ps_user[14].z) * _273;
      float _489 = (c_ps_user[14].w) * _244;
      float _490 = (c_ps_user[14].w) * _245;
      float _491 = (c_ps_user[14].w) * _246;
      float _494 = (c_ps_user[15].x) * _350;
      float _495 = (c_ps_user[15].x) * _351;
      float _496 = (c_ps_user[15].x) * _352;
      float _498 = _373 * _355;
      float _499 = _498 * (c_ps_user[15].y);
      float _500 = _376 * _356;
      float _501 = _500 * (c_ps_user[15].y);
      float _502 = _379 * _357;
      float _503 = _502 * (c_ps_user[15].y);
      float _505 = (c_ps_user[15].z) * _247;
      float _506 = (c_ps_user[15].z) * _248;
      float _507 = (c_ps_user[15].z) * _249;
      float _508 = _499 + _505;
      float _509 = _508 * _347;
      float _510 = _481 + (c_ps_user[13].x);
      float _511 = _510 + _485;
      float _512 = c_diffuse_scale.x * _511;
      float _513 = _512 * c_mat_diffuse.x;
      float _514 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _515 = _514 + _494;
      float _516 = c_mat_specular.x * _515;
      float _517 = _516 * c_specular_scale.x;
      float _518 = _435 + _422;
      float _519 = _518 + _441;
      float _520 = _519 + (c_ps_user[12].w);
      float _521 = _520 + _444;
      float _522 = _521 + _448;
      float _523 = _522 + _452;
      float _524 = _523 + _472;
      float _525 = _524 + _477;
      float _526 = _525 + _489;
      float _527 = _526 + _470;
      float _528 = _527 + _509;
      float _529 = _528 + _513;
      float _530 = _529 + _517;
      float _531 = _501 + _506;
      float _532 = _531 * _347;
      float _533 = _482 + (c_ps_user[13].x);
      float _534 = _533 + _486;
      float _535 = c_diffuse_scale.y * _534;
      float _536 = _535 * c_mat_diffuse.y;
      float _537 = _514 + _495;
      float _538 = c_mat_specular.y * _537;
      float _539 = _538 * c_specular_scale.y;
      float _540 = _436 + _423;
      float _541 = _540 + _441;
      float _542 = _541 + (c_ps_user[12].w);
      float _543 = _542 + _445;
      float _544 = _543 + _449;
      float _545 = _544 + _453;
      float _546 = _545 + _473;
      float _547 = _546 + _478;
      float _548 = _547 + _490;
      float _549 = _548 + _470;
      float _550 = _549 + _532;
      float _551 = _550 + _536;
      float _552 = _551 + _539;
      float _553 = _503 + _507;
      float _554 = _553 * _347;
      float _555 = _483 + (c_ps_user[13].x);
      float _556 = _555 + _487;
      float _557 = c_diffuse_scale.z * _556;
      float _558 = _557 * c_mat_diffuse.z;
      float _559 = _514 + _496;
      float _560 = c_mat_specular.z * _559;
      float _561 = _560 * c_specular_scale.z;
      float _562 = _441 + _424;
      float _563 = _562 + _437;
      float _564 = _563 + (c_ps_user[12].w);
      float _565 = _564 + _446;
      float _566 = _565 + _450;
      float _567 = _566 + _454;
      float _568 = _567 + _474;
      float _569 = _568 + _479;
      float _570 = _569 + _491;
      float _571 = _570 + _470;
      float _572 = _571 + _554;
      float _573 = _572 + _558;
      float _574 = _573 + _561;
      _576 = _530;
      _577 = _552;
      _578 = _574;
      _579 = 0.0f;
      _580 = 0.0f;
      _581 = 0.0f;
   }
   else
   {
      _576 = _414;
      _577 = _415;
      _578 = _416;
      _579 = _411;
      _580 = _412;
      _581 = _413;
   }
   float _582 = _579 + _576;
   float _583 = _580 + _577;
   float _584 = _581 + _578;
   float _585 = dot(float3(_78, _79, _80), float3(_73, _74, _75));
   float _586 = _585 * 2.0f;
   float _587 = _586 * _73;
   float _588 = _586 * _74;
   float _589 = _586 * _75;
   float _590 = _78 - _587;
   float _591 = _79 - _588;
   float _592 = _80 - _589;
   float _593 = -0.0f - _590;
   float4 _594 = s6c_t.Sample(s6c_s, float3(_593, _591, _592));
   float _601 = _119 * _118;
   float _602 = _88 + 0.75f;
   float _603 = saturate(_602);
   float _604 = _601 * _603;
   float _605 = max(_604, (c_light_vec[5].y));
   float _606 = max(_594.w, 0.003921568859368563f);
   float _607 = 0.25f / _606;
   float _608 = _607 * _594.x;
   float _609 = _607 * _594.y;
   float _610 = _607 * _594.z;
   float _611 = _605 * TEXCOORD_2.w;
   float _612 = _608 * _608;
   float _613 = _612 * _611;
   float _614 = _609 * _609;
   float _615 = _614 * _611;
   float _616 = _610 * _610;
   float _617 = _616 * _611;
   float _618 = _582 + _613;
   float _619 = _583 + _615;
   float _620 = _584 + _617;
   float _627 = c_mat_opacity * _45.w;
   float _628 = _627 * c_opacity_scale;
   float _629 = _628 * c_global_opacity;
   int _632 = c_bool & 16;
   bool _633 = (_632 == 0);
   if (!_633)
   {
      float _637 = c_tone_control.x * _618;
      float _638 = c_tone_control.x * _619;
      float _639 = c_tone_control.x * _620;
      float _641 = log2(_637);
      float _642 = log2(_638);
      float _643 = log2(_639);
      float _644 = _641 * c_tone_control.y;
      float _645 = _642 * c_tone_control.y;
      float _646 = _643 * c_tone_control.y;
      float _647 = exp2(_644);
      float _648 = exp2(_645);
      float _649 = exp2(_646);
      _699 = _647;
      _700 = _648;
      _701 = _649;
   }
   else
   {
      int _651 = c_bool & 32768;
      bool _652 = (_651 == 0);
      if (_652)
      {
         float _656 = c_tone_control.x * _618;
         float _657 = c_tone_control.x * _619;
         float _658 = c_tone_control.x * _620;
         float _659 = _656 * 2.0f;
         float _660 = _657 * 2.0f;
         float _661 = _658 * 2.0f;
         float _662 = _656 * 0.30000001192092896f;
         float _663 = _657 * 0.30000001192092896f;
         float _664 = _658 * 0.30000001192092896f;
         float _665 = _662 + 0.05000000074505806f;
         float _666 = _663 + 0.05000000074505806f;
         float _667 = _664 + 0.05000000074505806f;
         float _668 = _665 * _659;
         float _669 = _666 * _660;
         float _670 = _667 * _661;
         float _671 = _668 + 0.004000000189989805f;
         float _672 = _669 + 0.004000000189989805f;
         float _673 = _670 + 0.004000000189989805f;
         float _674 = _662 + 0.5f;
         float _675 = _663 + 0.5f;
         float _676 = _664 + 0.5f;
         float _677 = _674 * _659;
         float _678 = _675 * _660;
         float _679 = _676 * _661;
         float _680 = _677 + 0.06000000238418579f;
         float _681 = _678 + 0.06000000238418579f;
         float _682 = _679 + 0.06000000238418579f;
         float _683 = _671 / _680;
         float _684 = _672 / _681;
         float _685 = _673 / _682;
         float _686 = _683 + -0.06666666269302368f;
         float _687 = _684 + -0.06666666269302368f;
         float _688 = _685 + -0.06666666269302368f;
         float _689 = _686 * 1.3790643215179443f;
         float _690 = _687 * 1.3790643215179443f;
         float _691 = _688 * 1.3790643215179443f;
         float _692 = YKR_MaterialCurve(_689, _656);
         float _693 = YKR_MaterialCurve(_690, _657);
         float _694 = YKR_MaterialCurve(_691, _658);
         float _695 = sqrt(_692);
         float _696 = sqrt(_693);
         float _697 = sqrt(_694);
         _699 = _695;
         _700 = _696;
         _701 = _697;
      }
      else
      {
         _699 = _618;
         _700 = _619;
         _701 = _620;
      }
   }
   int _702 = c_bool & 4;
   bool _703 = (_702 == 0);
   if (_703)
   {
      float _705 = saturate(COLOR_1.x);
      float _710 = (c_fog_param[0].x) - _699;
      float _711 = (c_fog_param[0].y) - _700;
      float _712 = (c_fog_param[0].z) - _701;
      float _713 = _710 * _705;
      float _714 = _711 * _705;
      float _715 = _712 * _705;
      _717 = _713;
      _718 = _714;
      _719 = _715;
   }
   else
   {
      _717 = COLOR_1.x;
      _718 = COLOR_1.y;
      _719 = COLOR_1.z;
   }
   float _720 = _717 + _699;
   float _721 = _718 + _700;
   float _722 = _719 + _701;
   float _728 = (c_ps_col_scl[0].x) * _720;
   float _729 = (c_ps_col_scl[0].y) * _721;
   float _730 = (c_ps_col_scl[0].z) * _722;
   float _731 = (c_ps_col_scl[0].w) * _629;
   SV_Target.x = _728;
   SV_Target.y = _729;
   SV_Target.z = _730;
   SV_Target.w = _731;
   int _734 = c_bool & 8;
   bool _735 = (_734 != 0);
   float _736 = select(_735, _720, _728);
   float _737 = select(_735, _721, _729);
   float _738 = select(_735, _722, _730);
   float _739 = select(_735, _629, _731);
   SV_Target.x = _736;
   SV_Target.y = _737;
   SV_Target.z = _738;
   SV_Target.w = _739;
   return SV_Target;
}
