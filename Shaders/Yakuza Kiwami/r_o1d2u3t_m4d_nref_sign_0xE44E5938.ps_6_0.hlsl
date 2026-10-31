// Yakuza Kiwami material r_o1d2u3t_m4d[nref][sign] (0xE44E5938, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

Texture2D<float4> s7_t : register(t7);

Texture3D<float4> s12v_t : register(t12);

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

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s12v_s : register(s12);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _61 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _66 = _61.x * _61.x;
   float _67 = _61.y * _61.y;
   float _68 = _61.z * _61.z;
   float4 _69 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _72 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _75 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _78 = _72.x * 2.0f;
   float _79 = _72.y * 2.0f;
   float _80 = _78 + -1.0f;
   float _81 = _79 + -1.0f;
   float _82 = dot(float2(_80, _81), float2(_80, _81));
   float _83 = 1.0f - _82;
   float _84 = saturate(_83);
   float _85 = sqrt(_84);
   float _86 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _87 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _90 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _91 = _89 - _90;
   float _92 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _93 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _94 = _92 - _93;
   float _95 = _80 * TEXCOORD_6.x;
   float _96 = _80 * TEXCOORD_6.y;
   float _97 = _80 * TEXCOORD_6.z;
   float _98 = _81 * TEXCOORD_6.w;
   float _99 = _98 * _88;
   float _100 = _98 * _91;
   float _101 = _98 * _94;
   float _102 = _99 + _95;
   float _103 = _100 + _96;
   float _104 = _101 + _97;
   float _105 = _85 * TEXCOORD_2.x;
   float _106 = _85 * TEXCOORD_2.y;
   float _107 = _85 * TEXCOORD_2.z;
   float _108 = _102 + _105;
   float _109 = _103 + _106;
   float _110 = _104 + _107;
   float _111 = dot(float3(_108, _109, _110), float3(_108, _109, _110));
   float _112 = rsqrt(_111);
   float _113 = _108 * _112;
   float _114 = _109 * _112;
   float _115 = _110 * _112;
   float _116 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _117 = rsqrt(_116);
   float _118 = _117 * TEXCOORD_3.x;
   float _119 = _117 * TEXCOORD_3.y;
   float _120 = _117 * TEXCOORD_3.z;
   float _125 = -0.0f - (c_light_vec[0].x);
   float _126 = -0.0f - (c_light_vec[0].y);
   float _127 = -0.0f - (c_light_vec[0].z);
   float _128 = dot(float3(_113, _114, _115), float3(_125, _126, _127));
   float _132 = c_rt_size.z * SV_Position.x;
   float _133 = c_rt_size.w * SV_Position.y;
   int _136 = c_bool & 8;
   bool _137 = (_136 == 0);
   float _144;
   float _145;
   float _146;
   float _158;
   float _159;
   float _214;
   float _215;
   float _216;
   float _217;
   float _231;
   float _232;
   float _278;
   float _279;
   float _280;
   float _322;
   float _323;
   float _324;
   float _325;
   float _326;
   float _327;
   float _481;
   float _482;
   float _483;
   float _484;
   float _485;
   float _486;
   float _658;
   float _659;
   float _660;
   float _661;
   float _662;
   float _663;
   float _819;
   float _820;
   float _821;
   float _837;
   float _838;
   float _839;
   if (!_137)
   {
      float4 _139 = s13_t.Sample(s13_s, float2(_132, _133));
      _144 = _139.x;
      _145 = _139.y;
      _146 = _139.w;
   }
   else
   {
      _144 = 1.0f;
      _145 = 1.0f;
      _146 = 1.0f;
   }
   if (!_137)
   {
      float _151 = (c_sdw_param[0].x) * _144;
      float _152 = (c_sdw_param[0].z) * _145;
      float _155 = _151 + (c_sdw_param[0].y);
      float _156 = _152 + (c_sdw_param[0].w);
      _158 = _155;
      _159 = _156;
   }
   else
   {
      _158 = _144;
      _159 = _145;
   }
   float _166 = (c_sdw_param[1].x) * _75.y;
   float _168 = _166 + (c_sdw_param[1].y);
   float _169 = _168 * _75.x;
   float _170 = 1.0f - (c_light_col[5].x);
   float _171 = 1.0f - (c_light_col[5].y);
   float _172 = 1.0f - (c_light_col[5].z);
   float _173 = _169 * _170;
   float _174 = _169 * _171;
   float _175 = _169 * _172;
   float _176 = _173 + (c_light_col[5].x);
   float _177 = _174 + (c_light_col[5].y);
   float _178 = _175 + (c_light_col[5].z);
   float _182 = c_rt_size.z * SV_Position.x;
   float _183 = c_rt_size.w * SV_Position.y;
   float4 _184 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_182, _183)));
   int _191 = c_bool & 16384;
   bool _192 = (_191 == 0);
   if (!_192)
   {
      float _194 = _184.x * 2.0f;
      float _195 = _184.y * 2.0f;
      float _196 = _184.z * 2.0f;
      float _197 = _184.w * 2.0f;
      _214 = _194;
      _215 = _195;
      _216 = _196;
      _217 = _197;
   }
   else
   {
      int _199 = c_bool & 8192;
      bool _200 = (_199 == 0);
      if (!_200)
      {
         float _202 = _184.x * 2.0f;
         float _203 = _184.y * 2.0f;
         float _204 = _184.z * 2.0f;
         float _205 = _184.w * 2.0f;
         _214 = _202;
         _215 = _203;
         _216 = _204;
         _217 = _205;
      }
      else
      {
         float _209 = _184.x / c_tone_control.z;
         float _210 = _184.y / c_tone_control.z;
         float _211 = _184.z / c_tone_control.z;
         float _212 = _184.w / c_tone_control.z;
         _214 = _209;
         _215 = _210;
         _216 = _211;
         _217 = _212;
      }
   }
   int _218 = c_bool & 8;
   bool _219 = (_218 == 0);
   if (!_219)
   {
      float _224 = (c_sdw_param[0].x) * _158;
      float _225 = (c_sdw_param[0].z) * _159;
      float _228 = _224 + (c_sdw_param[0].y);
      float _229 = _225 + (c_sdw_param[0].w);
      _231 = _228;
      _232 = _229;
   }
   else
   {
      _231 = _158;
      _232 = _159;
   }
   float _233 = _232 * _231;
   float _238 = 1.0f - (c_light_col[5].x);
   float _239 = 1.0f - (c_light_col[5].y);
   float _240 = 1.0f - (c_light_col[5].z);
   float _241 = _238 * _233;
   float _242 = _239 * _233;
   float _243 = _240 * _233;
   float _244 = _241 + (c_light_col[5].x);
   float _245 = _242 + (c_light_col[5].y);
   float _246 = _243 + (c_light_col[5].z);
   float _247 = _146 * COLOR.x;
   float _248 = _247 * _176;
   float _249 = _146 * COLOR.y;
   float _250 = _249 * _177;
   float _251 = _146 * COLOR.z;
   float _252 = _251 * _178;
   float _253 = _244 * _176;
   float _254 = _245 * _177;
   float _255 = _246 * _178;
   if (!_219)
   {
      float _259 = (c_sdw_param[2].x) * _244;
      float _260 = (c_sdw_param[2].x) * _245;
      float _261 = (c_sdw_param[2].x) * _246;
      float _263 = _259 + (c_sdw_param[2].y);
      float _264 = _260 + (c_sdw_param[2].y);
      float _265 = _261 + (c_sdw_param[2].y);
      float _267 = (c_sdw_param[2].z) * _176;
      float _268 = (c_sdw_param[2].z) * _177;
      float _269 = (c_sdw_param[2].z) * _178;
      float _271 = _267 + (c_sdw_param[2].w);
      float _272 = _268 + (c_sdw_param[2].w);
      float _273 = _269 + (c_sdw_param[2].w);
      float _274 = _271 * _263;
      float _275 = _272 * _264;
      float _276 = _273 * _265;
      _278 = _274;
      _279 = _275;
      _280 = _276;
   }
   else
   {
      _278 = _253;
      _279 = _254;
      _280 = _255;
   }
   float _281 = _146 * _75.x;
   float _282 = _278 * _281;
   float _283 = _279 * _281;
   float _284 = _280 * _281;
   float _297 = (c_ps_user[0].x) * c_emissive_scale;
   float _298 = _297 * c_mat_emissive.x;
   float _299 = _297 * c_mat_emissive.y;
   float _300 = _297 * c_mat_emissive.z;
   float _302 = _298 + (c_ps_user[0].y);
   float _303 = _299 + (c_ps_user[0].y);
   float _304 = _300 + (c_ps_user[0].y);
   float _305 = _302 * _66;
   float _306 = _303 * _67;
   float _307 = _304 * _68;
   int _308 = c_bool & 2048;
   bool _309 = (_308 == 0);
   if (_309)
   {
      float _311 = _232 * _214;
      float _312 = _232 * _215;
      float _313 = _232 * _216;
      float _314 = _311 + TEXCOORD_4.x;
      float _315 = _312 + TEXCOORD_4.y;
      float _316 = _313 + TEXCOORD_4.z;
      float _317 = _232 * _217;
      float _318 = _317 + TEXCOORD_5.x;
      float _319 = _317 + TEXCOORD_5.y;
      float _320 = _317 + TEXCOORD_5.z;
      _322 = _318;
      _323 = _319;
      _324 = _320;
      _325 = _314;
      _326 = _315;
      _327 = _316;
   }
   else
   {
      _322 = TEXCOORD_5.x;
      _323 = TEXCOORD_5.y;
      _324 = TEXCOORD_5.z;
      _325 = TEXCOORD_4.x;
      _326 = TEXCOORD_4.y;
      _327 = TEXCOORD_4.z;
   }
   float _332 = -0.0f - (c_light_vec[1].x);
   float _333 = -0.0f - (c_light_vec[1].y);
   float _334 = -0.0f - (c_light_vec[1].z);
   float _335 = dot(float3(_113, _114, _115), float3(_332, _333, _334));
   float _336 = saturate(_335);
   float _341 = saturate(_128);
   float _342 = (c_light_col[0].x) * _341;
   float _343 = (c_light_col[0].y) * _341;
   float _344 = (c_light_col[0].z) * _341;
   float _349 = (c_light_col[1].x) * _336;
   float _350 = (c_light_col[1].y) * _336;
   float _351 = (c_light_col[1].z) * _336;
   float _360 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _361 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _362 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _363 = _342 * _244;
   float _364 = _343 * _245;
   float _365 = _344 * _246;
   float _366 = _349 * _232;
   float _367 = _350 * _232;
   float _368 = _351 * _232;
   float _369 = _366 + _363;
   float _370 = _367 + _364;
   float _371 = _368 + _365;
   float _372 = _360 * _369;
   float _373 = _361 * _370;
   float _374 = _362 * _371;
   float _375 = _305 + c_scene_ambient.x;
   float _376 = _375 + _372;
   float _377 = _306 + c_scene_ambient.y;
   float _378 = _377 + _373;
   float _379 = _307 + c_scene_ambient.z;
   float _380 = _379 + _374;
   float _381 = _376 * _248;
   float _382 = _378 * _250;
   float _383 = _380 * _252;
   float _384 = _325 * _282;
   float _385 = _326 * _283;
   float _386 = _327 * _284;
   float _387 = _381 + _384;
   float _388 = _382 + _385;
   float _389 = _383 + _386;
   float _394 = (c_light_vec[0].x) + _118;
   float _395 = (c_light_vec[0].y) + _119;
   float _396 = (c_light_vec[0].z) + _120;
   float _397 = dot(float3(_394, _395, _396), float3(_394, _395, _396));
   float _398 = rsqrt(_397);
   float _399 = _394 * _398;
   float _400 = -0.0f - _399;
   float _401 = _395 * _398;
   float _402 = -0.0f - _401;
   float _403 = _396 * _398;
   float _404 = -0.0f - _403;
   float _405 = dot(float3(_400, _402, _404), float3(_113, _114, _115));
   float _406 = saturate(_405);
   float _407 = _119 + -0.30000001192092896f;
   float _408 = dot(float3(_118, _407, _120), float3(_118, _407, _120));
   float _409 = rsqrt(_408);
   float _410 = _118 * _409;
   float _411 = -0.0f - _410;
   float _412 = _407 * _409;
   float _413 = -0.0f - _412;
   float _414 = _120 * _409;
   float _415 = -0.0f - _414;
   float _416 = dot(float3(_411, _413, _415), float3(_113, _114, _115));
   float _417 = saturate(_416);
   float _420 = log2(_406);
   float _421 = log2(_417);
   float _422 = _420 * c_mat_power;
   float _423 = _421 * c_mat_power;
   float _424 = exp2(_422);
   float _425 = exp2(_423);
   float _428 = _424 * (c_light_col[0].x);
   float _429 = _424 * (c_light_col[0].y);
   float _430 = _424 * (c_light_col[0].z);
   float _431 = _425 * 2.0f;
   float _432 = _431 * (c_light_vec[5].x);
   float _433 = _432 * (c_light_col[1].x);
   float _434 = _432 * (c_light_col[1].y);
   float _435 = _432 * (c_light_col[1].z);
   float _436 = _428 * _244;
   float _437 = _429 * _245;
   float _438 = _430 * _246;
   float _439 = _433 * _336;
   float _440 = _434 * _336;
   float _441 = _435 * _336;
   float _442 = _439 + _436;
   float _443 = _440 + _437;
   float _444 = _441 + _438;
   float _451 = c_mat_specular.x * c_specular_scale.x;
   float _452 = _451 * _248;
   float _453 = _452 * _442;
   float _454 = c_mat_specular.y * c_specular_scale.y;
   float _455 = _454 * _250;
   float _456 = _455 * _443;
   float _457 = c_mat_specular.z * c_specular_scale.z;
   float _458 = _457 * _252;
   float _459 = _458 * _444;
   float _460 = _322 * _282;
   float _461 = _460 * _425;
   float _462 = _323 * _283;
   float _463 = _462 * _425;
   float _464 = _324 * _284;
   float _465 = _464 * _425;
   float _466 = _453 + _461;
   float _467 = _456 + _463;
   float _468 = _459 + _465;
   if (!_309)
   {
      float _470 = _232 * _214;
      float _471 = _232 * _215;
      float _472 = _232 * _216;
      float _473 = _387 + _470;
      float _474 = _388 + _471;
      float _475 = _389 + _472;
      float _476 = _232 * _217;
      float _477 = _466 + _476;
      float _478 = _467 + _476;
      float _479 = _468 + _476;
      _481 = _473;
      _482 = _474;
      _483 = _475;
      _484 = _477;
      _485 = _478;
      _486 = _479;
   }
   else
   {
      _481 = _387;
      _482 = _388;
      _483 = _389;
      _484 = _466;
      _485 = _467;
      _486 = _468;
   }
   int _487 = c_bool & 128;
   bool _488 = (_487 != 0);
   float _489 = select(_488, 0.0f, _484);
   float _490 = select(_488, 0.0f, _485);
   float _491 = select(_488, 0.0f, _486);
   float _492 = _481 * _66;
   float _493 = _482 * _67;
   float _494 = _483 * _68;
   float _495 = _489 * _69.y;
   float _496 = _490 * _69.y;
   float _497 = _491 * _69.y;
   int _498 = c_bool & 32768;
   bool _499 = (_498 == 0);
   if (!_499)
   {
      float _503 = (c_ps_user[11].x) * _66;
      float _504 = (c_ps_user[11].x) * _67;
      float _505 = (c_ps_user[11].x) * _68;
      float _507 = _113 + 1.0f;
      float _508 = _114 + 1.0f;
      float _509 = _115 + 1.0f;
      float _510 = _507 * 0.5f;
      float _511 = _508 * 0.5f;
      float _512 = _509 * 0.5f;
      float _513 = saturate(_510);
      float _514 = saturate(_511);
      float _515 = saturate(_512);
      float _516 = (c_ps_user[11].y) * _513;
      float _517 = _514 * (c_ps_user[11].y);
      float _518 = _515 * (c_ps_user[11].y);
      float _520 = (c_ps_user[11].z) * _69.y;
      float _522 = c_mat_power * 0.009999999776482582f;
      float _523 = _522 * (c_ps_user[11].w);
      float _526 = (c_ps_user[12].x) * _244;
      float _527 = (c_ps_user[12].x) * _245;
      float _528 = (c_ps_user[12].x) * _246;
      float _530 = (c_ps_user[12].y) * _248;
      float _531 = (c_ps_user[12].y) * _250;
      float _532 = (c_ps_user[12].y) * _252;
      float _534 = (c_ps_user[12].z) * _282;
      float _535 = (c_ps_user[12].z) * _283;
      float _536 = (c_ps_user[12].z) * _284;
      float _542 = -0.0f - _118;
      float _543 = -0.0f - _119;
      float _544 = -0.0f - _120;
      float _545 = dot(float3(_542, _543, _544), float3(_113, _114, _115));
      float _546 = 1.0f - _545;
      float _547 = _546 * _546;
      float _548 = _546 * 0.9722222089767456f;
      float _549 = _547 * _547;
      float _550 = _549 * _548;
      float _551 = _550 + 0.02777777425944805f;
      float _552 = _551 * (c_ps_user[13].z);
      float _554 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _555 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _556 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _559 = (c_ps_user[14].x) * _305;
      float _560 = (c_ps_user[14].x) * _306;
      float _561 = (c_ps_user[14].x) * _307;
      float _563 = (c_ps_user[14].y) * _342;
      float _564 = (c_ps_user[14].y) * _343;
      float _565 = (c_ps_user[14].y) * _344;
      float _567 = (c_ps_user[14].z) * _349;
      float _568 = (c_ps_user[14].z) * _350;
      float _569 = (c_ps_user[14].z) * _351;
      float _571 = (c_ps_user[14].w) * _325;
      float _572 = (c_ps_user[14].w) * _326;
      float _573 = (c_ps_user[14].w) * _327;
      float _576 = (c_ps_user[15].x) * _428;
      float _577 = (c_ps_user[15].x) * _429;
      float _578 = (c_ps_user[15].x) * _430;
      float _580 = _451 * _433;
      float _581 = _580 * (c_ps_user[15].y);
      float _582 = _454 * _434;
      float _583 = _582 * (c_ps_user[15].y);
      float _584 = _457 * _435;
      float _585 = _584 * (c_ps_user[15].y);
      float _587 = (c_ps_user[15].z) * _322;
      float _588 = (c_ps_user[15].z) * _323;
      float _589 = (c_ps_user[15].z) * _324;
      float _590 = _581 + _587;
      float _591 = _590 * _425;
      float _592 = _563 + (c_ps_user[13].x);
      float _593 = _592 + _567;
      float _594 = c_diffuse_scale.x * _593;
      float _595 = _594 * c_mat_diffuse.x;
      float _596 = (c_ps_user[13].y) + _520;
      float _597 = _596 + _576;
      float _598 = c_mat_specular.x * _597;
      float _599 = _598 * c_specular_scale.x;
      float _600 = _516 + _503;
      float _601 = _600 + _523;
      float _602 = _601 + (c_ps_user[12].w);
      float _603 = _602 + _526;
      float _604 = _603 + _530;
      float _605 = _604 + _534;
      float _606 = _605 + _554;
      float _607 = _606 + _559;
      float _608 = _607 + _571;
      float _609 = _608 + _552;
      float _610 = _609 + _591;
      float _611 = _610 + _595;
      float _612 = _611 + _599;
      float _613 = _583 + _588;
      float _614 = _613 * _425;
      float _615 = _564 + (c_ps_user[13].x);
      float _616 = _615 + _568;
      float _617 = c_diffuse_scale.y * _616;
      float _618 = _617 * c_mat_diffuse.y;
      float _619 = _596 + _577;
      float _620 = c_mat_specular.y * _619;
      float _621 = _620 * c_specular_scale.y;
      float _622 = _517 + _504;
      float _623 = _622 + _523;
      float _624 = _623 + (c_ps_user[12].w);
      float _625 = _624 + _527;
      float _626 = _625 + _531;
      float _627 = _626 + _535;
      float _628 = _627 + _555;
      float _629 = _628 + _560;
      float _630 = _629 + _572;
      float _631 = _630 + _552;
      float _632 = _631 + _614;
      float _633 = _632 + _618;
      float _634 = _633 + _621;
      float _635 = _585 + _589;
      float _636 = _635 * _425;
      float _637 = _565 + (c_ps_user[13].x);
      float _638 = _637 + _569;
      float _639 = c_diffuse_scale.z * _638;
      float _640 = _639 * c_mat_diffuse.z;
      float _641 = _596 + _578;
      float _642 = c_mat_specular.z * _641;
      float _643 = _642 * c_specular_scale.z;
      float _644 = _523 + _505;
      float _645 = _644 + _518;
      float _646 = _645 + (c_ps_user[12].w);
      float _647 = _646 + _528;
      float _648 = _647 + _532;
      float _649 = _648 + _536;
      float _650 = _649 + _556;
      float _651 = _650 + _561;
      float _652 = _651 + _573;
      float _653 = _652 + _552;
      float _654 = _653 + _636;
      float _655 = _654 + _640;
      float _656 = _655 + _643;
      _658 = 0.0f;
      _659 = 0.0f;
      _660 = 0.0f;
      _661 = _612;
      _662 = _634;
      _663 = _656;
   }
   else
   {
      _658 = _495;
      _659 = _496;
      _660 = _497;
      _661 = _492;
      _662 = _493;
      _663 = _494;
   }
   float _664 = _661 + _658;
   float _665 = _662 + _659;
   float _666 = _663 + _660;
   float _670 = (c_attribute_ps[2].x) * TEXCOORD_3.w;
   float _671 = (c_attribute_ps[2].x) * TEXCOORD_4.w;
   float _672 = (c_attribute_ps[2].x) * TEXCOORD_5.w;
   float4 _673 = s12v_t.Sample(s12v_s, float3(_670, _671, _672));
   float _678 = _673.x * 2.0f;
   float _679 = _673.y * 2.0f;
   float _680 = _673.z * 2.0f;
   float _681 = _673.w * 2.0f;
   float _682 = _678 + -1.0f;
   float _683 = _679 + -1.0f;
   float _684 = _680 + -1.0f;
   float _685 = _681 + -1.0f;
   float _686 = _682 - _685;
   float _687 = _683 - _685;
   float _688 = _684 - _685;
   float _689 = _686 * (c_attribute_ps[2].y);
   float _690 = _687 * (c_attribute_ps[2].y);
   float _691 = _688 * (c_attribute_ps[2].y);
   float _692 = _689 + _113;
   float _693 = _690 + _114;
   float _694 = _691 + _115;
   float _695 = dot(float3(_692, _693, _694), float3(_692, _693, _694));
   float _696 = rsqrt(_695);
   float _697 = _692 * _696;
   float _698 = _693 * _696;
   float _699 = _694 * _696;
   float _700 = dot(float3(_118, _119, _120), float3(_697, _698, _699));
   float _701 = _700 * 2.0f;
   float _702 = _697 * _701;
   float _703 = _698 * _701;
   float _704 = _699 * _701;
   float _705 = _118 - _702;
   float _706 = _119 - _703;
   float _707 = _120 - _704;
   float _708 = -0.0f - _705;
   float4 _709 = s6c_t.Sample(s6c_s, float3(_708, _706, _707));
   float _716 = _159 * _158;
   float _717 = _128 + 0.75f;
   float _718 = saturate(_717);
   float _719 = _716 * _718;
   float _720 = max(_719, (c_light_vec[5].y));
   float _721 = max(_709.w, 0.003921568859368563f);
   float _722 = 0.25f / _721;
   float _723 = _722 * _709.x;
   float _724 = _722 * _709.y;
   float _725 = _722 * _709.z;
   float _726 = _69.x * TEXCOORD_2.w;
   float _727 = _726 * _75.x;
   float _728 = _720 * _727;
   float _729 = _728 * _176;
   float _730 = _723 * _723;
   float _731 = _730 * _729;
   float _732 = _728 * _177;
   float _733 = _724 * _724;
   float _734 = _733 * _732;
   float _735 = _728 * _178;
   float _736 = _725 * _725;
   float _737 = _736 * _735;
   float _738 = _664 + _731;
   float _739 = _665 + _734;
   float _740 = _666 + _737;
   float _747 = c_mat_opacity * _61.w;
   float _748 = _747 * c_opacity_scale;
   float _749 = _748 * c_global_opacity;
   int _752 = c_bool & 16;
   bool _753 = (_752 == 0);
   if (!_753)
   {
      float _757 = c_tone_control.x * _738;
      float _758 = c_tone_control.x * _739;
      float _759 = c_tone_control.x * _740;
      float _761 = log2(_757);
      float _762 = log2(_758);
      float _763 = log2(_759);
      float _764 = _761 * c_tone_control.y;
      float _765 = _762 * c_tone_control.y;
      float _766 = _763 * c_tone_control.y;
      float _767 = exp2(_764);
      float _768 = exp2(_765);
      float _769 = exp2(_766);
      _819 = _767;
      _820 = _768;
      _821 = _769;
   }
   else
   {
      int _771 = c_bool & 32768;
      bool _772 = (_771 == 0);
      if (_772)
      {
         float _776 = c_tone_control.x * _738;
         float _777 = c_tone_control.x * _739;
         float _778 = c_tone_control.x * _740;
         float _779 = _776 * 2.0f;
         float _780 = _777 * 2.0f;
         float _781 = _778 * 2.0f;
         float _782 = _776 * 0.30000001192092896f;
         float _783 = _777 * 0.30000001192092896f;
         float _784 = _778 * 0.30000001192092896f;
         float _785 = _782 + 0.05000000074505806f;
         float _786 = _783 + 0.05000000074505806f;
         float _787 = _784 + 0.05000000074505806f;
         float _788 = _785 * _779;
         float _789 = _786 * _780;
         float _790 = _787 * _781;
         float _791 = _788 + 0.004000000189989805f;
         float _792 = _789 + 0.004000000189989805f;
         float _793 = _790 + 0.004000000189989805f;
         float _794 = _782 + 0.5f;
         float _795 = _783 + 0.5f;
         float _796 = _784 + 0.5f;
         float _797 = _794 * _779;
         float _798 = _795 * _780;
         float _799 = _796 * _781;
         float _800 = _797 + 0.06000000238418579f;
         float _801 = _798 + 0.06000000238418579f;
         float _802 = _799 + 0.06000000238418579f;
         float _803 = _791 / _800;
         float _804 = _792 / _801;
         float _805 = _793 / _802;
         float _806 = _803 + -0.06666666269302368f;
         float _807 = _804 + -0.06666666269302368f;
         float _808 = _805 + -0.06666666269302368f;
         float _809 = _806 * 1.3790643215179443f;
         float _810 = _807 * 1.3790643215179443f;
         float _811 = _808 * 1.3790643215179443f;
         float _812 = YKR_MaterialCurve(_809, _776);
         float _813 = YKR_MaterialCurve(_810, _777);
         float _814 = YKR_MaterialCurve(_811, _778);
         float _815 = sqrt(_812);
         float _816 = sqrt(_813);
         float _817 = sqrt(_814);
         _819 = _815;
         _820 = _816;
         _821 = _817;
      }
      else
      {
         _819 = _738;
         _820 = _739;
         _821 = _740;
      }
   }
   int _822 = c_bool & 4;
   bool _823 = (_822 == 0);
   if (_823)
   {
      float _825 = saturate(COLOR_1.x);
      float _830 = (c_fog_param[0].x) - _819;
      float _831 = (c_fog_param[0].y) - _820;
      float _832 = (c_fog_param[0].z) - _821;
      float _833 = _830 * _825;
      float _834 = _831 * _825;
      float _835 = _832 * _825;
      _837 = _833;
      _838 = _834;
      _839 = _835;
   }
   else
   {
      _837 = COLOR_1.x;
      _838 = COLOR_1.y;
      _839 = COLOR_1.z;
   }
   float _840 = _837 + _819;
   float _841 = _838 + _820;
   float _842 = _839 + _821;
   float _848 = (c_ps_col_scl[0].x) * _840;
   float _849 = (c_ps_col_scl[0].y) * _841;
   float _850 = (c_ps_col_scl[0].z) * _842;
   float _851 = (c_ps_col_scl[0].w) * _749;
   SV_Target.x = _848;
   SV_Target.y = _849;
   SV_Target.z = _850;
   SV_Target.w = _851;
   int _854 = c_bool & 8;
   bool _855 = (_854 != 0);
   float _856 = select(_855, _840, _848);
   float _857 = select(_855, _841, _849);
   float _858 = select(_855, _842, _850);
   float _859 = select(_855, _749, _851);
   SV_Target.x = _856;
   SV_Target.y = _857;
   SV_Target.z = _858;
   SV_Target.w = _859;
   return SV_Target;
}
