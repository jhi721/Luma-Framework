// Yakuza Kiwami material r_o1d2s3t (0x1690E105, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
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
   float4 _49 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _54 = _49.x * _49.x;
   float _55 = _49.y * _49.y;
   float _56 = _49.z * _49.z;
   float4 _57 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _61 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _64 = _61.x * 2.0f;
   float _65 = _61.y * 2.0f;
   float _66 = _64 + -1.0f;
   float _67 = _65 + -1.0f;
   float _68 = dot(float2(_66, _67), float2(_66, _67));
   float _69 = 1.0f - _68;
   float _70 = saturate(_69);
   float _71 = sqrt(_70);
   float _72 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _73 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _74 = _72 - _73;
   float _75 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _76 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _77 = _75 - _76;
   float _78 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _79 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _80 = _78 - _79;
   float _81 = _66 * TEXCOORD_6.x;
   float _82 = _66 * TEXCOORD_6.y;
   float _83 = _66 * TEXCOORD_6.z;
   float _84 = _67 * TEXCOORD_6.w;
   float _85 = _84 * _74;
   float _86 = _84 * _77;
   float _87 = _84 * _80;
   float _88 = _71 * TEXCOORD_2.x;
   float _89 = _71 * TEXCOORD_2.y;
   float _90 = _71 * TEXCOORD_2.z;
   float _91 = _88 + _81;
   float _92 = _91 + _85;
   float _93 = _89 + _82;
   float _94 = _93 + _86;
   float _95 = _90 + _83;
   float _96 = _95 + _87;
   float _97 = dot(float3(_92, _94, _96), float3(_92, _94, _96));
   float _98 = rsqrt(_97);
   float _99 = _92 * _98;
   float _100 = _94 * _98;
   float _101 = _96 * _98;
   float _102 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _103 = rsqrt(_102);
   float _104 = _103 * TEXCOORD_3.x;
   float _105 = _103 * TEXCOORD_3.y;
   float _106 = _103 * TEXCOORD_3.z;
   float _111 = -0.0f - (c_light_vec[0].x);
   float _112 = -0.0f - (c_light_vec[0].y);
   float _113 = -0.0f - (c_light_vec[0].z);
   float _114 = dot(float3(_99, _100, _101), float3(_111, _112, _113));
   float _118 = c_rt_size.z * SV_Position.x;
   float _119 = c_rt_size.w * SV_Position.y;
   int _122 = c_bool & 8;
   bool _123 = (_122 == 0);
   float _130;
   float _131;
   float _132;
   float _144;
   float _145;
   float _181;
   float _182;
   float _183;
   float _184;
   float _198;
   float _199;
   float _234;
   float _235;
   float _236;
   float _270;
   float _271;
   float _272;
   float _273;
   float _274;
   float _275;
   float _429;
   float _430;
   float _431;
   float _432;
   float _433;
   float _434;
   float _610;
   float _611;
   float _612;
   float _613;
   float _614;
   float _615;
   float _691;
   float _692;
   float _693;
   float _709;
   float _710;
   float _711;
   if (!_123)
   {
      float4 _125 = s13_t.Sample(s13_s, float2(_118, _119));
      _130 = _125.x;
      _131 = _125.y;
      _132 = _125.w;
   }
   else
   {
      _130 = 1.0f;
      _131 = 1.0f;
      _132 = 1.0f;
   }
   if (!_123)
   {
      float _137 = (c_sdw_param[0].x) * _130;
      float _138 = (c_sdw_param[0].z) * _131;
      float _141 = _137 + (c_sdw_param[0].y);
      float _142 = _138 + (c_sdw_param[0].w);
      _144 = _141;
      _145 = _142;
   }
   else
   {
      _144 = _130;
      _145 = _131;
   }
   float _149 = c_rt_size.z * SV_Position.x;
   float _150 = c_rt_size.w * SV_Position.y;
   float4 _151 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_149, _150)));
   int _158 = c_bool & 16384;
   bool _159 = (_158 == 0);
   if (!_159)
   {
      float _161 = _151.x * 2.0f;
      float _162 = _151.y * 2.0f;
      float _163 = _151.z * 2.0f;
      float _164 = _151.w * 2.0f;
      _181 = _161;
      _182 = _162;
      _183 = _163;
      _184 = _164;
   }
   else
   {
      int _166 = c_bool & 8192;
      bool _167 = (_166 == 0);
      if (!_167)
      {
         float _169 = _151.x * 2.0f;
         float _170 = _151.y * 2.0f;
         float _171 = _151.z * 2.0f;
         float _172 = _151.w * 2.0f;
         _181 = _169;
         _182 = _170;
         _183 = _171;
         _184 = _172;
      }
      else
      {
         float _176 = _151.x / c_tone_control.z;
         float _177 = _151.y / c_tone_control.z;
         float _178 = _151.z / c_tone_control.z;
         float _179 = _151.w / c_tone_control.z;
         _181 = _176;
         _182 = _177;
         _183 = _178;
         _184 = _179;
      }
   }
   int _185 = c_bool & 8;
   bool _186 = (_185 == 0);
   if (!_186)
   {
      float _191 = (c_sdw_param[0].x) * _144;
      float _192 = (c_sdw_param[0].z) * _145;
      float _195 = _191 + (c_sdw_param[0].y);
      float _196 = _192 + (c_sdw_param[0].w);
      _198 = _195;
      _199 = _196;
   }
   else
   {
      _198 = _144;
      _199 = _145;
   }
   float _200 = _199 * _198;
   float _205 = 1.0f - (c_light_col[5].x);
   float _206 = 1.0f - (c_light_col[5].y);
   float _207 = 1.0f - (c_light_col[5].z);
   float _208 = _205 * _200;
   float _209 = _206 * _200;
   float _210 = _207 * _200;
   float _211 = _208 + (c_light_col[5].x);
   float _212 = _209 + (c_light_col[5].y);
   float _213 = _210 + (c_light_col[5].z);
   float _214 = _132 * COLOR.x;
   float _215 = _132 * COLOR.y;
   float _216 = _132 * COLOR.z;
   if (!_186)
   {
      float _220 = (c_sdw_param[2].x) * _211;
      float _221 = (c_sdw_param[2].x) * _212;
      float _222 = (c_sdw_param[2].x) * _213;
      float _224 = _220 + (c_sdw_param[2].y);
      float _225 = _221 + (c_sdw_param[2].y);
      float _226 = _222 + (c_sdw_param[2].y);
      float _229 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _230 = _224 * _229;
      float _231 = _225 * _229;
      float _232 = _226 * _229;
      _234 = _230;
      _235 = _231;
      _236 = _232;
   }
   else
   {
      _234 = _211;
      _235 = _212;
      _236 = _213;
   }
   float _237 = _234 * _132;
   float _238 = _235 * _132;
   float _239 = _236 * _132;
   float _250 = c_mat_emissive.x * _54;
   float _251 = _250 * c_emissive_scale;
   float _252 = c_mat_emissive.y * _55;
   float _253 = _252 * c_emissive_scale;
   float _254 = c_mat_emissive.z * _56;
   float _255 = _254 * c_emissive_scale;
   int _256 = c_bool & 2048;
   bool _257 = (_256 == 0);
   if (_257)
   {
      float _259 = _199 * _181;
      float _260 = _199 * _182;
      float _261 = _199 * _183;
      float _262 = _259 + TEXCOORD_4.x;
      float _263 = _260 + TEXCOORD_4.y;
      float _264 = _261 + TEXCOORD_4.z;
      float _265 = _199 * _184;
      float _266 = _265 + TEXCOORD_5.x;
      float _267 = _265 + TEXCOORD_5.y;
      float _268 = _265 + TEXCOORD_5.z;
      _270 = _266;
      _271 = _267;
      _272 = _268;
      _273 = _262;
      _274 = _263;
      _275 = _264;
   }
   else
   {
      _270 = TEXCOORD_5.x;
      _271 = TEXCOORD_5.y;
      _272 = TEXCOORD_5.z;
      _273 = TEXCOORD_4.x;
      _274 = TEXCOORD_4.y;
      _275 = TEXCOORD_4.z;
   }
   float _280 = -0.0f - (c_light_vec[1].x);
   float _281 = -0.0f - (c_light_vec[1].y);
   float _282 = -0.0f - (c_light_vec[1].z);
   float _283 = dot(float3(_99, _100, _101), float3(_280, _281, _282));
   float _284 = saturate(_283);
   float _289 = saturate(_114);
   float _290 = (c_light_col[0].x) * _289;
   float _291 = (c_light_col[0].y) * _289;
   float _292 = (c_light_col[0].z) * _289;
   float _297 = (c_light_col[1].x) * _284;
   float _298 = (c_light_col[1].y) * _284;
   float _299 = (c_light_col[1].z) * _284;
   float _308 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _309 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _310 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _311 = _290 * _211;
   float _312 = _291 * _212;
   float _313 = _292 * _213;
   float _314 = _297 * _199;
   float _315 = _298 * _199;
   float _316 = _299 * _199;
   float _317 = _314 + _311;
   float _318 = _315 + _312;
   float _319 = _316 + _313;
   float _320 = _308 * _317;
   float _321 = _309 * _318;
   float _322 = _310 * _319;
   float _323 = _251 + c_scene_ambient.x;
   float _324 = _323 + _320;
   float _325 = _253 + c_scene_ambient.y;
   float _326 = _325 + _321;
   float _327 = _255 + c_scene_ambient.z;
   float _328 = _327 + _322;
   float _329 = _324 * _214;
   float _330 = _326 * _215;
   float _331 = _328 * _216;
   float _332 = _273 * _237;
   float _333 = _274 * _238;
   float _334 = _275 * _239;
   float _335 = _329 + _332;
   float _336 = _330 + _333;
   float _337 = _331 + _334;
   float _342 = (c_light_vec[0].x) + _104;
   float _343 = (c_light_vec[0].y) + _105;
   float _344 = (c_light_vec[0].z) + _106;
   float _345 = dot(float3(_342, _343, _344), float3(_342, _343, _344));
   float _346 = rsqrt(_345);
   float _347 = _342 * _346;
   float _348 = -0.0f - _347;
   float _349 = _343 * _346;
   float _350 = -0.0f - _349;
   float _351 = _344 * _346;
   float _352 = -0.0f - _351;
   float _353 = dot(float3(_348, _350, _352), float3(_99, _100, _101));
   float _354 = saturate(_353);
   float _355 = _105 + -0.30000001192092896f;
   float _356 = dot(float3(_104, _355, _106), float3(_104, _355, _106));
   float _357 = rsqrt(_356);
   float _358 = _104 * _357;
   float _359 = -0.0f - _358;
   float _360 = _355 * _357;
   float _361 = -0.0f - _360;
   float _362 = _106 * _357;
   float _363 = -0.0f - _362;
   float _364 = dot(float3(_359, _361, _363), float3(_99, _100, _101));
   float _365 = saturate(_364);
   float _368 = log2(_354);
   float _369 = log2(_365);
   float _370 = _368 * c_mat_power;
   float _371 = _369 * c_mat_power;
   float _372 = exp2(_370);
   float _373 = exp2(_371);
   float _376 = _372 * (c_light_col[0].x);
   float _377 = _372 * (c_light_col[0].y);
   float _378 = _372 * (c_light_col[0].z);
   float _379 = _373 * 2.0f;
   float _380 = _379 * (c_light_vec[5].x);
   float _381 = _380 * (c_light_col[1].x);
   float _382 = _380 * (c_light_col[1].y);
   float _383 = _380 * (c_light_col[1].z);
   float _384 = _376 * _211;
   float _385 = _377 * _212;
   float _386 = _378 * _213;
   float _387 = _381 * _284;
   float _388 = _382 * _284;
   float _389 = _383 * _284;
   float _390 = _387 + _384;
   float _391 = _388 + _385;
   float _392 = _389 + _386;
   float _399 = c_mat_specular.x * c_specular_scale.x;
   float _400 = _399 * _214;
   float _401 = _400 * _390;
   float _402 = c_mat_specular.y * c_specular_scale.y;
   float _403 = _402 * _215;
   float _404 = _403 * _391;
   float _405 = c_mat_specular.z * c_specular_scale.z;
   float _406 = _405 * _216;
   float _407 = _406 * _392;
   float _408 = _270 * _237;
   float _409 = _408 * _373;
   float _410 = _271 * _238;
   float _411 = _410 * _373;
   float _412 = _272 * _239;
   float _413 = _412 * _373;
   float _414 = _401 + _409;
   float _415 = _404 + _411;
   float _416 = _407 + _413;
   if (!_257)
   {
      float _418 = _199 * _181;
      float _419 = _199 * _182;
      float _420 = _199 * _183;
      float _421 = _335 + _418;
      float _422 = _336 + _419;
      float _423 = _337 + _420;
      float _424 = _199 * _184;
      float _425 = _414 + _424;
      float _426 = _415 + _424;
      float _427 = _416 + _424;
      _429 = _421;
      _430 = _422;
      _431 = _423;
      _432 = _425;
      _433 = _426;
      _434 = _427;
   }
   else
   {
      _429 = _335;
      _430 = _336;
      _431 = _337;
      _432 = _414;
      _433 = _415;
      _434 = _416;
   }
   int _435 = c_bool & 128;
   bool _436 = (_435 != 0);
   float _437 = select(_436, 0.0f, _432);
   float _438 = select(_436, 0.0f, _433);
   float _439 = select(_436, 0.0f, _434);
   float _440 = _429 * _54;
   float _441 = _430 * _55;
   float _442 = _431 * _56;
   float _443 = _437 * _57.x;
   float _444 = _438 * _57.y;
   float _445 = _439 * _57.z;
   int _446 = c_bool & 32768;
   bool _447 = (_446 == 0);
   if (!_447)
   {
      float _451 = (c_ps_user[11].x) * _54;
      float _452 = (c_ps_user[11].x) * _55;
      float _453 = (c_ps_user[11].x) * _56;
      float _455 = _99 + 1.0f;
      float _456 = _100 + 1.0f;
      float _457 = _101 + 1.0f;
      float _458 = _455 * 0.5f;
      float _459 = _456 * 0.5f;
      float _460 = _457 * 0.5f;
      float _461 = saturate(_458);
      float _462 = saturate(_459);
      float _463 = saturate(_460);
      float _464 = (c_ps_user[11].y) * _461;
      float _465 = _462 * (c_ps_user[11].y);
      float _466 = _463 * (c_ps_user[11].y);
      float _468 = (c_ps_user[11].z) * _57.x;
      float _469 = (c_ps_user[11].z) * _57.y;
      float _470 = (c_ps_user[11].z) * _57.z;
      float _472 = c_mat_power * 0.009999999776482582f;
      float _473 = _472 * (c_ps_user[11].w);
      float _476 = (c_ps_user[12].x) * _211;
      float _477 = (c_ps_user[12].x) * _212;
      float _478 = (c_ps_user[12].x) * _213;
      float _480 = (c_ps_user[12].y) * _214;
      float _481 = (c_ps_user[12].y) * _215;
      float _482 = (c_ps_user[12].y) * _216;
      float _484 = (c_ps_user[12].z) * _237;
      float _485 = (c_ps_user[12].z) * _238;
      float _486 = (c_ps_user[12].z) * _239;
      float _492 = -0.0f - _104;
      float _493 = -0.0f - _105;
      float _494 = -0.0f - _106;
      float _495 = dot(float3(_492, _493, _494), float3(_99, _100, _101));
      float _496 = 1.0f - _495;
      float _497 = _496 * _496;
      float _498 = _496 * 0.9722222089767456f;
      float _499 = _497 * _497;
      float _500 = _499 * _498;
      float _501 = _500 + 0.02777777425944805f;
      float _502 = _501 * (c_ps_user[13].z);
      float _504 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _505 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _506 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _509 = (c_ps_user[14].x) * _251;
      float _510 = (c_ps_user[14].x) * _253;
      float _511 = (c_ps_user[14].x) * _255;
      float _513 = (c_ps_user[14].y) * _290;
      float _514 = (c_ps_user[14].y) * _291;
      float _515 = (c_ps_user[14].y) * _292;
      float _517 = (c_ps_user[14].z) * _297;
      float _518 = (c_ps_user[14].z) * _298;
      float _519 = (c_ps_user[14].z) * _299;
      float _521 = (c_ps_user[14].w) * _273;
      float _522 = (c_ps_user[14].w) * _274;
      float _523 = (c_ps_user[14].w) * _275;
      float _526 = (c_ps_user[15].x) * _376;
      float _527 = (c_ps_user[15].x) * _377;
      float _528 = (c_ps_user[15].x) * _378;
      float _530 = _399 * _381;
      float _531 = _530 * (c_ps_user[15].y);
      float _532 = _402 * _382;
      float _533 = _532 * (c_ps_user[15].y);
      float _534 = _405 * _383;
      float _535 = _534 * (c_ps_user[15].y);
      float _537 = (c_ps_user[15].z) * _270;
      float _538 = (c_ps_user[15].z) * _271;
      float _539 = (c_ps_user[15].z) * _272;
      float _540 = _531 + _537;
      float _541 = _540 * _373;
      float _542 = _513 + (c_ps_user[13].x);
      float _543 = _542 + _517;
      float _544 = c_diffuse_scale.x * _543;
      float _545 = _544 * c_mat_diffuse.x;
      float _546 = (c_ps_user[13].y) + _468;
      float _547 = _546 + _526;
      float _548 = c_mat_specular.x * _547;
      float _549 = _548 * c_specular_scale.x;
      float _550 = _464 + _451;
      float _551 = _550 + _473;
      float _552 = _551 + (c_ps_user[12].w);
      float _553 = _552 + _476;
      float _554 = _553 + _480;
      float _555 = _554 + _484;
      float _556 = _555 + _504;
      float _557 = _556 + _509;
      float _558 = _557 + _521;
      float _559 = _558 + _502;
      float _560 = _559 + _541;
      float _561 = _560 + _545;
      float _562 = _561 + _549;
      float _563 = _533 + _538;
      float _564 = _563 * _373;
      float _565 = _514 + (c_ps_user[13].x);
      float _566 = _565 + _518;
      float _567 = c_diffuse_scale.y * _566;
      float _568 = _567 * c_mat_diffuse.y;
      float _569 = (c_ps_user[13].y) + _469;
      float _570 = _569 + _527;
      float _571 = c_mat_specular.y * _570;
      float _572 = _571 * c_specular_scale.y;
      float _573 = _465 + _452;
      float _574 = _573 + _473;
      float _575 = _574 + (c_ps_user[12].w);
      float _576 = _575 + _477;
      float _577 = _576 + _481;
      float _578 = _577 + _485;
      float _579 = _578 + _505;
      float _580 = _579 + _510;
      float _581 = _580 + _522;
      float _582 = _581 + _502;
      float _583 = _582 + _564;
      float _584 = _583 + _568;
      float _585 = _584 + _572;
      float _586 = _535 + _539;
      float _587 = _586 * _373;
      float _588 = _515 + (c_ps_user[13].x);
      float _589 = _588 + _519;
      float _590 = c_diffuse_scale.z * _589;
      float _591 = _590 * c_mat_diffuse.z;
      float _592 = (c_ps_user[13].y) + _470;
      float _593 = _592 + _528;
      float _594 = c_mat_specular.z * _593;
      float _595 = _594 * c_specular_scale.z;
      float _596 = _473 + _453;
      float _597 = _596 + _466;
      float _598 = _597 + (c_ps_user[12].w);
      float _599 = _598 + _478;
      float _600 = _599 + _482;
      float _601 = _600 + _486;
      float _602 = _601 + _506;
      float _603 = _602 + _511;
      float _604 = _603 + _523;
      float _605 = _604 + _502;
      float _606 = _605 + _587;
      float _607 = _606 + _591;
      float _608 = _607 + _595;
      _610 = _562;
      _611 = _585;
      _612 = _608;
      _613 = 0.0f;
      _614 = 0.0f;
      _615 = 0.0f;
   }
   else
   {
      _610 = _440;
      _611 = _441;
      _612 = _442;
      _613 = _443;
      _614 = _444;
      _615 = _445;
   }
   float _616 = _613 + _610;
   float _617 = _614 + _611;
   float _618 = _615 + _612;
   float _623 = c_mat_opacity * _49.w;
   float _624 = _623 * c_global_opacity;
   float _625 = _624 * c_opacity_scale;
   int _626 = c_bool & 16;
   bool _627 = (_626 == 0);
   if (!_627)
   {
      float _631 = c_tone_control.x * _616;
      float _632 = c_tone_control.x * _617;
      float _633 = c_tone_control.x * _618;
      float _635 = log2(_631);
      float _636 = log2(_632);
      float _637 = log2(_633);
      float _638 = _635 * c_tone_control.y;
      float _639 = _636 * c_tone_control.y;
      float _640 = _637 * c_tone_control.y;
      float _641 = exp2(_638);
      float _642 = exp2(_639);
      float _643 = exp2(_640);
      _691 = _641;
      _692 = _642;
      _693 = _643;
   }
   else
   {
      if (_447)
      {
         float _648 = c_tone_control.x * _616;
         float _649 = c_tone_control.x * _617;
         float _650 = c_tone_control.x * _618;
         float _651 = _648 * 2.0f;
         float _652 = _649 * 2.0f;
         float _653 = _650 * 2.0f;
         float _654 = _648 * 0.30000001192092896f;
         float _655 = _649 * 0.30000001192092896f;
         float _656 = _650 * 0.30000001192092896f;
         float _657 = _654 + 0.05000000074505806f;
         float _658 = _655 + 0.05000000074505806f;
         float _659 = _656 + 0.05000000074505806f;
         float _660 = _657 * _651;
         float _661 = _658 * _652;
         float _662 = _659 * _653;
         float _663 = _660 + 0.004000000189989805f;
         float _664 = _661 + 0.004000000189989805f;
         float _665 = _662 + 0.004000000189989805f;
         float _666 = _654 + 0.5f;
         float _667 = _655 + 0.5f;
         float _668 = _656 + 0.5f;
         float _669 = _666 * _651;
         float _670 = _667 * _652;
         float _671 = _668 * _653;
         float _672 = _669 + 0.06000000238418579f;
         float _673 = _670 + 0.06000000238418579f;
         float _674 = _671 + 0.06000000238418579f;
         float _675 = _663 / _672;
         float _676 = _664 / _673;
         float _677 = _665 / _674;
         float _678 = _675 + -0.06666666269302368f;
         float _679 = _676 + -0.06666666269302368f;
         float _680 = _677 + -0.06666666269302368f;
         float _681 = _678 * 1.3790643215179443f;
         float _682 = _679 * 1.3790643215179443f;
         float _683 = _680 * 1.3790643215179443f;
         float _684 = YKR_MaterialCurve(_681, _648);
         float _685 = YKR_MaterialCurve(_682, _649);
         float _686 = YKR_MaterialCurve(_683, _650);
         float _687 = sqrt(_684);
         float _688 = sqrt(_685);
         float _689 = sqrt(_686);
         _691 = _687;
         _692 = _688;
         _693 = _689;
      }
      else
      {
         _691 = _616;
         _692 = _617;
         _693 = _618;
      }
   }
   int _694 = c_bool & 4;
   bool _695 = (_694 == 0);
   if (_695)
   {
      float _697 = saturate(COLOR_1.x);
      float _702 = (c_fog_param[0].x) - _691;
      float _703 = (c_fog_param[0].y) - _692;
      float _704 = (c_fog_param[0].z) - _693;
      float _705 = _702 * _697;
      float _706 = _703 * _697;
      float _707 = _704 * _697;
      _709 = _705;
      _710 = _706;
      _711 = _707;
   }
   else
   {
      _709 = COLOR_1.x;
      _710 = COLOR_1.y;
      _711 = COLOR_1.z;
   }
   float _712 = _709 + _691;
   float _713 = _710 + _692;
   float _714 = _711 + _693;
   float _720 = (c_ps_col_scl[0].x) * _712;
   float _721 = (c_ps_col_scl[0].y) * _713;
   float _722 = (c_ps_col_scl[0].z) * _714;
   float _723 = (c_ps_col_scl[0].w) * _625;
   SV_Target.x = _720;
   SV_Target.y = _721;
   SV_Target.z = _722;
   SV_Target.w = _723;
   int _726 = c_bool & 8;
   bool _727 = (_726 != 0);
   float _728 = select(_727, _712, _720);
   float _729 = select(_727, _713, _721);
   float _730 = select(_727, _714, _722);
   float _731 = select(_727, _625, _723);
   SV_Target.x = _728;
   SV_Target.y = _729;
   SV_Target.z = _730;
   SV_Target.w = _731;
   return SV_Target;
}
