// Yakuza Kiwami material s_o1dst[iris][makeup] (0x1BDF3DB9, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float TEXCOORD_6 : TEXCOORD6,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float4 _62 = s3_t.Sample(s3_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _66 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _71 = _66.x * 2.0f;
   float _72 = _66.y * 2.0f;
   float _73 = _71 + -1.0f;
   float _74 = _72 + -1.0f;
   float _75 = dot(float2(_73, _74), float2(_73, _74));
   float _76 = 1.0f - _75;
   float _77 = saturate(_76);
   float _78 = sqrt(_77);
   float _79 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _80 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _81 = _79 - _80;
   float _82 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _83 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _86 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _87 = _85 - _86;
   float _88 = _73 * TEXCOORD_3.x;
   float _89 = _73 * TEXCOORD_3.y;
   float _90 = _73 * TEXCOORD_3.z;
   float _91 = _74 * TEXCOORD_3.w;
   float _92 = _91 * _81;
   float _93 = _91 * _84;
   float _94 = _91 * _87;
   float _95 = _92 + _88;
   float _96 = _93 + _89;
   float _97 = _94 + _90;
   float _98 = _78 * TEXCOORD_2.x;
   float _99 = _78 * TEXCOORD_2.y;
   float _100 = _78 * TEXCOORD_2.z;
   float _101 = _95 + _98;
   float _102 = _96 + _99;
   float _103 = _97 + _100;
   float _104 = dot(float3(_101, _102, _103), float3(_101, _102, _103));
   float _105 = rsqrt(_104);
   float _106 = _101 * _105;
   float _107 = _102 * _105;
   float _108 = _103 * _105;
   float _113 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _114 = rsqrt(_113);
   float _115 = _114 * TEXCOORD_1.x;
   float _116 = _114 * TEXCOORD_1.y;
   float _117 = _114 * TEXCOORD_1.z;
   float _118 = _115 + (c_light_vec[0].x);
   float _119 = _116 + (c_light_vec[0].y);
   float _120 = _117 + (c_light_vec[0].z);
   float _121 = dot(float3(_118, _119, _120), float3(_118, _119, _120));
   float _122 = rsqrt(_121);
   float _123 = _118 * _122;
   float _124 = _119 * _122;
   float _125 = _120 * _122;
   float _126 = _107 + 0.10000000149011612f;
   float _127 = saturate(_126);
   float _130 = (c_attribute_ps[2].w) + 2.0f;
   float _132 = (c_attribute_ps[2].z) + 2.5f;
   float _133 = -0.0f - _115;
   float _134 = -0.0f - _116;
   float _135 = -0.0f - _117;
   float _136 = dot(float3(_133, _134, _135), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _137 = 1.0f - _136;
   float _138 = saturate(_137);
   float _139 = dot(float3(_133, _134, _135), float3(_106, _107, _108));
   float _140 = 1.0f - _139;
   float _141 = saturate(_140);
   float _142 = _141 * _138;
   float _143 = log2(_142);
   float _144 = _143 * _130;
   float _145 = exp2(_144);
   float _146 = _145 * _132;
   int _149 = c_bool & 4096;
   bool _150 = (_149 == 0);
   float _159;
   float _182;
   float _183;
   float _195;
   float _196;
   float _506;
   float _528;
   float _548;
   float _568;
   float _569;
   float _570;
   float _653;
   float _654;
   float _655;
   float _656;
   float _657;
   float _658;
   float _691;
   float _692;
   float _693;
   float _694;
   float _1025;
   float _1026;
   float _1027;
   float _1104;
   float _1105;
   float _1106;
   float _1122;
   float _1123;
   float _1124;
   float _1149;
   float _1150;
   float _1151;
   if (!_150)
   {
      float _155 = (c_ps_user[9].w) * _143;
      float _156 = exp2(_155);
      float _157 = _156 * (c_ps_user[9].z);
      _159 = _157;
   }
   else
   {
      _159 = _146;
   }
   float _160 = _127 * _159;
   float _163 = (c_ps_user[2].x) * _160;
   float _164 = _163 + 1.0f;
   float _165 = _59 * _164;
   float _166 = _60 * _164;
   float _167 = _61 * _164;
   float _171 = c_rt_size.z * SV_Position.x;
   float _172 = c_rt_size.w * SV_Position.y;
   int _175 = c_bool & 8;
   bool _176 = (_175 == 0);
   if (!_176)
   {
      float4 _178 = s13_t.Sample(s13_s, float2(_171, _172));
      _182 = _178.x;
      _183 = _178.y;
   }
   else
   {
      _182 = 1.0f;
      _183 = 1.0f;
   }
   if (!_176)
   {
      float _188 = (c_sdw_param_ch[0].x) * _182;
      float _189 = (c_sdw_param_ch[0].z) * _183;
      float _192 = _188 + (c_sdw_param_ch[0].y);
      float _193 = _189 + (c_sdw_param_ch[0].w);
      _195 = _192;
      _196 = _193;
   }
   else
   {
      _195 = _182;
      _196 = _183;
   }
   float _197 = min(_195, _196);
   float _206 = (c_light_vec[1].x) + _115;
   float _207 = (c_light_vec[1].y) + _116;
   float _208 = (c_light_vec[1].z) + _117;
   float _209 = dot(float3(_206, _207, _208), float3(_206, _207, _208));
   float _210 = rsqrt(_209);
   float _211 = _206 * _210;
   float _212 = _207 * _210;
   float _213 = _208 * _210;
   float _214 = -0.0f - (c_light_vec[1].x);
   float _215 = -0.0f - (c_light_vec[1].y);
   float _216 = -0.0f - (c_light_vec[1].z);
   float _217 = -0.0f - (c_light_vec[0].x);
   float _218 = -0.0f - (c_light_vec[0].y);
   float _219 = -0.0f - (c_light_vec[0].z);
   float _220 = dot(float3(_115, _116, _117), float3(_106, _107, _108));
   float _221 = _220 * 2.0f;
   float _222 = _221 * _106;
   float _223 = _221 * _107;
   float _224 = _221 * _108;
   float _225 = _115 - _222;
   float _226 = _116 - _223;
   float _227 = _117 - _224;
   float _228 = dot(float3(_225, _226, _227), float3(_217, _218, _219));
   float _229 = dot(float3(_225, _226, _227), float3(_214, _215, _216));
   float _230 = saturate(_228);
   float _231 = saturate(_229);
   float _232 = log2(_230);
   float _233 = log2(_231);
   float _234 = _232 * c_mat_power;
   float _235 = _233 * c_mat_power;
   float _236 = exp2(_234);
   float _237 = exp2(_235);
   float _242 = c_mat_fresnel * c_tone_control.w;
   float _243 = c_mat_power * 0.03974359855055809f;
   float _244 = _243 + 0.0856831967830658f;
   float _245 = _237 * _244;
   float _246 = dot(float3(_115, _116, _117), float3(_123, _124, _125));
   float _247 = dot(float3(_115, _116, _117), float3(_211, _212, _213));
   float _248 = 1.0f - _246;
   float _249 = 1.0f - _247;
   float _250 = _248 * _248;
   float _251 = _249 * _249;
   float _252 = 1.0f - _242;
   float _253 = _250 * _250;
   float _254 = _253 * _248;
   float _255 = _254 * _252;
   float _256 = _249 * _252;
   float _257 = _251 * _251;
   float _258 = _257 * _256;
   float _259 = _255 + _242;
   float _260 = _258 + _242;
   float _261 = dot(float3(_106, _107, _108), float3(_217, _218, _219));
   float _262 = saturate(_261);
   float _263 = dot(float3(_106, _107, _108), float3(_214, _215, _216));
   float _264 = saturate(_263);
   float _265 = _245 * _264;
   float _266 = _265 * _260;
   float _275 = c_specular_scale.x * c_mat_specular.x;
   float _276 = c_specular_scale.y * c_mat_specular.y;
   float _277 = c_specular_scale.z * c_mat_specular.z;
   float _282 = _197 * _244;
   float _283 = _282 * _236;
   float _284 = _283 * _262;
   float _285 = _284 * _259;
   float _286 = (c_light_col[0].x) * _285;
   float _287 = (c_light_col[0].y) * _285;
   float _288 = (c_light_col[0].z) * _285;
   float _293 = (c_light_col[1].x) * _266;
   float _294 = (c_light_col[1].y) * _266;
   float _295 = (c_light_col[1].z) * _266;
   float _296 = _293 + _286;
   float _297 = _294 + _287;
   float _298 = _295 + _288;
   float _299 = _275 * _296;
   float _300 = _276 * _297;
   float _301 = _277 * _298;
   float _302 = TEXCOORD_6 + -0.5f;
   bool _303 = (_302 < 0.0f);
   float _304 = float((bool)_303);
   float _310 = (c_ps_scratch[0].x) * _304;
   float _311 = (c_ps_scratch[0].y) * _304;
   float _312 = (c_ps_scratch[0].z) * _304;
   float _313 = (c_ps_scratch[0].w) * _304;
   float _314 = _304 * 30.0f;
   float _315 = TEXCOORD_6 + -1.5f;
   float _316 = _315 + _314;
   bool _317 = (_316 < 0.0f);
   float _318 = float((bool)_317);
   float _324 = (c_ps_scratch[1].x) * _318;
   float _325 = (c_ps_scratch[1].y) * _318;
   float _326 = (c_ps_scratch[1].z) * _318;
   float _327 = (c_ps_scratch[1].w) * _318;
   float _328 = _324 + _310;
   float _329 = _325 + _311;
   float _330 = _326 + _312;
   float _331 = _327 + _313;
   float _332 = _318 * 30.0f;
   float _333 = _316 + -1.0f;
   float _334 = _333 + _332;
   bool _335 = (_334 < 0.0f);
   float _336 = float((bool)_335);
   float _342 = (c_ps_scratch[2].x) * _336;
   float _343 = (c_ps_scratch[2].y) * _336;
   float _344 = (c_ps_scratch[2].z) * _336;
   float _345 = (c_ps_scratch[2].w) * _336;
   float _346 = _328 + _342;
   float _347 = _329 + _343;
   float _348 = _330 + _344;
   float _349 = _331 + _345;
   float _350 = _336 * 30.0f;
   float _351 = _334 + -1.0f;
   float _352 = _351 + _350;
   bool _353 = (_352 < 0.0f);
   float _354 = float((bool)_353);
   float _360 = (c_ps_scratch[3].x) * _354;
   float _361 = (c_ps_scratch[3].y) * _354;
   float _362 = (c_ps_scratch[3].z) * _354;
   float _363 = (c_ps_scratch[3].w) * _354;
   float _364 = _346 + _360;
   float _365 = _347 + _361;
   float _366 = _348 + _362;
   float _367 = _349 + _363;
   float _368 = _354 * 30.0f;
   float _369 = _352 + -1.0f;
   float _370 = _369 + _368;
   bool _371 = (_370 < 0.0f);
   float _372 = float((bool)_371);
   float _378 = (c_ps_scratch[4].x) * _372;
   float _379 = (c_ps_scratch[4].y) * _372;
   float _380 = (c_ps_scratch[4].z) * _372;
   float _381 = (c_ps_scratch[4].w) * _372;
   float _382 = _364 + _378;
   float _383 = _365 + _379;
   float _384 = _366 + _380;
   float _385 = _367 + _381;
   float _386 = _372 * 30.0f;
   float _387 = _370 + -1.0f;
   float _388 = _387 + _386;
   bool _389 = (_388 < 0.0f);
   float _390 = float((bool)_389);
   float _396 = (c_ps_scratch[5].x) * _390;
   float _397 = (c_ps_scratch[5].y) * _390;
   float _398 = (c_ps_scratch[5].z) * _390;
   float _399 = (c_ps_scratch[5].w) * _390;
   float _400 = _382 + _396;
   float _401 = _383 + _397;
   float _402 = _384 + _398;
   float _403 = _385 + _399;
   float _404 = _390 * 30.0f;
   float _405 = _388 + -1.0f;
   float _406 = _405 + _404;
   bool _407 = (_406 < 0.0f);
   float _408 = float((bool)_407);
   float _414 = (c_ps_scratch[6].x) * _408;
   float _415 = (c_ps_scratch[6].y) * _408;
   float _416 = (c_ps_scratch[6].z) * _408;
   float _417 = (c_ps_scratch[6].w) * _408;
   float _418 = _400 + _414;
   float _419 = _401 + _415;
   float _420 = _402 + _416;
   float _421 = _403 + _417;
   float _422 = _408 * 30.0f;
   float _423 = _406 + -1.0f;
   float _424 = _423 + _422;
   bool _425 = (_424 < 0.0f);
   float _426 = float((bool)_425);
   float _432 = (c_ps_scratch[7].x) * _426;
   float _433 = (c_ps_scratch[7].y) * _426;
   float _434 = (c_ps_scratch[7].z) * _426;
   float _435 = (c_ps_scratch[7].w) * _426;
   float _436 = _418 + _432;
   float _437 = _419 + _433;
   float _438 = _420 + _434;
   float _439 = _421 + _435;
   float _440 = _426 * 30.0f;
   float _441 = _424 + -1.0f;
   float _442 = _441 + _440;
   bool _443 = (_442 < 0.0f);
   float _444 = float((bool)_443);
   float _450 = (c_ps_scratch[8].x) * _444;
   float _451 = (c_ps_scratch[8].y) * _444;
   float _452 = (c_ps_scratch[8].z) * _444;
   float _453 = (c_ps_scratch[8].w) * _444;
   float _454 = _436 + _450;
   float _455 = _437 + _451;
   float _456 = _438 + _452;
   float _457 = _439 + _453;
   float _458 = _444 * 30.0f;
   float _459 = _442 + -1.0f;
   float _460 = _459 + _458;
   bool _461 = (_460 < 0.0f);
   float _462 = float((bool)_461);
   float _468 = (c_ps_scratch[9].x) * _462;
   float _469 = (c_ps_scratch[9].y) * _462;
   float _470 = (c_ps_scratch[9].z) * _462;
   float _471 = (c_ps_scratch[9].w) * _462;
   float _472 = _454 + _468;
   float _473 = _455 + _469;
   float _474 = _456 + _470;
   float _475 = _457 + _471;
   float _476 = max(_165, _166);
   float _477 = max(_476, _167);
   float _478 = min(_165, _166);
   float _479 = min(_478, _167);
   float _480 = _479 + _477;
   float _481 = _480 * 0.5f;
   float _482 = _474 * 2.0f;
   float _483 = _482 + -1.0f;
   bool _484 = (_483 > 0.0f);
   float _485 = float((bool)_484);
   float _486 = 1.0f - _481;
   float _487 = 2.0f - _482;
   float _488 = _486 * _487;
   float _489 = 1.0f - _488;
   float _490 = _489 * _485;
   float _491 = _481 * _482;
   float _492 = 1.0f - _485;
   float _493 = _491 * _492;
   float _494 = _490 + _493;
   bool _495 = (_473 > 0.0f);
   if (_495)
   {
      bool _497 = (_494 < 0.5f);
      if (_497)
      {
         float _499 = _473 + 1.0f;
         float _500 = _494 * _499;
         _506 = _500;
      }
      else
      {
         float _502 = _494 + _473;
         float _503 = _494 * _473;
         float _504 = _502 - _503;
         _506 = _504;
      }
      float _507 = _494 * 2.0f;
      float _508 = _507 - _506;
      float _509 = _472 + 1.3333333730697632f;
      float _510 = frac(_509);
      bool _511 = (_510 < 0.1666666716337204f);
      if (_511)
      {
         float _513 = _506 - _508;
         float _514 = _510 * 6.0f;
         float _515 = _514 * _513;
         float _516 = _515 + _508;
         _528 = _516;
      }
      else
      {
         bool _518 = (_510 < 0.5f);
         if (!_518)
         {
            bool _520 = (_510 < 0.6666666865348816f);
            if (_520)
            {
               float _522 = _506 - _508;
               float _523 = 0.6666666865348816f - _510;
               float _524 = _523 * 6.0f;
               float _525 = _524 * _522;
               float _526 = _525 + _508;
               _528 = _526;
            }
            else
            {
               _528 = _508;
            }
         }
         else
         {
            _528 = _506;
         }
      }
      float _529 = _472 + 1.0f;
      float _530 = frac(_529);
      bool _531 = (_530 < 0.1666666716337204f);
      if (_531)
      {
         float _533 = _506 - _508;
         float _534 = _533 * 6.0f;
         float _535 = _534 * _530;
         float _536 = _535 + _508;
         _548 = _536;
      }
      else
      {
         bool _538 = (_530 < 0.5f);
         if (!_538)
         {
            bool _540 = (_530 < 0.6666666865348816f);
            if (_540)
            {
               float _542 = _506 - _508;
               float _543 = 0.6666666865348816f - _530;
               float _544 = _542 * 6.0f;
               float _545 = _544 * _543;
               float _546 = _545 + _508;
               _548 = _546;
            }
            else
            {
               _548 = _508;
            }
         }
         else
         {
            _548 = _506;
         }
      }
      float _549 = _472 + 0.6666666269302368f;
      float _550 = frac(_549);
      bool _551 = (_550 < 0.1666666716337204f);
      if (_551)
      {
         float _553 = _506 - _508;
         float _554 = _553 * 6.0f;
         float _555 = _554 * _550;
         float _556 = _555 + _508;
         _568 = _528;
         _569 = _548;
         _570 = _556;
      }
      else
      {
         bool _558 = (_550 < 0.5f);
         if (!_558)
         {
            bool _560 = (_550 < 0.6666666865348816f);
            if (_560)
            {
               float _562 = _506 - _508;
               float _563 = 0.6666666865348816f - _550;
               float _564 = _562 * 6.0f;
               float _565 = _564 * _563;
               float _566 = _565 + _508;
               _568 = _528;
               _569 = _548;
               _570 = _566;
            }
            else
            {
               _568 = _528;
               _569 = _548;
               _570 = _508;
            }
         }
         else
         {
            _568 = _528;
            _569 = _548;
            _570 = _506;
         }
      }
   }
   else
   {
      _568 = _494;
      _569 = _494;
      _570 = _494;
   }
   int _574 = c_bool & 1;
   bool _575 = (_574 == 0);
   if (!_575)
   {
      float _577 = -0.0f - TEXCOORD_4.x;
      float _578 = -0.0f - TEXCOORD_4.y;
      float _579 = -0.0f - TEXCOORD_4.z;
      float _580 = dot(float3(_106, _107, _108), float3(_577, _578, _579));
      float _581 = saturate(_580);
      float _582 = _581 * TEXCOORD_1.w;
      float _583 = _581 * TEXCOORD_2.w;
      float _584 = _581 * TEXCOORD_4.w;
      float _585 = max(0.0f, _582);
      float _586 = max(0.0f, _583);
      float _587 = max(0.0f, _584);
      float _588 = -0.0f - TEXCOORD_5.x;
      float _589 = -0.0f - TEXCOORD_5.y;
      float _590 = -0.0f - TEXCOORD_5.z;
      float _591 = dot(float3(_106, _107, _108), float3(_588, _589, _590));
      float _592 = saturate(_591);
      float _593 = _592 * COLOR.w;
      float _594 = _592 * COLOR_1.w;
      float _595 = _592 * TEXCOORD_5.w;
      float _596 = max(0.0f, _593);
      float _597 = max(0.0f, _594);
      float _598 = max(0.0f, _595);
      float _599 = _596 + _585;
      float _600 = _597 + _586;
      float _601 = _598 + _587;
      float _602 = _115 + TEXCOORD_4.x;
      float _603 = _116 + TEXCOORD_4.y;
      float _604 = _117 + TEXCOORD_4.z;
      float _605 = dot(float3(_602, _603, _604), float3(_602, _603, _604));
      float _606 = rsqrt(_605);
      float _607 = _115 + TEXCOORD_5.x;
      float _608 = _116 + TEXCOORD_5.y;
      float _609 = _117 + TEXCOORD_5.z;
      float _610 = dot(float3(_607, _608, _609), float3(_607, _608, _609));
      float _611 = rsqrt(_610);
      float _612 = _607 * _611;
      float _613 = -0.0f - _612;
      float _614 = _608 * _611;
      float _615 = -0.0f - _614;
      float _616 = _609 * _611;
      float _617 = -0.0f - _616;
      float _618 = _602 * _606;
      float _619 = -0.0f - _618;
      float _620 = _603 * _606;
      float _621 = -0.0f - _620;
      float _622 = _604 * _606;
      float _623 = -0.0f - _622;
      float _624 = dot(float3(_619, _621, _623), float3(_106, _107, _108));
      float _625 = dot(float3(_613, _615, _617), float3(_106, _107, _108));
      float _626 = saturate(_624);
      float _627 = saturate(_625);
      float _628 = log2(_626);
      float _629 = log2(_627);
      float _630 = _628 * c_mat_power;
      float _631 = _629 * c_mat_power;
      float _632 = exp2(_630);
      float _633 = exp2(_631);
      float _634 = _632 * TEXCOORD_1.w;
      float _635 = _632 * TEXCOORD_2.w;
      float _636 = _632 * TEXCOORD_4.w;
      float _637 = _633 * COLOR.w;
      float _638 = _633 * COLOR_1.w;
      float _639 = _633 * TEXCOORD_5.w;
      float _640 = _637 + _634;
      float _641 = _638 + _635;
      float _642 = _639 + _636;
      float _643 = max(0.0f, _640);
      float _644 = max(0.0f, _641);
      float _645 = max(0.0f, _642);
      float _646 = _275 * _643;
      float _647 = _276 * _644;
      float _648 = _277 * _645;
      float _649 = min(_646, 1.0f);
      float _650 = min(_647, 1.0f);
      float _651 = min(_648, 1.0f);
      _653 = _649;
      _654 = _650;
      _655 = _651;
      _656 = _599;
      _657 = _600;
      _658 = _601;
   }
   else
   {
      _653 = 0.0f;
      _654 = 0.0f;
      _655 = 0.0f;
      _656 = 0.0f;
      _657 = 0.0f;
      _658 = 0.0f;
   }
   float _659 = c_rt_size.z * SV_Position.x;
   float _660 = c_rt_size.w * SV_Position.y;
   float4 _661 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_659, _660)));
   int _668 = c_bool & 16384;
   bool _669 = (_668 == 0);
   if (!_669)
   {
      float _671 = _661.x * 4.0f;
      float _672 = _661.y * 4.0f;
      float _673 = _661.z * 4.0f;
      float _674 = _661.w * 4.0f;
      _691 = _671;
      _692 = _672;
      _693 = _673;
      _694 = _674;
   }
   else
   {
      int _676 = c_bool & 8192;
      bool _677 = (_676 == 0);
      if (!_677)
      {
         float _679 = _661.x * 2.0f;
         float _680 = _661.y * 2.0f;
         float _681 = _661.z * 2.0f;
         float _682 = _661.w * 2.0f;
         _691 = _679;
         _692 = _680;
         _693 = _681;
         _694 = _682;
      }
      else
      {
         float _686 = _661.x / c_tone_control.z;
         float _687 = _661.y / c_tone_control.z;
         float _688 = _661.z / c_tone_control.z;
         float _689 = _661.w / c_tone_control.z;
         _691 = _686;
         _692 = _687;
         _693 = _688;
         _694 = _689;
      }
   }
   float _695 = _691 + _656;
   float _696 = _692 + _657;
   float _697 = _693 + _658;
   float _698 = _694 + _653;
   float _699 = _694 + _654;
   float _700 = _694 + _655;
   float _763 = dot(float3(_106, _107, _108), float3(_106, _107, _108));
   float _764 = rsqrt(_763);
   float _765 = _764 * _106;
   float _766 = _764 * _107;
   float _767 = _764 * _108;
   float _768 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_765, _766, _767, 1.0f));
   float _769 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_765, _766, _767, 1.0f));
   float _770 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_765, _766, _767, 1.0f));
   float _771 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_765, _766, _767, 1.0f));
   float _772 = dot(float4(_765, _766, _767, 1.0f), float4(_768, _769, _770, _771));
   float _773 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_765, _766, _767, 1.0f));
   float _774 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_765, _766, _767, 1.0f));
   float _775 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_765, _766, _767, 1.0f));
   float _776 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_765, _766, _767, 1.0f));
   float _777 = dot(float4(_765, _766, _767, 1.0f), float4(_773, _774, _775, _776));
   float _778 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_765, _766, _767, 1.0f));
   float _779 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_765, _766, _767, 1.0f));
   float _780 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_765, _766, _767, 1.0f));
   float _781 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_765, _766, _767, 1.0f));
   float _782 = dot(float4(_765, _766, _767, 1.0f), float4(_778, _779, _780, _781));
   float _783 = max(_772, 0.0625f);
   float _784 = max(_777, 0.0625f);
   float _785 = max(_782, 0.0625f);
   float _786 = _783 * (c_ps_scratch[79].x);
   float _787 = _784 * (c_ps_scratch[79].x);
   float _788 = _785 * (c_ps_scratch[79].x);
   float _793 = -0.0f - (c_light_vec[0].x);
   float _794 = -0.0f - (c_light_vec[0].y);
   float _795 = -0.0f - (c_light_vec[0].z);
   float _796 = dot(float3(_106, _107, _108), float3(_793, _794, _795));
   float _797 = -0.0f - (c_light_vec[1].x);
   float _798 = -0.0f - (c_light_vec[1].y);
   float _799 = -0.0f - (c_light_vec[1].z);
   float _800 = dot(float3(_106, _107, _108), float3(_797, _798, _799));
   float _809 = _107 * 0.5f;
   float _810 = _809 + 0.5f;
   float _811 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _812 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _813 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _814 = _811 * _810;
   float _815 = _812 * _810;
   float _816 = _813 * _810;
   float _817 = saturate(_796);
   float _818 = saturate(_800);
   float _823 = (c_light_col[0].x) * _817;
   float _824 = (c_light_col[0].y) * _817;
   float _825 = (c_light_col[0].z) * _817;
   float _830 = (c_light_col[1].x) * _818;
   float _831 = (c_light_col[1].y) * _818;
   float _832 = (c_light_col[1].z) * _818;
   float _833 = _698 + _299;
   float _834 = _699 + _300;
   float _835 = _700 + _301;
   int _836 = c_bool & 32768;
   bool _837 = (_836 == 0);
   float _846 = _786 + TEXCOORD.z;
   float _847 = _846 + (c_light_vec[4].x);
   float _848 = _847 + _814;
   float _849 = _787 + TEXCOORD.z;
   float _850 = _849 + (c_light_vec[4].y);
   float _851 = _850 + _815;
   float _852 = _788 + TEXCOORD.z;
   float _853 = _852 + (c_light_vec[4].z);
   float _854 = _853 + _816;
   if (!_837)
   {
      float _858 = (c_ps_user[11].x) * _568;
      float _859 = (c_ps_user[11].x) * _569;
      float _860 = (c_ps_user[11].x) * _570;
      float _862 = _106 + 1.0f;
      float _863 = _107 + 1.0f;
      float _864 = _108 + 1.0f;
      float _865 = _862 * 0.5f;
      float _866 = _863 * 0.5f;
      float _867 = _864 * 0.5f;
      float _868 = saturate(_865);
      float _869 = saturate(_866);
      float _870 = saturate(_867);
      float _871 = (c_ps_user[11].y) * _868;
      float _872 = _869 * (c_ps_user[11].y);
      float _873 = _870 * (c_ps_user[11].y);
      float _883 = (c_ps_user[11].z) * _62.x;
      float _884 = (c_ps_user[11].z) * _62.y;
      float _885 = (c_ps_user[11].z) * _62.z;
      float _887 = (c_ps_user[11].w) * _833;
      float _890 = (c_ps_user[12].x) * _197;
      float _896 = _140 * _140;
      float _897 = _140 * 0.9722222089767456f;
      float _898 = _896 * _896;
      float _899 = _898 * _897;
      float _900 = _899 + 0.02777777425944805f;
      float _901 = (c_ps_user[13].z) * _900;
      float _903 = (c_ps_user[13].w) * _848;
      float _904 = (c_ps_user[13].w) * _851;
      float _905 = (c_ps_user[13].w) * _854;
      float _908 = (c_ps_user[14].y) * _823;
      float _909 = (c_ps_user[14].y) * _824;
      float _910 = (c_ps_user[14].y) * _825;
      float _912 = (c_ps_user[14].z) * _830;
      float _913 = (c_ps_user[14].z) * _831;
      float _914 = (c_ps_user[14].z) * _832;
      float _916 = (c_ps_user[14].w) * _695;
      float _917 = (c_ps_user[14].w) * _696;
      float _918 = (c_ps_user[14].w) * _697;
      float _923 = (c_ps_user[15].z) * _698;
      float _924 = (c_ps_user[15].z) * _699;
      float _925 = (c_ps_user[15].z) * _700;
      float _926 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _927 = c_specular_scale.x * _926;
      float _928 = _927 * _833;
      float _929 = _908 + (c_ps_user[13].x);
      float _930 = _929 + _912;
      float _931 = c_diffuse_scale.x * _930;
      float _932 = _931 * c_mat_diffuse.x;
      float _933 = (c_ps_user[13].y) + _883;
      float _934 = _933 * c_specular_scale.x;
      float _935 = _934 + _928;
      float _936 = _935 * c_mat_specular.x;
      float _937 = _871 + _858;
      float _938 = _937 + _887;
      float _939 = _938 + (c_ps_user[12].y);
      float _940 = _939 + _890;
      float _941 = _940 + _903;
      float _942 = _941 + _901;
      float _943 = _942 + _916;
      float _944 = _943 + _923;
      float _945 = _944 + _932;
      float _946 = _945 + _936;
      float _947 = c_specular_scale.y * _926;
      float _948 = _947 * _834;
      float _949 = _909 + (c_ps_user[13].x);
      float _950 = _949 + _913;
      float _951 = c_diffuse_scale.y * _950;
      float _952 = _951 * c_mat_diffuse.y;
      float _953 = (c_ps_user[13].y) + _884;
      float _954 = _953 * c_specular_scale.y;
      float _955 = _954 + _948;
      float _956 = _955 * c_mat_specular.y;
      float _957 = _887 + _859;
      float _958 = _957 + _872;
      float _959 = _958 + (c_ps_user[12].y);
      float _960 = _959 + _890;
      float _961 = _960 + _904;
      float _962 = _961 + _901;
      float _963 = _962 + _917;
      float _964 = _963 + _924;
      float _965 = _964 + _952;
      float _966 = _965 + _956;
      float _967 = c_specular_scale.z * _926;
      float _968 = _967 * _835;
      float _969 = _910 + (c_ps_user[13].x);
      float _970 = _969 + _914;
      float _971 = c_diffuse_scale.z * _970;
      float _972 = _971 * c_mat_diffuse.z;
      float _973 = (c_ps_user[13].y) + _885;
      float _974 = _973 * c_specular_scale.z;
      float _975 = _974 + _968;
      float _976 = _975 * c_mat_specular.z;
      float _977 = _887 + _860;
      float _978 = _977 + _873;
      float _979 = _978 + (c_ps_user[12].y);
      float _980 = _979 + _890;
      float _981 = _980 + _905;
      float _982 = _981 + _901;
      float _983 = _982 + _918;
      float _984 = _983 + _925;
      float _985 = _984 + _972;
      float _986 = _985 + _976;
      float _987 = saturate(_946);
      float _988 = saturate(_966);
      float _989 = saturate(_986);
      _1025 = _987;
      _1026 = _988;
      _1027 = _989;
   }
   else
   {
      float _991 = _825 * _197;
      float _992 = _854 + _697;
      float _993 = _992 + _832;
      float _994 = _993 + _991;
      float _995 = _824 * _197;
      float _996 = _851 + _696;
      float _997 = _996 + _831;
      float _998 = _997 + _995;
      float _999 = _823 * _197;
      float _1000 = _848 + _695;
      float _1001 = _1000 + _830;
      float _1002 = _1001 + _999;
      float _1003 = saturate(_1002);
      float _1004 = saturate(_998);
      float _1005 = saturate(_994);
      float _1006 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _1007 = _1006 * _568;
      float _1008 = _1007 * _1003;
      float _1009 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _1010 = _1009 * _569;
      float _1011 = _1010 * _1004;
      float _1012 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _1013 = _1012 * _570;
      float _1014 = _1013 * _1005;
      float _1015 = saturate(_833);
      float _1016 = saturate(_834);
      float _1017 = saturate(_835);
      float _1018 = _1015 * _62.x;
      float _1019 = _1016 * _62.y;
      float _1020 = _1017 * _62.z;
      float _1021 = _1018 + _1008;
      float _1022 = _1019 + _1011;
      float _1023 = _1020 + _1014;
      _1025 = _1021;
      _1026 = _1022;
      _1027 = _1023;
   }
   float _1032 = _475 * _54.w;
   float _1033 = _1032 * c_mat_opacity;
   float _1034 = _1033 * c_opacity_scale;
   float _1035 = _1034 * c_global_opacity;
   float _1036 = _1025 * COLOR.x;
   float _1037 = _1026 * COLOR.y;
   float _1038 = _1027 * COLOR.z;
   int _1039 = c_bool & 16;
   bool _1040 = (_1039 == 0);
   if (!_1040)
   {
      float _1044 = c_tone_control.x * _1036;
      float _1045 = c_tone_control.x * _1037;
      float _1046 = c_tone_control.x * _1038;
      float _1048 = log2(_1044);
      float _1049 = log2(_1045);
      float _1050 = log2(_1046);
      float _1051 = _1048 * c_tone_control.y;
      float _1052 = _1049 * c_tone_control.y;
      float _1053 = _1050 * c_tone_control.y;
      float _1054 = exp2(_1051);
      float _1055 = exp2(_1052);
      float _1056 = exp2(_1053);
      _1104 = _1054;
      _1105 = _1055;
      _1106 = _1056;
   }
   else
   {
      if (_837)
      {
         float _1061 = c_tone_control.x * _1036;
         float _1062 = c_tone_control.x * _1037;
         float _1063 = c_tone_control.x * _1038;
         float _1064 = _1061 * 2.0f;
         float _1065 = _1062 * 2.0f;
         float _1066 = _1063 * 2.0f;
         float _1067 = _1061 * 0.30000001192092896f;
         float _1068 = _1062 * 0.30000001192092896f;
         float _1069 = _1063 * 0.30000001192092896f;
         float _1070 = _1067 + 0.05000000074505806f;
         float _1071 = _1068 + 0.05000000074505806f;
         float _1072 = _1069 + 0.05000000074505806f;
         float _1073 = _1070 * _1064;
         float _1074 = _1071 * _1065;
         float _1075 = _1072 * _1066;
         float _1076 = _1073 + 0.004000000189989805f;
         float _1077 = _1074 + 0.004000000189989805f;
         float _1078 = _1075 + 0.004000000189989805f;
         float _1079 = _1067 + 0.5f;
         float _1080 = _1068 + 0.5f;
         float _1081 = _1069 + 0.5f;
         float _1082 = _1079 * _1064;
         float _1083 = _1080 * _1065;
         float _1084 = _1081 * _1066;
         float _1085 = _1082 + 0.06000000238418579f;
         float _1086 = _1083 + 0.06000000238418579f;
         float _1087 = _1084 + 0.06000000238418579f;
         float _1088 = _1076 / _1085;
         float _1089 = _1077 / _1086;
         float _1090 = _1078 / _1087;
         float _1091 = _1088 + -0.06666666269302368f;
         float _1092 = _1089 + -0.06666666269302368f;
         float _1093 = _1090 + -0.06666666269302368f;
         float _1094 = _1091 * 1.3790643215179443f;
         float _1095 = _1092 * 1.3790643215179443f;
         float _1096 = _1093 * 1.3790643215179443f;
         float _1097 = YKR_MaterialCurve(_1094, _1061);
         float _1098 = YKR_MaterialCurve(_1095, _1062);
         float _1099 = YKR_MaterialCurve(_1096, _1063);
         float _1100 = sqrt(_1097);
         float _1101 = sqrt(_1098);
         float _1102 = sqrt(_1099);
         _1104 = _1100;
         _1105 = _1101;
         _1106 = _1102;
      }
      else
      {
         _1104 = _1036;
         _1105 = _1037;
         _1106 = _1038;
      }
   }
   int _1107 = c_bool & 4;
   bool _1108 = (_1107 == 0);
   if (_1108)
   {
      float _1110 = saturate(COLOR_1.x);
      float _1115 = (c_fog_param[0].x) - _1104;
      float _1116 = (c_fog_param[0].y) - _1105;
      float _1117 = (c_fog_param[0].z) - _1106;
      float _1118 = _1115 * _1110;
      float _1119 = _1116 * _1110;
      float _1120 = _1117 * _1110;
      _1122 = _1118;
      _1123 = _1119;
      _1124 = _1120;
   }
   else
   {
      _1122 = COLOR_1.x;
      _1123 = COLOR_1.y;
      _1124 = COLOR_1.z;
   }
   float _1125 = _1122 + _1104;
   float _1126 = _1123 + _1105;
   float _1127 = _1124 + _1106;
   if (!_837)
   {
      float _1135 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _1136 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _1137 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _1139 = (c_ps_user[12].w) * COLOR.x;
      float _1140 = (c_ps_user[12].w) * COLOR.y;
      float _1141 = (c_ps_user[12].w) * COLOR.z;
      float _1142 = _1139 + _1125;
      float _1143 = _1142 + _1135;
      float _1144 = _1140 + _1126;
      float _1145 = _1144 + _1136;
      float _1146 = _1141 + _1127;
      float _1147 = _1146 + _1137;
      _1149 = _1143;
      _1150 = _1145;
      _1151 = _1147;
   }
   else
   {
      _1149 = _1125;
      _1150 = _1126;
      _1151 = _1127;
   }
   float _1157 = (c_ps_col_scl[0].x) * _1149;
   float _1158 = (c_ps_col_scl[0].y) * _1150;
   float _1159 = (c_ps_col_scl[0].z) * _1151;
   float _1160 = (c_ps_col_scl[0].w) * _1035;
   SV_Target.x = _1157;
   SV_Target.y = _1158;
   SV_Target.z = _1159;
   SV_Target.w = _1160;
   int _1163 = c_bool & 8;
   bool _1164 = (_1163 != 0);
   float _1165 = select(_1164, _1149, _1157);
   float _1166 = select(_1164, _1150, _1158);
   float _1167 = select(_1164, _1151, _1159);
   float _1168 = select(_1164, _1035, _1160);
   SV_Target.x = _1165;
   SV_Target.y = _1166;
   SV_Target.z = _1167;
   SV_Target.w = _1168;
   return SV_Target;
}
