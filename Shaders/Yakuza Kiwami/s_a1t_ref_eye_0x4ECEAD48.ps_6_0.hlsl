// Yakuza Kiwami material s_a1t[ref][eye] (0x4ECEAD48, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

TextureCube<float4> s1c_t : register(t1);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s13_t : register(t13);

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

SamplerState s1c_s : register(s1);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    linear float3 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _49 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _54 = _49.x * 2.0f;
   float _55 = _49.y * 2.0f;
   float _56 = _54 + -1.0f;
   float _57 = _55 + -1.0f;
   float _58 = dot(float2(_56, _57), float2(_56, _57));
   float _59 = 1.0f - _58;
   float _60 = saturate(_59);
   float _61 = sqrt(_60);
   float _62 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _63 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _64 = _62 - _63;
   float _65 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _66 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _67 = _65 - _66;
   float _68 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _69 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _70 = _68 - _69;
   float _71 = _56 * TEXCOORD_3.x;
   float _72 = _56 * TEXCOORD_3.y;
   float _73 = _56 * TEXCOORD_3.z;
   float _74 = _57 * TEXCOORD_3.w;
   float _75 = _74 * _64;
   float _76 = _74 * _67;
   float _77 = _74 * _70;
   float _78 = _75 + _71;
   float _79 = _76 + _72;
   float _80 = _77 + _73;
   float _81 = _61 * TEXCOORD_2.x;
   float _82 = _61 * TEXCOORD_2.y;
   float _83 = _61 * TEXCOORD_2.z;
   float _84 = _78 + _81;
   float _85 = _79 + _82;
   float _86 = _80 + _83;
   float _87 = dot(float3(_84, _85, _86), float3(_84, _85, _86));
   float _88 = rsqrt(_87);
   float _89 = _84 * _88;
   float _90 = _85 * _88;
   float _91 = _86 * _88;
   float _96 = dot(float3(TEXCOORD_6.x, TEXCOORD_6.y, TEXCOORD_6.z), float3(TEXCOORD_6.x, TEXCOORD_6.y, TEXCOORD_6.z));
   float _97 = rsqrt(_96);
   float _98 = _97 * TEXCOORD_6.x;
   float _99 = _97 * TEXCOORD_6.y;
   float _100 = _97 * TEXCOORD_6.z;
   float _105 = (c_ps_user[4].x) + TEXCOORD_1.x;
   float _106 = (c_ps_user[4].y) + TEXCOORD_1.y;
   float _107 = (c_ps_user[4].z) + TEXCOORD_1.z;
   float _108 = dot(float3(_105, _106, _107), float3(_105, _106, _107));
   float _109 = rsqrt(_108);
   float _114 = (c_ps_user[5].x) + TEXCOORD_1.x;
   float _115 = (c_ps_user[5].y) + TEXCOORD_1.y;
   float _116 = (c_ps_user[5].z) + TEXCOORD_1.z;
   float _117 = dot(float3(_114, _115, _116), float3(_114, _115, _116));
   float _118 = rsqrt(_117);
   float _119 = dot(float3(_98, _99, _100), float3(_89, _90, _91));
   float _120 = _119 * 2.0f;
   float _121 = _120 * _89;
   float _122 = _120 * _90;
   float _123 = _120 * _91;
   float _124 = _98 - _121;
   float _125 = _99 - _122;
   float _126 = _100 - _123;
   float _127 = dot(float3(_124, _125, _126), float3(_124, _125, _126));
   float _128 = rsqrt(_127);
   float _129 = _125 * _128;
   float _130 = _126 * _128;
   float _131 = _128 * _124;
   float _132 = -0.0f - _131;
   float4 _133 = s1c_t.Sample(s1c_s, float3(_132, _129, _130));
   float _138 = max(_133.w, 0.003921568859368563f);
   float _139 = 0.25f / _138;
   float _140 = _139 * _133.x;
   float _141 = _139 * _133.y;
   float _142 = _139 * _133.z;
   float _143 = _140 * _140;
   float _144 = _141 * _141;
   float _145 = _142 * _142;
   float _148 = _143 * (c_ps_user[6].w);
   float _149 = _144 * (c_ps_user[6].w);
   float _150 = _145 * (c_ps_user[6].w);
   float _154 = c_rt_size.z * SV_Position.x;
   float _155 = c_rt_size.w * SV_Position.y;
   int _158 = c_bool & 8;
   bool _159 = (_158 == 0);
   float _165;
   float _166;
   float _178;
   float _179;
   float _265;
   float _266;
   float _267;
   float _570;
   float _571;
   float _572;
   float _573;
   float _652;
   float _653;
   float _654;
   float _676;
   float _677;
   float _678;
   if (!_159)
   {
      float4 _161 = s13_t.Sample(s13_s, float2(_154, _155));
      _165 = _161.x;
      _166 = _161.y;
   }
   else
   {
      _165 = 1.0f;
      _166 = 1.0f;
   }
   if (!_159)
   {
      float _171 = (c_sdw_param_ch[0].x) * _165;
      float _172 = (c_sdw_param_ch[0].z) * _166;
      float _175 = _171 + (c_sdw_param_ch[0].y);
      float _176 = _172 + (c_sdw_param_ch[0].w);
      _178 = _175;
      _179 = _176;
   }
   else
   {
      _178 = _165;
      _179 = _166;
   }
   float _180 = min(_178, _179);
   float _181 = _114 * _118;
   float _182 = -0.0f - _181;
   float _183 = _115 * _118;
   float _184 = -0.0f - _183;
   float _185 = _116 * _118;
   float _186 = -0.0f - _185;
   float _187 = _105 * _109;
   float _188 = -0.0f - _187;
   float _189 = _106 * _109;
   float _190 = -0.0f - _189;
   float _191 = _107 * _109;
   float _192 = -0.0f - _191;
   float _199 = (c_ps_user[6].x) * c_mat_power;
   float _200 = dot(float3(_188, _190, _192), float3(_89, _90, _91));
   float _201 = saturate(_200);
   float _202 = log2(_201);
   float _203 = _199 * _202;
   float _204 = exp2(_203);
   float _215 = (c_ps_user[4].w) * _204;
   float _217 = (c_ps_user[6].y) * c_mat_power;
   float _218 = dot(float3(_182, _184, _186), float3(_89, _90, _91));
   float _219 = saturate(_218);
   float _220 = log2(_219);
   float _221 = _217 * _220;
   float _222 = exp2(_221);
   float _225 = (c_ps_user[5].w) * _222;
   float _226 = _225 + _215;
   float _227 = c_specular_scale.x * _226;
   float _228 = (c_light_col[0].x) * c_mat_specular.x;
   float _229 = _228 * _227;
   float _230 = c_specular_scale.y * _226;
   float _231 = (c_light_col[0].y) * c_mat_specular.y;
   float _232 = _231 * _230;
   float _233 = c_specular_scale.z * _226;
   float _234 = (c_light_col[0].z) * c_mat_specular.z;
   float _235 = _234 * _233;
   int _236 = c_bool & 1;
   bool _237 = (_236 == 0);
   if (!_237)
   {
      float _239 = -0.0f - TEXCOORD_4.x;
      float _240 = -0.0f - TEXCOORD_4.y;
      float _241 = -0.0f - TEXCOORD_4.z;
      float _242 = dot(float3(_89, _90, _91), float3(_239, _240, _241));
      float _243 = saturate(_242);
      float _244 = _243 * TEXCOORD_1.w;
      float _245 = _243 * TEXCOORD_2.w;
      float _246 = _243 * TEXCOORD_4.w;
      float _247 = max(0.0f, _244);
      float _248 = max(0.0f, _245);
      float _249 = max(0.0f, _246);
      float _250 = -0.0f - TEXCOORD_5.x;
      float _251 = -0.0f - TEXCOORD_5.y;
      float _252 = -0.0f - TEXCOORD_5.z;
      float _253 = dot(float3(_89, _90, _91), float3(_250, _251, _252));
      float _254 = saturate(_253);
      float _255 = _254 * COLOR.w;
      float _256 = _254 * COLOR_1.w;
      float _257 = _254 * TEXCOORD_5.w;
      float _258 = max(0.0f, _255);
      float _259 = max(0.0f, _256);
      float _260 = max(0.0f, _257);
      float _261 = _258 + _247;
      float _262 = _259 + _248;
      float _263 = _260 + _249;
      _265 = _261;
      _266 = _262;
      _267 = _263;
   }
   else
   {
      _265 = 0.0f;
      _266 = 0.0f;
      _267 = 0.0f;
   }
   float _268 = saturate(_229);
   float _269 = saturate(_232);
   float _270 = saturate(_235);
   float _333 = dot(float3(_89, _90, _91), float3(_89, _90, _91));
   float _334 = rsqrt(_333);
   float _335 = _334 * _89;
   float _336 = _334 * _90;
   float _337 = _334 * _91;
   float _338 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_335, _336, _337, 1.0f));
   float _339 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_335, _336, _337, 1.0f));
   float _340 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_335, _336, _337, 1.0f));
   float _341 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_335, _336, _337, 1.0f));
   float _342 = dot(float4(_335, _336, _337, 1.0f), float4(_338, _339, _340, _341));
   float _343 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_335, _336, _337, 1.0f));
   float _344 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_335, _336, _337, 1.0f));
   float _345 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_335, _336, _337, 1.0f));
   float _346 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_335, _336, _337, 1.0f));
   float _347 = dot(float4(_335, _336, _337, 1.0f), float4(_343, _344, _345, _346));
   float _348 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_335, _336, _337, 1.0f));
   float _349 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_335, _336, _337, 1.0f));
   float _350 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_335, _336, _337, 1.0f));
   float _351 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_335, _336, _337, 1.0f));
   float _352 = dot(float4(_335, _336, _337, 1.0f), float4(_348, _349, _350, _351));
   float _353 = max(_342, 0.0625f);
   float _354 = max(_347, 0.0625f);
   float _355 = max(_352, 0.0625f);
   float _356 = _353 * (c_ps_scratch[79].x);
   float _357 = _354 * (c_ps_scratch[79].x);
   float _358 = _355 * (c_ps_scratch[79].x);
   float _363 = -0.0f - (c_light_vec[0].x);
   float _364 = -0.0f - (c_light_vec[0].y);
   float _365 = -0.0f - (c_light_vec[0].z);
   float _366 = dot(float3(_89, _90, _91), float3(_363, _364, _365));
   float _367 = -0.0f - (c_light_vec[1].x);
   float _368 = -0.0f - (c_light_vec[1].y);
   float _369 = -0.0f - (c_light_vec[1].z);
   float _370 = dot(float3(_89, _90, _91), float3(_367, _368, _369));
   float _379 = _90 * 0.5f;
   float _380 = _379 + 0.5f;
   float _381 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _382 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _383 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _384 = _381 * _380;
   float _385 = _382 * _380;
   float _386 = _383 * _380;
   float _387 = saturate(_366);
   float _388 = saturate(_370);
   float _389 = _387 * (c_light_col[0].x);
   float _390 = _387 * (c_light_col[0].y);
   float _391 = _387 * (c_light_col[0].z);
   float _396 = (c_light_col[1].x) * _388;
   float _397 = (c_light_col[1].y) * _388;
   float _398 = (c_light_col[1].z) * _388;
   float _399 = _389 * _180;
   float _400 = _390 * _180;
   float _401 = _391 * _180;
   float _402 = (c_light_vec[4].x) + _356;
   float _403 = _402 + _384;
   float _404 = _399 + _265;
   float _405 = _404 + _403;
   float _406 = _405 + _396;
   float _407 = (c_light_vec[4].y) + _357;
   float _408 = _407 + _385;
   float _409 = _400 + _266;
   float _410 = _409 + _408;
   float _411 = _410 + _397;
   float _412 = (c_light_vec[4].z) + _358;
   float _413 = _412 + _386;
   float _414 = _401 + _267;
   float _415 = _414 + _413;
   float _416 = _415 + _398;
   float _417 = max(_406, _411);
   float _418 = max(_417, _416);
   float _420 = _418 + (c_ps_user[6].z);
   float _421 = saturate(_420);
   float _422 = _421 * TEXCOORD.z;
   float _423 = _422 * _268;
   float _424 = _422 * _269;
   float _425 = _422 * _270;
   int _426 = c_bool & 32768;
   bool _427 = (_426 == 0);
   if (!_427)
   {
      float _439 = (c_ps_user[11].x) * _148;
      float _440 = (c_ps_user[11].x) * _149;
      float _441 = (c_ps_user[11].x) * _150;
      float _443 = _89 + 1.0f;
      float _444 = _90 + 1.0f;
      float _445 = _91 + 1.0f;
      float _446 = _443 * 0.5f;
      float _447 = _444 * 0.5f;
      float _448 = _445 * 0.5f;
      float _449 = saturate(_446);
      float _450 = saturate(_447);
      float _451 = saturate(_448);
      float _452 = (c_ps_user[11].y) * _449;
      float _453 = _450 * (c_ps_user[11].y);
      float _454 = _451 * (c_ps_user[11].y);
      float _457 = (c_ps_user[11].w) * _423;
      float _460 = (c_ps_user[12].x) * _180;
      float _466 = -0.0f - _98;
      float _467 = -0.0f - _99;
      float _468 = -0.0f - _100;
      float _469 = dot(float3(_466, _467, _468), float3(_89, _90, _91));
      float _470 = 1.0f - _469;
      float _471 = _470 * _470;
      float _472 = _470 * 0.9722222089767456f;
      float _473 = _471 * _471;
      float _474 = _473 * _472;
      float _475 = _474 + 0.02777777425944805f;
      float _476 = _475 * (c_ps_user[13].z);
      float _478 = (c_ps_user[13].w) * _403;
      float _479 = (c_ps_user[13].w) * _408;
      float _480 = (c_ps_user[13].w) * _413;
      float _483 = (c_ps_user[14].y) * _389;
      float _484 = (c_ps_user[14].y) * _390;
      float _485 = (c_ps_user[14].y) * _391;
      float _487 = (c_ps_user[14].z) * _396;
      float _488 = (c_ps_user[14].z) * _397;
      float _489 = (c_ps_user[14].z) * _398;
      float _491 = (c_ps_user[14].w) * _265;
      float _492 = (c_ps_user[14].w) * _266;
      float _493 = (c_ps_user[14].w) * _267;
      float _497 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _498 = c_specular_scale.x * _497;
      float _499 = _498 * _423;
      float _500 = _483 + (c_ps_user[13].x);
      float _501 = _500 + _487;
      float _502 = c_diffuse_scale.x * _501;
      float _503 = _502 * c_mat_diffuse.x;
      float _504 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _505 = _504 * c_specular_scale.x;
      float _506 = _505 + _499;
      float _507 = _506 * c_mat_specular.x;
      float _508 = _452 + _439;
      float _509 = _508 + _457;
      float _510 = _509 + (c_ps_user[12].y);
      float _511 = _510 + _460;
      float _512 = _511 + _478;
      float _513 = _512 + _491;
      float _514 = _513 + _476;
      float _515 = _514 + _503;
      float _516 = _515 + _507;
      float _517 = c_specular_scale.y * _497;
      float _518 = _517 * _424;
      float _519 = _484 + (c_ps_user[13].x);
      float _520 = _519 + _488;
      float _521 = c_diffuse_scale.y * _520;
      float _522 = _521 * c_mat_diffuse.y;
      float _523 = _504 * c_specular_scale.y;
      float _524 = _523 + _518;
      float _525 = _524 * c_mat_specular.y;
      float _526 = _457 + _440;
      float _527 = _526 + _453;
      float _528 = _527 + (c_ps_user[12].y);
      float _529 = _528 + _460;
      float _530 = _529 + _479;
      float _531 = _530 + _492;
      float _532 = _531 + _476;
      float _533 = _532 + _522;
      float _534 = _533 + _525;
      float _535 = c_specular_scale.z * _497;
      float _536 = _535 * _425;
      float _537 = _485 + (c_ps_user[13].x);
      float _538 = _537 + _489;
      float _539 = c_diffuse_scale.z * _538;
      float _540 = _539 * c_mat_diffuse.z;
      float _541 = _504 * c_specular_scale.z;
      float _542 = _541 + _536;
      float _543 = _542 * c_mat_specular.z;
      float _544 = _457 + _441;
      float _545 = _544 + _454;
      float _546 = _545 + (c_ps_user[12].y);
      float _547 = _546 + _460;
      float _548 = _547 + _480;
      float _549 = _548 + _493;
      float _550 = _549 + _476;
      float _551 = _550 + _540;
      float _552 = _551 + _543;
      float _553 = saturate(_516);
      float _554 = saturate(_534);
      float _555 = saturate(_552);
      _570 = _553;
      _571 = _554;
      _572 = _555;
      _573 = 1.0f;
   }
   else
   {
      float _557 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _558 = _557 * _148;
      float _559 = _558 * _406;
      float _560 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _561 = _560 * _149;
      float _562 = _561 * _411;
      float _563 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _564 = _563 * _150;
      float _565 = _564 * _416;
      float _566 = _559 + _423;
      float _567 = _562 + _424;
      float _568 = _565 + _425;
      _570 = _566;
      _571 = _567;
      _572 = _568;
      _573 = _423;
   }
   float _577 = c_opacity_scale * c_mat_opacity;
   float _579 = _577 * c_global_opacity;
   float _580 = 1.0f - _579;
   float _581 = saturate(_573);
   float _582 = _580 * _581;
   float _583 = _582 + _579;
   float _584 = _570 * COLOR.x;
   float _585 = _571 * COLOR.y;
   float _586 = _572 * COLOR.z;
   int _587 = c_bool & 16;
   bool _588 = (_587 == 0);
   if (!_588)
   {
      float _592 = c_tone_control.x * _584;
      float _593 = c_tone_control.x * _585;
      float _594 = c_tone_control.x * _586;
      float _596 = log2(_592);
      float _597 = log2(_593);
      float _598 = log2(_594);
      float _599 = _596 * c_tone_control.y;
      float _600 = _597 * c_tone_control.y;
      float _601 = _598 * c_tone_control.y;
      float _602 = exp2(_599);
      float _603 = exp2(_600);
      float _604 = exp2(_601);
      _652 = _602;
      _653 = _603;
      _654 = _604;
   }
   else
   {
      if (_427)
      {
         float _609 = c_tone_control.x * _584;
         float _610 = c_tone_control.x * _585;
         float _611 = c_tone_control.x * _586;
         float _612 = _609 * 2.0f;
         float _613 = _610 * 2.0f;
         float _614 = _611 * 2.0f;
         float _615 = _609 * 0.30000001192092896f;
         float _616 = _610 * 0.30000001192092896f;
         float _617 = _611 * 0.30000001192092896f;
         float _618 = _615 + 0.05000000074505806f;
         float _619 = _616 + 0.05000000074505806f;
         float _620 = _617 + 0.05000000074505806f;
         float _621 = _618 * _612;
         float _622 = _619 * _613;
         float _623 = _620 * _614;
         float _624 = _621 + 0.004000000189989805f;
         float _625 = _622 + 0.004000000189989805f;
         float _626 = _623 + 0.004000000189989805f;
         float _627 = _615 + 0.5f;
         float _628 = _616 + 0.5f;
         float _629 = _617 + 0.5f;
         float _630 = _627 * _612;
         float _631 = _628 * _613;
         float _632 = _629 * _614;
         float _633 = _630 + 0.06000000238418579f;
         float _634 = _631 + 0.06000000238418579f;
         float _635 = _632 + 0.06000000238418579f;
         float _636 = _624 / _633;
         float _637 = _625 / _634;
         float _638 = _626 / _635;
         float _639 = _636 + -0.06666666269302368f;
         float _640 = _637 + -0.06666666269302368f;
         float _641 = _638 + -0.06666666269302368f;
         float _642 = _639 * 1.3790643215179443f;
         float _643 = _640 * 1.3790643215179443f;
         float _644 = _641 * 1.3790643215179443f;
         float _645 = YKR_MaterialCurve(_642, _609);
         float _646 = YKR_MaterialCurve(_643, _610);
         float _647 = YKR_MaterialCurve(_644, _611);
         float _648 = sqrt(_645);
         float _649 = sqrt(_646);
         float _650 = sqrt(_647);
         _652 = _648;
         _653 = _649;
         _654 = _650;
      }
      else
      {
         _652 = _584;
         _653 = _585;
         _654 = _586;
      }
   }
   if (!_427)
   {
      float _662 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _663 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _664 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _666 = (c_ps_user[12].w) * COLOR.x;
      float _667 = (c_ps_user[12].w) * COLOR.y;
      float _668 = (c_ps_user[12].w) * COLOR.z;
      float _669 = _666 + _652;
      float _670 = _669 + _662;
      float _671 = _667 + _653;
      float _672 = _671 + _663;
      float _673 = _668 + _654;
      float _674 = _673 + _664;
      _676 = _670;
      _677 = _672;
      _678 = _674;
   }
   else
   {
      _676 = _652;
      _677 = _653;
      _678 = _654;
   }
   float _681 = _583 - c_mat_punchthrough_ref;
   bool _682 = (_681 < 0.0f);
   if (_682)
      discard;
   float _688 = (c_ps_col_scl[0].x) * _676;
   float _689 = (c_ps_col_scl[0].y) * _677;
   float _690 = (c_ps_col_scl[0].z) * _678;
   float _691 = (c_ps_col_scl[0].w) * _583;
   SV_Target.x = _688;
   SV_Target.y = _689;
   SV_Target.z = _690;
   SV_Target.w = _691;
   int _694 = c_bool & 8;
   bool _695 = (_694 != 0);
   float _696 = select(_695, _676, _688);
   float _697 = select(_695, _677, _689);
   float _698 = select(_695, _678, _690);
   float _699 = select(_695, _583, _691);
   SV_Target.x = _696;
   SV_Target.y = _697;
   SV_Target.z = _698;
   SV_Target.w = _699;
   return SV_Target;
}
