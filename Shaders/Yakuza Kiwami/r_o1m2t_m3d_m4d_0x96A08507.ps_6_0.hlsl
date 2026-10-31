// Yakuza Kiwami material r_o1m2t_m3d_m4d (0x96A08507, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s2_s : register(s2);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
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
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float4 _61 = s5_t.Sample(s5_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _64 = s2_t.Sample(s2_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _68 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _71 = _61.x * 2.0f;
   float _72 = _61.y * 2.0f;
   float _73 = _71 + -1.0f;
   float _74 = _72 + -1.0f;
   float _75 = dot(float2(_73, _74), float2(_73, _74));
   float _76 = 1.0f - _75;
   float _77 = saturate(_76);
   float _78 = sqrt(_77);
   float _79 = _58 * _64.x;
   float _80 = _59 * _64.y;
   float _81 = _60 * _64.z;
   float _82 = _64.x * _53.w;
   float _83 = _64.y * _53.w;
   float _84 = _64.z * _53.w;
   float _85 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _86 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _87 = _85 - _86;
   float _88 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _89 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _90 = _88 - _89;
   float _91 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _92 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _93 = _91 - _92;
   float _94 = _73 * TEXCOORD_6.x;
   float _95 = _73 * TEXCOORD_6.y;
   float _96 = _73 * TEXCOORD_6.z;
   float _97 = _74 * TEXCOORD_6.w;
   float _98 = _97 * _87;
   float _99 = _97 * _90;
   float _100 = _97 * _93;
   float _101 = _98 + _94;
   float _102 = _99 + _95;
   float _103 = _100 + _96;
   float _104 = _78 * TEXCOORD_2.x;
   float _105 = _78 * TEXCOORD_2.y;
   float _106 = _78 * TEXCOORD_2.z;
   float _107 = _101 + _104;
   float _108 = _102 + _105;
   float _109 = _103 + _106;
   float _110 = dot(float3(_107, _108, _109), float3(_107, _108, _109));
   float _111 = rsqrt(_110);
   float _112 = _107 * _111;
   float _113 = _108 * _111;
   float _114 = _109 * _111;
   float _115 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _116 = rsqrt(_115);
   float _117 = _116 * TEXCOORD_3.x;
   float _118 = _116 * TEXCOORD_3.y;
   float _119 = _116 * TEXCOORD_3.z;
   float _124 = -0.0f - (c_light_vec[0].x);
   float _125 = -0.0f - (c_light_vec[0].y);
   float _126 = -0.0f - (c_light_vec[0].z);
   float _127 = dot(float3(_112, _113, _114), float3(_124, _125, _126));
   float _131 = c_rt_size.z * SV_Position.x;
   float _132 = c_rt_size.w * SV_Position.y;
   int _135 = c_bool & 8;
   bool _136 = (_135 == 0);
   float _143;
   float _144;
   float _145;
   float _157;
   float _158;
   float _213;
   float _214;
   float _215;
   float _216;
   float _230;
   float _231;
   float _277;
   float _278;
   float _279;
   float _314;
   float _315;
   float _316;
   float _317;
   float _318;
   float _319;
   float _473;
   float _474;
   float _475;
   float _476;
   float _477;
   float _478;
   float _654;
   float _655;
   float _656;
   float _657;
   float _658;
   float _659;
   float _734;
   float _735;
   float _736;
   float _752;
   float _753;
   float _754;
   if (!_136)
   {
      float4 _138 = s13_t.Sample(s13_s, float2(_131, _132));
      _143 = _138.x;
      _144 = _138.y;
      _145 = _138.w;
   }
   else
   {
      _143 = 1.0f;
      _144 = 1.0f;
      _145 = 1.0f;
   }
   if (!_136)
   {
      float _150 = (c_sdw_param[0].x) * _143;
      float _151 = (c_sdw_param[0].z) * _144;
      float _154 = _150 + (c_sdw_param[0].y);
      float _155 = _151 + (c_sdw_param[0].w);
      _157 = _154;
      _158 = _155;
   }
   else
   {
      _157 = _143;
      _158 = _144;
   }
   float _165 = (c_sdw_param[1].x) * _68.y;
   float _167 = _165 + (c_sdw_param[1].y);
   float _168 = _167 * _68.x;
   float _169 = 1.0f - (c_light_col[5].x);
   float _170 = 1.0f - (c_light_col[5].y);
   float _171 = 1.0f - (c_light_col[5].z);
   float _172 = _168 * _169;
   float _173 = _168 * _170;
   float _174 = _168 * _171;
   float _175 = _172 + (c_light_col[5].x);
   float _176 = _173 + (c_light_col[5].y);
   float _177 = _174 + (c_light_col[5].z);
   float _181 = c_rt_size.z * SV_Position.x;
   float _182 = c_rt_size.w * SV_Position.y;
   float4 _183 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_181, _182)));
   int _190 = c_bool & 16384;
   bool _191 = (_190 == 0);
   if (!_191)
   {
      float _193 = _183.x * 2.0f;
      float _194 = _183.y * 2.0f;
      float _195 = _183.z * 2.0f;
      float _196 = _183.w * 2.0f;
      _213 = _193;
      _214 = _194;
      _215 = _195;
      _216 = _196;
   }
   else
   {
      int _198 = c_bool & 8192;
      bool _199 = (_198 == 0);
      if (!_199)
      {
         float _201 = _183.x * 2.0f;
         float _202 = _183.y * 2.0f;
         float _203 = _183.z * 2.0f;
         float _204 = _183.w * 2.0f;
         _213 = _201;
         _214 = _202;
         _215 = _203;
         _216 = _204;
      }
      else
      {
         float _208 = _183.x / c_tone_control.z;
         float _209 = _183.y / c_tone_control.z;
         float _210 = _183.z / c_tone_control.z;
         float _211 = _183.w / c_tone_control.z;
         _213 = _208;
         _214 = _209;
         _215 = _210;
         _216 = _211;
      }
   }
   int _217 = c_bool & 8;
   bool _218 = (_217 == 0);
   if (!_218)
   {
      float _223 = (c_sdw_param[0].x) * _157;
      float _224 = (c_sdw_param[0].z) * _158;
      float _227 = _223 + (c_sdw_param[0].y);
      float _228 = _224 + (c_sdw_param[0].w);
      _230 = _227;
      _231 = _228;
   }
   else
   {
      _230 = _157;
      _231 = _158;
   }
   float _232 = _231 * _230;
   float _237 = 1.0f - (c_light_col[5].x);
   float _238 = 1.0f - (c_light_col[5].y);
   float _239 = 1.0f - (c_light_col[5].z);
   float _240 = _237 * _232;
   float _241 = _238 * _232;
   float _242 = _239 * _232;
   float _243 = _240 + (c_light_col[5].x);
   float _244 = _241 + (c_light_col[5].y);
   float _245 = _242 + (c_light_col[5].z);
   float _246 = _145 * COLOR.x;
   float _247 = _246 * _175;
   float _248 = _145 * COLOR.y;
   float _249 = _248 * _176;
   float _250 = _145 * COLOR.z;
   float _251 = _250 * _177;
   float _252 = _243 * _175;
   float _253 = _244 * _176;
   float _254 = _245 * _177;
   if (!_218)
   {
      float _258 = (c_sdw_param[2].x) * _243;
      float _259 = (c_sdw_param[2].x) * _244;
      float _260 = (c_sdw_param[2].x) * _245;
      float _262 = _258 + (c_sdw_param[2].y);
      float _263 = _259 + (c_sdw_param[2].y);
      float _264 = _260 + (c_sdw_param[2].y);
      float _266 = (c_sdw_param[2].z) * _175;
      float _267 = (c_sdw_param[2].z) * _176;
      float _268 = (c_sdw_param[2].z) * _177;
      float _270 = _266 + (c_sdw_param[2].w);
      float _271 = _267 + (c_sdw_param[2].w);
      float _272 = _268 + (c_sdw_param[2].w);
      float _273 = _270 * _262;
      float _274 = _271 * _263;
      float _275 = _272 * _264;
      _277 = _273;
      _278 = _274;
      _279 = _275;
   }
   else
   {
      _277 = _252;
      _278 = _253;
      _279 = _254;
   }
   float _280 = _145 * _68.x;
   float _281 = _277 * _280;
   float _282 = _278 * _280;
   float _283 = _279 * _280;
   float _294 = c_mat_emissive.x * _79;
   float _295 = _294 * c_emissive_scale;
   float _296 = c_mat_emissive.y * _80;
   float _297 = _296 * c_emissive_scale;
   float _298 = c_mat_emissive.z * _81;
   float _299 = _298 * c_emissive_scale;
   int _300 = c_bool & 2048;
   bool _301 = (_300 == 0);
   if (_301)
   {
      float _303 = _231 * _213;
      float _304 = _231 * _214;
      float _305 = _231 * _215;
      float _306 = _303 + TEXCOORD_4.x;
      float _307 = _304 + TEXCOORD_4.y;
      float _308 = _305 + TEXCOORD_4.z;
      float _309 = _231 * _216;
      float _310 = _309 + TEXCOORD_5.x;
      float _311 = _309 + TEXCOORD_5.y;
      float _312 = _309 + TEXCOORD_5.z;
      _314 = _310;
      _315 = _311;
      _316 = _312;
      _317 = _306;
      _318 = _307;
      _319 = _308;
   }
   else
   {
      _314 = TEXCOORD_5.x;
      _315 = TEXCOORD_5.y;
      _316 = TEXCOORD_5.z;
      _317 = TEXCOORD_4.x;
      _318 = TEXCOORD_4.y;
      _319 = TEXCOORD_4.z;
   }
   float _324 = -0.0f - (c_light_vec[1].x);
   float _325 = -0.0f - (c_light_vec[1].y);
   float _326 = -0.0f - (c_light_vec[1].z);
   float _327 = dot(float3(_112, _113, _114), float3(_324, _325, _326));
   float _328 = saturate(_327);
   float _333 = saturate(_127);
   float _334 = (c_light_col[0].x) * _333;
   float _335 = (c_light_col[0].y) * _333;
   float _336 = (c_light_col[0].z) * _333;
   float _341 = (c_light_col[1].x) * _328;
   float _342 = (c_light_col[1].y) * _328;
   float _343 = (c_light_col[1].z) * _328;
   float _352 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _353 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _354 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _355 = _334 * _243;
   float _356 = _335 * _244;
   float _357 = _336 * _245;
   float _358 = _341 * _231;
   float _359 = _342 * _231;
   float _360 = _343 * _231;
   float _361 = _358 + _355;
   float _362 = _359 + _356;
   float _363 = _360 + _357;
   float _364 = _352 * _361;
   float _365 = _353 * _362;
   float _366 = _354 * _363;
   float _367 = _295 + c_scene_ambient.x;
   float _368 = _367 + _364;
   float _369 = _297 + c_scene_ambient.y;
   float _370 = _369 + _365;
   float _371 = _299 + c_scene_ambient.z;
   float _372 = _371 + _366;
   float _373 = _368 * _247;
   float _374 = _370 * _249;
   float _375 = _372 * _251;
   float _376 = _317 * _281;
   float _377 = _318 * _282;
   float _378 = _319 * _283;
   float _379 = _373 + _376;
   float _380 = _374 + _377;
   float _381 = _375 + _378;
   float _386 = (c_light_vec[0].x) + _117;
   float _387 = (c_light_vec[0].y) + _118;
   float _388 = (c_light_vec[0].z) + _119;
   float _389 = dot(float3(_386, _387, _388), float3(_386, _387, _388));
   float _390 = rsqrt(_389);
   float _391 = _386 * _390;
   float _392 = -0.0f - _391;
   float _393 = _387 * _390;
   float _394 = -0.0f - _393;
   float _395 = _388 * _390;
   float _396 = -0.0f - _395;
   float _397 = dot(float3(_392, _394, _396), float3(_112, _113, _114));
   float _398 = saturate(_397);
   float _399 = _118 + -0.30000001192092896f;
   float _400 = dot(float3(_117, _399, _119), float3(_117, _399, _119));
   float _401 = rsqrt(_400);
   float _402 = _117 * _401;
   float _403 = -0.0f - _402;
   float _404 = _399 * _401;
   float _405 = -0.0f - _404;
   float _406 = _119 * _401;
   float _407 = -0.0f - _406;
   float _408 = dot(float3(_403, _405, _407), float3(_112, _113, _114));
   float _409 = saturate(_408);
   float _412 = log2(_398);
   float _413 = log2(_409);
   float _414 = _412 * c_mat_power;
   float _415 = _413 * c_mat_power;
   float _416 = exp2(_414);
   float _417 = exp2(_415);
   float _420 = _416 * (c_light_col[0].x);
   float _421 = _416 * (c_light_col[0].y);
   float _422 = _416 * (c_light_col[0].z);
   float _423 = _417 * 2.0f;
   float _424 = _423 * (c_light_vec[5].x);
   float _425 = _424 * (c_light_col[1].x);
   float _426 = _424 * (c_light_col[1].y);
   float _427 = _424 * (c_light_col[1].z);
   float _428 = _420 * _243;
   float _429 = _421 * _244;
   float _430 = _422 * _245;
   float _431 = _425 * _328;
   float _432 = _426 * _328;
   float _433 = _427 * _328;
   float _434 = _431 + _428;
   float _435 = _432 + _429;
   float _436 = _433 + _430;
   float _443 = c_mat_specular.x * c_specular_scale.x;
   float _444 = _443 * _247;
   float _445 = _444 * _434;
   float _446 = c_mat_specular.y * c_specular_scale.y;
   float _447 = _446 * _249;
   float _448 = _447 * _435;
   float _449 = c_mat_specular.z * c_specular_scale.z;
   float _450 = _449 * _251;
   float _451 = _450 * _436;
   float _452 = _314 * _281;
   float _453 = _452 * _417;
   float _454 = _315 * _282;
   float _455 = _454 * _417;
   float _456 = _316 * _283;
   float _457 = _456 * _417;
   float _458 = _445 + _453;
   float _459 = _448 + _455;
   float _460 = _451 + _457;
   if (!_301)
   {
      float _462 = _231 * _213;
      float _463 = _231 * _214;
      float _464 = _231 * _215;
      float _465 = _379 + _462;
      float _466 = _380 + _463;
      float _467 = _381 + _464;
      float _468 = _231 * _216;
      float _469 = _458 + _468;
      float _470 = _459 + _468;
      float _471 = _460 + _468;
      _473 = _465;
      _474 = _466;
      _475 = _467;
      _476 = _469;
      _477 = _470;
      _478 = _471;
   }
   else
   {
      _473 = _379;
      _474 = _380;
      _475 = _381;
      _476 = _458;
      _477 = _459;
      _478 = _460;
   }
   int _479 = c_bool & 128;
   bool _480 = (_479 != 0);
   float _481 = select(_480, 0.0f, _476);
   float _482 = select(_480, 0.0f, _477);
   float _483 = select(_480, 0.0f, _478);
   float _484 = _473 * _79;
   float _485 = _474 * _80;
   float _486 = _475 * _81;
   float _487 = _481 * _82;
   float _488 = _482 * _83;
   float _489 = _483 * _84;
   int _490 = c_bool & 32768;
   bool _491 = (_490 == 0);
   if (!_491)
   {
      float _495 = (c_ps_user[11].x) * _79;
      float _496 = (c_ps_user[11].x) * _80;
      float _497 = (c_ps_user[11].x) * _81;
      float _499 = _112 + 1.0f;
      float _500 = _113 + 1.0f;
      float _501 = _114 + 1.0f;
      float _502 = _499 * 0.5f;
      float _503 = _500 * 0.5f;
      float _504 = _501 * 0.5f;
      float _505 = saturate(_502);
      float _506 = saturate(_503);
      float _507 = saturate(_504);
      float _508 = (c_ps_user[11].y) * _505;
      float _509 = _506 * (c_ps_user[11].y);
      float _510 = _507 * (c_ps_user[11].y);
      float _512 = (c_ps_user[11].z) * _82;
      float _513 = (c_ps_user[11].z) * _83;
      float _514 = (c_ps_user[11].z) * _84;
      float _516 = c_mat_power * 0.009999999776482582f;
      float _517 = _516 * (c_ps_user[11].w);
      float _520 = (c_ps_user[12].x) * _243;
      float _521 = (c_ps_user[12].x) * _244;
      float _522 = (c_ps_user[12].x) * _245;
      float _524 = (c_ps_user[12].y) * _247;
      float _525 = (c_ps_user[12].y) * _249;
      float _526 = (c_ps_user[12].y) * _251;
      float _528 = (c_ps_user[12].z) * _281;
      float _529 = (c_ps_user[12].z) * _282;
      float _530 = (c_ps_user[12].z) * _283;
      float _536 = -0.0f - _117;
      float _537 = -0.0f - _118;
      float _538 = -0.0f - _119;
      float _539 = dot(float3(_536, _537, _538), float3(_112, _113, _114));
      float _540 = 1.0f - _539;
      float _541 = _540 * _540;
      float _542 = _540 * 0.9722222089767456f;
      float _543 = _541 * _541;
      float _544 = _543 * _542;
      float _545 = _544 + 0.02777777425944805f;
      float _546 = _545 * (c_ps_user[13].z);
      float _548 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _549 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _550 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _553 = (c_ps_user[14].x) * _295;
      float _554 = (c_ps_user[14].x) * _297;
      float _555 = (c_ps_user[14].x) * _299;
      float _557 = (c_ps_user[14].y) * _334;
      float _558 = (c_ps_user[14].y) * _335;
      float _559 = (c_ps_user[14].y) * _336;
      float _561 = (c_ps_user[14].z) * _341;
      float _562 = (c_ps_user[14].z) * _342;
      float _563 = (c_ps_user[14].z) * _343;
      float _565 = (c_ps_user[14].w) * _317;
      float _566 = (c_ps_user[14].w) * _318;
      float _567 = (c_ps_user[14].w) * _319;
      float _570 = (c_ps_user[15].x) * _420;
      float _571 = (c_ps_user[15].x) * _421;
      float _572 = (c_ps_user[15].x) * _422;
      float _574 = _443 * _425;
      float _575 = _574 * (c_ps_user[15].y);
      float _576 = _446 * _426;
      float _577 = _576 * (c_ps_user[15].y);
      float _578 = _449 * _427;
      float _579 = _578 * (c_ps_user[15].y);
      float _581 = (c_ps_user[15].z) * _314;
      float _582 = (c_ps_user[15].z) * _315;
      float _583 = (c_ps_user[15].z) * _316;
      float _584 = _575 + _581;
      float _585 = _584 * _417;
      float _586 = _557 + (c_ps_user[13].x);
      float _587 = _586 + _561;
      float _588 = c_diffuse_scale.x * _587;
      float _589 = _588 * c_mat_diffuse.x;
      float _590 = (c_ps_user[13].y) + _512;
      float _591 = _590 + _570;
      float _592 = c_mat_specular.x * _591;
      float _593 = _592 * c_specular_scale.x;
      float _594 = _508 + _495;
      float _595 = _594 + _517;
      float _596 = _595 + (c_ps_user[12].w);
      float _597 = _596 + _520;
      float _598 = _597 + _524;
      float _599 = _598 + _528;
      float _600 = _599 + _548;
      float _601 = _600 + _553;
      float _602 = _601 + _565;
      float _603 = _602 + _546;
      float _604 = _603 + _585;
      float _605 = _604 + _589;
      float _606 = _605 + _593;
      float _607 = _577 + _582;
      float _608 = _607 * _417;
      float _609 = _558 + (c_ps_user[13].x);
      float _610 = _609 + _562;
      float _611 = c_diffuse_scale.y * _610;
      float _612 = _611 * c_mat_diffuse.y;
      float _613 = (c_ps_user[13].y) + _513;
      float _614 = _613 + _571;
      float _615 = c_mat_specular.y * _614;
      float _616 = _615 * c_specular_scale.y;
      float _617 = _509 + _496;
      float _618 = _617 + _517;
      float _619 = _618 + (c_ps_user[12].w);
      float _620 = _619 + _521;
      float _621 = _620 + _525;
      float _622 = _621 + _529;
      float _623 = _622 + _549;
      float _624 = _623 + _554;
      float _625 = _624 + _566;
      float _626 = _625 + _546;
      float _627 = _626 + _608;
      float _628 = _627 + _612;
      float _629 = _628 + _616;
      float _630 = _579 + _583;
      float _631 = _630 * _417;
      float _632 = _559 + (c_ps_user[13].x);
      float _633 = _632 + _563;
      float _634 = c_diffuse_scale.z * _633;
      float _635 = _634 * c_mat_diffuse.z;
      float _636 = (c_ps_user[13].y) + _514;
      float _637 = _636 + _572;
      float _638 = c_mat_specular.z * _637;
      float _639 = _638 * c_specular_scale.z;
      float _640 = _517 + _497;
      float _641 = _640 + _510;
      float _642 = _641 + (c_ps_user[12].w);
      float _643 = _642 + _522;
      float _644 = _643 + _526;
      float _645 = _644 + _530;
      float _646 = _645 + _550;
      float _647 = _646 + _555;
      float _648 = _647 + _567;
      float _649 = _648 + _546;
      float _650 = _649 + _631;
      float _651 = _650 + _635;
      float _652 = _651 + _639;
      _654 = _606;
      _655 = _629;
      _656 = _652;
      _657 = 0.0f;
      _658 = 0.0f;
      _659 = 0.0f;
   }
   else
   {
      _654 = _484;
      _655 = _485;
      _656 = _486;
      _657 = _487;
      _658 = _488;
      _659 = _489;
   }
   float _660 = _657 + _654;
   float _661 = _658 + _655;
   float _662 = _659 + _656;
   float _667 = c_global_opacity * c_mat_opacity;
   float _668 = _667 * c_opacity_scale;
   int _669 = c_bool & 16;
   bool _670 = (_669 == 0);
   if (!_670)
   {
      float _674 = c_tone_control.x * _660;
      float _675 = c_tone_control.x * _661;
      float _676 = c_tone_control.x * _662;
      float _678 = log2(_674);
      float _679 = log2(_675);
      float _680 = log2(_676);
      float _681 = _678 * c_tone_control.y;
      float _682 = _679 * c_tone_control.y;
      float _683 = _680 * c_tone_control.y;
      float _684 = exp2(_681);
      float _685 = exp2(_682);
      float _686 = exp2(_683);
      _734 = _684;
      _735 = _685;
      _736 = _686;
   }
   else
   {
      if (_491)
      {
         float _691 = c_tone_control.x * _660;
         float _692 = c_tone_control.x * _661;
         float _693 = c_tone_control.x * _662;
         float _694 = _691 * 2.0f;
         float _695 = _692 * 2.0f;
         float _696 = _693 * 2.0f;
         float _697 = _691 * 0.30000001192092896f;
         float _698 = _692 * 0.30000001192092896f;
         float _699 = _693 * 0.30000001192092896f;
         float _700 = _697 + 0.05000000074505806f;
         float _701 = _698 + 0.05000000074505806f;
         float _702 = _699 + 0.05000000074505806f;
         float _703 = _700 * _694;
         float _704 = _701 * _695;
         float _705 = _702 * _696;
         float _706 = _703 + 0.004000000189989805f;
         float _707 = _704 + 0.004000000189989805f;
         float _708 = _705 + 0.004000000189989805f;
         float _709 = _697 + 0.5f;
         float _710 = _698 + 0.5f;
         float _711 = _699 + 0.5f;
         float _712 = _709 * _694;
         float _713 = _710 * _695;
         float _714 = _711 * _696;
         float _715 = _712 + 0.06000000238418579f;
         float _716 = _713 + 0.06000000238418579f;
         float _717 = _714 + 0.06000000238418579f;
         float _718 = _706 / _715;
         float _719 = _707 / _716;
         float _720 = _708 / _717;
         float _721 = _718 + -0.06666666269302368f;
         float _722 = _719 + -0.06666666269302368f;
         float _723 = _720 + -0.06666666269302368f;
         float _724 = _721 * 1.3790643215179443f;
         float _725 = _722 * 1.3790643215179443f;
         float _726 = _723 * 1.3790643215179443f;
         float _727 = YKR_MaterialCurve(_724, _691);
         float _728 = YKR_MaterialCurve(_725, _692);
         float _729 = YKR_MaterialCurve(_726, _693);
         float _730 = sqrt(_727);
         float _731 = sqrt(_728);
         float _732 = sqrt(_729);
         _734 = _730;
         _735 = _731;
         _736 = _732;
      }
      else
      {
         _734 = _660;
         _735 = _661;
         _736 = _662;
      }
   }
   int _737 = c_bool & 4;
   bool _738 = (_737 == 0);
   if (_738)
   {
      float _740 = saturate(COLOR_1.x);
      float _745 = (c_fog_param[0].x) - _734;
      float _746 = (c_fog_param[0].y) - _735;
      float _747 = (c_fog_param[0].z) - _736;
      float _748 = _745 * _740;
      float _749 = _746 * _740;
      float _750 = _747 * _740;
      _752 = _748;
      _753 = _749;
      _754 = _750;
   }
   else
   {
      _752 = COLOR_1.x;
      _753 = COLOR_1.y;
      _754 = COLOR_1.z;
   }
   float _755 = _752 + _734;
   float _756 = _753 + _735;
   float _757 = _754 + _736;
   float _763 = (c_ps_col_scl[0].x) * _755;
   float _764 = (c_ps_col_scl[0].y) * _756;
   float _765 = (c_ps_col_scl[0].z) * _757;
   float _766 = (c_ps_col_scl[0].w) * _668;
   SV_Target.x = _763;
   SV_Target.y = _764;
   SV_Target.z = _765;
   SV_Target.w = _766;
   int _769 = c_bool & 8;
   bool _770 = (_769 != 0);
   float _771 = select(_770, _755, _763);
   float _772 = select(_770, _756, _764);
   float _773 = select(_770, _757, _765);
   float _774 = select(_770, _668, _766);
   SV_Target.x = _771;
   SV_Target.y = _772;
   SV_Target.z = _773;
   SV_Target.w = _774;
   return SV_Target;
}
