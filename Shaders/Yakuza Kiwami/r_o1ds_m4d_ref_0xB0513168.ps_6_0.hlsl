// Yakuza Kiwami material r_o1ds_m4d[ref] (0xB0513168, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s3_s : register(s3);

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

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
   float4 _46 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _51 = _46.x * _46.x;
   float _52 = _46.y * _46.y;
   float _53 = _46.z * _46.z;
   float _57 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _58 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _59 = s3_t.Sample(s3_s, float2(_57, _58));
   float4 _64 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _67 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _68 = rsqrt(_67);
   float _69 = _68 * TEXCOORD_2.x;
   float _70 = _68 * TEXCOORD_2.y;
   float _71 = _68 * TEXCOORD_2.z;
   float _72 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _73 = rsqrt(_72);
   float _74 = _73 * TEXCOORD_3.x;
   float _75 = _73 * TEXCOORD_3.y;
   float _76 = _73 * TEXCOORD_3.z;
   float _81 = -0.0f - (c_light_vec[0].x);
   float _82 = -0.0f - (c_light_vec[0].y);
   float _83 = -0.0f - (c_light_vec[0].z);
   float _84 = dot(float3(_69, _70, _71), float3(_81, _82, _83));
   float _88 = c_rt_size.z * SV_Position.x;
   float _89 = c_rt_size.w * SV_Position.y;
   int _92 = c_bool & 8;
   bool _93 = (_92 == 0);
   float _100;
   float _101;
   float _102;
   float _114;
   float _115;
   float _170;
   float _171;
   float _172;
   float _173;
   float _187;
   float _188;
   float _234;
   float _235;
   float _236;
   float _271;
   float _272;
   float _273;
   float _274;
   float _275;
   float _276;
   float _430;
   float _431;
   float _432;
   float _433;
   float _434;
   float _435;
   float _611;
   float _612;
   float _613;
   float _614;
   float _615;
   float _616;
   float _744;
   float _745;
   float _746;
   float _762;
   float _763;
   float _764;
   if (!_93)
   {
      float4 _95 = s13_t.Sample(s13_s, float2(_88, _89));
      _100 = _95.x;
      _101 = _95.y;
      _102 = _95.w;
   }
   else
   {
      _100 = 1.0f;
      _101 = 1.0f;
      _102 = 1.0f;
   }
   if (!_93)
   {
      float _107 = (c_sdw_param[0].x) * _100;
      float _108 = (c_sdw_param[0].z) * _101;
      float _111 = _107 + (c_sdw_param[0].y);
      float _112 = _108 + (c_sdw_param[0].w);
      _114 = _111;
      _115 = _112;
   }
   else
   {
      _114 = _100;
      _115 = _101;
   }
   float _122 = (c_sdw_param[1].x) * _64.y;
   float _124 = _122 + (c_sdw_param[1].y);
   float _125 = _124 * _64.x;
   float _126 = 1.0f - (c_light_col[5].x);
   float _127 = 1.0f - (c_light_col[5].y);
   float _128 = 1.0f - (c_light_col[5].z);
   float _129 = _125 * _126;
   float _130 = _125 * _127;
   float _131 = _125 * _128;
   float _132 = _129 + (c_light_col[5].x);
   float _133 = _130 + (c_light_col[5].y);
   float _134 = _131 + (c_light_col[5].z);
   float _138 = c_rt_size.z * SV_Position.x;
   float _139 = c_rt_size.w * SV_Position.y;
   float4 _140 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_138, _139)));
   int _147 = c_bool & 16384;
   bool _148 = (_147 == 0);
   if (!_148)
   {
      float _150 = _140.x * 2.0f;
      float _151 = _140.y * 2.0f;
      float _152 = _140.z * 2.0f;
      float _153 = _140.w * 2.0f;
      _170 = _150;
      _171 = _151;
      _172 = _152;
      _173 = _153;
   }
   else
   {
      int _155 = c_bool & 8192;
      bool _156 = (_155 == 0);
      if (!_156)
      {
         float _158 = _140.x * 2.0f;
         float _159 = _140.y * 2.0f;
         float _160 = _140.z * 2.0f;
         float _161 = _140.w * 2.0f;
         _170 = _158;
         _171 = _159;
         _172 = _160;
         _173 = _161;
      }
      else
      {
         float _165 = _140.x / c_tone_control.z;
         float _166 = _140.y / c_tone_control.z;
         float _167 = _140.z / c_tone_control.z;
         float _168 = _140.w / c_tone_control.z;
         _170 = _165;
         _171 = _166;
         _172 = _167;
         _173 = _168;
      }
   }
   int _174 = c_bool & 8;
   bool _175 = (_174 == 0);
   if (!_175)
   {
      float _180 = (c_sdw_param[0].x) * _114;
      float _181 = (c_sdw_param[0].z) * _115;
      float _184 = _180 + (c_sdw_param[0].y);
      float _185 = _181 + (c_sdw_param[0].w);
      _187 = _184;
      _188 = _185;
   }
   else
   {
      _187 = _114;
      _188 = _115;
   }
   float _189 = _188 * _187;
   float _194 = 1.0f - (c_light_col[5].x);
   float _195 = 1.0f - (c_light_col[5].y);
   float _196 = 1.0f - (c_light_col[5].z);
   float _197 = _194 * _189;
   float _198 = _195 * _189;
   float _199 = _196 * _189;
   float _200 = _197 + (c_light_col[5].x);
   float _201 = _198 + (c_light_col[5].y);
   float _202 = _199 + (c_light_col[5].z);
   float _203 = _102 * COLOR.x;
   float _204 = _203 * _132;
   float _205 = _102 * COLOR.y;
   float _206 = _205 * _133;
   float _207 = _102 * COLOR.z;
   float _208 = _207 * _134;
   float _209 = _200 * _132;
   float _210 = _201 * _133;
   float _211 = _202 * _134;
   if (!_175)
   {
      float _215 = (c_sdw_param[2].x) * _200;
      float _216 = (c_sdw_param[2].x) * _201;
      float _217 = (c_sdw_param[2].x) * _202;
      float _219 = _215 + (c_sdw_param[2].y);
      float _220 = _216 + (c_sdw_param[2].y);
      float _221 = _217 + (c_sdw_param[2].y);
      float _223 = (c_sdw_param[2].z) * _132;
      float _224 = (c_sdw_param[2].z) * _133;
      float _225 = (c_sdw_param[2].z) * _134;
      float _227 = _223 + (c_sdw_param[2].w);
      float _228 = _224 + (c_sdw_param[2].w);
      float _229 = _225 + (c_sdw_param[2].w);
      float _230 = _227 * _219;
      float _231 = _228 * _220;
      float _232 = _229 * _221;
      _234 = _230;
      _235 = _231;
      _236 = _232;
   }
   else
   {
      _234 = _209;
      _235 = _210;
      _236 = _211;
   }
   float _237 = _102 * _64.x;
   float _238 = _234 * _237;
   float _239 = _235 * _237;
   float _240 = _236 * _237;
   float _251 = c_mat_emissive.x * _51;
   float _252 = _251 * c_emissive_scale;
   float _253 = c_mat_emissive.y * _52;
   float _254 = _253 * c_emissive_scale;
   float _255 = c_mat_emissive.z * _53;
   float _256 = _255 * c_emissive_scale;
   int _257 = c_bool & 2048;
   bool _258 = (_257 == 0);
   if (_258)
   {
      float _260 = _188 * _170;
      float _261 = _188 * _171;
      float _262 = _188 * _172;
      float _263 = _260 + TEXCOORD_4.x;
      float _264 = _261 + TEXCOORD_4.y;
      float _265 = _262 + TEXCOORD_4.z;
      float _266 = _188 * _173;
      float _267 = _266 + TEXCOORD_5.x;
      float _268 = _266 + TEXCOORD_5.y;
      float _269 = _266 + TEXCOORD_5.z;
      _271 = _263;
      _272 = _264;
      _273 = _265;
      _274 = _267;
      _275 = _268;
      _276 = _269;
   }
   else
   {
      _271 = TEXCOORD_4.x;
      _272 = TEXCOORD_4.y;
      _273 = TEXCOORD_4.z;
      _274 = TEXCOORD_5.x;
      _275 = TEXCOORD_5.y;
      _276 = TEXCOORD_5.z;
   }
   float _281 = -0.0f - (c_light_vec[1].x);
   float _282 = -0.0f - (c_light_vec[1].y);
   float _283 = -0.0f - (c_light_vec[1].z);
   float _284 = dot(float3(_69, _70, _71), float3(_281, _282, _283));
   float _285 = saturate(_284);
   float _290 = saturate(_84);
   float _291 = (c_light_col[0].x) * _290;
   float _292 = (c_light_col[0].y) * _290;
   float _293 = (c_light_col[0].z) * _290;
   float _298 = (c_light_col[1].x) * _285;
   float _299 = (c_light_col[1].y) * _285;
   float _300 = (c_light_col[1].z) * _285;
   float _309 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _310 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _311 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _312 = _291 * _200;
   float _313 = _292 * _201;
   float _314 = _293 * _202;
   float _315 = _298 * _188;
   float _316 = _299 * _188;
   float _317 = _300 * _188;
   float _318 = _315 + _312;
   float _319 = _316 + _313;
   float _320 = _317 + _314;
   float _321 = _309 * _318;
   float _322 = _310 * _319;
   float _323 = _311 * _320;
   float _324 = _252 + c_scene_ambient.x;
   float _325 = _324 + _321;
   float _326 = _254 + c_scene_ambient.y;
   float _327 = _326 + _322;
   float _328 = _256 + c_scene_ambient.z;
   float _329 = _328 + _323;
   float _330 = _325 * _204;
   float _331 = _327 * _206;
   float _332 = _329 * _208;
   float _333 = _271 * _238;
   float _334 = _272 * _239;
   float _335 = _273 * _240;
   float _336 = _330 + _333;
   float _337 = _331 + _334;
   float _338 = _332 + _335;
   float _343 = (c_light_vec[0].x) + _74;
   float _344 = (c_light_vec[0].y) + _75;
   float _345 = (c_light_vec[0].z) + _76;
   float _346 = dot(float3(_343, _344, _345), float3(_343, _344, _345));
   float _347 = rsqrt(_346);
   float _348 = _343 * _347;
   float _349 = -0.0f - _348;
   float _350 = _344 * _347;
   float _351 = -0.0f - _350;
   float _352 = _345 * _347;
   float _353 = -0.0f - _352;
   float _354 = dot(float3(_349, _351, _353), float3(_69, _70, _71));
   float _355 = saturate(_354);
   float _356 = _75 + -0.30000001192092896f;
   float _357 = dot(float3(_74, _356, _76), float3(_74, _356, _76));
   float _358 = rsqrt(_357);
   float _359 = _74 * _358;
   float _360 = -0.0f - _359;
   float _361 = _356 * _358;
   float _362 = -0.0f - _361;
   float _363 = _76 * _358;
   float _364 = -0.0f - _363;
   float _365 = dot(float3(_360, _362, _364), float3(_69, _70, _71));
   float _366 = saturate(_365);
   float _369 = log2(_355);
   float _370 = log2(_366);
   float _371 = _369 * c_mat_power;
   float _372 = _370 * c_mat_power;
   float _373 = exp2(_371);
   float _374 = exp2(_372);
   float _377 = _373 * (c_light_col[0].x);
   float _378 = _373 * (c_light_col[0].y);
   float _379 = _373 * (c_light_col[0].z);
   float _380 = _374 * 2.0f;
   float _381 = _380 * (c_light_vec[5].x);
   float _382 = _381 * (c_light_col[1].x);
   float _383 = _381 * (c_light_col[1].y);
   float _384 = _381 * (c_light_col[1].z);
   float _385 = _377 * _200;
   float _386 = _378 * _201;
   float _387 = _379 * _202;
   float _388 = _382 * _285;
   float _389 = _383 * _285;
   float _390 = _384 * _285;
   float _391 = _388 + _385;
   float _392 = _389 + _386;
   float _393 = _390 + _387;
   float _400 = c_mat_specular.x * c_specular_scale.x;
   float _401 = _400 * _204;
   float _402 = _401 * _391;
   float _403 = c_mat_specular.y * c_specular_scale.y;
   float _404 = _403 * _206;
   float _405 = _404 * _392;
   float _406 = c_mat_specular.z * c_specular_scale.z;
   float _407 = _406 * _208;
   float _408 = _407 * _393;
   float _409 = _274 * _238;
   float _410 = _409 * _374;
   float _411 = _275 * _239;
   float _412 = _411 * _374;
   float _413 = _276 * _240;
   float _414 = _413 * _374;
   float _415 = _402 + _410;
   float _416 = _405 + _412;
   float _417 = _408 + _414;
   if (!_258)
   {
      float _419 = _188 * _170;
      float _420 = _188 * _171;
      float _421 = _188 * _172;
      float _422 = _336 + _419;
      float _423 = _337 + _420;
      float _424 = _338 + _421;
      float _425 = _188 * _173;
      float _426 = _415 + _425;
      float _427 = _416 + _425;
      float _428 = _417 + _425;
      _430 = _422;
      _431 = _423;
      _432 = _424;
      _433 = _426;
      _434 = _427;
      _435 = _428;
   }
   else
   {
      _430 = _336;
      _431 = _337;
      _432 = _338;
      _433 = _415;
      _434 = _416;
      _435 = _417;
   }
   int _436 = c_bool & 128;
   bool _437 = (_436 != 0);
   float _438 = select(_437, 0.0f, _433);
   float _439 = select(_437, 0.0f, _434);
   float _440 = select(_437, 0.0f, _435);
   float _441 = _430 * _51;
   float _442 = _431 * _52;
   float _443 = _432 * _53;
   float _444 = _438 * _59.x;
   float _445 = _439 * _59.y;
   float _446 = _440 * _59.z;
   int _447 = c_bool & 32768;
   bool _448 = (_447 == 0);
   if (!_448)
   {
      float _452 = (c_ps_user[11].x) * _51;
      float _453 = (c_ps_user[11].x) * _52;
      float _454 = (c_ps_user[11].x) * _53;
      float _456 = _69 + 1.0f;
      float _457 = _70 + 1.0f;
      float _458 = _71 + 1.0f;
      float _459 = _456 * 0.5f;
      float _460 = _457 * 0.5f;
      float _461 = _458 * 0.5f;
      float _462 = saturate(_459);
      float _463 = saturate(_460);
      float _464 = saturate(_461);
      float _465 = (c_ps_user[11].y) * _462;
      float _466 = _463 * (c_ps_user[11].y);
      float _467 = _464 * (c_ps_user[11].y);
      float _469 = (c_ps_user[11].z) * _59.x;
      float _470 = (c_ps_user[11].z) * _59.y;
      float _471 = (c_ps_user[11].z) * _59.z;
      float _473 = c_mat_power * 0.009999999776482582f;
      float _474 = _473 * (c_ps_user[11].w);
      float _477 = (c_ps_user[12].x) * _200;
      float _478 = (c_ps_user[12].x) * _201;
      float _479 = (c_ps_user[12].x) * _202;
      float _481 = (c_ps_user[12].y) * _204;
      float _482 = (c_ps_user[12].y) * _206;
      float _483 = (c_ps_user[12].y) * _208;
      float _485 = (c_ps_user[12].z) * _238;
      float _486 = (c_ps_user[12].z) * _239;
      float _487 = (c_ps_user[12].z) * _240;
      float _493 = -0.0f - _74;
      float _494 = -0.0f - _75;
      float _495 = -0.0f - _76;
      float _496 = dot(float3(_493, _494, _495), float3(_69, _70, _71));
      float _497 = 1.0f - _496;
      float _498 = _497 * _497;
      float _499 = _497 * 0.9722222089767456f;
      float _500 = _498 * _498;
      float _501 = _500 * _499;
      float _502 = _501 + 0.02777777425944805f;
      float _503 = _502 * (c_ps_user[13].z);
      float _505 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _506 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _507 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _510 = (c_ps_user[14].x) * _252;
      float _511 = (c_ps_user[14].x) * _254;
      float _512 = (c_ps_user[14].x) * _256;
      float _514 = (c_ps_user[14].y) * _291;
      float _515 = (c_ps_user[14].y) * _292;
      float _516 = (c_ps_user[14].y) * _293;
      float _518 = (c_ps_user[14].z) * _298;
      float _519 = (c_ps_user[14].z) * _299;
      float _520 = (c_ps_user[14].z) * _300;
      float _522 = (c_ps_user[14].w) * _271;
      float _523 = (c_ps_user[14].w) * _272;
      float _524 = (c_ps_user[14].w) * _273;
      float _527 = (c_ps_user[15].x) * _377;
      float _528 = (c_ps_user[15].x) * _378;
      float _529 = (c_ps_user[15].x) * _379;
      float _531 = _400 * _382;
      float _532 = _531 * (c_ps_user[15].y);
      float _533 = _403 * _383;
      float _534 = _533 * (c_ps_user[15].y);
      float _535 = _406 * _384;
      float _536 = _535 * (c_ps_user[15].y);
      float _538 = (c_ps_user[15].z) * _274;
      float _539 = (c_ps_user[15].z) * _275;
      float _540 = (c_ps_user[15].z) * _276;
      float _541 = _532 + _538;
      float _542 = _541 * _374;
      float _543 = _514 + (c_ps_user[13].x);
      float _544 = _543 + _518;
      float _545 = c_diffuse_scale.x * _544;
      float _546 = _545 * c_mat_diffuse.x;
      float _547 = (c_ps_user[13].y) + _469;
      float _548 = _547 + _527;
      float _549 = c_mat_specular.x * _548;
      float _550 = _549 * c_specular_scale.x;
      float _551 = _465 + _452;
      float _552 = _551 + _474;
      float _553 = _552 + (c_ps_user[12].w);
      float _554 = _553 + _477;
      float _555 = _554 + _481;
      float _556 = _555 + _485;
      float _557 = _556 + _505;
      float _558 = _557 + _510;
      float _559 = _558 + _522;
      float _560 = _559 + _503;
      float _561 = _560 + _542;
      float _562 = _561 + _546;
      float _563 = _562 + _550;
      float _564 = _534 + _539;
      float _565 = _564 * _374;
      float _566 = _515 + (c_ps_user[13].x);
      float _567 = _566 + _519;
      float _568 = c_diffuse_scale.y * _567;
      float _569 = _568 * c_mat_diffuse.y;
      float _570 = (c_ps_user[13].y) + _470;
      float _571 = _570 + _528;
      float _572 = c_mat_specular.y * _571;
      float _573 = _572 * c_specular_scale.y;
      float _574 = _466 + _453;
      float _575 = _574 + _474;
      float _576 = _575 + (c_ps_user[12].w);
      float _577 = _576 + _478;
      float _578 = _577 + _482;
      float _579 = _578 + _486;
      float _580 = _579 + _506;
      float _581 = _580 + _511;
      float _582 = _581 + _523;
      float _583 = _582 + _503;
      float _584 = _583 + _565;
      float _585 = _584 + _569;
      float _586 = _585 + _573;
      float _587 = _536 + _540;
      float _588 = _587 * _374;
      float _589 = _516 + (c_ps_user[13].x);
      float _590 = _589 + _520;
      float _591 = c_diffuse_scale.z * _590;
      float _592 = _591 * c_mat_diffuse.z;
      float _593 = (c_ps_user[13].y) + _471;
      float _594 = _593 + _529;
      float _595 = c_mat_specular.z * _594;
      float _596 = _595 * c_specular_scale.z;
      float _597 = _474 + _454;
      float _598 = _597 + _467;
      float _599 = _598 + (c_ps_user[12].w);
      float _600 = _599 + _479;
      float _601 = _600 + _483;
      float _602 = _601 + _487;
      float _603 = _602 + _507;
      float _604 = _603 + _512;
      float _605 = _604 + _524;
      float _606 = _605 + _503;
      float _607 = _606 + _588;
      float _608 = _607 + _592;
      float _609 = _608 + _596;
      _611 = _563;
      _612 = _586;
      _613 = _609;
      _614 = 0.0f;
      _615 = 0.0f;
      _616 = 0.0f;
   }
   else
   {
      _611 = _441;
      _612 = _442;
      _613 = _443;
      _614 = _444;
      _615 = _445;
      _616 = _446;
   }
   float _617 = _614 + _611;
   float _618 = _615 + _612;
   float _619 = _616 + _613;
   float _620 = dot(float3(_74, _75, _76), float3(_69, _70, _71));
   float _621 = _620 * 2.0f;
   float _622 = _621 * _69;
   float _623 = _621 * _70;
   float _624 = _621 * _71;
   float _625 = _74 - _622;
   float _626 = _75 - _623;
   float _627 = _76 - _624;
   float _628 = -0.0f - _625;
   float4 _629 = s6c_t.Sample(s6c_s, float3(_628, _626, _627));
   float _636 = _115 * _114;
   float _637 = _84 + 0.75f;
   float _638 = saturate(_637);
   float _639 = _636 * _638;
   float _640 = max(_639, (c_light_vec[5].y));
   float _641 = max(_629.w, 0.003921568859368563f);
   float _642 = 0.25f / _641;
   float _643 = _642 * _629.x;
   float _644 = _642 * _629.y;
   float _645 = _642 * _629.z;
   float _646 = _59.w * TEXCOORD_2.w;
   float _647 = _64.x * _646;
   float _648 = _647 * _59.x;
   float _649 = _648 * _132;
   float _650 = _649 * _640;
   float _651 = _643 * _643;
   float _652 = _651 * _650;
   float _653 = _647 * _59.y;
   float _654 = _653 * _133;
   float _655 = _654 * _640;
   float _656 = _644 * _644;
   float _657 = _656 * _655;
   float _658 = _647 * _59.z;
   float _659 = _658 * _134;
   float _660 = _659 * _640;
   float _661 = _645 * _645;
   float _662 = _661 * _660;
   float _663 = _617 + _652;
   float _664 = _618 + _657;
   float _665 = _619 + _662;
   float _672 = c_mat_opacity * _46.w;
   float _673 = _672 * c_opacity_scale;
   float _674 = _673 * c_global_opacity;
   int _677 = c_bool & 16;
   bool _678 = (_677 == 0);
   if (!_678)
   {
      float _682 = c_tone_control.x * _663;
      float _683 = c_tone_control.x * _664;
      float _684 = c_tone_control.x * _665;
      float _686 = log2(_682);
      float _687 = log2(_683);
      float _688 = log2(_684);
      float _689 = _686 * c_tone_control.y;
      float _690 = _687 * c_tone_control.y;
      float _691 = _688 * c_tone_control.y;
      float _692 = exp2(_689);
      float _693 = exp2(_690);
      float _694 = exp2(_691);
      _744 = _692;
      _745 = _693;
      _746 = _694;
   }
   else
   {
      int _696 = c_bool & 32768;
      bool _697 = (_696 == 0);
      if (_697)
      {
         float _701 = c_tone_control.x * _663;
         float _702 = c_tone_control.x * _664;
         float _703 = c_tone_control.x * _665;
         float _704 = _701 * 2.0f;
         float _705 = _702 * 2.0f;
         float _706 = _703 * 2.0f;
         float _707 = _701 * 0.30000001192092896f;
         float _708 = _702 * 0.30000001192092896f;
         float _709 = _703 * 0.30000001192092896f;
         float _710 = _707 + 0.05000000074505806f;
         float _711 = _708 + 0.05000000074505806f;
         float _712 = _709 + 0.05000000074505806f;
         float _713 = _710 * _704;
         float _714 = _711 * _705;
         float _715 = _712 * _706;
         float _716 = _713 + 0.004000000189989805f;
         float _717 = _714 + 0.004000000189989805f;
         float _718 = _715 + 0.004000000189989805f;
         float _719 = _707 + 0.5f;
         float _720 = _708 + 0.5f;
         float _721 = _709 + 0.5f;
         float _722 = _719 * _704;
         float _723 = _720 * _705;
         float _724 = _721 * _706;
         float _725 = _722 + 0.06000000238418579f;
         float _726 = _723 + 0.06000000238418579f;
         float _727 = _724 + 0.06000000238418579f;
         float _728 = _716 / _725;
         float _729 = _717 / _726;
         float _730 = _718 / _727;
         float _731 = _728 + -0.06666666269302368f;
         float _732 = _729 + -0.06666666269302368f;
         float _733 = _730 + -0.06666666269302368f;
         float _734 = _731 * 1.3790643215179443f;
         float _735 = _732 * 1.3790643215179443f;
         float _736 = _733 * 1.3790643215179443f;
         float _737 = YKR_MaterialCurve(_734, _701);
         float _738 = YKR_MaterialCurve(_735, _702);
         float _739 = YKR_MaterialCurve(_736, _703);
         float _740 = sqrt(_737);
         float _741 = sqrt(_738);
         float _742 = sqrt(_739);
         _744 = _740;
         _745 = _741;
         _746 = _742;
      }
      else
      {
         _744 = _663;
         _745 = _664;
         _746 = _665;
      }
   }
   int _747 = c_bool & 4;
   bool _748 = (_747 == 0);
   if (_748)
   {
      float _750 = saturate(COLOR_1.x);
      float _755 = (c_fog_param[0].x) - _744;
      float _756 = (c_fog_param[0].y) - _745;
      float _757 = (c_fog_param[0].z) - _746;
      float _758 = _755 * _750;
      float _759 = _756 * _750;
      float _760 = _757 * _750;
      _762 = _758;
      _763 = _759;
      _764 = _760;
   }
   else
   {
      _762 = COLOR_1.x;
      _763 = COLOR_1.y;
      _764 = COLOR_1.z;
   }
   float _765 = _762 + _744;
   float _766 = _763 + _745;
   float _767 = _764 + _746;
   float _773 = (c_ps_col_scl[0].x) * _765;
   float _774 = (c_ps_col_scl[0].y) * _766;
   float _775 = (c_ps_col_scl[0].z) * _767;
   float _776 = (c_ps_col_scl[0].w) * _674;
   SV_Target.x = _773;
   SV_Target.y = _774;
   SV_Target.z = _775;
   SV_Target.w = _776;
   int _779 = c_bool & 8;
   bool _780 = (_779 != 0);
   float _781 = select(_780, _765, _773);
   float _782 = select(_780, _766, _774);
   float _783 = select(_780, _767, _775);
   float _784 = select(_780, _674, _776);
   SV_Target.x = _781;
   SV_Target.y = _782;
   SV_Target.z = _783;
   SV_Target.w = _784;
   return SV_Target;
}
