// Yakuza Kiwami material s_o1d[eye] (0xE0AB5C4C, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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

cbuffer cb2 : register(b2)
{
   float4 c_ps_scratch[92] : packoffset(c000.x);
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
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _45 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _50 = _45.x * _45.x;
   float _51 = _45.y * _45.y;
   float _52 = _45.z * _45.z;
   float _55 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _56 = rsqrt(_55);
   float _57 = _56 * TEXCOORD_2.x;
   float _58 = _56 * TEXCOORD_2.y;
   float _59 = _56 * TEXCOORD_2.z;
   float _64 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _65 = rsqrt(_64);
   float _66 = _65 * TEXCOORD_1.x;
   float _67 = _65 * TEXCOORD_1.y;
   float _68 = _65 * TEXCOORD_1.z;
   float _73 = (c_ps_user[4].x) + _66;
   float _74 = (c_ps_user[4].y) + _67;
   float _75 = (c_ps_user[4].z) + _68;
   float _76 = dot(float3(_73, _74, _75), float3(_73, _74, _75));
   float _77 = rsqrt(_76);
   float _82 = (c_ps_user[5].x) + _66;
   float _83 = (c_ps_user[5].y) + _67;
   float _84 = (c_ps_user[5].z) + _68;
   float _85 = dot(float3(_82, _83, _84), float3(_82, _83, _84));
   float _86 = rsqrt(_85);
   float _90 = c_rt_size.z * SV_Position.x;
   float _91 = c_rt_size.w * SV_Position.y;
   int _94 = c_bool & 8;
   bool _95 = (_94 == 0);
   float _101;
   float _102;
   float _114;
   float _115;
   float _204;
   float _205;
   float _206;
   float _235;
   float _236;
   float _237;
   float _556;
   float _557;
   float _558;
   float _559;
   float _639;
   float _640;
   float _641;
   float _657;
   float _658;
   float _659;
   float _684;
   float _685;
   float _686;
   if (!_95)
   {
      float4 _97 = s13_t.Sample(s13_s, float2(_90, _91));
      _101 = _97.x;
      _102 = _97.y;
   }
   else
   {
      _101 = 1.0f;
      _102 = 1.0f;
   }
   if (!_95)
   {
      float _107 = (c_sdw_param_ch[0].x) * _101;
      float _108 = (c_sdw_param_ch[0].z) * _102;
      float _111 = _107 + (c_sdw_param_ch[0].y);
      float _112 = _108 + (c_sdw_param_ch[0].w);
      _114 = _111;
      _115 = _112;
   }
   else
   {
      _114 = _101;
      _115 = _102;
   }
   float _116 = min(_114, _115);
   float _117 = _82 * _86;
   float _118 = -0.0f - _117;
   float _119 = _83 * _86;
   float _120 = -0.0f - _119;
   float _121 = _84 * _86;
   float _122 = -0.0f - _121;
   float _123 = _73 * _77;
   float _124 = -0.0f - _123;
   float _125 = _74 * _77;
   float _126 = -0.0f - _125;
   float _127 = _75 * _77;
   float _128 = -0.0f - _127;
   float _135 = (c_ps_user[6].x) * c_mat_power;
   float _136 = dot(float3(_124, _126, _128), float3(_57, _58, _59));
   float _137 = saturate(_136);
   float _138 = log2(_137);
   float _139 = _135 * _138;
   float _140 = exp2(_139);
   float _151 = (c_ps_user[4].w) * _140;
   float _153 = (c_ps_user[6].y) * c_mat_power;
   float _154 = dot(float3(_118, _120, _122), float3(_57, _58, _59));
   float _155 = saturate(_154);
   float _156 = log2(_155);
   float _157 = _153 * _156;
   float _158 = exp2(_157);
   float _161 = (c_ps_user[5].w) * _158;
   float _162 = _161 + _151;
   float _163 = c_specular_scale.x * _162;
   float _164 = (c_light_col[0].x) * c_mat_specular.x;
   float _165 = _164 * _163;
   float _166 = c_specular_scale.y * _162;
   float _167 = (c_light_col[0].y) * c_mat_specular.y;
   float _168 = _167 * _166;
   float _169 = c_specular_scale.z * _162;
   float _170 = (c_light_col[0].z) * c_mat_specular.z;
   float _171 = _170 * _169;
   int _175 = c_bool & 1;
   bool _176 = (_175 == 0);
   if (!_176)
   {
      float _178 = -0.0f - TEXCOORD_4.x;
      float _179 = -0.0f - TEXCOORD_4.y;
      float _180 = -0.0f - TEXCOORD_4.z;
      float _181 = dot(float3(_57, _58, _59), float3(_178, _179, _180));
      float _182 = saturate(_181);
      float _183 = _182 * TEXCOORD_1.w;
      float _184 = _182 * TEXCOORD_2.w;
      float _185 = _182 * TEXCOORD_4.w;
      float _186 = max(0.0f, _183);
      float _187 = max(0.0f, _184);
      float _188 = max(0.0f, _185);
      float _189 = -0.0f - TEXCOORD_5.x;
      float _190 = -0.0f - TEXCOORD_5.y;
      float _191 = -0.0f - TEXCOORD_5.z;
      float _192 = dot(float3(_57, _58, _59), float3(_189, _190, _191));
      float _193 = saturate(_192);
      float _194 = _193 * COLOR.w;
      float _195 = _193 * COLOR_1.w;
      float _196 = _193 * TEXCOORD_5.w;
      float _197 = max(0.0f, _194);
      float _198 = max(0.0f, _195);
      float _199 = max(0.0f, _196);
      float _200 = _197 + _186;
      float _201 = _198 + _187;
      float _202 = _199 + _188;
      _204 = _200;
      _205 = _201;
      _206 = _202;
   }
   else
   {
      _204 = 0.0f;
      _205 = 0.0f;
      _206 = 0.0f;
   }
   float _207 = c_rt_size.z * SV_Position.x;
   float _208 = c_rt_size.w * SV_Position.y;
   float4 _209 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_207, _208)));
   int _215 = c_bool & 16384;
   bool _216 = (_215 == 0);
   if (!_216)
   {
      float _218 = _209.x * 4.0f;
      float _219 = _209.y * 4.0f;
      float _220 = _209.z * 4.0f;
      _235 = _218;
      _236 = _219;
      _237 = _220;
   }
   else
   {
      int _222 = c_bool & 8192;
      bool _223 = (_222 == 0);
      if (!_223)
      {
         float _225 = _209.x * 2.0f;
         float _226 = _209.y * 2.0f;
         float _227 = _209.z * 2.0f;
         _235 = _225;
         _236 = _226;
         _237 = _227;
      }
      else
      {
         float _231 = _209.x / c_tone_control.z;
         float _232 = _209.y / c_tone_control.z;
         float _233 = _209.z / c_tone_control.z;
         _235 = _231;
         _236 = _232;
         _237 = _233;
      }
   }
   float _238 = _235 + _204;
   float _239 = _236 + _205;
   float _240 = _237 + _206;
   float _241 = saturate(_165);
   float _242 = saturate(_168);
   float _243 = saturate(_171);
   float _306 = dot(float3(_57, _58, _59), float3(_57, _58, _59));
   float _307 = rsqrt(_306);
   float _308 = _307 * _57;
   float _309 = _307 * _58;
   float _310 = _307 * _59;
   float _311 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_308, _309, _310, 1.0f));
   float _312 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_308, _309, _310, 1.0f));
   float _313 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_308, _309, _310, 1.0f));
   float _314 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_308, _309, _310, 1.0f));
   float _315 = dot(float4(_308, _309, _310, 1.0f), float4(_311, _312, _313, _314));
   float _316 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_308, _309, _310, 1.0f));
   float _317 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_308, _309, _310, 1.0f));
   float _318 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_308, _309, _310, 1.0f));
   float _319 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_308, _309, _310, 1.0f));
   float _320 = dot(float4(_308, _309, _310, 1.0f), float4(_316, _317, _318, _319));
   float _321 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_308, _309, _310, 1.0f));
   float _322 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_308, _309, _310, 1.0f));
   float _323 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_308, _309, _310, 1.0f));
   float _324 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_308, _309, _310, 1.0f));
   float _325 = dot(float4(_308, _309, _310, 1.0f), float4(_321, _322, _323, _324));
   float _326 = max(_315, 0.0625f);
   float _327 = max(_320, 0.0625f);
   float _328 = max(_325, 0.0625f);
   float _329 = _326 * (c_ps_scratch[79].x);
   float _330 = _327 * (c_ps_scratch[79].x);
   float _331 = _328 * (c_ps_scratch[79].x);
   float _336 = -0.0f - (c_light_vec[0].x);
   float _337 = -0.0f - (c_light_vec[0].y);
   float _338 = -0.0f - (c_light_vec[0].z);
   float _339 = dot(float3(_57, _58, _59), float3(_336, _337, _338));
   float _340 = -0.0f - (c_light_vec[1].x);
   float _341 = -0.0f - (c_light_vec[1].y);
   float _342 = -0.0f - (c_light_vec[1].z);
   float _343 = dot(float3(_57, _58, _59), float3(_340, _341, _342));
   float _352 = _58 * 0.5f;
   float _353 = _352 + 0.5f;
   float _354 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _355 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _356 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _357 = _354 * _353;
   float _358 = _355 * _353;
   float _359 = _356 * _353;
   float _360 = saturate(_339);
   float _361 = saturate(_343);
   float _366 = (c_light_col[0].x) * _360;
   float _367 = (c_light_col[0].y) * _360;
   float _368 = (c_light_col[0].z) * _360;
   float _373 = (c_light_col[1].x) * _361;
   float _374 = (c_light_col[1].y) * _361;
   float _375 = (c_light_col[1].z) * _361;
   float _376 = _366 * _116;
   float _377 = _367 * _116;
   float _378 = _368 * _116;
   float _379 = (c_light_vec[4].x) + _329;
   float _380 = _379 + _357;
   float _381 = _380 + _238;
   float _382 = _381 + _373;
   float _383 = _382 + _376;
   float _384 = (c_light_vec[4].y) + _330;
   float _385 = _384 + _358;
   float _386 = _385 + _239;
   float _387 = _386 + _374;
   float _388 = _387 + _377;
   float _389 = (c_light_vec[4].z) + _331;
   float _390 = _389 + _359;
   float _391 = _390 + _240;
   float _392 = _391 + _375;
   float _393 = _392 + _378;
   float _394 = max(_383, _388);
   float _395 = max(_394, _393);
   float _398 = (c_ps_user[6].z) + _395;
   float _399 = saturate(_398);
   float _400 = _399 * TEXCOORD.z;
   float _401 = _400 * _241;
   float _402 = _400 * _242;
   float _403 = _400 * _243;
   int _404 = c_bool & 32768;
   bool _405 = (_404 == 0);
   if (!_405)
   {
      float _417 = (c_ps_user[11].x) * _50;
      float _418 = (c_ps_user[11].x) * _51;
      float _419 = (c_ps_user[11].x) * _52;
      float _421 = _57 + 1.0f;
      float _422 = _58 + 1.0f;
      float _423 = _59 + 1.0f;
      float _424 = _421 * 0.5f;
      float _425 = _422 * 0.5f;
      float _426 = _423 * 0.5f;
      float _427 = saturate(_424);
      float _428 = saturate(_425);
      float _429 = saturate(_426);
      float _430 = (c_ps_user[11].y) * _427;
      float _431 = _428 * (c_ps_user[11].y);
      float _432 = _429 * (c_ps_user[11].y);
      float _443 = (c_ps_user[11].w) * _401;
      float _446 = (c_ps_user[12].x) * _116;
      float _452 = -0.0f - _66;
      float _453 = -0.0f - _67;
      float _454 = -0.0f - _68;
      float _455 = dot(float3(_452, _453, _454), float3(_57, _58, _59));
      float _456 = 1.0f - _455;
      float _457 = _456 * _456;
      float _458 = _456 * 0.9722222089767456f;
      float _459 = _457 * _457;
      float _460 = _459 * _458;
      float _461 = _460 + 0.02777777425944805f;
      float _462 = _461 * (c_ps_user[13].z);
      float _464 = (c_ps_user[13].w) * _380;
      float _465 = (c_ps_user[13].w) * _385;
      float _466 = (c_ps_user[13].w) * _390;
      float _469 = (c_ps_user[14].y) * _366;
      float _470 = (c_ps_user[14].y) * _367;
      float _471 = (c_ps_user[14].y) * _368;
      float _473 = (c_ps_user[14].z) * _373;
      float _474 = (c_ps_user[14].z) * _374;
      float _475 = (c_ps_user[14].z) * _375;
      float _477 = (c_ps_user[14].w) * _238;
      float _478 = (c_ps_user[14].w) * _239;
      float _479 = (c_ps_user[14].w) * _240;
      float _483 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _484 = c_specular_scale.x * _483;
      float _485 = _484 * _401;
      float _486 = _469 + (c_ps_user[13].x);
      float _487 = _486 + _473;
      float _488 = c_diffuse_scale.x * _487;
      float _489 = _488 * c_mat_diffuse.x;
      float _490 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _491 = _490 * c_specular_scale.x;
      float _492 = _491 + _485;
      float _493 = _492 * c_mat_specular.x;
      float _494 = _430 + _417;
      float _495 = _494 + _443;
      float _496 = _495 + (c_ps_user[12].y);
      float _497 = _496 + _446;
      float _498 = _497 + _464;
      float _499 = _498 + _477;
      float _500 = _499 + _462;
      float _501 = _500 + _489;
      float _502 = _501 + _493;
      float _503 = c_specular_scale.y * _483;
      float _504 = _503 * _402;
      float _505 = _470 + (c_ps_user[13].x);
      float _506 = _505 + _474;
      float _507 = c_diffuse_scale.y * _506;
      float _508 = _507 * c_mat_diffuse.y;
      float _509 = _490 * c_specular_scale.y;
      float _510 = _509 + _504;
      float _511 = _510 * c_mat_specular.y;
      float _512 = _443 + _418;
      float _513 = _512 + _431;
      float _514 = _513 + (c_ps_user[12].y);
      float _515 = _514 + _446;
      float _516 = _515 + _465;
      float _517 = _516 + _478;
      float _518 = _517 + _462;
      float _519 = _518 + _508;
      float _520 = _519 + _511;
      float _521 = c_specular_scale.z * _483;
      float _522 = _521 * _403;
      float _523 = _471 + (c_ps_user[13].x);
      float _524 = _523 + _475;
      float _525 = c_diffuse_scale.z * _524;
      float _526 = _525 * c_mat_diffuse.z;
      float _527 = _490 * c_specular_scale.z;
      float _528 = _527 + _522;
      float _529 = _528 * c_mat_specular.z;
      float _530 = _443 + _419;
      float _531 = _530 + _432;
      float _532 = _531 + (c_ps_user[12].y);
      float _533 = _532 + _446;
      float _534 = _533 + _466;
      float _535 = _534 + _479;
      float _536 = _535 + _462;
      float _537 = _536 + _526;
      float _538 = _537 + _529;
      float _539 = saturate(_502);
      float _540 = saturate(_520);
      float _541 = saturate(_538);
      _556 = _539;
      _557 = _540;
      _558 = _541;
      _559 = 1.0f;
   }
   else
   {
      float _543 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _544 = _543 * _50;
      float _545 = _544 * _383;
      float _546 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _547 = _546 * _51;
      float _548 = _547 * _388;
      float _549 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _550 = _549 * _52;
      float _551 = _550 * _393;
      float _552 = _545 + _401;
      float _553 = _548 + _402;
      float _554 = _551 + _403;
      _556 = _552;
      _557 = _553;
      _558 = _554;
      _559 = _401;
   }
   float _564 = c_mat_opacity * _45.w;
   float _565 = _564 * c_opacity_scale;
   float _566 = _565 * c_global_opacity;
   float _567 = 1.0f - _566;
   float _568 = saturate(_559);
   float _569 = _567 * _568;
   float _570 = _569 + _566;
   float _571 = _556 * COLOR.x;
   float _572 = _557 * COLOR.y;
   float _573 = _558 * COLOR.z;
   int _574 = c_bool & 16;
   bool _575 = (_574 == 0);
   if (!_575)
   {
      float _579 = c_tone_control.x * _571;
      float _580 = c_tone_control.x * _572;
      float _581 = c_tone_control.x * _573;
      float _583 = log2(_579);
      float _584 = log2(_580);
      float _585 = log2(_581);
      float _586 = _583 * c_tone_control.y;
      float _587 = _584 * c_tone_control.y;
      float _588 = _585 * c_tone_control.y;
      float _589 = exp2(_586);
      float _590 = exp2(_587);
      float _591 = exp2(_588);
      _639 = _589;
      _640 = _590;
      _641 = _591;
   }
   else
   {
      if (_405)
      {
         float _596 = c_tone_control.x * _571;
         float _597 = c_tone_control.x * _572;
         float _598 = c_tone_control.x * _573;
         float _599 = _596 * 2.0f;
         float _600 = _597 * 2.0f;
         float _601 = _598 * 2.0f;
         float _602 = _596 * 0.30000001192092896f;
         float _603 = _597 * 0.30000001192092896f;
         float _604 = _598 * 0.30000001192092896f;
         float _605 = _602 + 0.05000000074505806f;
         float _606 = _603 + 0.05000000074505806f;
         float _607 = _604 + 0.05000000074505806f;
         float _608 = _605 * _599;
         float _609 = _606 * _600;
         float _610 = _607 * _601;
         float _611 = _608 + 0.004000000189989805f;
         float _612 = _609 + 0.004000000189989805f;
         float _613 = _610 + 0.004000000189989805f;
         float _614 = _602 + 0.5f;
         float _615 = _603 + 0.5f;
         float _616 = _604 + 0.5f;
         float _617 = _614 * _599;
         float _618 = _615 * _600;
         float _619 = _616 * _601;
         float _620 = _617 + 0.06000000238418579f;
         float _621 = _618 + 0.06000000238418579f;
         float _622 = _619 + 0.06000000238418579f;
         float _623 = _611 / _620;
         float _624 = _612 / _621;
         float _625 = _613 / _622;
         float _626 = _623 + -0.06666666269302368f;
         float _627 = _624 + -0.06666666269302368f;
         float _628 = _625 + -0.06666666269302368f;
         float _629 = _626 * 1.3790643215179443f;
         float _630 = _627 * 1.3790643215179443f;
         float _631 = _628 * 1.3790643215179443f;
         float _632 = YKR_MaterialCurve(_629, _596);
         float _633 = YKR_MaterialCurve(_630, _597);
         float _634 = YKR_MaterialCurve(_631, _598);
         float _635 = sqrt(_632);
         float _636 = sqrt(_633);
         float _637 = sqrt(_634);
         _639 = _635;
         _640 = _636;
         _641 = _637;
      }
      else
      {
         _639 = _571;
         _640 = _572;
         _641 = _573;
      }
   }
   int _642 = c_bool & 4;
   bool _643 = (_642 == 0);
   if (_643)
   {
      float _645 = saturate(COLOR_1.x);
      float _650 = (c_fog_param[0].x) - _639;
      float _651 = (c_fog_param[0].y) - _640;
      float _652 = (c_fog_param[0].z) - _641;
      float _653 = _650 * _645;
      float _654 = _651 * _645;
      float _655 = _652 * _645;
      _657 = _653;
      _658 = _654;
      _659 = _655;
   }
   else
   {
      _657 = COLOR_1.x;
      _658 = COLOR_1.y;
      _659 = COLOR_1.z;
   }
   float _660 = _657 + _639;
   float _661 = _658 + _640;
   float _662 = _659 + _641;
   if (!_405)
   {
      float _670 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _671 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _672 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _674 = (c_ps_user[12].w) * COLOR.x;
      float _675 = (c_ps_user[12].w) * COLOR.y;
      float _676 = (c_ps_user[12].w) * COLOR.z;
      float _677 = _674 + _660;
      float _678 = _677 + _670;
      float _679 = _675 + _661;
      float _680 = _679 + _671;
      float _681 = _676 + _662;
      float _682 = _681 + _672;
      _684 = _678;
      _685 = _680;
      _686 = _682;
   }
   else
   {
      _684 = _660;
      _685 = _661;
      _686 = _662;
   }
   float _692 = (c_ps_col_scl[0].x) * _684;
   float _693 = (c_ps_col_scl[0].y) * _685;
   float _694 = (c_ps_col_scl[0].z) * _686;
   float _695 = (c_ps_col_scl[0].w) * _570;
   SV_Target.x = _692;
   SV_Target.y = _693;
   SV_Target.z = _694;
   SV_Target.w = _695;
   int _698 = c_bool & 8;
   bool _699 = (_698 != 0);
   float _700 = select(_699, _684, _692);
   float _701 = select(_699, _685, _693);
   float _702 = select(_699, _686, _694);
   float _703 = select(_699, _570, _695);
   SV_Target.x = _700;
   SV_Target.y = _701;
   SV_Target.z = _702;
   SV_Target.w = _703;
   return SV_Target;
}
