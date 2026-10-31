// Yakuza Kiwami material r_o1dut_m(vr)2dut_m4d (0xFECBA39E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
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
   float4 _58 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _63 = _58.x * _58.x;
   float _64 = _58.y * _58.y;
   float _65 = _58.z * _58.z;
   float _69 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _70 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _71 = s3_t.Sample(s3_s, float2(_69, _70));
   float _76 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _77 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _78 = s5_t.Sample(s5_s, float2(_76, _77));
   float4 _81 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _86 = _81.x * _81.x;
   float _87 = _81.y * _81.y;
   float _88 = _81.z * _81.z;
   float _92 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _93 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _94 = s4_t.Sample(s4_s, float2(_92, _93));
   float _99 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _100 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _101 = s6_t.Sample(s6_s, float2(_99, _100));
   float4 _104 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _107 = _81.w * COLOR_1.w;
   float _108 = 1.0f - _94.y;
   float _109 = _108 * _107;
   float _110 = 1.0f - _109;
   float _111 = _110 * _71.y;
   float _112 = 1.0f - _86;
   float _113 = 1.0f - _87;
   float _114 = 1.0f - _88;
   float _115 = _112 * _107;
   float _116 = _113 * _107;
   float _117 = _114 * _107;
   float _118 = 1.0f - _115;
   float _119 = 1.0f - _116;
   float _120 = 1.0f - _117;
   float _121 = _63 * _118;
   float _122 = _64 * _119;
   float _123 = _65 * _120;
   float _124 = _101.x - _78.x;
   float _125 = _101.y - _78.y;
   float _126 = _124 * _107;
   float _127 = _125 * _107;
   float _128 = _126 + _78.x;
   float _129 = _127 + _78.y;
   float _130 = _128 * 2.0f;
   float _131 = _129 * 2.0f;
   float _132 = _130 + -1.0f;
   float _133 = _131 + -1.0f;
   float _134 = dot(float2(_132, _133), float2(_132, _133));
   float _135 = 1.0f - _134;
   float _136 = saturate(_135);
   float _137 = sqrt(_136);
   float _138 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _139 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _140 = _138 - _139;
   float _141 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _142 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _143 = _141 - _142;
   float _144 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _145 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _146 = _144 - _145;
   float _147 = _132 * TEXCOORD_6.x;
   float _148 = _132 * TEXCOORD_6.y;
   float _149 = _132 * TEXCOORD_6.z;
   float _150 = _133 * TEXCOORD_6.w;
   float _151 = _150 * _140;
   float _152 = _150 * _143;
   float _153 = _150 * _146;
   float _154 = _151 + _147;
   float _155 = _152 + _148;
   float _156 = _153 + _149;
   float _157 = _137 * TEXCOORD_2.x;
   float _158 = _137 * TEXCOORD_2.y;
   float _159 = _137 * TEXCOORD_2.z;
   float _160 = _154 + _157;
   float _161 = _155 + _158;
   float _162 = _156 + _159;
   float _163 = dot(float3(_160, _161, _162), float3(_160, _161, _162));
   float _164 = rsqrt(_163);
   float _165 = _160 * _164;
   float _166 = _161 * _164;
   float _167 = _162 * _164;
   float _168 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _169 = rsqrt(_168);
   float _170 = _169 * TEXCOORD_3.x;
   float _171 = _169 * TEXCOORD_3.y;
   float _172 = _169 * TEXCOORD_3.z;
   float _177 = -0.0f - (c_light_vec[0].x);
   float _178 = -0.0f - (c_light_vec[0].y);
   float _179 = -0.0f - (c_light_vec[0].z);
   float _180 = dot(float3(_165, _166, _167), float3(_177, _178, _179));
   float _184 = c_rt_size.z * SV_Position.x;
   float _185 = c_rt_size.w * SV_Position.y;
   int _188 = c_bool & 8;
   bool _189 = (_188 == 0);
   float _196;
   float _197;
   float _198;
   float _210;
   float _211;
   float _266;
   float _267;
   float _268;
   float _269;
   float _283;
   float _284;
   float _330;
   float _331;
   float _332;
   float _367;
   float _368;
   float _369;
   float _370;
   float _371;
   float _372;
   float _526;
   float _527;
   float _528;
   float _529;
   float _530;
   float _531;
   float _703;
   float _704;
   float _705;
   float _706;
   float _707;
   float _708;
   float _784;
   float _785;
   float _786;
   float _802;
   float _803;
   float _804;
   if (!_189)
   {
      float4 _191 = s13_t.Sample(s13_s, float2(_184, _185));
      _196 = _191.x;
      _197 = _191.y;
      _198 = _191.w;
   }
   else
   {
      _196 = 1.0f;
      _197 = 1.0f;
      _198 = 1.0f;
   }
   if (!_189)
   {
      float _203 = (c_sdw_param[0].x) * _196;
      float _204 = (c_sdw_param[0].z) * _197;
      float _207 = _203 + (c_sdw_param[0].y);
      float _208 = _204 + (c_sdw_param[0].w);
      _210 = _207;
      _211 = _208;
   }
   else
   {
      _210 = _196;
      _211 = _197;
   }
   float _218 = (c_sdw_param[1].x) * _104.y;
   float _220 = _218 + (c_sdw_param[1].y);
   float _221 = _220 * _104.x;
   float _222 = 1.0f - (c_light_col[5].x);
   float _223 = 1.0f - (c_light_col[5].y);
   float _224 = 1.0f - (c_light_col[5].z);
   float _225 = _221 * _222;
   float _226 = _221 * _223;
   float _227 = _221 * _224;
   float _228 = _225 + (c_light_col[5].x);
   float _229 = _226 + (c_light_col[5].y);
   float _230 = _227 + (c_light_col[5].z);
   float _234 = c_rt_size.z * SV_Position.x;
   float _235 = c_rt_size.w * SV_Position.y;
   float4 _236 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_234, _235)));
   int _243 = c_bool & 16384;
   bool _244 = (_243 == 0);
   if (!_244)
   {
      float _246 = _236.x * 2.0f;
      float _247 = _236.y * 2.0f;
      float _248 = _236.z * 2.0f;
      float _249 = _236.w * 2.0f;
      _266 = _246;
      _267 = _247;
      _268 = _248;
      _269 = _249;
   }
   else
   {
      int _251 = c_bool & 8192;
      bool _252 = (_251 == 0);
      if (!_252)
      {
         float _254 = _236.x * 2.0f;
         float _255 = _236.y * 2.0f;
         float _256 = _236.z * 2.0f;
         float _257 = _236.w * 2.0f;
         _266 = _254;
         _267 = _255;
         _268 = _256;
         _269 = _257;
      }
      else
      {
         float _261 = _236.x / c_tone_control.z;
         float _262 = _236.y / c_tone_control.z;
         float _263 = _236.z / c_tone_control.z;
         float _264 = _236.w / c_tone_control.z;
         _266 = _261;
         _267 = _262;
         _268 = _263;
         _269 = _264;
      }
   }
   int _270 = c_bool & 8;
   bool _271 = (_270 == 0);
   if (!_271)
   {
      float _276 = (c_sdw_param[0].x) * _210;
      float _277 = (c_sdw_param[0].z) * _211;
      float _280 = _276 + (c_sdw_param[0].y);
      float _281 = _277 + (c_sdw_param[0].w);
      _283 = _280;
      _284 = _281;
   }
   else
   {
      _283 = _210;
      _284 = _211;
   }
   float _285 = _284 * _283;
   float _290 = 1.0f - (c_light_col[5].x);
   float _291 = 1.0f - (c_light_col[5].y);
   float _292 = 1.0f - (c_light_col[5].z);
   float _293 = _290 * _285;
   float _294 = _291 * _285;
   float _295 = _292 * _285;
   float _296 = _293 + (c_light_col[5].x);
   float _297 = _294 + (c_light_col[5].y);
   float _298 = _295 + (c_light_col[5].z);
   float _299 = _198 * COLOR.x;
   float _300 = _299 * _228;
   float _301 = _198 * COLOR.y;
   float _302 = _301 * _229;
   float _303 = _198 * COLOR.z;
   float _304 = _303 * _230;
   float _305 = _296 * _228;
   float _306 = _297 * _229;
   float _307 = _298 * _230;
   if (!_271)
   {
      float _311 = (c_sdw_param[2].x) * _296;
      float _312 = (c_sdw_param[2].x) * _297;
      float _313 = (c_sdw_param[2].x) * _298;
      float _315 = _311 + (c_sdw_param[2].y);
      float _316 = _312 + (c_sdw_param[2].y);
      float _317 = _313 + (c_sdw_param[2].y);
      float _319 = (c_sdw_param[2].z) * _228;
      float _320 = (c_sdw_param[2].z) * _229;
      float _321 = (c_sdw_param[2].z) * _230;
      float _323 = _319 + (c_sdw_param[2].w);
      float _324 = _320 + (c_sdw_param[2].w);
      float _325 = _321 + (c_sdw_param[2].w);
      float _326 = _323 * _315;
      float _327 = _324 * _316;
      float _328 = _325 * _317;
      _330 = _326;
      _331 = _327;
      _332 = _328;
   }
   else
   {
      _330 = _305;
      _331 = _306;
      _332 = _307;
   }
   float _333 = _198 * _104.x;
   float _334 = _330 * _333;
   float _335 = _331 * _333;
   float _336 = _332 * _333;
   float _347 = c_mat_emissive.x * _121;
   float _348 = _347 * c_emissive_scale;
   float _349 = c_mat_emissive.y * _122;
   float _350 = _349 * c_emissive_scale;
   float _351 = c_mat_emissive.z * _123;
   float _352 = _351 * c_emissive_scale;
   int _353 = c_bool & 2048;
   bool _354 = (_353 == 0);
   if (_354)
   {
      float _356 = _284 * _266;
      float _357 = _284 * _267;
      float _358 = _284 * _268;
      float _359 = _356 + TEXCOORD_4.x;
      float _360 = _357 + TEXCOORD_4.y;
      float _361 = _358 + TEXCOORD_4.z;
      float _362 = _284 * _269;
      float _363 = _362 + TEXCOORD_5.x;
      float _364 = _362 + TEXCOORD_5.y;
      float _365 = _362 + TEXCOORD_5.z;
      _367 = _359;
      _368 = _360;
      _369 = _361;
      _370 = _363;
      _371 = _364;
      _372 = _365;
   }
   else
   {
      _367 = TEXCOORD_4.x;
      _368 = TEXCOORD_4.y;
      _369 = TEXCOORD_4.z;
      _370 = TEXCOORD_5.x;
      _371 = TEXCOORD_5.y;
      _372 = TEXCOORD_5.z;
   }
   float _377 = -0.0f - (c_light_vec[1].x);
   float _378 = -0.0f - (c_light_vec[1].y);
   float _379 = -0.0f - (c_light_vec[1].z);
   float _380 = dot(float3(_165, _166, _167), float3(_377, _378, _379));
   float _381 = saturate(_380);
   float _386 = saturate(_180);
   float _387 = (c_light_col[0].x) * _386;
   float _388 = (c_light_col[0].y) * _386;
   float _389 = (c_light_col[0].z) * _386;
   float _394 = (c_light_col[1].x) * _381;
   float _395 = (c_light_col[1].y) * _381;
   float _396 = (c_light_col[1].z) * _381;
   float _405 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _406 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _407 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _408 = _387 * _296;
   float _409 = _388 * _297;
   float _410 = _389 * _298;
   float _411 = _394 * _284;
   float _412 = _395 * _284;
   float _413 = _396 * _284;
   float _414 = _411 + _408;
   float _415 = _412 + _409;
   float _416 = _413 + _410;
   float _417 = _405 * _414;
   float _418 = _406 * _415;
   float _419 = _407 * _416;
   float _420 = _348 + c_scene_ambient.x;
   float _421 = _420 + _417;
   float _422 = _350 + c_scene_ambient.y;
   float _423 = _422 + _418;
   float _424 = _352 + c_scene_ambient.z;
   float _425 = _424 + _419;
   float _426 = _421 * _300;
   float _427 = _423 * _302;
   float _428 = _425 * _304;
   float _429 = _367 * _334;
   float _430 = _368 * _335;
   float _431 = _369 * _336;
   float _432 = _426 + _429;
   float _433 = _427 + _430;
   float _434 = _428 + _431;
   float _439 = (c_light_vec[0].x) + _170;
   float _440 = (c_light_vec[0].y) + _171;
   float _441 = (c_light_vec[0].z) + _172;
   float _442 = dot(float3(_439, _440, _441), float3(_439, _440, _441));
   float _443 = rsqrt(_442);
   float _444 = _439 * _443;
   float _445 = -0.0f - _444;
   float _446 = _440 * _443;
   float _447 = -0.0f - _446;
   float _448 = _441 * _443;
   float _449 = -0.0f - _448;
   float _450 = dot(float3(_445, _447, _449), float3(_165, _166, _167));
   float _451 = saturate(_450);
   float _452 = _171 + -0.30000001192092896f;
   float _453 = dot(float3(_170, _452, _172), float3(_170, _452, _172));
   float _454 = rsqrt(_453);
   float _455 = _170 * _454;
   float _456 = -0.0f - _455;
   float _457 = _452 * _454;
   float _458 = -0.0f - _457;
   float _459 = _172 * _454;
   float _460 = -0.0f - _459;
   float _461 = dot(float3(_456, _458, _460), float3(_165, _166, _167));
   float _462 = saturate(_461);
   float _465 = log2(_451);
   float _466 = log2(_462);
   float _467 = _465 * c_mat_power;
   float _468 = _466 * c_mat_power;
   float _469 = exp2(_467);
   float _470 = exp2(_468);
   float _473 = _469 * (c_light_col[0].x);
   float _474 = _469 * (c_light_col[0].y);
   float _475 = _469 * (c_light_col[0].z);
   float _476 = _470 * 2.0f;
   float _477 = _476 * (c_light_vec[5].x);
   float _478 = _477 * (c_light_col[1].x);
   float _479 = _477 * (c_light_col[1].y);
   float _480 = _477 * (c_light_col[1].z);
   float _481 = _473 * _296;
   float _482 = _474 * _297;
   float _483 = _475 * _298;
   float _484 = _478 * _381;
   float _485 = _479 * _381;
   float _486 = _480 * _381;
   float _487 = _484 + _481;
   float _488 = _485 + _482;
   float _489 = _486 + _483;
   float _496 = c_mat_specular.x * c_specular_scale.x;
   float _497 = _496 * _300;
   float _498 = _497 * _487;
   float _499 = c_mat_specular.y * c_specular_scale.y;
   float _500 = _499 * _302;
   float _501 = _500 * _488;
   float _502 = c_mat_specular.z * c_specular_scale.z;
   float _503 = _502 * _304;
   float _504 = _503 * _489;
   float _505 = _370 * _334;
   float _506 = _505 * _470;
   float _507 = _371 * _335;
   float _508 = _507 * _470;
   float _509 = _372 * _336;
   float _510 = _509 * _470;
   float _511 = _498 + _506;
   float _512 = _501 + _508;
   float _513 = _504 + _510;
   if (!_354)
   {
      float _515 = _284 * _266;
      float _516 = _284 * _267;
      float _517 = _284 * _268;
      float _518 = _432 + _515;
      float _519 = _433 + _516;
      float _520 = _434 + _517;
      float _521 = _284 * _269;
      float _522 = _511 + _521;
      float _523 = _512 + _521;
      float _524 = _513 + _521;
      _526 = _518;
      _527 = _519;
      _528 = _520;
      _529 = _522;
      _530 = _523;
      _531 = _524;
   }
   else
   {
      _526 = _432;
      _527 = _433;
      _528 = _434;
      _529 = _511;
      _530 = _512;
      _531 = _513;
   }
   int _532 = c_bool & 128;
   bool _533 = (_532 != 0);
   float _534 = select(_533, 0.0f, _529);
   float _535 = select(_533, 0.0f, _530);
   float _536 = select(_533, 0.0f, _531);
   float _537 = _526 * _121;
   float _538 = _527 * _122;
   float _539 = _528 * _123;
   float _540 = _534 * _111;
   float _541 = _535 * _111;
   float _542 = _536 * _111;
   int _543 = c_bool & 32768;
   bool _544 = (_543 == 0);
   if (!_544)
   {
      float _548 = (c_ps_user[11].x) * _121;
      float _549 = (c_ps_user[11].x) * _122;
      float _550 = (c_ps_user[11].x) * _123;
      float _552 = _165 + 1.0f;
      float _553 = _166 + 1.0f;
      float _554 = _167 + 1.0f;
      float _555 = _552 * 0.5f;
      float _556 = _553 * 0.5f;
      float _557 = _554 * 0.5f;
      float _558 = saturate(_555);
      float _559 = saturate(_556);
      float _560 = saturate(_557);
      float _561 = (c_ps_user[11].y) * _558;
      float _562 = _559 * (c_ps_user[11].y);
      float _563 = _560 * (c_ps_user[11].y);
      float _565 = (c_ps_user[11].z) * _111;
      float _567 = c_mat_power * 0.009999999776482582f;
      float _568 = _567 * (c_ps_user[11].w);
      float _571 = (c_ps_user[12].x) * _296;
      float _572 = (c_ps_user[12].x) * _297;
      float _573 = (c_ps_user[12].x) * _298;
      float _575 = (c_ps_user[12].y) * _300;
      float _576 = (c_ps_user[12].y) * _302;
      float _577 = (c_ps_user[12].y) * _304;
      float _579 = (c_ps_user[12].z) * _334;
      float _580 = (c_ps_user[12].z) * _335;
      float _581 = (c_ps_user[12].z) * _336;
      float _587 = -0.0f - _170;
      float _588 = -0.0f - _171;
      float _589 = -0.0f - _172;
      float _590 = dot(float3(_587, _588, _589), float3(_165, _166, _167));
      float _591 = 1.0f - _590;
      float _592 = _591 * _591;
      float _593 = _591 * 0.9722222089767456f;
      float _594 = _592 * _592;
      float _595 = _594 * _593;
      float _596 = _595 + 0.02777777425944805f;
      float _597 = _596 * (c_ps_user[13].z);
      float _599 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _600 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _601 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _604 = (c_ps_user[14].x) * _348;
      float _605 = (c_ps_user[14].x) * _350;
      float _606 = (c_ps_user[14].x) * _352;
      float _608 = (c_ps_user[14].y) * _387;
      float _609 = (c_ps_user[14].y) * _388;
      float _610 = (c_ps_user[14].y) * _389;
      float _612 = (c_ps_user[14].z) * _394;
      float _613 = (c_ps_user[14].z) * _395;
      float _614 = (c_ps_user[14].z) * _396;
      float _616 = (c_ps_user[14].w) * _367;
      float _617 = (c_ps_user[14].w) * _368;
      float _618 = (c_ps_user[14].w) * _369;
      float _621 = (c_ps_user[15].x) * _473;
      float _622 = (c_ps_user[15].x) * _474;
      float _623 = (c_ps_user[15].x) * _475;
      float _625 = _496 * _478;
      float _626 = _625 * (c_ps_user[15].y);
      float _627 = _499 * _479;
      float _628 = _627 * (c_ps_user[15].y);
      float _629 = _502 * _480;
      float _630 = _629 * (c_ps_user[15].y);
      float _632 = (c_ps_user[15].z) * _370;
      float _633 = (c_ps_user[15].z) * _371;
      float _634 = (c_ps_user[15].z) * _372;
      float _635 = _626 + _632;
      float _636 = _635 * _470;
      float _637 = _608 + (c_ps_user[13].x);
      float _638 = _637 + _612;
      float _639 = c_diffuse_scale.x * _638;
      float _640 = _639 * c_mat_diffuse.x;
      float _641 = (c_ps_user[13].y) + _565;
      float _642 = _641 + _621;
      float _643 = c_mat_specular.x * _642;
      float _644 = _643 * c_specular_scale.x;
      float _645 = _561 + _548;
      float _646 = _645 + _568;
      float _647 = _646 + (c_ps_user[12].w);
      float _648 = _647 + _571;
      float _649 = _648 + _575;
      float _650 = _649 + _579;
      float _651 = _650 + _599;
      float _652 = _651 + _604;
      float _653 = _652 + _616;
      float _654 = _653 + _597;
      float _655 = _654 + _636;
      float _656 = _655 + _640;
      float _657 = _656 + _644;
      float _658 = _628 + _633;
      float _659 = _658 * _470;
      float _660 = _609 + (c_ps_user[13].x);
      float _661 = _660 + _613;
      float _662 = c_diffuse_scale.y * _661;
      float _663 = _662 * c_mat_diffuse.y;
      float _664 = _641 + _622;
      float _665 = c_mat_specular.y * _664;
      float _666 = _665 * c_specular_scale.y;
      float _667 = _562 + _549;
      float _668 = _667 + _568;
      float _669 = _668 + (c_ps_user[12].w);
      float _670 = _669 + _572;
      float _671 = _670 + _576;
      float _672 = _671 + _580;
      float _673 = _672 + _600;
      float _674 = _673 + _605;
      float _675 = _674 + _617;
      float _676 = _675 + _597;
      float _677 = _676 + _659;
      float _678 = _677 + _663;
      float _679 = _678 + _666;
      float _680 = _630 + _634;
      float _681 = _680 * _470;
      float _682 = _610 + (c_ps_user[13].x);
      float _683 = _682 + _614;
      float _684 = c_diffuse_scale.z * _683;
      float _685 = _684 * c_mat_diffuse.z;
      float _686 = _641 + _623;
      float _687 = c_mat_specular.z * _686;
      float _688 = _687 * c_specular_scale.z;
      float _689 = _568 + _550;
      float _690 = _689 + _563;
      float _691 = _690 + (c_ps_user[12].w);
      float _692 = _691 + _573;
      float _693 = _692 + _577;
      float _694 = _693 + _581;
      float _695 = _694 + _601;
      float _696 = _695 + _606;
      float _697 = _696 + _618;
      float _698 = _697 + _597;
      float _699 = _698 + _681;
      float _700 = _699 + _685;
      float _701 = _700 + _688;
      _703 = _657;
      _704 = _679;
      _705 = _701;
      _706 = 0.0f;
      _707 = 0.0f;
      _708 = 0.0f;
   }
   else
   {
      _703 = _537;
      _704 = _538;
      _705 = _539;
      _706 = _540;
      _707 = _541;
      _708 = _542;
   }
   float _709 = _706 + _703;
   float _710 = _707 + _704;
   float _711 = _708 + _705;
   float _716 = c_mat_opacity * _58.w;
   float _717 = _716 * c_global_opacity;
   float _718 = _717 * c_opacity_scale;
   int _719 = c_bool & 16;
   bool _720 = (_719 == 0);
   if (!_720)
   {
      float _724 = c_tone_control.x * _709;
      float _725 = c_tone_control.x * _710;
      float _726 = c_tone_control.x * _711;
      float _728 = log2(_724);
      float _729 = log2(_725);
      float _730 = log2(_726);
      float _731 = _728 * c_tone_control.y;
      float _732 = _729 * c_tone_control.y;
      float _733 = _730 * c_tone_control.y;
      float _734 = exp2(_731);
      float _735 = exp2(_732);
      float _736 = exp2(_733);
      _784 = _734;
      _785 = _735;
      _786 = _736;
   }
   else
   {
      if (_544)
      {
         float _741 = c_tone_control.x * _709;
         float _742 = c_tone_control.x * _710;
         float _743 = c_tone_control.x * _711;
         float _744 = _741 * 2.0f;
         float _745 = _742 * 2.0f;
         float _746 = _743 * 2.0f;
         float _747 = _741 * 0.30000001192092896f;
         float _748 = _742 * 0.30000001192092896f;
         float _749 = _743 * 0.30000001192092896f;
         float _750 = _747 + 0.05000000074505806f;
         float _751 = _748 + 0.05000000074505806f;
         float _752 = _749 + 0.05000000074505806f;
         float _753 = _750 * _744;
         float _754 = _751 * _745;
         float _755 = _752 * _746;
         float _756 = _753 + 0.004000000189989805f;
         float _757 = _754 + 0.004000000189989805f;
         float _758 = _755 + 0.004000000189989805f;
         float _759 = _747 + 0.5f;
         float _760 = _748 + 0.5f;
         float _761 = _749 + 0.5f;
         float _762 = _759 * _744;
         float _763 = _760 * _745;
         float _764 = _761 * _746;
         float _765 = _762 + 0.06000000238418579f;
         float _766 = _763 + 0.06000000238418579f;
         float _767 = _764 + 0.06000000238418579f;
         float _768 = _756 / _765;
         float _769 = _757 / _766;
         float _770 = _758 / _767;
         float _771 = _768 + -0.06666666269302368f;
         float _772 = _769 + -0.06666666269302368f;
         float _773 = _770 + -0.06666666269302368f;
         float _774 = _771 * 1.3790643215179443f;
         float _775 = _772 * 1.3790643215179443f;
         float _776 = _773 * 1.3790643215179443f;
         float _777 = YKR_MaterialCurve(_774, _741);
         float _778 = YKR_MaterialCurve(_775, _742);
         float _779 = YKR_MaterialCurve(_776, _743);
         float _780 = sqrt(_777);
         float _781 = sqrt(_778);
         float _782 = sqrt(_779);
         _784 = _780;
         _785 = _781;
         _786 = _782;
      }
      else
      {
         _784 = _709;
         _785 = _710;
         _786 = _711;
      }
   }
   int _787 = c_bool & 4;
   bool _788 = (_787 == 0);
   if (_788)
   {
      float _790 = saturate(COLOR_1.x);
      float _795 = (c_fog_param[0].x) - _784;
      float _796 = (c_fog_param[0].y) - _785;
      float _797 = (c_fog_param[0].z) - _786;
      float _798 = _795 * _790;
      float _799 = _796 * _790;
      float _800 = _797 * _790;
      _802 = _798;
      _803 = _799;
      _804 = _800;
   }
   else
   {
      _802 = COLOR_1.x;
      _803 = COLOR_1.y;
      _804 = COLOR_1.z;
   }
   float _805 = _802 + _784;
   float _806 = _803 + _785;
   float _807 = _804 + _786;
   float _813 = (c_ps_col_scl[0].x) * _805;
   float _814 = (c_ps_col_scl[0].y) * _806;
   float _815 = (c_ps_col_scl[0].z) * _807;
   float _816 = (c_ps_col_scl[0].w) * _718;
   SV_Target.x = _813;
   SV_Target.y = _814;
   SV_Target.z = _815;
   SV_Target.w = _816;
   int _819 = c_bool & 8;
   bool _820 = (_819 != 0);
   float _821 = select(_820, _805, _813);
   float _822 = select(_820, _806, _814);
   float _823 = select(_820, _807, _815);
   float _824 = select(_820, _718, _816);
   SV_Target.x = _821;
   SV_Target.y = _822;
   SV_Target.z = _823;
   SV_Target.w = _824;
   return SV_Target;
}
