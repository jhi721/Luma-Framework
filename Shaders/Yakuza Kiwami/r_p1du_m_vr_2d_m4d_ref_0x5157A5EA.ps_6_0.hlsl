// Yakuza Kiwami material r_p1du_m(vr)2d_m4d[ref] (0x5157A5EA, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
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
   float4 _67 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _71 = _67.x * _67.x;
   float _72 = _67.y * _67.y;
   float _73 = _67.z * _67.z;
   float4 _74 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _77 = 1.0f - _71;
   float _78 = 1.0f - _72;
   float _79 = 1.0f - _73;
   float _80 = _77 * COLOR_1.w;
   float _81 = _78 * COLOR_1.w;
   float _82 = _79 * COLOR_1.w;
   float _83 = 1.0f - _80;
   float _84 = 1.0f - _81;
   float _85 = 1.0f - _82;
   float _86 = _56 * _83;
   float _87 = _57 * _84;
   float _88 = _58 * _85;
   float _89 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _90 = rsqrt(_89);
   float _91 = _90 * TEXCOORD_2.x;
   float _92 = _90 * TEXCOORD_2.y;
   float _93 = _90 * TEXCOORD_2.z;
   float _94 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _95 = rsqrt(_94);
   float _96 = _95 * TEXCOORD_3.x;
   float _97 = _95 * TEXCOORD_3.y;
   float _98 = _95 * TEXCOORD_3.z;
   float _103 = -0.0f - (c_light_vec[0].x);
   float _104 = -0.0f - (c_light_vec[0].y);
   float _105 = -0.0f - (c_light_vec[0].z);
   float _106 = dot(float3(_91, _92, _93), float3(_103, _104, _105));
   float _110 = c_rt_size.z * SV_Position.x;
   float _111 = c_rt_size.w * SV_Position.y;
   int _114 = c_bool & 8;
   bool _115 = (_114 == 0);
   float _122;
   float _123;
   float _124;
   float _136;
   float _137;
   float _192;
   float _193;
   float _194;
   float _195;
   float _209;
   float _210;
   float _256;
   float _257;
   float _258;
   float _293;
   float _294;
   float _295;
   float _296;
   float _297;
   float _298;
   float _452;
   float _453;
   float _454;
   float _455;
   float _456;
   float _457;
   float _629;
   float _630;
   float _631;
   float _632;
   float _633;
   float _634;
   float _757;
   float _758;
   float _759;
   float _775;
   float _776;
   float _777;
   if (!_115)
   {
      float4 _117 = s13_t.Sample(s13_s, float2(_110, _111));
      _122 = _117.x;
      _123 = _117.y;
      _124 = _117.w;
   }
   else
   {
      _122 = 1.0f;
      _123 = 1.0f;
      _124 = 1.0f;
   }
   if (!_115)
   {
      float _129 = (c_sdw_param[0].x) * _122;
      float _130 = (c_sdw_param[0].z) * _123;
      float _133 = _129 + (c_sdw_param[0].y);
      float _134 = _130 + (c_sdw_param[0].w);
      _136 = _133;
      _137 = _134;
   }
   else
   {
      _136 = _122;
      _137 = _123;
   }
   float _144 = (c_sdw_param[1].x) * _74.y;
   float _146 = _144 + (c_sdw_param[1].y);
   float _147 = _146 * _74.x;
   float _148 = 1.0f - (c_light_col[5].x);
   float _149 = 1.0f - (c_light_col[5].y);
   float _150 = 1.0f - (c_light_col[5].z);
   float _151 = _147 * _148;
   float _152 = _147 * _149;
   float _153 = _147 * _150;
   float _154 = _151 + (c_light_col[5].x);
   float _155 = _152 + (c_light_col[5].y);
   float _156 = _153 + (c_light_col[5].z);
   float _160 = c_rt_size.z * SV_Position.x;
   float _161 = c_rt_size.w * SV_Position.y;
   float4 _162 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_160, _161)));
   int _169 = c_bool & 16384;
   bool _170 = (_169 == 0);
   if (!_170)
   {
      float _172 = _162.x * 2.0f;
      float _173 = _162.y * 2.0f;
      float _174 = _162.z * 2.0f;
      float _175 = _162.w * 2.0f;
      _192 = _172;
      _193 = _173;
      _194 = _174;
      _195 = _175;
   }
   else
   {
      int _177 = c_bool & 8192;
      bool _178 = (_177 == 0);
      if (!_178)
      {
         float _180 = _162.x * 2.0f;
         float _181 = _162.y * 2.0f;
         float _182 = _162.z * 2.0f;
         float _183 = _162.w * 2.0f;
         _192 = _180;
         _193 = _181;
         _194 = _182;
         _195 = _183;
      }
      else
      {
         float _187 = _162.x / c_tone_control.z;
         float _188 = _162.y / c_tone_control.z;
         float _189 = _162.z / c_tone_control.z;
         float _190 = _162.w / c_tone_control.z;
         _192 = _187;
         _193 = _188;
         _194 = _189;
         _195 = _190;
      }
   }
   int _196 = c_bool & 8;
   bool _197 = (_196 == 0);
   if (!_197)
   {
      float _202 = (c_sdw_param[0].x) * _136;
      float _203 = (c_sdw_param[0].z) * _137;
      float _206 = _202 + (c_sdw_param[0].y);
      float _207 = _203 + (c_sdw_param[0].w);
      _209 = _206;
      _210 = _207;
   }
   else
   {
      _209 = _136;
      _210 = _137;
   }
   float _211 = _210 * _209;
   float _216 = 1.0f - (c_light_col[5].x);
   float _217 = 1.0f - (c_light_col[5].y);
   float _218 = 1.0f - (c_light_col[5].z);
   float _219 = _216 * _211;
   float _220 = _217 * _211;
   float _221 = _218 * _211;
   float _222 = _219 + (c_light_col[5].x);
   float _223 = _220 + (c_light_col[5].y);
   float _224 = _221 + (c_light_col[5].z);
   float _225 = _124 * COLOR.x;
   float _226 = _225 * _154;
   float _227 = _124 * COLOR.y;
   float _228 = _227 * _155;
   float _229 = _124 * COLOR.z;
   float _230 = _229 * _156;
   float _231 = _222 * _154;
   float _232 = _223 * _155;
   float _233 = _224 * _156;
   if (!_197)
   {
      float _237 = (c_sdw_param[2].x) * _222;
      float _238 = (c_sdw_param[2].x) * _223;
      float _239 = (c_sdw_param[2].x) * _224;
      float _241 = _237 + (c_sdw_param[2].y);
      float _242 = _238 + (c_sdw_param[2].y);
      float _243 = _239 + (c_sdw_param[2].y);
      float _245 = (c_sdw_param[2].z) * _154;
      float _246 = (c_sdw_param[2].z) * _155;
      float _247 = (c_sdw_param[2].z) * _156;
      float _249 = _245 + (c_sdw_param[2].w);
      float _250 = _246 + (c_sdw_param[2].w);
      float _251 = _247 + (c_sdw_param[2].w);
      float _252 = _249 * _241;
      float _253 = _250 * _242;
      float _254 = _251 * _243;
      _256 = _252;
      _257 = _253;
      _258 = _254;
   }
   else
   {
      _256 = _231;
      _257 = _232;
      _258 = _233;
   }
   float _259 = _124 * _74.x;
   float _260 = _256 * _259;
   float _261 = _257 * _259;
   float _262 = _258 * _259;
   float _273 = c_mat_emissive.x * _86;
   float _274 = _273 * c_emissive_scale;
   float _275 = c_mat_emissive.y * _87;
   float _276 = _275 * c_emissive_scale;
   float _277 = c_mat_emissive.z * _88;
   float _278 = _277 * c_emissive_scale;
   int _279 = c_bool & 2048;
   bool _280 = (_279 == 0);
   if (_280)
   {
      float _282 = _210 * _192;
      float _283 = _210 * _193;
      float _284 = _210 * _194;
      float _285 = _282 + TEXCOORD_4.x;
      float _286 = _283 + TEXCOORD_4.y;
      float _287 = _284 + TEXCOORD_4.z;
      float _288 = _210 * _195;
      float _289 = _288 + TEXCOORD_5.x;
      float _290 = _288 + TEXCOORD_5.y;
      float _291 = _288 + TEXCOORD_5.z;
      _293 = _285;
      _294 = _286;
      _295 = _287;
      _296 = _289;
      _297 = _290;
      _298 = _291;
   }
   else
   {
      _293 = TEXCOORD_4.x;
      _294 = TEXCOORD_4.y;
      _295 = TEXCOORD_4.z;
      _296 = TEXCOORD_5.x;
      _297 = TEXCOORD_5.y;
      _298 = TEXCOORD_5.z;
   }
   float _303 = -0.0f - (c_light_vec[1].x);
   float _304 = -0.0f - (c_light_vec[1].y);
   float _305 = -0.0f - (c_light_vec[1].z);
   float _306 = dot(float3(_91, _92, _93), float3(_303, _304, _305));
   float _307 = saturate(_306);
   float _312 = saturate(_106);
   float _313 = (c_light_col[0].x) * _312;
   float _314 = (c_light_col[0].y) * _312;
   float _315 = (c_light_col[0].z) * _312;
   float _320 = (c_light_col[1].x) * _307;
   float _321 = (c_light_col[1].y) * _307;
   float _322 = (c_light_col[1].z) * _307;
   float _331 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _332 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _333 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _334 = _313 * _222;
   float _335 = _314 * _223;
   float _336 = _315 * _224;
   float _337 = _320 * _210;
   float _338 = _321 * _210;
   float _339 = _322 * _210;
   float _340 = _337 + _334;
   float _341 = _338 + _335;
   float _342 = _339 + _336;
   float _343 = _331 * _340;
   float _344 = _332 * _341;
   float _345 = _333 * _342;
   float _346 = _274 + c_scene_ambient.x;
   float _347 = _346 + _343;
   float _348 = _276 + c_scene_ambient.y;
   float _349 = _348 + _344;
   float _350 = _278 + c_scene_ambient.z;
   float _351 = _350 + _345;
   float _352 = _347 * _226;
   float _353 = _349 * _228;
   float _354 = _351 * _230;
   float _355 = _293 * _260;
   float _356 = _294 * _261;
   float _357 = _295 * _262;
   float _358 = _352 + _355;
   float _359 = _353 + _356;
   float _360 = _354 + _357;
   float _365 = (c_light_vec[0].x) + _96;
   float _366 = (c_light_vec[0].y) + _97;
   float _367 = (c_light_vec[0].z) + _98;
   float _368 = dot(float3(_365, _366, _367), float3(_365, _366, _367));
   float _369 = rsqrt(_368);
   float _370 = _365 * _369;
   float _371 = -0.0f - _370;
   float _372 = _366 * _369;
   float _373 = -0.0f - _372;
   float _374 = _367 * _369;
   float _375 = -0.0f - _374;
   float _376 = dot(float3(_371, _373, _375), float3(_91, _92, _93));
   float _377 = saturate(_376);
   float _378 = _97 + -0.30000001192092896f;
   float _379 = dot(float3(_96, _378, _98), float3(_96, _378, _98));
   float _380 = rsqrt(_379);
   float _381 = _96 * _380;
   float _382 = -0.0f - _381;
   float _383 = _378 * _380;
   float _384 = -0.0f - _383;
   float _385 = _98 * _380;
   float _386 = -0.0f - _385;
   float _387 = dot(float3(_382, _384, _386), float3(_91, _92, _93));
   float _388 = saturate(_387);
   float _391 = log2(_377);
   float _392 = log2(_388);
   float _393 = _391 * c_mat_power;
   float _394 = _392 * c_mat_power;
   float _395 = exp2(_393);
   float _396 = exp2(_394);
   float _399 = _395 * (c_light_col[0].x);
   float _400 = _395 * (c_light_col[0].y);
   float _401 = _395 * (c_light_col[0].z);
   float _402 = _396 * 2.0f;
   float _403 = _402 * (c_light_vec[5].x);
   float _404 = _403 * (c_light_col[1].x);
   float _405 = _403 * (c_light_col[1].y);
   float _406 = _403 * (c_light_col[1].z);
   float _407 = _399 * _222;
   float _408 = _400 * _223;
   float _409 = _401 * _224;
   float _410 = _404 * _307;
   float _411 = _405 * _307;
   float _412 = _406 * _307;
   float _413 = _410 + _407;
   float _414 = _411 + _408;
   float _415 = _412 + _409;
   float _422 = c_mat_specular.x * c_specular_scale.x;
   float _423 = _422 * _226;
   float _424 = _423 * _413;
   float _425 = c_mat_specular.y * c_specular_scale.y;
   float _426 = _425 * _228;
   float _427 = _426 * _414;
   float _428 = c_mat_specular.z * c_specular_scale.z;
   float _429 = _428 * _230;
   float _430 = _429 * _415;
   float _431 = _296 * _260;
   float _432 = _431 * _396;
   float _433 = _297 * _261;
   float _434 = _433 * _396;
   float _435 = _298 * _262;
   float _436 = _435 * _396;
   float _437 = _424 + _432;
   float _438 = _427 + _434;
   float _439 = _430 + _436;
   if (!_280)
   {
      float _441 = _210 * _192;
      float _442 = _210 * _193;
      float _443 = _210 * _194;
      float _444 = _358 + _441;
      float _445 = _359 + _442;
      float _446 = _360 + _443;
      float _447 = _210 * _195;
      float _448 = _437 + _447;
      float _449 = _438 + _447;
      float _450 = _439 + _447;
      _452 = _444;
      _453 = _445;
      _454 = _446;
      _455 = _448;
      _456 = _449;
      _457 = _450;
   }
   else
   {
      _452 = _358;
      _453 = _359;
      _454 = _360;
      _455 = _437;
      _456 = _438;
      _457 = _439;
   }
   int _458 = c_bool & 128;
   bool _459 = (_458 != 0);
   float _460 = select(_459, 0.0f, _455);
   float _461 = select(_459, 0.0f, _456);
   float _462 = select(_459, 0.0f, _457);
   float _463 = _452 * _86;
   float _464 = _453 * _87;
   float _465 = _454 * _88;
   float _466 = _460 * _64.y;
   float _467 = _461 * _64.y;
   float _468 = _462 * _64.y;
   int _469 = c_bool & 32768;
   bool _470 = (_469 == 0);
   if (!_470)
   {
      float _474 = (c_ps_user[11].x) * _86;
      float _475 = (c_ps_user[11].x) * _87;
      float _476 = (c_ps_user[11].x) * _88;
      float _478 = _91 + 1.0f;
      float _479 = _92 + 1.0f;
      float _480 = _93 + 1.0f;
      float _481 = _478 * 0.5f;
      float _482 = _479 * 0.5f;
      float _483 = _480 * 0.5f;
      float _484 = saturate(_481);
      float _485 = saturate(_482);
      float _486 = saturate(_483);
      float _487 = (c_ps_user[11].y) * _484;
      float _488 = _485 * (c_ps_user[11].y);
      float _489 = _486 * (c_ps_user[11].y);
      float _491 = (c_ps_user[11].z) * _64.y;
      float _493 = c_mat_power * 0.009999999776482582f;
      float _494 = _493 * (c_ps_user[11].w);
      float _497 = (c_ps_user[12].x) * _222;
      float _498 = (c_ps_user[12].x) * _223;
      float _499 = (c_ps_user[12].x) * _224;
      float _501 = (c_ps_user[12].y) * _226;
      float _502 = (c_ps_user[12].y) * _228;
      float _503 = (c_ps_user[12].y) * _230;
      float _505 = (c_ps_user[12].z) * _260;
      float _506 = (c_ps_user[12].z) * _261;
      float _507 = (c_ps_user[12].z) * _262;
      float _513 = -0.0f - _96;
      float _514 = -0.0f - _97;
      float _515 = -0.0f - _98;
      float _516 = dot(float3(_513, _514, _515), float3(_91, _92, _93));
      float _517 = 1.0f - _516;
      float _518 = _517 * _517;
      float _519 = _517 * 0.9722222089767456f;
      float _520 = _518 * _518;
      float _521 = _520 * _519;
      float _522 = _521 + 0.02777777425944805f;
      float _523 = _522 * (c_ps_user[13].z);
      float _525 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _526 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _527 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _530 = (c_ps_user[14].x) * _274;
      float _531 = (c_ps_user[14].x) * _276;
      float _532 = (c_ps_user[14].x) * _278;
      float _534 = (c_ps_user[14].y) * _313;
      float _535 = (c_ps_user[14].y) * _314;
      float _536 = (c_ps_user[14].y) * _315;
      float _538 = (c_ps_user[14].z) * _320;
      float _539 = (c_ps_user[14].z) * _321;
      float _540 = (c_ps_user[14].z) * _322;
      float _542 = (c_ps_user[14].w) * _293;
      float _543 = (c_ps_user[14].w) * _294;
      float _544 = (c_ps_user[14].w) * _295;
      float _547 = (c_ps_user[15].x) * _399;
      float _548 = (c_ps_user[15].x) * _400;
      float _549 = (c_ps_user[15].x) * _401;
      float _551 = _422 * _404;
      float _552 = _551 * (c_ps_user[15].y);
      float _553 = _425 * _405;
      float _554 = _553 * (c_ps_user[15].y);
      float _555 = _428 * _406;
      float _556 = _555 * (c_ps_user[15].y);
      float _558 = (c_ps_user[15].z) * _296;
      float _559 = (c_ps_user[15].z) * _297;
      float _560 = (c_ps_user[15].z) * _298;
      float _561 = _552 + _558;
      float _562 = _561 * _396;
      float _563 = _534 + (c_ps_user[13].x);
      float _564 = _563 + _538;
      float _565 = c_diffuse_scale.x * _564;
      float _566 = _565 * c_mat_diffuse.x;
      float _567 = (c_ps_user[13].y) + _491;
      float _568 = _567 + _547;
      float _569 = c_mat_specular.x * _568;
      float _570 = _569 * c_specular_scale.x;
      float _571 = _487 + _474;
      float _572 = _571 + _494;
      float _573 = _572 + (c_ps_user[12].w);
      float _574 = _573 + _497;
      float _575 = _574 + _501;
      float _576 = _575 + _505;
      float _577 = _576 + _525;
      float _578 = _577 + _530;
      float _579 = _578 + _542;
      float _580 = _579 + _523;
      float _581 = _580 + _562;
      float _582 = _581 + _566;
      float _583 = _582 + _570;
      float _584 = _554 + _559;
      float _585 = _584 * _396;
      float _586 = _535 + (c_ps_user[13].x);
      float _587 = _586 + _539;
      float _588 = c_diffuse_scale.y * _587;
      float _589 = _588 * c_mat_diffuse.y;
      float _590 = _567 + _548;
      float _591 = c_mat_specular.y * _590;
      float _592 = _591 * c_specular_scale.y;
      float _593 = _488 + _475;
      float _594 = _593 + _494;
      float _595 = _594 + (c_ps_user[12].w);
      float _596 = _595 + _498;
      float _597 = _596 + _502;
      float _598 = _597 + _506;
      float _599 = _598 + _526;
      float _600 = _599 + _531;
      float _601 = _600 + _543;
      float _602 = _601 + _523;
      float _603 = _602 + _585;
      float _604 = _603 + _589;
      float _605 = _604 + _592;
      float _606 = _556 + _560;
      float _607 = _606 * _396;
      float _608 = _536 + (c_ps_user[13].x);
      float _609 = _608 + _540;
      float _610 = c_diffuse_scale.z * _609;
      float _611 = _610 * c_mat_diffuse.z;
      float _612 = _567 + _549;
      float _613 = c_mat_specular.z * _612;
      float _614 = _613 * c_specular_scale.z;
      float _615 = _494 + _476;
      float _616 = _615 + _489;
      float _617 = _616 + (c_ps_user[12].w);
      float _618 = _617 + _499;
      float _619 = _618 + _503;
      float _620 = _619 + _507;
      float _621 = _620 + _527;
      float _622 = _621 + _532;
      float _623 = _622 + _544;
      float _624 = _623 + _523;
      float _625 = _624 + _607;
      float _626 = _625 + _611;
      float _627 = _626 + _614;
      _629 = _583;
      _630 = _605;
      _631 = _627;
      _632 = 0.0f;
      _633 = 0.0f;
      _634 = 0.0f;
   }
   else
   {
      _629 = _463;
      _630 = _464;
      _631 = _465;
      _632 = _466;
      _633 = _467;
      _634 = _468;
   }
   float _635 = _632 + _629;
   float _636 = _633 + _630;
   float _637 = _634 + _631;
   float _638 = dot(float3(_96, _97, _98), float3(_91, _92, _93));
   float _639 = _638 * 2.0f;
   float _640 = _639 * _91;
   float _641 = _639 * _92;
   float _642 = _639 * _93;
   float _643 = _96 - _640;
   float _644 = _97 - _641;
   float _645 = _98 - _642;
   float _646 = -0.0f - _643;
   float4 _647 = s6c_t.Sample(s6c_s, float3(_646, _644, _645));
   float _654 = _137 * _136;
   float _655 = _106 + 0.75f;
   float _656 = saturate(_655);
   float _657 = _654 * _656;
   float _658 = max(_657, (c_light_vec[5].y));
   float _659 = max(_647.w, 0.003921568859368563f);
   float _660 = 0.25f / _659;
   float _661 = _660 * _647.x;
   float _662 = _660 * _647.y;
   float _663 = _660 * _647.z;
   float _664 = _64.x * TEXCOORD_2.w;
   float _665 = _664 * _74.x;
   float _666 = _658 * _665;
   float _667 = _666 * _154;
   float _668 = _661 * _661;
   float _669 = _668 * _667;
   float _670 = _666 * _155;
   float _671 = _662 * _662;
   float _672 = _671 * _670;
   float _673 = _666 * _156;
   float _674 = _663 * _663;
   float _675 = _674 * _673;
   float _676 = _635 + _669;
   float _677 = _636 + _672;
   float _678 = _637 + _675;
   float _685 = c_mat_opacity * _51.w;
   float _686 = _685 * c_opacity_scale;
   float _687 = _686 * c_global_opacity;
   int _690 = c_bool & 16;
   bool _691 = (_690 == 0);
   if (!_691)
   {
      float _695 = c_tone_control.x * _676;
      float _696 = c_tone_control.x * _677;
      float _697 = c_tone_control.x * _678;
      float _699 = log2(_695);
      float _700 = log2(_696);
      float _701 = log2(_697);
      float _702 = _699 * c_tone_control.y;
      float _703 = _700 * c_tone_control.y;
      float _704 = _701 * c_tone_control.y;
      float _705 = exp2(_702);
      float _706 = exp2(_703);
      float _707 = exp2(_704);
      _757 = _705;
      _758 = _706;
      _759 = _707;
   }
   else
   {
      int _709 = c_bool & 32768;
      bool _710 = (_709 == 0);
      if (_710)
      {
         float _714 = c_tone_control.x * _676;
         float _715 = c_tone_control.x * _677;
         float _716 = c_tone_control.x * _678;
         float _717 = _714 * 2.0f;
         float _718 = _715 * 2.0f;
         float _719 = _716 * 2.0f;
         float _720 = _714 * 0.30000001192092896f;
         float _721 = _715 * 0.30000001192092896f;
         float _722 = _716 * 0.30000001192092896f;
         float _723 = _720 + 0.05000000074505806f;
         float _724 = _721 + 0.05000000074505806f;
         float _725 = _722 + 0.05000000074505806f;
         float _726 = _723 * _717;
         float _727 = _724 * _718;
         float _728 = _725 * _719;
         float _729 = _726 + 0.004000000189989805f;
         float _730 = _727 + 0.004000000189989805f;
         float _731 = _728 + 0.004000000189989805f;
         float _732 = _720 + 0.5f;
         float _733 = _721 + 0.5f;
         float _734 = _722 + 0.5f;
         float _735 = _732 * _717;
         float _736 = _733 * _718;
         float _737 = _734 * _719;
         float _738 = _735 + 0.06000000238418579f;
         float _739 = _736 + 0.06000000238418579f;
         float _740 = _737 + 0.06000000238418579f;
         float _741 = _729 / _738;
         float _742 = _730 / _739;
         float _743 = _731 / _740;
         float _744 = _741 + -0.06666666269302368f;
         float _745 = _742 + -0.06666666269302368f;
         float _746 = _743 + -0.06666666269302368f;
         float _747 = _744 * 1.3790643215179443f;
         float _748 = _745 * 1.3790643215179443f;
         float _749 = _746 * 1.3790643215179443f;
         float _750 = YKR_MaterialCurve(_747, _714);
         float _751 = YKR_MaterialCurve(_748, _715);
         float _752 = YKR_MaterialCurve(_749, _716);
         float _753 = sqrt(_750);
         float _754 = sqrt(_751);
         float _755 = sqrt(_752);
         _757 = _753;
         _758 = _754;
         _759 = _755;
      }
      else
      {
         _757 = _676;
         _758 = _677;
         _759 = _678;
      }
   }
   int _760 = c_bool & 4;
   bool _761 = (_760 == 0);
   if (_761)
   {
      float _763 = saturate(COLOR_1.x);
      float _768 = (c_fog_param[0].x) - _757;
      float _769 = (c_fog_param[0].y) - _758;
      float _770 = (c_fog_param[0].z) - _759;
      float _771 = _768 * _763;
      float _772 = _769 * _763;
      float _773 = _770 * _763;
      _775 = _771;
      _776 = _772;
      _777 = _773;
   }
   else
   {
      _775 = COLOR_1.x;
      _776 = COLOR_1.y;
      _777 = COLOR_1.z;
   }
   float _778 = _775 + _757;
   float _779 = _776 + _758;
   float _780 = _777 + _759;
   float _783 = _687 - c_mat_punchthrough_ref;
   bool _784 = (_783 < 0.0f);
   if (_784)
      discard;
   float _790 = (c_ps_col_scl[0].x) * _778;
   float _791 = (c_ps_col_scl[0].y) * _779;
   float _792 = (c_ps_col_scl[0].z) * _780;
   float _793 = (c_ps_col_scl[0].w) * _687;
   SV_Target.x = _790;
   SV_Target.y = _791;
   SV_Target.z = _792;
   SV_Target.w = _793;
   int _796 = c_bool & 8;
   bool _797 = (_796 != 0);
   float _798 = select(_797, _778, _790);
   float _799 = select(_797, _779, _791);
   float _800 = select(_797, _780, _792);
   float _801 = select(_797, _687, _793);
   SV_Target.x = _798;
   SV_Target.y = _799;
   SV_Target.z = _800;
   SV_Target.w = _801;
   return SV_Target;
}
