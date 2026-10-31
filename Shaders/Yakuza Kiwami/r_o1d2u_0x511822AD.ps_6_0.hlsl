// Yakuza Kiwami material r_o1d2u (0x511822AD, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _41 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _46 = _41.x * _41.x;
   float _47 = _41.y * _41.y;
   float _48 = _41.z * _41.z;
   float4 _49 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _51 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _52 = rsqrt(_51);
   float _53 = _52 * TEXCOORD_2.x;
   float _54 = _52 * TEXCOORD_2.y;
   float _55 = _52 * TEXCOORD_2.z;
   float _56 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _57 = rsqrt(_56);
   float _58 = _57 * TEXCOORD_3.x;
   float _59 = _57 * TEXCOORD_3.y;
   float _60 = _57 * TEXCOORD_3.z;
   float _65 = -0.0f - (c_light_vec[0].x);
   float _66 = -0.0f - (c_light_vec[0].y);
   float _67 = -0.0f - (c_light_vec[0].z);
   float _68 = dot(float3(_53, _54, _55), float3(_65, _66, _67));
   float _72 = c_rt_size.z * SV_Position.x;
   float _73 = c_rt_size.w * SV_Position.y;
   int _76 = c_bool & 8;
   bool _77 = (_76 == 0);
   float _84;
   float _85;
   float _86;
   float _98;
   float _99;
   float _135;
   float _136;
   float _137;
   float _138;
   float _152;
   float _153;
   float _188;
   float _189;
   float _190;
   float _224;
   float _225;
   float _226;
   float _227;
   float _228;
   float _229;
   float _383;
   float _384;
   float _385;
   float _386;
   float _387;
   float _388;
   float _560;
   float _561;
   float _562;
   float _563;
   float _564;
   float _565;
   float _641;
   float _642;
   float _643;
   float _659;
   float _660;
   float _661;
   if (!_77)
   {
      float4 _79 = s13_t.Sample(s13_s, float2(_72, _73));
      _84 = _79.x;
      _85 = _79.y;
      _86 = _79.w;
   }
   else
   {
      _84 = 1.0f;
      _85 = 1.0f;
      _86 = 1.0f;
   }
   if (!_77)
   {
      float _91 = (c_sdw_param[0].x) * _84;
      float _92 = (c_sdw_param[0].z) * _85;
      float _95 = _91 + (c_sdw_param[0].y);
      float _96 = _92 + (c_sdw_param[0].w);
      _98 = _95;
      _99 = _96;
   }
   else
   {
      _98 = _84;
      _99 = _85;
   }
   float _103 = c_rt_size.z * SV_Position.x;
   float _104 = c_rt_size.w * SV_Position.y;
   float4 _105 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_103, _104)));
   int _112 = c_bool & 16384;
   bool _113 = (_112 == 0);
   if (!_113)
   {
      float _115 = _105.x * 2.0f;
      float _116 = _105.y * 2.0f;
      float _117 = _105.z * 2.0f;
      float _118 = _105.w * 2.0f;
      _135 = _115;
      _136 = _116;
      _137 = _117;
      _138 = _118;
   }
   else
   {
      int _120 = c_bool & 8192;
      bool _121 = (_120 == 0);
      if (!_121)
      {
         float _123 = _105.x * 2.0f;
         float _124 = _105.y * 2.0f;
         float _125 = _105.z * 2.0f;
         float _126 = _105.w * 2.0f;
         _135 = _123;
         _136 = _124;
         _137 = _125;
         _138 = _126;
      }
      else
      {
         float _130 = _105.x / c_tone_control.z;
         float _131 = _105.y / c_tone_control.z;
         float _132 = _105.z / c_tone_control.z;
         float _133 = _105.w / c_tone_control.z;
         _135 = _130;
         _136 = _131;
         _137 = _132;
         _138 = _133;
      }
   }
   int _139 = c_bool & 8;
   bool _140 = (_139 == 0);
   if (!_140)
   {
      float _145 = (c_sdw_param[0].x) * _98;
      float _146 = (c_sdw_param[0].z) * _99;
      float _149 = _145 + (c_sdw_param[0].y);
      float _150 = _146 + (c_sdw_param[0].w);
      _152 = _149;
      _153 = _150;
   }
   else
   {
      _152 = _98;
      _153 = _99;
   }
   float _154 = _153 * _152;
   float _159 = 1.0f - (c_light_col[5].x);
   float _160 = 1.0f - (c_light_col[5].y);
   float _161 = 1.0f - (c_light_col[5].z);
   float _162 = _159 * _154;
   float _163 = _160 * _154;
   float _164 = _161 * _154;
   float _165 = _162 + (c_light_col[5].x);
   float _166 = _163 + (c_light_col[5].y);
   float _167 = _164 + (c_light_col[5].z);
   float _168 = _86 * COLOR.x;
   float _169 = _86 * COLOR.y;
   float _170 = _86 * COLOR.z;
   if (!_140)
   {
      float _174 = (c_sdw_param[2].x) * _165;
      float _175 = (c_sdw_param[2].x) * _166;
      float _176 = (c_sdw_param[2].x) * _167;
      float _178 = _174 + (c_sdw_param[2].y);
      float _179 = _175 + (c_sdw_param[2].y);
      float _180 = _176 + (c_sdw_param[2].y);
      float _183 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _184 = _178 * _183;
      float _185 = _179 * _183;
      float _186 = _180 * _183;
      _188 = _184;
      _189 = _185;
      _190 = _186;
   }
   else
   {
      _188 = _165;
      _189 = _166;
      _190 = _167;
   }
   float _191 = _188 * _86;
   float _192 = _189 * _86;
   float _193 = _190 * _86;
   float _204 = c_mat_emissive.x * _46;
   float _205 = _204 * c_emissive_scale;
   float _206 = c_mat_emissive.y * _47;
   float _207 = _206 * c_emissive_scale;
   float _208 = c_mat_emissive.z * _48;
   float _209 = _208 * c_emissive_scale;
   int _210 = c_bool & 2048;
   bool _211 = (_210 == 0);
   if (_211)
   {
      float _213 = _153 * _135;
      float _214 = _153 * _136;
      float _215 = _153 * _137;
      float _216 = _213 + TEXCOORD_4.x;
      float _217 = _214 + TEXCOORD_4.y;
      float _218 = _215 + TEXCOORD_4.z;
      float _219 = _153 * _138;
      float _220 = _219 + TEXCOORD_5.x;
      float _221 = _219 + TEXCOORD_5.y;
      float _222 = _219 + TEXCOORD_5.z;
      _224 = _220;
      _225 = _221;
      _226 = _222;
      _227 = _216;
      _228 = _217;
      _229 = _218;
   }
   else
   {
      _224 = TEXCOORD_5.x;
      _225 = TEXCOORD_5.y;
      _226 = TEXCOORD_5.z;
      _227 = TEXCOORD_4.x;
      _228 = TEXCOORD_4.y;
      _229 = TEXCOORD_4.z;
   }
   float _234 = -0.0f - (c_light_vec[1].x);
   float _235 = -0.0f - (c_light_vec[1].y);
   float _236 = -0.0f - (c_light_vec[1].z);
   float _237 = dot(float3(_53, _54, _55), float3(_234, _235, _236));
   float _238 = saturate(_237);
   float _243 = saturate(_68);
   float _244 = (c_light_col[0].x) * _243;
   float _245 = (c_light_col[0].y) * _243;
   float _246 = (c_light_col[0].z) * _243;
   float _251 = (c_light_col[1].x) * _238;
   float _252 = (c_light_col[1].y) * _238;
   float _253 = (c_light_col[1].z) * _238;
   float _262 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _263 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _264 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _265 = _244 * _165;
   float _266 = _245 * _166;
   float _267 = _246 * _167;
   float _268 = _251 * _153;
   float _269 = _252 * _153;
   float _270 = _253 * _153;
   float _271 = _268 + _265;
   float _272 = _269 + _266;
   float _273 = _270 + _267;
   float _274 = _262 * _271;
   float _275 = _263 * _272;
   float _276 = _264 * _273;
   float _277 = _205 + c_scene_ambient.x;
   float _278 = _277 + _274;
   float _279 = _207 + c_scene_ambient.y;
   float _280 = _279 + _275;
   float _281 = _209 + c_scene_ambient.z;
   float _282 = _281 + _276;
   float _283 = _278 * _168;
   float _284 = _280 * _169;
   float _285 = _282 * _170;
   float _286 = _227 * _191;
   float _287 = _228 * _192;
   float _288 = _229 * _193;
   float _289 = _283 + _286;
   float _290 = _284 + _287;
   float _291 = _285 + _288;
   float _296 = (c_light_vec[0].x) + _58;
   float _297 = (c_light_vec[0].y) + _59;
   float _298 = (c_light_vec[0].z) + _60;
   float _299 = dot(float3(_296, _297, _298), float3(_296, _297, _298));
   float _300 = rsqrt(_299);
   float _301 = _296 * _300;
   float _302 = -0.0f - _301;
   float _303 = _297 * _300;
   float _304 = -0.0f - _303;
   float _305 = _298 * _300;
   float _306 = -0.0f - _305;
   float _307 = dot(float3(_302, _304, _306), float3(_53, _54, _55));
   float _308 = saturate(_307);
   float _309 = _59 + -0.30000001192092896f;
   float _310 = dot(float3(_58, _309, _60), float3(_58, _309, _60));
   float _311 = rsqrt(_310);
   float _312 = _58 * _311;
   float _313 = -0.0f - _312;
   float _314 = _309 * _311;
   float _315 = -0.0f - _314;
   float _316 = _60 * _311;
   float _317 = -0.0f - _316;
   float _318 = dot(float3(_313, _315, _317), float3(_53, _54, _55));
   float _319 = saturate(_318);
   float _322 = log2(_308);
   float _323 = log2(_319);
   float _324 = _322 * c_mat_power;
   float _325 = _323 * c_mat_power;
   float _326 = exp2(_324);
   float _327 = exp2(_325);
   float _330 = _326 * (c_light_col[0].x);
   float _331 = _326 * (c_light_col[0].y);
   float _332 = _326 * (c_light_col[0].z);
   float _333 = _327 * 2.0f;
   float _334 = _333 * (c_light_vec[5].x);
   float _335 = _334 * (c_light_col[1].x);
   float _336 = _334 * (c_light_col[1].y);
   float _337 = _334 * (c_light_col[1].z);
   float _338 = _330 * _165;
   float _339 = _331 * _166;
   float _340 = _332 * _167;
   float _341 = _335 * _238;
   float _342 = _336 * _238;
   float _343 = _337 * _238;
   float _344 = _341 + _338;
   float _345 = _342 + _339;
   float _346 = _343 + _340;
   float _353 = c_mat_specular.x * c_specular_scale.x;
   float _354 = _353 * _168;
   float _355 = _354 * _344;
   float _356 = c_mat_specular.y * c_specular_scale.y;
   float _357 = _356 * _169;
   float _358 = _357 * _345;
   float _359 = c_mat_specular.z * c_specular_scale.z;
   float _360 = _359 * _170;
   float _361 = _360 * _346;
   float _362 = _224 * _191;
   float _363 = _362 * _327;
   float _364 = _225 * _192;
   float _365 = _364 * _327;
   float _366 = _226 * _193;
   float _367 = _366 * _327;
   float _368 = _355 + _363;
   float _369 = _358 + _365;
   float _370 = _361 + _367;
   if (!_211)
   {
      float _372 = _153 * _135;
      float _373 = _153 * _136;
      float _374 = _153 * _137;
      float _375 = _289 + _372;
      float _376 = _290 + _373;
      float _377 = _291 + _374;
      float _378 = _153 * _138;
      float _379 = _368 + _378;
      float _380 = _369 + _378;
      float _381 = _370 + _378;
      _383 = _375;
      _384 = _376;
      _385 = _377;
      _386 = _379;
      _387 = _380;
      _388 = _381;
   }
   else
   {
      _383 = _289;
      _384 = _290;
      _385 = _291;
      _386 = _368;
      _387 = _369;
      _388 = _370;
   }
   int _389 = c_bool & 128;
   bool _390 = (_389 != 0);
   float _391 = select(_390, 0.0f, _386);
   float _392 = select(_390, 0.0f, _387);
   float _393 = select(_390, 0.0f, _388);
   float _394 = _383 * _46;
   float _395 = _384 * _47;
   float _396 = _385 * _48;
   float _397 = _391 * _49.y;
   float _398 = _392 * _49.y;
   float _399 = _393 * _49.y;
   int _400 = c_bool & 32768;
   bool _401 = (_400 == 0);
   if (!_401)
   {
      float _405 = (c_ps_user[11].x) * _46;
      float _406 = (c_ps_user[11].x) * _47;
      float _407 = (c_ps_user[11].x) * _48;
      float _409 = _53 + 1.0f;
      float _410 = _54 + 1.0f;
      float _411 = _55 + 1.0f;
      float _412 = _409 * 0.5f;
      float _413 = _410 * 0.5f;
      float _414 = _411 * 0.5f;
      float _415 = saturate(_412);
      float _416 = saturate(_413);
      float _417 = saturate(_414);
      float _418 = (c_ps_user[11].y) * _415;
      float _419 = _416 * (c_ps_user[11].y);
      float _420 = _417 * (c_ps_user[11].y);
      float _422 = (c_ps_user[11].z) * _49.y;
      float _424 = c_mat_power * 0.009999999776482582f;
      float _425 = _424 * (c_ps_user[11].w);
      float _428 = (c_ps_user[12].x) * _165;
      float _429 = (c_ps_user[12].x) * _166;
      float _430 = (c_ps_user[12].x) * _167;
      float _432 = (c_ps_user[12].y) * _168;
      float _433 = (c_ps_user[12].y) * _169;
      float _434 = (c_ps_user[12].y) * _170;
      float _436 = (c_ps_user[12].z) * _191;
      float _437 = (c_ps_user[12].z) * _192;
      float _438 = (c_ps_user[12].z) * _193;
      float _444 = -0.0f - _58;
      float _445 = -0.0f - _59;
      float _446 = -0.0f - _60;
      float _447 = dot(float3(_444, _445, _446), float3(_53, _54, _55));
      float _448 = 1.0f - _447;
      float _449 = _448 * _448;
      float _450 = _448 * 0.9722222089767456f;
      float _451 = _449 * _449;
      float _452 = _451 * _450;
      float _453 = _452 + 0.02777777425944805f;
      float _454 = _453 * (c_ps_user[13].z);
      float _456 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _457 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _458 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _461 = (c_ps_user[14].x) * _205;
      float _462 = (c_ps_user[14].x) * _207;
      float _463 = (c_ps_user[14].x) * _209;
      float _465 = (c_ps_user[14].y) * _244;
      float _466 = (c_ps_user[14].y) * _245;
      float _467 = (c_ps_user[14].y) * _246;
      float _469 = (c_ps_user[14].z) * _251;
      float _470 = (c_ps_user[14].z) * _252;
      float _471 = (c_ps_user[14].z) * _253;
      float _473 = (c_ps_user[14].w) * _227;
      float _474 = (c_ps_user[14].w) * _228;
      float _475 = (c_ps_user[14].w) * _229;
      float _478 = (c_ps_user[15].x) * _330;
      float _479 = (c_ps_user[15].x) * _331;
      float _480 = (c_ps_user[15].x) * _332;
      float _482 = _353 * _335;
      float _483 = _482 * (c_ps_user[15].y);
      float _484 = _356 * _336;
      float _485 = _484 * (c_ps_user[15].y);
      float _486 = _359 * _337;
      float _487 = _486 * (c_ps_user[15].y);
      float _489 = (c_ps_user[15].z) * _224;
      float _490 = (c_ps_user[15].z) * _225;
      float _491 = (c_ps_user[15].z) * _226;
      float _492 = _483 + _489;
      float _493 = _492 * _327;
      float _494 = _465 + (c_ps_user[13].x);
      float _495 = _494 + _469;
      float _496 = c_diffuse_scale.x * _495;
      float _497 = _496 * c_mat_diffuse.x;
      float _498 = (c_ps_user[13].y) + _422;
      float _499 = _498 + _478;
      float _500 = c_mat_specular.x * _499;
      float _501 = _500 * c_specular_scale.x;
      float _502 = _418 + _405;
      float _503 = _502 + _425;
      float _504 = _503 + (c_ps_user[12].w);
      float _505 = _504 + _428;
      float _506 = _505 + _432;
      float _507 = _506 + _436;
      float _508 = _507 + _456;
      float _509 = _508 + _461;
      float _510 = _509 + _473;
      float _511 = _510 + _454;
      float _512 = _511 + _493;
      float _513 = _512 + _497;
      float _514 = _513 + _501;
      float _515 = _485 + _490;
      float _516 = _515 * _327;
      float _517 = _466 + (c_ps_user[13].x);
      float _518 = _517 + _470;
      float _519 = c_diffuse_scale.y * _518;
      float _520 = _519 * c_mat_diffuse.y;
      float _521 = _498 + _479;
      float _522 = c_mat_specular.y * _521;
      float _523 = _522 * c_specular_scale.y;
      float _524 = _419 + _406;
      float _525 = _524 + _425;
      float _526 = _525 + (c_ps_user[12].w);
      float _527 = _526 + _429;
      float _528 = _527 + _433;
      float _529 = _528 + _437;
      float _530 = _529 + _457;
      float _531 = _530 + _462;
      float _532 = _531 + _474;
      float _533 = _532 + _454;
      float _534 = _533 + _516;
      float _535 = _534 + _520;
      float _536 = _535 + _523;
      float _537 = _487 + _491;
      float _538 = _537 * _327;
      float _539 = _467 + (c_ps_user[13].x);
      float _540 = _539 + _471;
      float _541 = c_diffuse_scale.z * _540;
      float _542 = _541 * c_mat_diffuse.z;
      float _543 = _498 + _480;
      float _544 = c_mat_specular.z * _543;
      float _545 = _544 * c_specular_scale.z;
      float _546 = _425 + _407;
      float _547 = _546 + _420;
      float _548 = _547 + (c_ps_user[12].w);
      float _549 = _548 + _430;
      float _550 = _549 + _434;
      float _551 = _550 + _438;
      float _552 = _551 + _458;
      float _553 = _552 + _463;
      float _554 = _553 + _475;
      float _555 = _554 + _454;
      float _556 = _555 + _538;
      float _557 = _556 + _542;
      float _558 = _557 + _545;
      _560 = _514;
      _561 = _536;
      _562 = _558;
      _563 = 0.0f;
      _564 = 0.0f;
      _565 = 0.0f;
   }
   else
   {
      _560 = _394;
      _561 = _395;
      _562 = _396;
      _563 = _397;
      _564 = _398;
      _565 = _399;
   }
   float _566 = _563 + _560;
   float _567 = _564 + _561;
   float _568 = _565 + _562;
   float _573 = c_mat_opacity * _41.w;
   float _574 = _573 * c_global_opacity;
   float _575 = _574 * c_opacity_scale;
   int _576 = c_bool & 16;
   bool _577 = (_576 == 0);
   if (!_577)
   {
      float _581 = c_tone_control.x * _566;
      float _582 = c_tone_control.x * _567;
      float _583 = c_tone_control.x * _568;
      float _585 = log2(_581);
      float _586 = log2(_582);
      float _587 = log2(_583);
      float _588 = _585 * c_tone_control.y;
      float _589 = _586 * c_tone_control.y;
      float _590 = _587 * c_tone_control.y;
      float _591 = exp2(_588);
      float _592 = exp2(_589);
      float _593 = exp2(_590);
      _641 = _591;
      _642 = _592;
      _643 = _593;
   }
   else
   {
      if (_401)
      {
         float _598 = c_tone_control.x * _566;
         float _599 = c_tone_control.x * _567;
         float _600 = c_tone_control.x * _568;
         float _601 = _598 * 2.0f;
         float _602 = _599 * 2.0f;
         float _603 = _600 * 2.0f;
         float _604 = _598 * 0.30000001192092896f;
         float _605 = _599 * 0.30000001192092896f;
         float _606 = _600 * 0.30000001192092896f;
         float _607 = _604 + 0.05000000074505806f;
         float _608 = _605 + 0.05000000074505806f;
         float _609 = _606 + 0.05000000074505806f;
         float _610 = _607 * _601;
         float _611 = _608 * _602;
         float _612 = _609 * _603;
         float _613 = _610 + 0.004000000189989805f;
         float _614 = _611 + 0.004000000189989805f;
         float _615 = _612 + 0.004000000189989805f;
         float _616 = _604 + 0.5f;
         float _617 = _605 + 0.5f;
         float _618 = _606 + 0.5f;
         float _619 = _616 * _601;
         float _620 = _617 * _602;
         float _621 = _618 * _603;
         float _622 = _619 + 0.06000000238418579f;
         float _623 = _620 + 0.06000000238418579f;
         float _624 = _621 + 0.06000000238418579f;
         float _625 = _613 / _622;
         float _626 = _614 / _623;
         float _627 = _615 / _624;
         float _628 = _625 + -0.06666666269302368f;
         float _629 = _626 + -0.06666666269302368f;
         float _630 = _627 + -0.06666666269302368f;
         float _631 = _628 * 1.3790643215179443f;
         float _632 = _629 * 1.3790643215179443f;
         float _633 = _630 * 1.3790643215179443f;
         float _634 = YKR_MaterialCurve(_631, _598);
         float _635 = YKR_MaterialCurve(_632, _599);
         float _636 = YKR_MaterialCurve(_633, _600);
         float _637 = sqrt(_634);
         float _638 = sqrt(_635);
         float _639 = sqrt(_636);
         _641 = _637;
         _642 = _638;
         _643 = _639;
      }
      else
      {
         _641 = _566;
         _642 = _567;
         _643 = _568;
      }
   }
   int _644 = c_bool & 4;
   bool _645 = (_644 == 0);
   if (_645)
   {
      float _647 = saturate(COLOR_1.x);
      float _652 = (c_fog_param[0].x) - _641;
      float _653 = (c_fog_param[0].y) - _642;
      float _654 = (c_fog_param[0].z) - _643;
      float _655 = _652 * _647;
      float _656 = _653 * _647;
      float _657 = _654 * _647;
      _659 = _655;
      _660 = _656;
      _661 = _657;
   }
   else
   {
      _659 = COLOR_1.x;
      _660 = COLOR_1.y;
      _661 = COLOR_1.z;
   }
   float _662 = _659 + _641;
   float _663 = _660 + _642;
   float _664 = _661 + _643;
   float _670 = (c_ps_col_scl[0].x) * _662;
   float _671 = (c_ps_col_scl[0].y) * _663;
   float _672 = (c_ps_col_scl[0].z) * _664;
   float _673 = (c_ps_col_scl[0].w) * _575;
   SV_Target.x = _670;
   SV_Target.y = _671;
   SV_Target.z = _672;
   SV_Target.w = _673;
   int _676 = c_bool & 8;
   bool _677 = (_676 != 0);
   float _678 = select(_677, _662, _670);
   float _679 = select(_677, _663, _671);
   float _680 = select(_677, _664, _672);
   float _681 = select(_677, _575, _673);
   SV_Target.x = _678;
   SV_Target.y = _679;
   SV_Target.z = _680;
   SV_Target.w = _681;
   return SV_Target;
}
