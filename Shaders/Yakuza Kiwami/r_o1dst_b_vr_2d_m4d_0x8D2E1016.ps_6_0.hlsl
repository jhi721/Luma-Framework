// Yakuza Kiwami material r_o1dst_b(vr)2d_m4d (0x8D2E1016, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float _65 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _66 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _67 = s3_t.Sample(s3_s, float2(_65, _66));
   float _74 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _75 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _76 = s5_t.Sample(s5_s, float2(_74, _75));
   float4 _79 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _84 = _79.x * _79.x;
   float _85 = _79.y * _79.y;
   float _86 = _79.z * _79.z;
   float4 _87 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _90 = _79.w * COLOR_1.w;
   float _91 = _84 - _59;
   float _92 = _85 - _60;
   float _93 = _86 - _61;
   float _94 = _91 * _90;
   float _95 = _92 * _90;
   float _96 = _93 * _90;
   float _97 = _94 + _59;
   float _98 = _95 + _60;
   float _99 = _96 + _61;
   float _100 = 1.0f - _67.x;
   float _101 = 1.0f - _67.y;
   float _102 = 1.0f - _67.z;
   float _103 = _90 * _100;
   float _104 = _90 * _101;
   float _105 = _90 * _102;
   float _106 = _103 + _67.x;
   float _107 = _104 + _67.y;
   float _108 = _105 + _67.z;
   float _109 = 0.5f - _76.x;
   float _110 = 0.5f - _76.y;
   float _111 = _90 * _109;
   float _112 = _90 * _110;
   float _113 = _111 + _76.x;
   float _114 = _112 + _76.y;
   float _115 = _113 * 2.0f;
   float _116 = _114 * 2.0f;
   float _117 = _115 + -1.0f;
   float _118 = _116 + -1.0f;
   float _119 = dot(float2(_117, _118), float2(_117, _118));
   float _120 = 1.0f - _119;
   float _121 = saturate(_120);
   float _122 = sqrt(_121);
   float _123 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _124 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _125 = _123 - _124;
   float _126 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _127 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _128 = _126 - _127;
   float _129 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _130 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _131 = _129 - _130;
   float _132 = _117 * TEXCOORD_6.x;
   float _133 = _117 * TEXCOORD_6.y;
   float _134 = _117 * TEXCOORD_6.z;
   float _135 = _118 * TEXCOORD_6.w;
   float _136 = _135 * _125;
   float _137 = _135 * _128;
   float _138 = _135 * _131;
   float _139 = _136 + _132;
   float _140 = _137 + _133;
   float _141 = _138 + _134;
   float _142 = _122 * TEXCOORD_2.x;
   float _143 = _122 * TEXCOORD_2.y;
   float _144 = _122 * TEXCOORD_2.z;
   float _145 = _139 + _142;
   float _146 = _140 + _143;
   float _147 = _141 + _144;
   float _148 = dot(float3(_145, _146, _147), float3(_145, _146, _147));
   float _149 = rsqrt(_148);
   float _150 = _145 * _149;
   float _151 = _146 * _149;
   float _152 = _147 * _149;
   float _153 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _154 = rsqrt(_153);
   float _155 = _154 * TEXCOORD_3.x;
   float _156 = _154 * TEXCOORD_3.y;
   float _157 = _154 * TEXCOORD_3.z;
   float _162 = -0.0f - (c_light_vec[0].x);
   float _163 = -0.0f - (c_light_vec[0].y);
   float _164 = -0.0f - (c_light_vec[0].z);
   float _165 = dot(float3(_150, _151, _152), float3(_162, _163, _164));
   float _169 = c_rt_size.z * SV_Position.x;
   float _170 = c_rt_size.w * SV_Position.y;
   int _173 = c_bool & 8;
   bool _174 = (_173 == 0);
   float _181;
   float _182;
   float _183;
   float _195;
   float _196;
   float _251;
   float _252;
   float _253;
   float _254;
   float _268;
   float _269;
   float _315;
   float _316;
   float _317;
   float _352;
   float _353;
   float _354;
   float _355;
   float _356;
   float _357;
   float _511;
   float _512;
   float _513;
   float _514;
   float _515;
   float _516;
   float _692;
   float _693;
   float _694;
   float _695;
   float _696;
   float _697;
   float _773;
   float _774;
   float _775;
   float _791;
   float _792;
   float _793;
   if (!_174)
   {
      float4 _176 = s13_t.Sample(s13_s, float2(_169, _170));
      _181 = _176.x;
      _182 = _176.y;
      _183 = _176.w;
   }
   else
   {
      _181 = 1.0f;
      _182 = 1.0f;
      _183 = 1.0f;
   }
   if (!_174)
   {
      float _188 = (c_sdw_param[0].x) * _181;
      float _189 = (c_sdw_param[0].z) * _182;
      float _192 = _188 + (c_sdw_param[0].y);
      float _193 = _189 + (c_sdw_param[0].w);
      _195 = _192;
      _196 = _193;
   }
   else
   {
      _195 = _181;
      _196 = _182;
   }
   float _203 = (c_sdw_param[1].x) * _87.y;
   float _205 = _203 + (c_sdw_param[1].y);
   float _206 = _205 * _87.x;
   float _207 = 1.0f - (c_light_col[5].x);
   float _208 = 1.0f - (c_light_col[5].y);
   float _209 = 1.0f - (c_light_col[5].z);
   float _210 = _206 * _207;
   float _211 = _206 * _208;
   float _212 = _206 * _209;
   float _213 = _210 + (c_light_col[5].x);
   float _214 = _211 + (c_light_col[5].y);
   float _215 = _212 + (c_light_col[5].z);
   float _219 = c_rt_size.z * SV_Position.x;
   float _220 = c_rt_size.w * SV_Position.y;
   float4 _221 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_219, _220)));
   int _228 = c_bool & 16384;
   bool _229 = (_228 == 0);
   if (!_229)
   {
      float _231 = _221.x * 2.0f;
      float _232 = _221.y * 2.0f;
      float _233 = _221.z * 2.0f;
      float _234 = _221.w * 2.0f;
      _251 = _231;
      _252 = _232;
      _253 = _233;
      _254 = _234;
   }
   else
   {
      int _236 = c_bool & 8192;
      bool _237 = (_236 == 0);
      if (!_237)
      {
         float _239 = _221.x * 2.0f;
         float _240 = _221.y * 2.0f;
         float _241 = _221.z * 2.0f;
         float _242 = _221.w * 2.0f;
         _251 = _239;
         _252 = _240;
         _253 = _241;
         _254 = _242;
      }
      else
      {
         float _246 = _221.x / c_tone_control.z;
         float _247 = _221.y / c_tone_control.z;
         float _248 = _221.z / c_tone_control.z;
         float _249 = _221.w / c_tone_control.z;
         _251 = _246;
         _252 = _247;
         _253 = _248;
         _254 = _249;
      }
   }
   int _255 = c_bool & 8;
   bool _256 = (_255 == 0);
   if (!_256)
   {
      float _261 = (c_sdw_param[0].x) * _195;
      float _262 = (c_sdw_param[0].z) * _196;
      float _265 = _261 + (c_sdw_param[0].y);
      float _266 = _262 + (c_sdw_param[0].w);
      _268 = _265;
      _269 = _266;
   }
   else
   {
      _268 = _195;
      _269 = _196;
   }
   float _270 = _269 * _268;
   float _275 = 1.0f - (c_light_col[5].x);
   float _276 = 1.0f - (c_light_col[5].y);
   float _277 = 1.0f - (c_light_col[5].z);
   float _278 = _275 * _270;
   float _279 = _276 * _270;
   float _280 = _277 * _270;
   float _281 = _278 + (c_light_col[5].x);
   float _282 = _279 + (c_light_col[5].y);
   float _283 = _280 + (c_light_col[5].z);
   float _284 = _183 * COLOR.x;
   float _285 = _284 * _213;
   float _286 = _183 * COLOR.y;
   float _287 = _286 * _214;
   float _288 = _183 * COLOR.z;
   float _289 = _288 * _215;
   float _290 = _281 * _213;
   float _291 = _282 * _214;
   float _292 = _283 * _215;
   if (!_256)
   {
      float _296 = (c_sdw_param[2].x) * _281;
      float _297 = (c_sdw_param[2].x) * _282;
      float _298 = (c_sdw_param[2].x) * _283;
      float _300 = _296 + (c_sdw_param[2].y);
      float _301 = _297 + (c_sdw_param[2].y);
      float _302 = _298 + (c_sdw_param[2].y);
      float _304 = (c_sdw_param[2].z) * _213;
      float _305 = (c_sdw_param[2].z) * _214;
      float _306 = (c_sdw_param[2].z) * _215;
      float _308 = _304 + (c_sdw_param[2].w);
      float _309 = _305 + (c_sdw_param[2].w);
      float _310 = _306 + (c_sdw_param[2].w);
      float _311 = _308 * _300;
      float _312 = _309 * _301;
      float _313 = _310 * _302;
      _315 = _311;
      _316 = _312;
      _317 = _313;
   }
   else
   {
      _315 = _290;
      _316 = _291;
      _317 = _292;
   }
   float _318 = _183 * _87.x;
   float _319 = _315 * _318;
   float _320 = _316 * _318;
   float _321 = _317 * _318;
   float _332 = c_mat_emissive.x * _97;
   float _333 = _332 * c_emissive_scale;
   float _334 = c_mat_emissive.y * _98;
   float _335 = _334 * c_emissive_scale;
   float _336 = c_mat_emissive.z * _99;
   float _337 = _336 * c_emissive_scale;
   int _338 = c_bool & 2048;
   bool _339 = (_338 == 0);
   if (_339)
   {
      float _341 = _269 * _251;
      float _342 = _269 * _252;
      float _343 = _269 * _253;
      float _344 = _341 + TEXCOORD_4.x;
      float _345 = _342 + TEXCOORD_4.y;
      float _346 = _343 + TEXCOORD_4.z;
      float _347 = _269 * _254;
      float _348 = _347 + TEXCOORD_5.x;
      float _349 = _347 + TEXCOORD_5.y;
      float _350 = _347 + TEXCOORD_5.z;
      _352 = _344;
      _353 = _345;
      _354 = _346;
      _355 = _348;
      _356 = _349;
      _357 = _350;
   }
   else
   {
      _352 = TEXCOORD_4.x;
      _353 = TEXCOORD_4.y;
      _354 = TEXCOORD_4.z;
      _355 = TEXCOORD_5.x;
      _356 = TEXCOORD_5.y;
      _357 = TEXCOORD_5.z;
   }
   float _362 = -0.0f - (c_light_vec[1].x);
   float _363 = -0.0f - (c_light_vec[1].y);
   float _364 = -0.0f - (c_light_vec[1].z);
   float _365 = dot(float3(_150, _151, _152), float3(_362, _363, _364));
   float _366 = saturate(_365);
   float _371 = saturate(_165);
   float _372 = (c_light_col[0].x) * _371;
   float _373 = (c_light_col[0].y) * _371;
   float _374 = (c_light_col[0].z) * _371;
   float _379 = (c_light_col[1].x) * _366;
   float _380 = (c_light_col[1].y) * _366;
   float _381 = (c_light_col[1].z) * _366;
   float _390 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _391 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _392 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _393 = _372 * _281;
   float _394 = _373 * _282;
   float _395 = _374 * _283;
   float _396 = _379 * _269;
   float _397 = _380 * _269;
   float _398 = _381 * _269;
   float _399 = _396 + _393;
   float _400 = _397 + _394;
   float _401 = _398 + _395;
   float _402 = _390 * _399;
   float _403 = _391 * _400;
   float _404 = _392 * _401;
   float _405 = _333 + c_scene_ambient.x;
   float _406 = _405 + _402;
   float _407 = _335 + c_scene_ambient.y;
   float _408 = _407 + _403;
   float _409 = _337 + c_scene_ambient.z;
   float _410 = _409 + _404;
   float _411 = _406 * _285;
   float _412 = _408 * _287;
   float _413 = _410 * _289;
   float _414 = _352 * _319;
   float _415 = _353 * _320;
   float _416 = _354 * _321;
   float _417 = _411 + _414;
   float _418 = _412 + _415;
   float _419 = _413 + _416;
   float _424 = (c_light_vec[0].x) + _155;
   float _425 = (c_light_vec[0].y) + _156;
   float _426 = (c_light_vec[0].z) + _157;
   float _427 = dot(float3(_424, _425, _426), float3(_424, _425, _426));
   float _428 = rsqrt(_427);
   float _429 = _424 * _428;
   float _430 = -0.0f - _429;
   float _431 = _425 * _428;
   float _432 = -0.0f - _431;
   float _433 = _426 * _428;
   float _434 = -0.0f - _433;
   float _435 = dot(float3(_430, _432, _434), float3(_150, _151, _152));
   float _436 = saturate(_435);
   float _437 = _156 + -0.30000001192092896f;
   float _438 = dot(float3(_155, _437, _157), float3(_155, _437, _157));
   float _439 = rsqrt(_438);
   float _440 = _155 * _439;
   float _441 = -0.0f - _440;
   float _442 = _437 * _439;
   float _443 = -0.0f - _442;
   float _444 = _157 * _439;
   float _445 = -0.0f - _444;
   float _446 = dot(float3(_441, _443, _445), float3(_150, _151, _152));
   float _447 = saturate(_446);
   float _450 = log2(_436);
   float _451 = log2(_447);
   float _452 = _450 * c_mat_power;
   float _453 = _451 * c_mat_power;
   float _454 = exp2(_452);
   float _455 = exp2(_453);
   float _458 = _454 * (c_light_col[0].x);
   float _459 = _454 * (c_light_col[0].y);
   float _460 = _454 * (c_light_col[0].z);
   float _461 = _455 * 2.0f;
   float _462 = _461 * (c_light_vec[5].x);
   float _463 = _462 * (c_light_col[1].x);
   float _464 = _462 * (c_light_col[1].y);
   float _465 = _462 * (c_light_col[1].z);
   float _466 = _458 * _281;
   float _467 = _459 * _282;
   float _468 = _460 * _283;
   float _469 = _463 * _366;
   float _470 = _464 * _366;
   float _471 = _465 * _366;
   float _472 = _469 + _466;
   float _473 = _470 + _467;
   float _474 = _471 + _468;
   float _481 = c_mat_specular.x * c_specular_scale.x;
   float _482 = _481 * _285;
   float _483 = _482 * _472;
   float _484 = c_mat_specular.y * c_specular_scale.y;
   float _485 = _484 * _287;
   float _486 = _485 * _473;
   float _487 = c_mat_specular.z * c_specular_scale.z;
   float _488 = _487 * _289;
   float _489 = _488 * _474;
   float _490 = _355 * _319;
   float _491 = _490 * _455;
   float _492 = _356 * _320;
   float _493 = _492 * _455;
   float _494 = _357 * _321;
   float _495 = _494 * _455;
   float _496 = _483 + _491;
   float _497 = _486 + _493;
   float _498 = _489 + _495;
   if (!_339)
   {
      float _500 = _269 * _251;
      float _501 = _269 * _252;
      float _502 = _269 * _253;
      float _503 = _417 + _500;
      float _504 = _418 + _501;
      float _505 = _419 + _502;
      float _506 = _269 * _254;
      float _507 = _496 + _506;
      float _508 = _497 + _506;
      float _509 = _498 + _506;
      _511 = _503;
      _512 = _504;
      _513 = _505;
      _514 = _507;
      _515 = _508;
      _516 = _509;
   }
   else
   {
      _511 = _417;
      _512 = _418;
      _513 = _419;
      _514 = _496;
      _515 = _497;
      _516 = _498;
   }
   int _517 = c_bool & 128;
   bool _518 = (_517 != 0);
   float _519 = select(_518, 0.0f, _514);
   float _520 = select(_518, 0.0f, _515);
   float _521 = select(_518, 0.0f, _516);
   float _522 = _511 * _97;
   float _523 = _512 * _98;
   float _524 = _513 * _99;
   float _525 = _519 * _106;
   float _526 = _520 * _107;
   float _527 = _521 * _108;
   int _528 = c_bool & 32768;
   bool _529 = (_528 == 0);
   if (!_529)
   {
      float _533 = (c_ps_user[11].x) * _97;
      float _534 = (c_ps_user[11].x) * _98;
      float _535 = (c_ps_user[11].x) * _99;
      float _537 = _150 + 1.0f;
      float _538 = _151 + 1.0f;
      float _539 = _152 + 1.0f;
      float _540 = _537 * 0.5f;
      float _541 = _538 * 0.5f;
      float _542 = _539 * 0.5f;
      float _543 = saturate(_540);
      float _544 = saturate(_541);
      float _545 = saturate(_542);
      float _546 = (c_ps_user[11].y) * _543;
      float _547 = _544 * (c_ps_user[11].y);
      float _548 = _545 * (c_ps_user[11].y);
      float _550 = (c_ps_user[11].z) * _106;
      float _551 = (c_ps_user[11].z) * _107;
      float _552 = (c_ps_user[11].z) * _108;
      float _554 = c_mat_power * 0.009999999776482582f;
      float _555 = _554 * (c_ps_user[11].w);
      float _558 = (c_ps_user[12].x) * _281;
      float _559 = (c_ps_user[12].x) * _282;
      float _560 = (c_ps_user[12].x) * _283;
      float _562 = (c_ps_user[12].y) * _285;
      float _563 = (c_ps_user[12].y) * _287;
      float _564 = (c_ps_user[12].y) * _289;
      float _566 = (c_ps_user[12].z) * _319;
      float _567 = (c_ps_user[12].z) * _320;
      float _568 = (c_ps_user[12].z) * _321;
      float _574 = -0.0f - _155;
      float _575 = -0.0f - _156;
      float _576 = -0.0f - _157;
      float _577 = dot(float3(_574, _575, _576), float3(_150, _151, _152));
      float _578 = 1.0f - _577;
      float _579 = _578 * _578;
      float _580 = _578 * 0.9722222089767456f;
      float _581 = _579 * _579;
      float _582 = _581 * _580;
      float _583 = _582 + 0.02777777425944805f;
      float _584 = _583 * (c_ps_user[13].z);
      float _586 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _587 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _588 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _591 = (c_ps_user[14].x) * _333;
      float _592 = (c_ps_user[14].x) * _335;
      float _593 = (c_ps_user[14].x) * _337;
      float _595 = (c_ps_user[14].y) * _372;
      float _596 = (c_ps_user[14].y) * _373;
      float _597 = (c_ps_user[14].y) * _374;
      float _599 = (c_ps_user[14].z) * _379;
      float _600 = (c_ps_user[14].z) * _380;
      float _601 = (c_ps_user[14].z) * _381;
      float _603 = (c_ps_user[14].w) * _352;
      float _604 = (c_ps_user[14].w) * _353;
      float _605 = (c_ps_user[14].w) * _354;
      float _608 = (c_ps_user[15].x) * _458;
      float _609 = (c_ps_user[15].x) * _459;
      float _610 = (c_ps_user[15].x) * _460;
      float _612 = _481 * _463;
      float _613 = _612 * (c_ps_user[15].y);
      float _614 = _484 * _464;
      float _615 = _614 * (c_ps_user[15].y);
      float _616 = _487 * _465;
      float _617 = _616 * (c_ps_user[15].y);
      float _619 = (c_ps_user[15].z) * _355;
      float _620 = (c_ps_user[15].z) * _356;
      float _621 = (c_ps_user[15].z) * _357;
      float _622 = _613 + _619;
      float _623 = _622 * _455;
      float _624 = _595 + (c_ps_user[13].x);
      float _625 = _624 + _599;
      float _626 = c_diffuse_scale.x * _625;
      float _627 = _626 * c_mat_diffuse.x;
      float _628 = (c_ps_user[13].y) + _550;
      float _629 = _628 + _608;
      float _630 = c_mat_specular.x * _629;
      float _631 = _630 * c_specular_scale.x;
      float _632 = _546 + _533;
      float _633 = _632 + _555;
      float _634 = _633 + (c_ps_user[12].w);
      float _635 = _634 + _558;
      float _636 = _635 + _562;
      float _637 = _636 + _566;
      float _638 = _637 + _586;
      float _639 = _638 + _591;
      float _640 = _639 + _603;
      float _641 = _640 + _584;
      float _642 = _641 + _623;
      float _643 = _642 + _627;
      float _644 = _643 + _631;
      float _645 = _615 + _620;
      float _646 = _645 * _455;
      float _647 = _596 + (c_ps_user[13].x);
      float _648 = _647 + _600;
      float _649 = c_diffuse_scale.y * _648;
      float _650 = _649 * c_mat_diffuse.y;
      float _651 = (c_ps_user[13].y) + _551;
      float _652 = _651 + _609;
      float _653 = c_mat_specular.y * _652;
      float _654 = _653 * c_specular_scale.y;
      float _655 = _547 + _534;
      float _656 = _655 + _555;
      float _657 = _656 + (c_ps_user[12].w);
      float _658 = _657 + _559;
      float _659 = _658 + _563;
      float _660 = _659 + _567;
      float _661 = _660 + _587;
      float _662 = _661 + _592;
      float _663 = _662 + _604;
      float _664 = _663 + _584;
      float _665 = _664 + _646;
      float _666 = _665 + _650;
      float _667 = _666 + _654;
      float _668 = _617 + _621;
      float _669 = _668 * _455;
      float _670 = _597 + (c_ps_user[13].x);
      float _671 = _670 + _601;
      float _672 = c_diffuse_scale.z * _671;
      float _673 = _672 * c_mat_diffuse.z;
      float _674 = (c_ps_user[13].y) + _552;
      float _675 = _674 + _610;
      float _676 = c_mat_specular.z * _675;
      float _677 = _676 * c_specular_scale.z;
      float _678 = _555 + _535;
      float _679 = _678 + _548;
      float _680 = _679 + (c_ps_user[12].w);
      float _681 = _680 + _560;
      float _682 = _681 + _564;
      float _683 = _682 + _568;
      float _684 = _683 + _588;
      float _685 = _684 + _593;
      float _686 = _685 + _605;
      float _687 = _686 + _584;
      float _688 = _687 + _669;
      float _689 = _688 + _673;
      float _690 = _689 + _677;
      _692 = _644;
      _693 = _667;
      _694 = _690;
      _695 = 0.0f;
      _696 = 0.0f;
      _697 = 0.0f;
   }
   else
   {
      _692 = _522;
      _693 = _523;
      _694 = _524;
      _695 = _525;
      _696 = _526;
      _697 = _527;
   }
   float _698 = _695 + _692;
   float _699 = _696 + _693;
   float _700 = _697 + _694;
   float _705 = c_mat_opacity * _54.w;
   float _706 = _705 * c_global_opacity;
   float _707 = _706 * c_opacity_scale;
   int _708 = c_bool & 16;
   bool _709 = (_708 == 0);
   if (!_709)
   {
      float _713 = c_tone_control.x * _698;
      float _714 = c_tone_control.x * _699;
      float _715 = c_tone_control.x * _700;
      float _717 = log2(_713);
      float _718 = log2(_714);
      float _719 = log2(_715);
      float _720 = _717 * c_tone_control.y;
      float _721 = _718 * c_tone_control.y;
      float _722 = _719 * c_tone_control.y;
      float _723 = exp2(_720);
      float _724 = exp2(_721);
      float _725 = exp2(_722);
      _773 = _723;
      _774 = _724;
      _775 = _725;
   }
   else
   {
      if (_529)
      {
         float _730 = c_tone_control.x * _698;
         float _731 = c_tone_control.x * _699;
         float _732 = c_tone_control.x * _700;
         float _733 = _730 * 2.0f;
         float _734 = _731 * 2.0f;
         float _735 = _732 * 2.0f;
         float _736 = _730 * 0.30000001192092896f;
         float _737 = _731 * 0.30000001192092896f;
         float _738 = _732 * 0.30000001192092896f;
         float _739 = _736 + 0.05000000074505806f;
         float _740 = _737 + 0.05000000074505806f;
         float _741 = _738 + 0.05000000074505806f;
         float _742 = _739 * _733;
         float _743 = _740 * _734;
         float _744 = _741 * _735;
         float _745 = _742 + 0.004000000189989805f;
         float _746 = _743 + 0.004000000189989805f;
         float _747 = _744 + 0.004000000189989805f;
         float _748 = _736 + 0.5f;
         float _749 = _737 + 0.5f;
         float _750 = _738 + 0.5f;
         float _751 = _748 * _733;
         float _752 = _749 * _734;
         float _753 = _750 * _735;
         float _754 = _751 + 0.06000000238418579f;
         float _755 = _752 + 0.06000000238418579f;
         float _756 = _753 + 0.06000000238418579f;
         float _757 = _745 / _754;
         float _758 = _746 / _755;
         float _759 = _747 / _756;
         float _760 = _757 + -0.06666666269302368f;
         float _761 = _758 + -0.06666666269302368f;
         float _762 = _759 + -0.06666666269302368f;
         float _763 = _760 * 1.3790643215179443f;
         float _764 = _761 * 1.3790643215179443f;
         float _765 = _762 * 1.3790643215179443f;
         float _766 = YKR_MaterialCurve(_763, _730);
         float _767 = YKR_MaterialCurve(_764, _731);
         float _768 = YKR_MaterialCurve(_765, _732);
         float _769 = sqrt(_766);
         float _770 = sqrt(_767);
         float _771 = sqrt(_768);
         _773 = _769;
         _774 = _770;
         _775 = _771;
      }
      else
      {
         _773 = _698;
         _774 = _699;
         _775 = _700;
      }
   }
   int _776 = c_bool & 4;
   bool _777 = (_776 == 0);
   if (_777)
   {
      float _779 = saturate(COLOR_1.x);
      float _784 = (c_fog_param[0].x) - _773;
      float _785 = (c_fog_param[0].y) - _774;
      float _786 = (c_fog_param[0].z) - _775;
      float _787 = _784 * _779;
      float _788 = _785 * _779;
      float _789 = _786 * _779;
      _791 = _787;
      _792 = _788;
      _793 = _789;
   }
   else
   {
      _791 = COLOR_1.x;
      _792 = COLOR_1.y;
      _793 = COLOR_1.z;
   }
   float _794 = _791 + _773;
   float _795 = _792 + _774;
   float _796 = _793 + _775;
   float _802 = (c_ps_col_scl[0].x) * _794;
   float _803 = (c_ps_col_scl[0].y) * _795;
   float _804 = (c_ps_col_scl[0].z) * _796;
   float _805 = (c_ps_col_scl[0].w) * _707;
   SV_Target.x = _802;
   SV_Target.y = _803;
   SV_Target.z = _804;
   SV_Target.w = _805;
   int _808 = c_bool & 8;
   bool _809 = (_808 != 0);
   float _810 = select(_809, _794, _802);
   float _811 = select(_809, _795, _803);
   float _812 = select(_809, _796, _804);
   float _813 = select(_809, _707, _805);
   SV_Target.x = _810;
   SV_Target.y = _811;
   SV_Target.z = _812;
   SV_Target.w = _813;
   return SV_Target;
}
