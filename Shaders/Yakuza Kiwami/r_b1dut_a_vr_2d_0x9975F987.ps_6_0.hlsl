// Yakuza Kiwami material r_b1dut_a(vr)2d (0x9975F987, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _51 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _51.x * _51.x;
   float _57 = _51.y * _51.y;
   float _58 = _51.z * _51.z;
   float _62 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _63 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _64 = s3_t.Sample(s3_s, float2(_62, _63));
   float _69 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _70 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _71 = s5_t.Sample(s5_s, float2(_69, _70));
   float4 _74 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _78 = _71.x * 2.0f;
   float _79 = _71.y * 2.0f;
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
   float _195;
   float _196;
   float _197;
   float _198;
   float _212;
   float _213;
   float _248;
   float _249;
   float _250;
   float _284;
   float _285;
   float _286;
   float _287;
   float _288;
   float _289;
   float _443;
   float _444;
   float _445;
   float _446;
   float _447;
   float _448;
   float _620;
   float _621;
   float _622;
   float _623;
   float _624;
   float _625;
   float _711;
   float _712;
   float _713;
   float _729;
   float _730;
   float _731;
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
   float _163 = c_rt_size.z * SV_Position.x;
   float _164 = c_rt_size.w * SV_Position.y;
   float4 _165 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_163, _164)));
   int _172 = c_bool & 16384;
   bool _173 = (_172 == 0);
   if (!_173)
   {
      float _175 = _165.x * 2.0f;
      float _176 = _165.y * 2.0f;
      float _177 = _165.z * 2.0f;
      float _178 = _165.w * 2.0f;
      _195 = _175;
      _196 = _176;
      _197 = _177;
      _198 = _178;
   }
   else
   {
      int _180 = c_bool & 8192;
      bool _181 = (_180 == 0);
      if (!_181)
      {
         float _183 = _165.x * 2.0f;
         float _184 = _165.y * 2.0f;
         float _185 = _165.z * 2.0f;
         float _186 = _165.w * 2.0f;
         _195 = _183;
         _196 = _184;
         _197 = _185;
         _198 = _186;
      }
      else
      {
         float _190 = _165.x / c_tone_control.z;
         float _191 = _165.y / c_tone_control.z;
         float _192 = _165.z / c_tone_control.z;
         float _193 = _165.w / c_tone_control.z;
         _195 = _190;
         _196 = _191;
         _197 = _192;
         _198 = _193;
      }
   }
   int _199 = c_bool & 8;
   bool _200 = (_199 == 0);
   if (!_200)
   {
      float _205 = (c_sdw_param[0].x) * _158;
      float _206 = (c_sdw_param[0].z) * _159;
      float _209 = _205 + (c_sdw_param[0].y);
      float _210 = _206 + (c_sdw_param[0].w);
      _212 = _209;
      _213 = _210;
   }
   else
   {
      _212 = _158;
      _213 = _159;
   }
   float _214 = _213 * _212;
   float _219 = 1.0f - (c_light_col[5].x);
   float _220 = 1.0f - (c_light_col[5].y);
   float _221 = 1.0f - (c_light_col[5].z);
   float _222 = _219 * _214;
   float _223 = _220 * _214;
   float _224 = _221 * _214;
   float _225 = _222 + (c_light_col[5].x);
   float _226 = _223 + (c_light_col[5].y);
   float _227 = _224 + (c_light_col[5].z);
   float _228 = _146 * COLOR.x;
   float _229 = _146 * COLOR.y;
   float _230 = _146 * COLOR.z;
   if (!_200)
   {
      float _234 = (c_sdw_param[2].x) * _225;
      float _235 = (c_sdw_param[2].x) * _226;
      float _236 = (c_sdw_param[2].x) * _227;
      float _238 = _234 + (c_sdw_param[2].y);
      float _239 = _235 + (c_sdw_param[2].y);
      float _240 = _236 + (c_sdw_param[2].y);
      float _243 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _244 = _238 * _243;
      float _245 = _239 * _243;
      float _246 = _240 * _243;
      _248 = _244;
      _249 = _245;
      _250 = _246;
   }
   else
   {
      _248 = _225;
      _249 = _226;
      _250 = _227;
   }
   float _251 = _248 * _146;
   float _252 = _249 * _146;
   float _253 = _250 * _146;
   float _264 = c_mat_emissive.x * _56;
   float _265 = _264 * c_emissive_scale;
   float _266 = c_mat_emissive.y * _57;
   float _267 = _266 * c_emissive_scale;
   float _268 = c_mat_emissive.z * _58;
   float _269 = _268 * c_emissive_scale;
   int _270 = c_bool & 2048;
   bool _271 = (_270 == 0);
   if (_271)
   {
      float _273 = _213 * _195;
      float _274 = _213 * _196;
      float _275 = _213 * _197;
      float _276 = _273 + TEXCOORD_4.x;
      float _277 = _274 + TEXCOORD_4.y;
      float _278 = _275 + TEXCOORD_4.z;
      float _279 = _213 * _198;
      float _280 = _279 + TEXCOORD_5.x;
      float _281 = _279 + TEXCOORD_5.y;
      float _282 = _279 + TEXCOORD_5.z;
      _284 = _276;
      _285 = _277;
      _286 = _278;
      _287 = _280;
      _288 = _281;
      _289 = _282;
   }
   else
   {
      _284 = TEXCOORD_4.x;
      _285 = TEXCOORD_4.y;
      _286 = TEXCOORD_4.z;
      _287 = TEXCOORD_5.x;
      _288 = TEXCOORD_5.y;
      _289 = TEXCOORD_5.z;
   }
   float _294 = -0.0f - (c_light_vec[1].x);
   float _295 = -0.0f - (c_light_vec[1].y);
   float _296 = -0.0f - (c_light_vec[1].z);
   float _297 = dot(float3(_113, _114, _115), float3(_294, _295, _296));
   float _298 = saturate(_297);
   float _303 = saturate(_128);
   float _304 = (c_light_col[0].x) * _303;
   float _305 = (c_light_col[0].y) * _303;
   float _306 = (c_light_col[0].z) * _303;
   float _311 = (c_light_col[1].x) * _298;
   float _312 = (c_light_col[1].y) * _298;
   float _313 = (c_light_col[1].z) * _298;
   float _322 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _323 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _324 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _325 = _304 * _225;
   float _326 = _305 * _226;
   float _327 = _306 * _227;
   float _328 = _311 * _213;
   float _329 = _312 * _213;
   float _330 = _313 * _213;
   float _331 = _328 + _325;
   float _332 = _329 + _326;
   float _333 = _330 + _327;
   float _334 = _322 * _331;
   float _335 = _323 * _332;
   float _336 = _324 * _333;
   float _337 = _265 + c_scene_ambient.x;
   float _338 = _337 + _334;
   float _339 = _267 + c_scene_ambient.y;
   float _340 = _339 + _335;
   float _341 = _269 + c_scene_ambient.z;
   float _342 = _341 + _336;
   float _343 = _338 * _228;
   float _344 = _340 * _229;
   float _345 = _342 * _230;
   float _346 = _284 * _251;
   float _347 = _285 * _252;
   float _348 = _286 * _253;
   float _349 = _343 + _346;
   float _350 = _344 + _347;
   float _351 = _345 + _348;
   float _356 = (c_light_vec[0].x) + _118;
   float _357 = (c_light_vec[0].y) + _119;
   float _358 = (c_light_vec[0].z) + _120;
   float _359 = dot(float3(_356, _357, _358), float3(_356, _357, _358));
   float _360 = rsqrt(_359);
   float _361 = _356 * _360;
   float _362 = -0.0f - _361;
   float _363 = _357 * _360;
   float _364 = -0.0f - _363;
   float _365 = _358 * _360;
   float _366 = -0.0f - _365;
   float _367 = dot(float3(_362, _364, _366), float3(_113, _114, _115));
   float _368 = saturate(_367);
   float _369 = _119 + -0.30000001192092896f;
   float _370 = dot(float3(_118, _369, _120), float3(_118, _369, _120));
   float _371 = rsqrt(_370);
   float _372 = _118 * _371;
   float _373 = -0.0f - _372;
   float _374 = _369 * _371;
   float _375 = -0.0f - _374;
   float _376 = _120 * _371;
   float _377 = -0.0f - _376;
   float _378 = dot(float3(_373, _375, _377), float3(_113, _114, _115));
   float _379 = saturate(_378);
   float _382 = log2(_368);
   float _383 = log2(_379);
   float _384 = _382 * c_mat_power;
   float _385 = _383 * c_mat_power;
   float _386 = exp2(_384);
   float _387 = exp2(_385);
   float _390 = _386 * (c_light_col[0].x);
   float _391 = _386 * (c_light_col[0].y);
   float _392 = _386 * (c_light_col[0].z);
   float _393 = _387 * 2.0f;
   float _394 = _393 * (c_light_vec[5].x);
   float _395 = _394 * (c_light_col[1].x);
   float _396 = _394 * (c_light_col[1].y);
   float _397 = _394 * (c_light_col[1].z);
   float _398 = _390 * _225;
   float _399 = _391 * _226;
   float _400 = _392 * _227;
   float _401 = _395 * _298;
   float _402 = _396 * _298;
   float _403 = _397 * _298;
   float _404 = _401 + _398;
   float _405 = _402 + _399;
   float _406 = _403 + _400;
   float _413 = c_mat_specular.x * c_specular_scale.x;
   float _414 = _413 * _228;
   float _415 = _414 * _404;
   float _416 = c_mat_specular.y * c_specular_scale.y;
   float _417 = _416 * _229;
   float _418 = _417 * _405;
   float _419 = c_mat_specular.z * c_specular_scale.z;
   float _420 = _419 * _230;
   float _421 = _420 * _406;
   float _422 = _287 * _251;
   float _423 = _422 * _387;
   float _424 = _288 * _252;
   float _425 = _424 * _387;
   float _426 = _289 * _253;
   float _427 = _426 * _387;
   float _428 = _415 + _423;
   float _429 = _418 + _425;
   float _430 = _421 + _427;
   if (!_271)
   {
      float _432 = _213 * _195;
      float _433 = _213 * _196;
      float _434 = _213 * _197;
      float _435 = _349 + _432;
      float _436 = _350 + _433;
      float _437 = _351 + _434;
      float _438 = _213 * _198;
      float _439 = _428 + _438;
      float _440 = _429 + _438;
      float _441 = _430 + _438;
      _443 = _435;
      _444 = _436;
      _445 = _437;
      _446 = _439;
      _447 = _440;
      _448 = _441;
   }
   else
   {
      _443 = _349;
      _444 = _350;
      _445 = _351;
      _446 = _428;
      _447 = _429;
      _448 = _430;
   }
   int _449 = c_bool & 128;
   bool _450 = (_449 != 0);
   float _451 = select(_450, 0.0f, _446);
   float _452 = select(_450, 0.0f, _447);
   float _453 = select(_450, 0.0f, _448);
   float _454 = _443 * _56;
   float _455 = _444 * _57;
   float _456 = _445 * _58;
   float _457 = _451 * _64.y;
   float _458 = _452 * _64.y;
   float _459 = _453 * _64.y;
   int _460 = c_bool & 32768;
   bool _461 = (_460 == 0);
   if (!_461)
   {
      float _465 = (c_ps_user[11].x) * _56;
      float _466 = (c_ps_user[11].x) * _57;
      float _467 = (c_ps_user[11].x) * _58;
      float _469 = _113 + 1.0f;
      float _470 = _114 + 1.0f;
      float _471 = _115 + 1.0f;
      float _472 = _469 * 0.5f;
      float _473 = _470 * 0.5f;
      float _474 = _471 * 0.5f;
      float _475 = saturate(_472);
      float _476 = saturate(_473);
      float _477 = saturate(_474);
      float _478 = (c_ps_user[11].y) * _475;
      float _479 = _476 * (c_ps_user[11].y);
      float _480 = _477 * (c_ps_user[11].y);
      float _482 = (c_ps_user[11].z) * _64.y;
      float _484 = c_mat_power * 0.009999999776482582f;
      float _485 = _484 * (c_ps_user[11].w);
      float _488 = (c_ps_user[12].x) * _225;
      float _489 = (c_ps_user[12].x) * _226;
      float _490 = (c_ps_user[12].x) * _227;
      float _492 = (c_ps_user[12].y) * _228;
      float _493 = (c_ps_user[12].y) * _229;
      float _494 = (c_ps_user[12].y) * _230;
      float _496 = (c_ps_user[12].z) * _251;
      float _497 = (c_ps_user[12].z) * _252;
      float _498 = (c_ps_user[12].z) * _253;
      float _504 = -0.0f - _118;
      float _505 = -0.0f - _119;
      float _506 = -0.0f - _120;
      float _507 = dot(float3(_504, _505, _506), float3(_113, _114, _115));
      float _508 = 1.0f - _507;
      float _509 = _508 * _508;
      float _510 = _508 * 0.9722222089767456f;
      float _511 = _509 * _509;
      float _512 = _511 * _510;
      float _513 = _512 + 0.02777777425944805f;
      float _514 = _513 * (c_ps_user[13].z);
      float _516 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _517 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _518 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _521 = (c_ps_user[14].x) * _265;
      float _522 = (c_ps_user[14].x) * _267;
      float _523 = (c_ps_user[14].x) * _269;
      float _525 = (c_ps_user[14].y) * _304;
      float _526 = (c_ps_user[14].y) * _305;
      float _527 = (c_ps_user[14].y) * _306;
      float _529 = (c_ps_user[14].z) * _311;
      float _530 = (c_ps_user[14].z) * _312;
      float _531 = (c_ps_user[14].z) * _313;
      float _533 = (c_ps_user[14].w) * _284;
      float _534 = (c_ps_user[14].w) * _285;
      float _535 = (c_ps_user[14].w) * _286;
      float _538 = (c_ps_user[15].x) * _390;
      float _539 = (c_ps_user[15].x) * _391;
      float _540 = (c_ps_user[15].x) * _392;
      float _542 = _413 * _395;
      float _543 = _542 * (c_ps_user[15].y);
      float _544 = _416 * _396;
      float _545 = _544 * (c_ps_user[15].y);
      float _546 = _419 * _397;
      float _547 = _546 * (c_ps_user[15].y);
      float _549 = (c_ps_user[15].z) * _287;
      float _550 = (c_ps_user[15].z) * _288;
      float _551 = (c_ps_user[15].z) * _289;
      float _552 = _543 + _549;
      float _553 = _552 * _387;
      float _554 = _525 + (c_ps_user[13].x);
      float _555 = _554 + _529;
      float _556 = c_diffuse_scale.x * _555;
      float _557 = _556 * c_mat_diffuse.x;
      float _558 = (c_ps_user[13].y) + _482;
      float _559 = _558 + _538;
      float _560 = c_mat_specular.x * _559;
      float _561 = _560 * c_specular_scale.x;
      float _562 = _478 + _465;
      float _563 = _562 + _485;
      float _564 = _563 + (c_ps_user[12].w);
      float _565 = _564 + _488;
      float _566 = _565 + _492;
      float _567 = _566 + _496;
      float _568 = _567 + _516;
      float _569 = _568 + _521;
      float _570 = _569 + _533;
      float _571 = _570 + _514;
      float _572 = _571 + _553;
      float _573 = _572 + _557;
      float _574 = _573 + _561;
      float _575 = _545 + _550;
      float _576 = _575 * _387;
      float _577 = _526 + (c_ps_user[13].x);
      float _578 = _577 + _530;
      float _579 = c_diffuse_scale.y * _578;
      float _580 = _579 * c_mat_diffuse.y;
      float _581 = _558 + _539;
      float _582 = c_mat_specular.y * _581;
      float _583 = _582 * c_specular_scale.y;
      float _584 = _479 + _466;
      float _585 = _584 + _485;
      float _586 = _585 + (c_ps_user[12].w);
      float _587 = _586 + _489;
      float _588 = _587 + _493;
      float _589 = _588 + _497;
      float _590 = _589 + _517;
      float _591 = _590 + _522;
      float _592 = _591 + _534;
      float _593 = _592 + _514;
      float _594 = _593 + _576;
      float _595 = _594 + _580;
      float _596 = _595 + _583;
      float _597 = _547 + _551;
      float _598 = _597 * _387;
      float _599 = _527 + (c_ps_user[13].x);
      float _600 = _599 + _531;
      float _601 = c_diffuse_scale.z * _600;
      float _602 = _601 * c_mat_diffuse.z;
      float _603 = _558 + _540;
      float _604 = c_mat_specular.z * _603;
      float _605 = _604 * c_specular_scale.z;
      float _606 = _485 + _467;
      float _607 = _606 + _480;
      float _608 = _607 + (c_ps_user[12].w);
      float _609 = _608 + _490;
      float _610 = _609 + _494;
      float _611 = _610 + _498;
      float _612 = _611 + _518;
      float _613 = _612 + _523;
      float _614 = _613 + _535;
      float _615 = _614 + _514;
      float _616 = _615 + _598;
      float _617 = _616 + _602;
      float _618 = _617 + _605;
      _620 = _574;
      _621 = _596;
      _622 = _618;
      _623 = 0.0f;
      _624 = 0.0f;
      _625 = 0.0f;
   }
   else
   {
      _620 = _454;
      _621 = _455;
      _622 = _456;
      _623 = _457;
      _624 = _458;
      _625 = _459;
   }
   float _626 = _74.x * _74.x;
   float _627 = _626 * COLOR_1.w;
   float _628 = _74.y * _74.y;
   float _629 = _628 * COLOR_1.w;
   float _630 = _74.z * _74.z;
   float _631 = _630 * COLOR_1.w;
   float _632 = _620 + _627;
   float _633 = _632 + _623;
   float _634 = _621 + _629;
   float _635 = _634 + _624;
   float _636 = _622 + _631;
   float _637 = _636 + _625;
   float _642 = _51.w * COLOR.w;
   float _643 = _642 * c_mat_opacity;
   float _644 = _643 * c_global_opacity;
   float _645 = _644 * c_opacity_scale;
   int _646 = c_bool & 16;
   bool _647 = (_646 == 0);
   if (!_647)
   {
      float _651 = c_tone_control.x * _633;
      float _652 = c_tone_control.x * _635;
      float _653 = c_tone_control.x * _637;
      float _655 = log2(_651);
      float _656 = log2(_652);
      float _657 = log2(_653);
      float _658 = _655 * c_tone_control.y;
      float _659 = _656 * c_tone_control.y;
      float _660 = _657 * c_tone_control.y;
      float _661 = exp2(_658);
      float _662 = exp2(_659);
      float _663 = exp2(_660);
      _711 = _661;
      _712 = _662;
      _713 = _663;
   }
   else
   {
      if (_461)
      {
         float _668 = c_tone_control.x * _633;
         float _669 = c_tone_control.x * _635;
         float _670 = c_tone_control.x * _637;
         float _671 = _668 * 2.0f;
         float _672 = _669 * 2.0f;
         float _673 = _670 * 2.0f;
         float _674 = _668 * 0.30000001192092896f;
         float _675 = _669 * 0.30000001192092896f;
         float _676 = _670 * 0.30000001192092896f;
         float _677 = _674 + 0.05000000074505806f;
         float _678 = _675 + 0.05000000074505806f;
         float _679 = _676 + 0.05000000074505806f;
         float _680 = _677 * _671;
         float _681 = _678 * _672;
         float _682 = _679 * _673;
         float _683 = _680 + 0.004000000189989805f;
         float _684 = _681 + 0.004000000189989805f;
         float _685 = _682 + 0.004000000189989805f;
         float _686 = _674 + 0.5f;
         float _687 = _675 + 0.5f;
         float _688 = _676 + 0.5f;
         float _689 = _686 * _671;
         float _690 = _687 * _672;
         float _691 = _688 * _673;
         float _692 = _689 + 0.06000000238418579f;
         float _693 = _690 + 0.06000000238418579f;
         float _694 = _691 + 0.06000000238418579f;
         float _695 = _683 / _692;
         float _696 = _684 / _693;
         float _697 = _685 / _694;
         float _698 = _695 + -0.06666666269302368f;
         float _699 = _696 + -0.06666666269302368f;
         float _700 = _697 + -0.06666666269302368f;
         float _701 = _698 * 1.3790643215179443f;
         float _702 = _699 * 1.3790643215179443f;
         float _703 = _700 * 1.3790643215179443f;
         float _704 = YKR_MaterialCurve(_701, _668);
         float _705 = YKR_MaterialCurve(_702, _669);
         float _706 = YKR_MaterialCurve(_703, _670);
         float _707 = sqrt(_704);
         float _708 = sqrt(_705);
         float _709 = sqrt(_706);
         _711 = _707;
         _712 = _708;
         _713 = _709;
      }
      else
      {
         _711 = _633;
         _712 = _635;
         _713 = _637;
      }
   }
   int _714 = c_bool & 4;
   bool _715 = (_714 == 0);
   if (_715)
   {
      float _717 = saturate(COLOR_1.x);
      float _722 = (c_fog_param[0].x) - _711;
      float _723 = (c_fog_param[0].y) - _712;
      float _724 = (c_fog_param[0].z) - _713;
      float _725 = _722 * _717;
      float _726 = _723 * _717;
      float _727 = _724 * _717;
      _729 = _725;
      _730 = _726;
      _731 = _727;
   }
   else
   {
      _729 = COLOR_1.x;
      _730 = COLOR_1.y;
      _731 = COLOR_1.z;
   }
   float _732 = _729 + _711;
   float _733 = _730 + _712;
   float _734 = _731 + _713;
   float _737 = _645 - c_mat_punchthrough_ref;
   bool _738 = (_737 < 0.0f);
   if (_738)
      discard;
   float _744 = (c_ps_col_scl[0].x) * _732;
   float _745 = (c_ps_col_scl[0].y) * _733;
   float _746 = (c_ps_col_scl[0].z) * _734;
   float _747 = (c_ps_col_scl[0].w) * _645;
   SV_Target.x = _744;
   SV_Target.y = _745;
   SV_Target.z = _746;
   SV_Target.w = _747;
   int _750 = c_bool & 8;
   bool _751 = (_750 != 0);
   float _752 = select(_751, _732, _744);
   float _753 = select(_751, _733, _745);
   float _754 = select(_751, _734, _746);
   float _755 = select(_751, _645, _747);
   SV_Target.x = _752;
   SV_Target.y = _753;
   SV_Target.z = _754;
   SV_Target.w = _755;
   return SV_Target;
}
