// Yakuza Kiwami material s_a1[ref][eye] (0x823DCD62, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

TextureCube<float4> s1c_t : register(t1);

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

SamplerState s13_s : register(s13);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float _43 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _44 = rsqrt(_43);
   float _45 = _44 * TEXCOORD_2.x;
   float _46 = _44 * TEXCOORD_2.y;
   float _47 = _44 * TEXCOORD_2.z;
   float _52 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _53 = rsqrt(_52);
   float _54 = _53 * TEXCOORD_3.x;
   float _55 = _53 * TEXCOORD_3.y;
   float _56 = _53 * TEXCOORD_3.z;
   float _61 = (c_ps_user[4].x) + TEXCOORD_1.x;
   float _62 = (c_ps_user[4].y) + TEXCOORD_1.y;
   float _63 = (c_ps_user[4].z) + TEXCOORD_1.z;
   float _64 = dot(float3(_61, _62, _63), float3(_61, _62, _63));
   float _65 = rsqrt(_64);
   float _70 = (c_ps_user[5].x) + TEXCOORD_1.x;
   float _71 = (c_ps_user[5].y) + TEXCOORD_1.y;
   float _72 = (c_ps_user[5].z) + TEXCOORD_1.z;
   float _73 = dot(float3(_70, _71, _72), float3(_70, _71, _72));
   float _74 = rsqrt(_73);
   float _75 = dot(float3(_54, _55, _56), float3(_45, _46, _47));
   float _76 = _75 * 2.0f;
   float _77 = _76 * _45;
   float _78 = _76 * _46;
   float _79 = _76 * _47;
   float _80 = _54 - _77;
   float _81 = _55 - _78;
   float _82 = _56 - _79;
   float _83 = dot(float3(_80, _81, _82), float3(_80, _81, _82));
   float _84 = rsqrt(_83);
   float _85 = _81 * _84;
   float _86 = _82 * _84;
   float _87 = _84 * _80;
   float _88 = -0.0f - _87;
   float4 _89 = s1c_t.Sample(s1c_s, float3(_88, _85, _86));
   float _94 = max(_89.w, 0.003921568859368563f);
   float _95 = 0.25f / _94;
   float _96 = _95 * _89.x;
   float _97 = _95 * _89.y;
   float _98 = _95 * _89.z;
   float _99 = _96 * _96;
   float _100 = _97 * _97;
   float _101 = _98 * _98;
   float _104 = _99 * (c_ps_user[6].w);
   float _105 = _100 * (c_ps_user[6].w);
   float _106 = _101 * (c_ps_user[6].w);
   float _110 = c_rt_size.z * SV_Position.x;
   float _111 = c_rt_size.w * SV_Position.y;
   int _114 = c_bool & 8;
   bool _115 = (_114 == 0);
   float _121;
   float _122;
   float _134;
   float _135;
   float _221;
   float _222;
   float _223;
   float _526;
   float _527;
   float _528;
   float _529;
   float _608;
   float _609;
   float _610;
   float _632;
   float _633;
   float _634;
   if (!_115)
   {
      float4 _117 = s13_t.Sample(s13_s, float2(_110, _111));
      _121 = _117.x;
      _122 = _117.y;
   }
   else
   {
      _121 = 1.0f;
      _122 = 1.0f;
   }
   if (!_115)
   {
      float _127 = (c_sdw_param_ch[0].x) * _121;
      float _128 = (c_sdw_param_ch[0].z) * _122;
      float _131 = _127 + (c_sdw_param_ch[0].y);
      float _132 = _128 + (c_sdw_param_ch[0].w);
      _134 = _131;
      _135 = _132;
   }
   else
   {
      _134 = _121;
      _135 = _122;
   }
   float _136 = min(_134, _135);
   float _137 = _70 * _74;
   float _138 = -0.0f - _137;
   float _139 = _71 * _74;
   float _140 = -0.0f - _139;
   float _141 = _72 * _74;
   float _142 = -0.0f - _141;
   float _143 = _61 * _65;
   float _144 = -0.0f - _143;
   float _145 = _62 * _65;
   float _146 = -0.0f - _145;
   float _147 = _63 * _65;
   float _148 = -0.0f - _147;
   float _155 = (c_ps_user[6].x) * c_mat_power;
   float _156 = dot(float3(_144, _146, _148), float3(_45, _46, _47));
   float _157 = saturate(_156);
   float _158 = log2(_157);
   float _159 = _155 * _158;
   float _160 = exp2(_159);
   float _171 = (c_ps_user[4].w) * _160;
   float _173 = (c_ps_user[6].y) * c_mat_power;
   float _174 = dot(float3(_138, _140, _142), float3(_45, _46, _47));
   float _175 = saturate(_174);
   float _176 = log2(_175);
   float _177 = _173 * _176;
   float _178 = exp2(_177);
   float _181 = (c_ps_user[5].w) * _178;
   float _182 = _181 + _171;
   float _183 = c_specular_scale.x * _182;
   float _184 = (c_light_col[0].x) * c_mat_specular.x;
   float _185 = _184 * _183;
   float _186 = c_specular_scale.y * _182;
   float _187 = (c_light_col[0].y) * c_mat_specular.y;
   float _188 = _187 * _186;
   float _189 = c_specular_scale.z * _182;
   float _190 = (c_light_col[0].z) * c_mat_specular.z;
   float _191 = _190 * _189;
   int _192 = c_bool & 1;
   bool _193 = (_192 == 0);
   if (!_193)
   {
      float _195 = -0.0f - TEXCOORD_4.x;
      float _196 = -0.0f - TEXCOORD_4.y;
      float _197 = -0.0f - TEXCOORD_4.z;
      float _198 = dot(float3(_45, _46, _47), float3(_195, _196, _197));
      float _199 = saturate(_198);
      float _200 = _199 * TEXCOORD_1.w;
      float _201 = _199 * TEXCOORD_2.w;
      float _202 = _199 * TEXCOORD_4.w;
      float _203 = max(0.0f, _200);
      float _204 = max(0.0f, _201);
      float _205 = max(0.0f, _202);
      float _206 = -0.0f - TEXCOORD_5.x;
      float _207 = -0.0f - TEXCOORD_5.y;
      float _208 = -0.0f - TEXCOORD_5.z;
      float _209 = dot(float3(_45, _46, _47), float3(_206, _207, _208));
      float _210 = saturate(_209);
      float _211 = _210 * COLOR.w;
      float _212 = _210 * COLOR_1.w;
      float _213 = _210 * TEXCOORD_5.w;
      float _214 = max(0.0f, _211);
      float _215 = max(0.0f, _212);
      float _216 = max(0.0f, _213);
      float _217 = _214 + _203;
      float _218 = _215 + _204;
      float _219 = _216 + _205;
      _221 = _217;
      _222 = _218;
      _223 = _219;
   }
   else
   {
      _221 = 0.0f;
      _222 = 0.0f;
      _223 = 0.0f;
   }
   float _224 = saturate(_185);
   float _225 = saturate(_188);
   float _226 = saturate(_191);
   float _289 = dot(float3(_45, _46, _47), float3(_45, _46, _47));
   float _290 = rsqrt(_289);
   float _291 = _290 * _45;
   float _292 = _290 * _46;
   float _293 = _290 * _47;
   float _294 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_291, _292, _293, 1.0f));
   float _295 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_291, _292, _293, 1.0f));
   float _296 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_291, _292, _293, 1.0f));
   float _297 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_291, _292, _293, 1.0f));
   float _298 = dot(float4(_291, _292, _293, 1.0f), float4(_294, _295, _296, _297));
   float _299 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_291, _292, _293, 1.0f));
   float _300 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_291, _292, _293, 1.0f));
   float _301 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_291, _292, _293, 1.0f));
   float _302 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_291, _292, _293, 1.0f));
   float _303 = dot(float4(_291, _292, _293, 1.0f), float4(_299, _300, _301, _302));
   float _304 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_291, _292, _293, 1.0f));
   float _305 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_291, _292, _293, 1.0f));
   float _306 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_291, _292, _293, 1.0f));
   float _307 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_291, _292, _293, 1.0f));
   float _308 = dot(float4(_291, _292, _293, 1.0f), float4(_304, _305, _306, _307));
   float _309 = max(_298, 0.0625f);
   float _310 = max(_303, 0.0625f);
   float _311 = max(_308, 0.0625f);
   float _312 = _309 * (c_ps_scratch[79].x);
   float _313 = _310 * (c_ps_scratch[79].x);
   float _314 = _311 * (c_ps_scratch[79].x);
   float _319 = -0.0f - (c_light_vec[0].x);
   float _320 = -0.0f - (c_light_vec[0].y);
   float _321 = -0.0f - (c_light_vec[0].z);
   float _322 = dot(float3(_45, _46, _47), float3(_319, _320, _321));
   float _323 = -0.0f - (c_light_vec[1].x);
   float _324 = -0.0f - (c_light_vec[1].y);
   float _325 = -0.0f - (c_light_vec[1].z);
   float _326 = dot(float3(_45, _46, _47), float3(_323, _324, _325));
   float _335 = _46 * 0.5f;
   float _336 = _335 + 0.5f;
   float _337 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _338 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _339 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _340 = _337 * _336;
   float _341 = _338 * _336;
   float _342 = _339 * _336;
   float _343 = saturate(_322);
   float _344 = saturate(_326);
   float _345 = _343 * (c_light_col[0].x);
   float _346 = _343 * (c_light_col[0].y);
   float _347 = _343 * (c_light_col[0].z);
   float _352 = (c_light_col[1].x) * _344;
   float _353 = (c_light_col[1].y) * _344;
   float _354 = (c_light_col[1].z) * _344;
   float _355 = _345 * _136;
   float _356 = _346 * _136;
   float _357 = _347 * _136;
   float _358 = (c_light_vec[4].x) + _312;
   float _359 = _358 + _340;
   float _360 = _355 + _221;
   float _361 = _360 + _359;
   float _362 = _361 + _352;
   float _363 = (c_light_vec[4].y) + _313;
   float _364 = _363 + _341;
   float _365 = _356 + _222;
   float _366 = _365 + _364;
   float _367 = _366 + _353;
   float _368 = (c_light_vec[4].z) + _314;
   float _369 = _368 + _342;
   float _370 = _357 + _223;
   float _371 = _370 + _369;
   float _372 = _371 + _354;
   float _373 = max(_362, _367);
   float _374 = max(_373, _372);
   float _376 = _374 + (c_ps_user[6].z);
   float _377 = saturate(_376);
   float _378 = _377 * TEXCOORD.z;
   float _379 = _378 * _224;
   float _380 = _378 * _225;
   float _381 = _378 * _226;
   int _382 = c_bool & 32768;
   bool _383 = (_382 == 0);
   if (!_383)
   {
      float _395 = (c_ps_user[11].x) * _104;
      float _396 = (c_ps_user[11].x) * _105;
      float _397 = (c_ps_user[11].x) * _106;
      float _399 = _45 + 1.0f;
      float _400 = _46 + 1.0f;
      float _401 = _47 + 1.0f;
      float _402 = _399 * 0.5f;
      float _403 = _400 * 0.5f;
      float _404 = _401 * 0.5f;
      float _405 = saturate(_402);
      float _406 = saturate(_403);
      float _407 = saturate(_404);
      float _408 = (c_ps_user[11].y) * _405;
      float _409 = _406 * (c_ps_user[11].y);
      float _410 = _407 * (c_ps_user[11].y);
      float _413 = (c_ps_user[11].w) * _379;
      float _416 = (c_ps_user[12].x) * _136;
      float _422 = -0.0f - _54;
      float _423 = -0.0f - _55;
      float _424 = -0.0f - _56;
      float _425 = dot(float3(_422, _423, _424), float3(_45, _46, _47));
      float _426 = 1.0f - _425;
      float _427 = _426 * _426;
      float _428 = _426 * 0.9722222089767456f;
      float _429 = _427 * _427;
      float _430 = _429 * _428;
      float _431 = _430 + 0.02777777425944805f;
      float _432 = _431 * (c_ps_user[13].z);
      float _434 = (c_ps_user[13].w) * _359;
      float _435 = (c_ps_user[13].w) * _364;
      float _436 = (c_ps_user[13].w) * _369;
      float _439 = (c_ps_user[14].y) * _345;
      float _440 = (c_ps_user[14].y) * _346;
      float _441 = (c_ps_user[14].y) * _347;
      float _443 = (c_ps_user[14].z) * _352;
      float _444 = (c_ps_user[14].z) * _353;
      float _445 = (c_ps_user[14].z) * _354;
      float _447 = (c_ps_user[14].w) * _221;
      float _448 = (c_ps_user[14].w) * _222;
      float _449 = (c_ps_user[14].w) * _223;
      float _453 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _454 = c_specular_scale.x * _453;
      float _455 = _454 * _379;
      float _456 = _439 + (c_ps_user[13].x);
      float _457 = _456 + _443;
      float _458 = c_diffuse_scale.x * _457;
      float _459 = _458 * c_mat_diffuse.x;
      float _460 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _461 = _460 * c_specular_scale.x;
      float _462 = _461 + _455;
      float _463 = _462 * c_mat_specular.x;
      float _464 = _408 + _395;
      float _465 = _464 + _413;
      float _466 = _465 + (c_ps_user[12].y);
      float _467 = _466 + _416;
      float _468 = _467 + _434;
      float _469 = _468 + _447;
      float _470 = _469 + _432;
      float _471 = _470 + _459;
      float _472 = _471 + _463;
      float _473 = c_specular_scale.y * _453;
      float _474 = _473 * _380;
      float _475 = _440 + (c_ps_user[13].x);
      float _476 = _475 + _444;
      float _477 = c_diffuse_scale.y * _476;
      float _478 = _477 * c_mat_diffuse.y;
      float _479 = _460 * c_specular_scale.y;
      float _480 = _479 + _474;
      float _481 = _480 * c_mat_specular.y;
      float _482 = _413 + _396;
      float _483 = _482 + _409;
      float _484 = _483 + (c_ps_user[12].y);
      float _485 = _484 + _416;
      float _486 = _485 + _435;
      float _487 = _486 + _448;
      float _488 = _487 + _432;
      float _489 = _488 + _478;
      float _490 = _489 + _481;
      float _491 = c_specular_scale.z * _453;
      float _492 = _491 * _381;
      float _493 = _441 + (c_ps_user[13].x);
      float _494 = _493 + _445;
      float _495 = c_diffuse_scale.z * _494;
      float _496 = _495 * c_mat_diffuse.z;
      float _497 = _460 * c_specular_scale.z;
      float _498 = _497 + _492;
      float _499 = _498 * c_mat_specular.z;
      float _500 = _413 + _397;
      float _501 = _500 + _410;
      float _502 = _501 + (c_ps_user[12].y);
      float _503 = _502 + _416;
      float _504 = _503 + _436;
      float _505 = _504 + _449;
      float _506 = _505 + _432;
      float _507 = _506 + _496;
      float _508 = _507 + _499;
      float _509 = saturate(_472);
      float _510 = saturate(_490);
      float _511 = saturate(_508);
      _526 = _509;
      _527 = _510;
      _528 = _511;
      _529 = 1.0f;
   }
   else
   {
      float _513 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _514 = _513 * _104;
      float _515 = _514 * _362;
      float _516 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _517 = _516 * _105;
      float _518 = _517 * _367;
      float _519 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _520 = _519 * _106;
      float _521 = _520 * _372;
      float _522 = _515 + _379;
      float _523 = _518 + _380;
      float _524 = _521 + _381;
      _526 = _522;
      _527 = _523;
      _528 = _524;
      _529 = _379;
   }
   float _533 = c_opacity_scale * c_mat_opacity;
   float _535 = _533 * c_global_opacity;
   float _536 = 1.0f - _535;
   float _537 = saturate(_529);
   float _538 = _536 * _537;
   float _539 = _538 + _535;
   float _540 = _526 * COLOR.x;
   float _541 = _527 * COLOR.y;
   float _542 = _528 * COLOR.z;
   int _543 = c_bool & 16;
   bool _544 = (_543 == 0);
   if (!_544)
   {
      float _548 = c_tone_control.x * _540;
      float _549 = c_tone_control.x * _541;
      float _550 = c_tone_control.x * _542;
      float _552 = log2(_548);
      float _553 = log2(_549);
      float _554 = log2(_550);
      float _555 = _552 * c_tone_control.y;
      float _556 = _553 * c_tone_control.y;
      float _557 = _554 * c_tone_control.y;
      float _558 = exp2(_555);
      float _559 = exp2(_556);
      float _560 = exp2(_557);
      _608 = _558;
      _609 = _559;
      _610 = _560;
   }
   else
   {
      if (_383)
      {
         float _565 = c_tone_control.x * _540;
         float _566 = c_tone_control.x * _541;
         float _567 = c_tone_control.x * _542;
         float _568 = _565 * 2.0f;
         float _569 = _566 * 2.0f;
         float _570 = _567 * 2.0f;
         float _571 = _565 * 0.30000001192092896f;
         float _572 = _566 * 0.30000001192092896f;
         float _573 = _567 * 0.30000001192092896f;
         float _574 = _571 + 0.05000000074505806f;
         float _575 = _572 + 0.05000000074505806f;
         float _576 = _573 + 0.05000000074505806f;
         float _577 = _574 * _568;
         float _578 = _575 * _569;
         float _579 = _576 * _570;
         float _580 = _577 + 0.004000000189989805f;
         float _581 = _578 + 0.004000000189989805f;
         float _582 = _579 + 0.004000000189989805f;
         float _583 = _571 + 0.5f;
         float _584 = _572 + 0.5f;
         float _585 = _573 + 0.5f;
         float _586 = _583 * _568;
         float _587 = _584 * _569;
         float _588 = _585 * _570;
         float _589 = _586 + 0.06000000238418579f;
         float _590 = _587 + 0.06000000238418579f;
         float _591 = _588 + 0.06000000238418579f;
         float _592 = _580 / _589;
         float _593 = _581 / _590;
         float _594 = _582 / _591;
         float _595 = _592 + -0.06666666269302368f;
         float _596 = _593 + -0.06666666269302368f;
         float _597 = _594 + -0.06666666269302368f;
         float _598 = _595 * 1.3790643215179443f;
         float _599 = _596 * 1.3790643215179443f;
         float _600 = _597 * 1.3790643215179443f;
         float _601 = YKR_MaterialCurve(_598, _565);
         float _602 = YKR_MaterialCurve(_599, _566);
         float _603 = YKR_MaterialCurve(_600, _567);
         float _604 = sqrt(_601);
         float _605 = sqrt(_602);
         float _606 = sqrt(_603);
         _608 = _604;
         _609 = _605;
         _610 = _606;
      }
      else
      {
         _608 = _540;
         _609 = _541;
         _610 = _542;
      }
   }
   if (!_383)
   {
      float _618 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _619 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _620 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _622 = (c_ps_user[12].w) * COLOR.x;
      float _623 = (c_ps_user[12].w) * COLOR.y;
      float _624 = (c_ps_user[12].w) * COLOR.z;
      float _625 = _622 + _608;
      float _626 = _625 + _618;
      float _627 = _623 + _609;
      float _628 = _627 + _619;
      float _629 = _624 + _610;
      float _630 = _629 + _620;
      _632 = _626;
      _633 = _628;
      _634 = _630;
   }
   else
   {
      _632 = _608;
      _633 = _609;
      _634 = _610;
   }
   float _637 = _539 - c_mat_punchthrough_ref;
   bool _638 = (_637 < 0.0f);
   if (_638)
      discard;
   float _644 = (c_ps_col_scl[0].x) * _632;
   float _645 = (c_ps_col_scl[0].y) * _633;
   float _646 = (c_ps_col_scl[0].z) * _634;
   float _647 = (c_ps_col_scl[0].w) * _539;
   SV_Target.x = _644;
   SV_Target.y = _645;
   SV_Target.z = _646;
   SV_Target.w = _647;
   int _650 = c_bool & 8;
   bool _651 = (_650 != 0);
   float _652 = select(_651, _632, _644);
   float _653 = select(_651, _633, _645);
   float _654 = select(_651, _634, _646);
   float _655 = select(_651, _539, _647);
   SV_Target.x = _652;
   SV_Target.y = _653;
   SV_Target.z = _654;
   SV_Target.w = _655;
   return SV_Target;
}
