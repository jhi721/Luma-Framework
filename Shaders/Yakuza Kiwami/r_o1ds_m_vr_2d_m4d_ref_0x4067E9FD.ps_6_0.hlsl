// Yakuza Kiwami material r_o1ds_m(vr)2d_m4d[ref] (0x4067E9FD, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _69 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _73 = _69.x * _69.x;
   float _74 = _69.y * _69.y;
   float _75 = _69.z * _69.z;
   float4 _76 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _79 = 1.0f - _73;
   float _80 = 1.0f - _74;
   float _81 = 1.0f - _75;
   float _82 = _79 * COLOR_1.w;
   float _83 = _80 * COLOR_1.w;
   float _84 = _81 * COLOR_1.w;
   float _85 = 1.0f - _82;
   float _86 = 1.0f - _83;
   float _87 = 1.0f - _84;
   float _88 = _56 * _85;
   float _89 = _57 * _86;
   float _90 = _58 * _87;
   float _91 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _92 = rsqrt(_91);
   float _93 = _92 * TEXCOORD_2.x;
   float _94 = _92 * TEXCOORD_2.y;
   float _95 = _92 * TEXCOORD_2.z;
   float _96 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _97 = rsqrt(_96);
   float _98 = _97 * TEXCOORD_3.x;
   float _99 = _97 * TEXCOORD_3.y;
   float _100 = _97 * TEXCOORD_3.z;
   float _105 = -0.0f - (c_light_vec[0].x);
   float _106 = -0.0f - (c_light_vec[0].y);
   float _107 = -0.0f - (c_light_vec[0].z);
   float _108 = dot(float3(_93, _94, _95), float3(_105, _106, _107));
   float _112 = c_rt_size.z * SV_Position.x;
   float _113 = c_rt_size.w * SV_Position.y;
   int _116 = c_bool & 8;
   bool _117 = (_116 == 0);
   float _124;
   float _125;
   float _126;
   float _138;
   float _139;
   float _194;
   float _195;
   float _196;
   float _197;
   float _211;
   float _212;
   float _258;
   float _259;
   float _260;
   float _295;
   float _296;
   float _297;
   float _298;
   float _299;
   float _300;
   float _454;
   float _455;
   float _456;
   float _457;
   float _458;
   float _459;
   float _635;
   float _636;
   float _637;
   float _638;
   float _639;
   float _640;
   float _768;
   float _769;
   float _770;
   float _786;
   float _787;
   float _788;
   if (!_117)
   {
      float4 _119 = s13_t.Sample(s13_s, float2(_112, _113));
      _124 = _119.x;
      _125 = _119.y;
      _126 = _119.w;
   }
   else
   {
      _124 = 1.0f;
      _125 = 1.0f;
      _126 = 1.0f;
   }
   if (!_117)
   {
      float _131 = (c_sdw_param[0].x) * _124;
      float _132 = (c_sdw_param[0].z) * _125;
      float _135 = _131 + (c_sdw_param[0].y);
      float _136 = _132 + (c_sdw_param[0].w);
      _138 = _135;
      _139 = _136;
   }
   else
   {
      _138 = _124;
      _139 = _125;
   }
   float _146 = (c_sdw_param[1].x) * _76.y;
   float _148 = _146 + (c_sdw_param[1].y);
   float _149 = _148 * _76.x;
   float _150 = 1.0f - (c_light_col[5].x);
   float _151 = 1.0f - (c_light_col[5].y);
   float _152 = 1.0f - (c_light_col[5].z);
   float _153 = _149 * _150;
   float _154 = _149 * _151;
   float _155 = _149 * _152;
   float _156 = _153 + (c_light_col[5].x);
   float _157 = _154 + (c_light_col[5].y);
   float _158 = _155 + (c_light_col[5].z);
   float _162 = c_rt_size.z * SV_Position.x;
   float _163 = c_rt_size.w * SV_Position.y;
   float4 _164 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_162, _163)));
   int _171 = c_bool & 16384;
   bool _172 = (_171 == 0);
   if (!_172)
   {
      float _174 = _164.x * 2.0f;
      float _175 = _164.y * 2.0f;
      float _176 = _164.z * 2.0f;
      float _177 = _164.w * 2.0f;
      _194 = _174;
      _195 = _175;
      _196 = _176;
      _197 = _177;
   }
   else
   {
      int _179 = c_bool & 8192;
      bool _180 = (_179 == 0);
      if (!_180)
      {
         float _182 = _164.x * 2.0f;
         float _183 = _164.y * 2.0f;
         float _184 = _164.z * 2.0f;
         float _185 = _164.w * 2.0f;
         _194 = _182;
         _195 = _183;
         _196 = _184;
         _197 = _185;
      }
      else
      {
         float _189 = _164.x / c_tone_control.z;
         float _190 = _164.y / c_tone_control.z;
         float _191 = _164.z / c_tone_control.z;
         float _192 = _164.w / c_tone_control.z;
         _194 = _189;
         _195 = _190;
         _196 = _191;
         _197 = _192;
      }
   }
   int _198 = c_bool & 8;
   bool _199 = (_198 == 0);
   if (!_199)
   {
      float _204 = (c_sdw_param[0].x) * _138;
      float _205 = (c_sdw_param[0].z) * _139;
      float _208 = _204 + (c_sdw_param[0].y);
      float _209 = _205 + (c_sdw_param[0].w);
      _211 = _208;
      _212 = _209;
   }
   else
   {
      _211 = _138;
      _212 = _139;
   }
   float _213 = _212 * _211;
   float _218 = 1.0f - (c_light_col[5].x);
   float _219 = 1.0f - (c_light_col[5].y);
   float _220 = 1.0f - (c_light_col[5].z);
   float _221 = _218 * _213;
   float _222 = _219 * _213;
   float _223 = _220 * _213;
   float _224 = _221 + (c_light_col[5].x);
   float _225 = _222 + (c_light_col[5].y);
   float _226 = _223 + (c_light_col[5].z);
   float _227 = _126 * COLOR.x;
   float _228 = _227 * _156;
   float _229 = _126 * COLOR.y;
   float _230 = _229 * _157;
   float _231 = _126 * COLOR.z;
   float _232 = _231 * _158;
   float _233 = _224 * _156;
   float _234 = _225 * _157;
   float _235 = _226 * _158;
   if (!_199)
   {
      float _239 = (c_sdw_param[2].x) * _224;
      float _240 = (c_sdw_param[2].x) * _225;
      float _241 = (c_sdw_param[2].x) * _226;
      float _243 = _239 + (c_sdw_param[2].y);
      float _244 = _240 + (c_sdw_param[2].y);
      float _245 = _241 + (c_sdw_param[2].y);
      float _247 = (c_sdw_param[2].z) * _156;
      float _248 = (c_sdw_param[2].z) * _157;
      float _249 = (c_sdw_param[2].z) * _158;
      float _251 = _247 + (c_sdw_param[2].w);
      float _252 = _248 + (c_sdw_param[2].w);
      float _253 = _249 + (c_sdw_param[2].w);
      float _254 = _251 * _243;
      float _255 = _252 * _244;
      float _256 = _253 * _245;
      _258 = _254;
      _259 = _255;
      _260 = _256;
   }
   else
   {
      _258 = _233;
      _259 = _234;
      _260 = _235;
   }
   float _261 = _126 * _76.x;
   float _262 = _258 * _261;
   float _263 = _259 * _261;
   float _264 = _260 * _261;
   float _275 = c_mat_emissive.x * _88;
   float _276 = _275 * c_emissive_scale;
   float _277 = c_mat_emissive.y * _89;
   float _278 = _277 * c_emissive_scale;
   float _279 = c_mat_emissive.z * _90;
   float _280 = _279 * c_emissive_scale;
   int _281 = c_bool & 2048;
   bool _282 = (_281 == 0);
   if (_282)
   {
      float _284 = _212 * _194;
      float _285 = _212 * _195;
      float _286 = _212 * _196;
      float _287 = _284 + TEXCOORD_4.x;
      float _288 = _285 + TEXCOORD_4.y;
      float _289 = _286 + TEXCOORD_4.z;
      float _290 = _212 * _197;
      float _291 = _290 + TEXCOORD_5.x;
      float _292 = _290 + TEXCOORD_5.y;
      float _293 = _290 + TEXCOORD_5.z;
      _295 = _287;
      _296 = _288;
      _297 = _289;
      _298 = _291;
      _299 = _292;
      _300 = _293;
   }
   else
   {
      _295 = TEXCOORD_4.x;
      _296 = TEXCOORD_4.y;
      _297 = TEXCOORD_4.z;
      _298 = TEXCOORD_5.x;
      _299 = TEXCOORD_5.y;
      _300 = TEXCOORD_5.z;
   }
   float _305 = -0.0f - (c_light_vec[1].x);
   float _306 = -0.0f - (c_light_vec[1].y);
   float _307 = -0.0f - (c_light_vec[1].z);
   float _308 = dot(float3(_93, _94, _95), float3(_305, _306, _307));
   float _309 = saturate(_308);
   float _314 = saturate(_108);
   float _315 = (c_light_col[0].x) * _314;
   float _316 = (c_light_col[0].y) * _314;
   float _317 = (c_light_col[0].z) * _314;
   float _322 = (c_light_col[1].x) * _309;
   float _323 = (c_light_col[1].y) * _309;
   float _324 = (c_light_col[1].z) * _309;
   float _333 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _334 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _335 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _336 = _315 * _224;
   float _337 = _316 * _225;
   float _338 = _317 * _226;
   float _339 = _322 * _212;
   float _340 = _323 * _212;
   float _341 = _324 * _212;
   float _342 = _339 + _336;
   float _343 = _340 + _337;
   float _344 = _341 + _338;
   float _345 = _333 * _342;
   float _346 = _334 * _343;
   float _347 = _335 * _344;
   float _348 = _276 + c_scene_ambient.x;
   float _349 = _348 + _345;
   float _350 = _278 + c_scene_ambient.y;
   float _351 = _350 + _346;
   float _352 = _280 + c_scene_ambient.z;
   float _353 = _352 + _347;
   float _354 = _349 * _228;
   float _355 = _351 * _230;
   float _356 = _353 * _232;
   float _357 = _295 * _262;
   float _358 = _296 * _263;
   float _359 = _297 * _264;
   float _360 = _354 + _357;
   float _361 = _355 + _358;
   float _362 = _356 + _359;
   float _367 = (c_light_vec[0].x) + _98;
   float _368 = (c_light_vec[0].y) + _99;
   float _369 = (c_light_vec[0].z) + _100;
   float _370 = dot(float3(_367, _368, _369), float3(_367, _368, _369));
   float _371 = rsqrt(_370);
   float _372 = _367 * _371;
   float _373 = -0.0f - _372;
   float _374 = _368 * _371;
   float _375 = -0.0f - _374;
   float _376 = _369 * _371;
   float _377 = -0.0f - _376;
   float _378 = dot(float3(_373, _375, _377), float3(_93, _94, _95));
   float _379 = saturate(_378);
   float _380 = _99 + -0.30000001192092896f;
   float _381 = dot(float3(_98, _380, _100), float3(_98, _380, _100));
   float _382 = rsqrt(_381);
   float _383 = _98 * _382;
   float _384 = -0.0f - _383;
   float _385 = _380 * _382;
   float _386 = -0.0f - _385;
   float _387 = _100 * _382;
   float _388 = -0.0f - _387;
   float _389 = dot(float3(_384, _386, _388), float3(_93, _94, _95));
   float _390 = saturate(_389);
   float _393 = log2(_379);
   float _394 = log2(_390);
   float _395 = _393 * c_mat_power;
   float _396 = _394 * c_mat_power;
   float _397 = exp2(_395);
   float _398 = exp2(_396);
   float _401 = _397 * (c_light_col[0].x);
   float _402 = _397 * (c_light_col[0].y);
   float _403 = _397 * (c_light_col[0].z);
   float _404 = _398 * 2.0f;
   float _405 = _404 * (c_light_vec[5].x);
   float _406 = _405 * (c_light_col[1].x);
   float _407 = _405 * (c_light_col[1].y);
   float _408 = _405 * (c_light_col[1].z);
   float _409 = _401 * _224;
   float _410 = _402 * _225;
   float _411 = _403 * _226;
   float _412 = _406 * _309;
   float _413 = _407 * _309;
   float _414 = _408 * _309;
   float _415 = _412 + _409;
   float _416 = _413 + _410;
   float _417 = _414 + _411;
   float _424 = c_mat_specular.x * c_specular_scale.x;
   float _425 = _424 * _228;
   float _426 = _425 * _415;
   float _427 = c_mat_specular.y * c_specular_scale.y;
   float _428 = _427 * _230;
   float _429 = _428 * _416;
   float _430 = c_mat_specular.z * c_specular_scale.z;
   float _431 = _430 * _232;
   float _432 = _431 * _417;
   float _433 = _298 * _262;
   float _434 = _433 * _398;
   float _435 = _299 * _263;
   float _436 = _435 * _398;
   float _437 = _300 * _264;
   float _438 = _437 * _398;
   float _439 = _426 + _434;
   float _440 = _429 + _436;
   float _441 = _432 + _438;
   if (!_282)
   {
      float _443 = _212 * _194;
      float _444 = _212 * _195;
      float _445 = _212 * _196;
      float _446 = _360 + _443;
      float _447 = _361 + _444;
      float _448 = _362 + _445;
      float _449 = _212 * _197;
      float _450 = _439 + _449;
      float _451 = _440 + _449;
      float _452 = _441 + _449;
      _454 = _446;
      _455 = _447;
      _456 = _448;
      _457 = _450;
      _458 = _451;
      _459 = _452;
   }
   else
   {
      _454 = _360;
      _455 = _361;
      _456 = _362;
      _457 = _439;
      _458 = _440;
      _459 = _441;
   }
   int _460 = c_bool & 128;
   bool _461 = (_460 != 0);
   float _462 = select(_461, 0.0f, _457);
   float _463 = select(_461, 0.0f, _458);
   float _464 = select(_461, 0.0f, _459);
   float _465 = _454 * _88;
   float _466 = _455 * _89;
   float _467 = _456 * _90;
   float _468 = _462 * _64.x;
   float _469 = _463 * _64.y;
   float _470 = _464 * _64.z;
   int _471 = c_bool & 32768;
   bool _472 = (_471 == 0);
   if (!_472)
   {
      float _476 = (c_ps_user[11].x) * _88;
      float _477 = (c_ps_user[11].x) * _89;
      float _478 = (c_ps_user[11].x) * _90;
      float _480 = _93 + 1.0f;
      float _481 = _94 + 1.0f;
      float _482 = _95 + 1.0f;
      float _483 = _480 * 0.5f;
      float _484 = _481 * 0.5f;
      float _485 = _482 * 0.5f;
      float _486 = saturate(_483);
      float _487 = saturate(_484);
      float _488 = saturate(_485);
      float _489 = (c_ps_user[11].y) * _486;
      float _490 = _487 * (c_ps_user[11].y);
      float _491 = _488 * (c_ps_user[11].y);
      float _493 = (c_ps_user[11].z) * _64.x;
      float _494 = (c_ps_user[11].z) * _64.y;
      float _495 = (c_ps_user[11].z) * _64.z;
      float _497 = c_mat_power * 0.009999999776482582f;
      float _498 = _497 * (c_ps_user[11].w);
      float _501 = (c_ps_user[12].x) * _224;
      float _502 = (c_ps_user[12].x) * _225;
      float _503 = (c_ps_user[12].x) * _226;
      float _505 = (c_ps_user[12].y) * _228;
      float _506 = (c_ps_user[12].y) * _230;
      float _507 = (c_ps_user[12].y) * _232;
      float _509 = (c_ps_user[12].z) * _262;
      float _510 = (c_ps_user[12].z) * _263;
      float _511 = (c_ps_user[12].z) * _264;
      float _517 = -0.0f - _98;
      float _518 = -0.0f - _99;
      float _519 = -0.0f - _100;
      float _520 = dot(float3(_517, _518, _519), float3(_93, _94, _95));
      float _521 = 1.0f - _520;
      float _522 = _521 * _521;
      float _523 = _521 * 0.9722222089767456f;
      float _524 = _522 * _522;
      float _525 = _524 * _523;
      float _526 = _525 + 0.02777777425944805f;
      float _527 = _526 * (c_ps_user[13].z);
      float _529 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _530 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _531 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _534 = (c_ps_user[14].x) * _276;
      float _535 = (c_ps_user[14].x) * _278;
      float _536 = (c_ps_user[14].x) * _280;
      float _538 = (c_ps_user[14].y) * _315;
      float _539 = (c_ps_user[14].y) * _316;
      float _540 = (c_ps_user[14].y) * _317;
      float _542 = (c_ps_user[14].z) * _322;
      float _543 = (c_ps_user[14].z) * _323;
      float _544 = (c_ps_user[14].z) * _324;
      float _546 = (c_ps_user[14].w) * _295;
      float _547 = (c_ps_user[14].w) * _296;
      float _548 = (c_ps_user[14].w) * _297;
      float _551 = (c_ps_user[15].x) * _401;
      float _552 = (c_ps_user[15].x) * _402;
      float _553 = (c_ps_user[15].x) * _403;
      float _555 = _424 * _406;
      float _556 = _555 * (c_ps_user[15].y);
      float _557 = _427 * _407;
      float _558 = _557 * (c_ps_user[15].y);
      float _559 = _430 * _408;
      float _560 = _559 * (c_ps_user[15].y);
      float _562 = (c_ps_user[15].z) * _298;
      float _563 = (c_ps_user[15].z) * _299;
      float _564 = (c_ps_user[15].z) * _300;
      float _565 = _556 + _562;
      float _566 = _565 * _398;
      float _567 = _538 + (c_ps_user[13].x);
      float _568 = _567 + _542;
      float _569 = c_diffuse_scale.x * _568;
      float _570 = _569 * c_mat_diffuse.x;
      float _571 = (c_ps_user[13].y) + _493;
      float _572 = _571 + _551;
      float _573 = c_mat_specular.x * _572;
      float _574 = _573 * c_specular_scale.x;
      float _575 = _489 + _476;
      float _576 = _575 + _498;
      float _577 = _576 + (c_ps_user[12].w);
      float _578 = _577 + _501;
      float _579 = _578 + _505;
      float _580 = _579 + _509;
      float _581 = _580 + _529;
      float _582 = _581 + _534;
      float _583 = _582 + _546;
      float _584 = _583 + _527;
      float _585 = _584 + _566;
      float _586 = _585 + _570;
      float _587 = _586 + _574;
      float _588 = _558 + _563;
      float _589 = _588 * _398;
      float _590 = _539 + (c_ps_user[13].x);
      float _591 = _590 + _543;
      float _592 = c_diffuse_scale.y * _591;
      float _593 = _592 * c_mat_diffuse.y;
      float _594 = (c_ps_user[13].y) + _494;
      float _595 = _594 + _552;
      float _596 = c_mat_specular.y * _595;
      float _597 = _596 * c_specular_scale.y;
      float _598 = _490 + _477;
      float _599 = _598 + _498;
      float _600 = _599 + (c_ps_user[12].w);
      float _601 = _600 + _502;
      float _602 = _601 + _506;
      float _603 = _602 + _510;
      float _604 = _603 + _530;
      float _605 = _604 + _535;
      float _606 = _605 + _547;
      float _607 = _606 + _527;
      float _608 = _607 + _589;
      float _609 = _608 + _593;
      float _610 = _609 + _597;
      float _611 = _560 + _564;
      float _612 = _611 * _398;
      float _613 = _540 + (c_ps_user[13].x);
      float _614 = _613 + _544;
      float _615 = c_diffuse_scale.z * _614;
      float _616 = _615 * c_mat_diffuse.z;
      float _617 = (c_ps_user[13].y) + _495;
      float _618 = _617 + _553;
      float _619 = c_mat_specular.z * _618;
      float _620 = _619 * c_specular_scale.z;
      float _621 = _498 + _478;
      float _622 = _621 + _491;
      float _623 = _622 + (c_ps_user[12].w);
      float _624 = _623 + _503;
      float _625 = _624 + _507;
      float _626 = _625 + _511;
      float _627 = _626 + _531;
      float _628 = _627 + _536;
      float _629 = _628 + _548;
      float _630 = _629 + _527;
      float _631 = _630 + _612;
      float _632 = _631 + _616;
      float _633 = _632 + _620;
      _635 = _587;
      _636 = _610;
      _637 = _633;
      _638 = 0.0f;
      _639 = 0.0f;
      _640 = 0.0f;
   }
   else
   {
      _635 = _465;
      _636 = _466;
      _637 = _467;
      _638 = _468;
      _639 = _469;
      _640 = _470;
   }
   float _641 = _638 + _635;
   float _642 = _639 + _636;
   float _643 = _640 + _637;
   float _644 = dot(float3(_98, _99, _100), float3(_93, _94, _95));
   float _645 = _644 * 2.0f;
   float _646 = _645 * _93;
   float _647 = _645 * _94;
   float _648 = _645 * _95;
   float _649 = _98 - _646;
   float _650 = _99 - _647;
   float _651 = _100 - _648;
   float _652 = -0.0f - _649;
   float4 _653 = s6c_t.Sample(s6c_s, float3(_652, _650, _651));
   float _660 = _139 * _138;
   float _661 = _108 + 0.75f;
   float _662 = saturate(_661);
   float _663 = _660 * _662;
   float _664 = max(_663, (c_light_vec[5].y));
   float _665 = max(_653.w, 0.003921568859368563f);
   float _666 = 0.25f / _665;
   float _667 = _666 * _653.x;
   float _668 = _666 * _653.y;
   float _669 = _666 * _653.z;
   float _670 = _64.w * TEXCOORD_2.w;
   float _671 = _76.x * _670;
   float _672 = _671 * _64.x;
   float _673 = _672 * _156;
   float _674 = _673 * _664;
   float _675 = _667 * _667;
   float _676 = _675 * _674;
   float _677 = _671 * _64.y;
   float _678 = _677 * _157;
   float _679 = _678 * _664;
   float _680 = _668 * _668;
   float _681 = _680 * _679;
   float _682 = _671 * _64.z;
   float _683 = _682 * _158;
   float _684 = _683 * _664;
   float _685 = _669 * _669;
   float _686 = _685 * _684;
   float _687 = _641 + _676;
   float _688 = _642 + _681;
   float _689 = _643 + _686;
   float _696 = c_mat_opacity * _51.w;
   float _697 = _696 * c_opacity_scale;
   float _698 = _697 * c_global_opacity;
   int _701 = c_bool & 16;
   bool _702 = (_701 == 0);
   if (!_702)
   {
      float _706 = c_tone_control.x * _687;
      float _707 = c_tone_control.x * _688;
      float _708 = c_tone_control.x * _689;
      float _710 = log2(_706);
      float _711 = log2(_707);
      float _712 = log2(_708);
      float _713 = _710 * c_tone_control.y;
      float _714 = _711 * c_tone_control.y;
      float _715 = _712 * c_tone_control.y;
      float _716 = exp2(_713);
      float _717 = exp2(_714);
      float _718 = exp2(_715);
      _768 = _716;
      _769 = _717;
      _770 = _718;
   }
   else
   {
      int _720 = c_bool & 32768;
      bool _721 = (_720 == 0);
      if (_721)
      {
         float _725 = c_tone_control.x * _687;
         float _726 = c_tone_control.x * _688;
         float _727 = c_tone_control.x * _689;
         float _728 = _725 * 2.0f;
         float _729 = _726 * 2.0f;
         float _730 = _727 * 2.0f;
         float _731 = _725 * 0.30000001192092896f;
         float _732 = _726 * 0.30000001192092896f;
         float _733 = _727 * 0.30000001192092896f;
         float _734 = _731 + 0.05000000074505806f;
         float _735 = _732 + 0.05000000074505806f;
         float _736 = _733 + 0.05000000074505806f;
         float _737 = _734 * _728;
         float _738 = _735 * _729;
         float _739 = _736 * _730;
         float _740 = _737 + 0.004000000189989805f;
         float _741 = _738 + 0.004000000189989805f;
         float _742 = _739 + 0.004000000189989805f;
         float _743 = _731 + 0.5f;
         float _744 = _732 + 0.5f;
         float _745 = _733 + 0.5f;
         float _746 = _743 * _728;
         float _747 = _744 * _729;
         float _748 = _745 * _730;
         float _749 = _746 + 0.06000000238418579f;
         float _750 = _747 + 0.06000000238418579f;
         float _751 = _748 + 0.06000000238418579f;
         float _752 = _740 / _749;
         float _753 = _741 / _750;
         float _754 = _742 / _751;
         float _755 = _752 + -0.06666666269302368f;
         float _756 = _753 + -0.06666666269302368f;
         float _757 = _754 + -0.06666666269302368f;
         float _758 = _755 * 1.3790643215179443f;
         float _759 = _756 * 1.3790643215179443f;
         float _760 = _757 * 1.3790643215179443f;
         float _761 = YKR_MaterialCurve(_758, _725);
         float _762 = YKR_MaterialCurve(_759, _726);
         float _763 = YKR_MaterialCurve(_760, _727);
         float _764 = sqrt(_761);
         float _765 = sqrt(_762);
         float _766 = sqrt(_763);
         _768 = _764;
         _769 = _765;
         _770 = _766;
      }
      else
      {
         _768 = _687;
         _769 = _688;
         _770 = _689;
      }
   }
   int _771 = c_bool & 4;
   bool _772 = (_771 == 0);
   if (_772)
   {
      float _774 = saturate(COLOR_1.x);
      float _779 = (c_fog_param[0].x) - _768;
      float _780 = (c_fog_param[0].y) - _769;
      float _781 = (c_fog_param[0].z) - _770;
      float _782 = _779 * _774;
      float _783 = _780 * _774;
      float _784 = _781 * _774;
      _786 = _782;
      _787 = _783;
      _788 = _784;
   }
   else
   {
      _786 = COLOR_1.x;
      _787 = COLOR_1.y;
      _788 = COLOR_1.z;
   }
   float _789 = _786 + _768;
   float _790 = _787 + _769;
   float _791 = _788 + _770;
   float _797 = (c_ps_col_scl[0].x) * _789;
   float _798 = (c_ps_col_scl[0].y) * _790;
   float _799 = (c_ps_col_scl[0].z) * _791;
   float _800 = (c_ps_col_scl[0].w) * _698;
   SV_Target.x = _797;
   SV_Target.y = _798;
   SV_Target.z = _799;
   SV_Target.w = _800;
   int _803 = c_bool & 8;
   bool _804 = (_803 != 0);
   float _805 = select(_804, _789, _797);
   float _806 = select(_804, _790, _798);
   float _807 = select(_804, _791, _799);
   float _808 = select(_804, _698, _800);
   SV_Target.x = _805;
   SV_Target.y = _806;
   SV_Target.z = _807;
   SV_Target.w = _808;
   return SV_Target;
}
