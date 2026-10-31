// Yakuza Kiwami material r_o1dst_b(vr)2d (0x716E2CC1, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _61 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _62 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _63 = s3_t.Sample(s3_s, float2(_61, _62));
   float _70 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _72 = s5_t.Sample(s5_s, float2(_70, _71));
   float4 _75 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _80 = _75.x * _75.x;
   float _81 = _75.y * _75.y;
   float _82 = _75.z * _75.z;
   float _83 = _75.w * COLOR_1.w;
   float _84 = _80 - _55;
   float _85 = _81 - _56;
   float _86 = _82 - _57;
   float _87 = _84 * _83;
   float _88 = _85 * _83;
   float _89 = _86 * _83;
   float _90 = _87 + _55;
   float _91 = _88 + _56;
   float _92 = _89 + _57;
   float _93 = 1.0f - _63.x;
   float _94 = 1.0f - _63.y;
   float _95 = 1.0f - _63.z;
   float _96 = _83 * _93;
   float _97 = _83 * _94;
   float _98 = _83 * _95;
   float _99 = _96 + _63.x;
   float _100 = _97 + _63.y;
   float _101 = _98 + _63.z;
   float _102 = 0.5f - _72.x;
   float _103 = 0.5f - _72.y;
   float _104 = _83 * _102;
   float _105 = _83 * _103;
   float _106 = _104 + _72.x;
   float _107 = _105 + _72.y;
   float _108 = _106 * 2.0f;
   float _109 = _107 * 2.0f;
   float _110 = _108 + -1.0f;
   float _111 = _109 + -1.0f;
   float _112 = dot(float2(_110, _111), float2(_110, _111));
   float _113 = 1.0f - _112;
   float _114 = saturate(_113);
   float _115 = sqrt(_114);
   float _116 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _117 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _118 = _116 - _117;
   float _119 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _120 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _121 = _119 - _120;
   float _122 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _123 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _124 = _122 - _123;
   float _125 = _110 * TEXCOORD_6.x;
   float _126 = _110 * TEXCOORD_6.y;
   float _127 = _110 * TEXCOORD_6.z;
   float _128 = _111 * TEXCOORD_6.w;
   float _129 = _128 * _118;
   float _130 = _128 * _121;
   float _131 = _128 * _124;
   float _132 = _115 * TEXCOORD_2.x;
   float _133 = _115 * TEXCOORD_2.y;
   float _134 = _115 * TEXCOORD_2.z;
   float _135 = _125 + _132;
   float _136 = _135 + _129;
   float _137 = _126 + _133;
   float _138 = _137 + _130;
   float _139 = _127 + _134;
   float _140 = _139 + _131;
   float _141 = dot(float3(_136, _138, _140), float3(_136, _138, _140));
   float _142 = rsqrt(_141);
   float _143 = _136 * _142;
   float _144 = _138 * _142;
   float _145 = _140 * _142;
   float _146 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _147 = rsqrt(_146);
   float _148 = _147 * TEXCOORD_3.x;
   float _149 = _147 * TEXCOORD_3.y;
   float _150 = _147 * TEXCOORD_3.z;
   float _155 = -0.0f - (c_light_vec[0].x);
   float _156 = -0.0f - (c_light_vec[0].y);
   float _157 = -0.0f - (c_light_vec[0].z);
   float _158 = dot(float3(_143, _144, _145), float3(_155, _156, _157));
   float _162 = c_rt_size.z * SV_Position.x;
   float _163 = c_rt_size.w * SV_Position.y;
   int _166 = c_bool & 8;
   bool _167 = (_166 == 0);
   float _174;
   float _175;
   float _176;
   float _188;
   float _189;
   float _225;
   float _226;
   float _227;
   float _228;
   float _242;
   float _243;
   float _278;
   float _279;
   float _280;
   float _314;
   float _315;
   float _316;
   float _317;
   float _318;
   float _319;
   float _473;
   float _474;
   float _475;
   float _476;
   float _477;
   float _478;
   float _654;
   float _655;
   float _656;
   float _657;
   float _658;
   float _659;
   float _735;
   float _736;
   float _737;
   float _753;
   float _754;
   float _755;
   if (!_167)
   {
      float4 _169 = s13_t.Sample(s13_s, float2(_162, _163));
      _174 = _169.x;
      _175 = _169.y;
      _176 = _169.w;
   }
   else
   {
      _174 = 1.0f;
      _175 = 1.0f;
      _176 = 1.0f;
   }
   if (!_167)
   {
      float _181 = (c_sdw_param[0].x) * _174;
      float _182 = (c_sdw_param[0].z) * _175;
      float _185 = _181 + (c_sdw_param[0].y);
      float _186 = _182 + (c_sdw_param[0].w);
      _188 = _185;
      _189 = _186;
   }
   else
   {
      _188 = _174;
      _189 = _175;
   }
   float _193 = c_rt_size.z * SV_Position.x;
   float _194 = c_rt_size.w * SV_Position.y;
   float4 _195 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_193, _194)));
   int _202 = c_bool & 16384;
   bool _203 = (_202 == 0);
   if (!_203)
   {
      float _205 = _195.x * 2.0f;
      float _206 = _195.y * 2.0f;
      float _207 = _195.z * 2.0f;
      float _208 = _195.w * 2.0f;
      _225 = _205;
      _226 = _206;
      _227 = _207;
      _228 = _208;
   }
   else
   {
      int _210 = c_bool & 8192;
      bool _211 = (_210 == 0);
      if (!_211)
      {
         float _213 = _195.x * 2.0f;
         float _214 = _195.y * 2.0f;
         float _215 = _195.z * 2.0f;
         float _216 = _195.w * 2.0f;
         _225 = _213;
         _226 = _214;
         _227 = _215;
         _228 = _216;
      }
      else
      {
         float _220 = _195.x / c_tone_control.z;
         float _221 = _195.y / c_tone_control.z;
         float _222 = _195.z / c_tone_control.z;
         float _223 = _195.w / c_tone_control.z;
         _225 = _220;
         _226 = _221;
         _227 = _222;
         _228 = _223;
      }
   }
   int _229 = c_bool & 8;
   bool _230 = (_229 == 0);
   if (!_230)
   {
      float _235 = (c_sdw_param[0].x) * _188;
      float _236 = (c_sdw_param[0].z) * _189;
      float _239 = _235 + (c_sdw_param[0].y);
      float _240 = _236 + (c_sdw_param[0].w);
      _242 = _239;
      _243 = _240;
   }
   else
   {
      _242 = _188;
      _243 = _189;
   }
   float _244 = _243 * _242;
   float _249 = 1.0f - (c_light_col[5].x);
   float _250 = 1.0f - (c_light_col[5].y);
   float _251 = 1.0f - (c_light_col[5].z);
   float _252 = _249 * _244;
   float _253 = _250 * _244;
   float _254 = _251 * _244;
   float _255 = _252 + (c_light_col[5].x);
   float _256 = _253 + (c_light_col[5].y);
   float _257 = _254 + (c_light_col[5].z);
   float _258 = _176 * COLOR.x;
   float _259 = _176 * COLOR.y;
   float _260 = _176 * COLOR.z;
   if (!_230)
   {
      float _264 = (c_sdw_param[2].x) * _255;
      float _265 = (c_sdw_param[2].x) * _256;
      float _266 = (c_sdw_param[2].x) * _257;
      float _268 = _264 + (c_sdw_param[2].y);
      float _269 = _265 + (c_sdw_param[2].y);
      float _270 = _266 + (c_sdw_param[2].y);
      float _273 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _274 = _268 * _273;
      float _275 = _269 * _273;
      float _276 = _270 * _273;
      _278 = _274;
      _279 = _275;
      _280 = _276;
   }
   else
   {
      _278 = _255;
      _279 = _256;
      _280 = _257;
   }
   float _281 = _278 * _176;
   float _282 = _279 * _176;
   float _283 = _280 * _176;
   float _294 = c_mat_emissive.x * _90;
   float _295 = _294 * c_emissive_scale;
   float _296 = c_mat_emissive.y * _91;
   float _297 = _296 * c_emissive_scale;
   float _298 = c_mat_emissive.z * _92;
   float _299 = _298 * c_emissive_scale;
   int _300 = c_bool & 2048;
   bool _301 = (_300 == 0);
   if (_301)
   {
      float _303 = _243 * _225;
      float _304 = _243 * _226;
      float _305 = _243 * _227;
      float _306 = _303 + TEXCOORD_4.x;
      float _307 = _304 + TEXCOORD_4.y;
      float _308 = _305 + TEXCOORD_4.z;
      float _309 = _243 * _228;
      float _310 = _309 + TEXCOORD_5.x;
      float _311 = _309 + TEXCOORD_5.y;
      float _312 = _309 + TEXCOORD_5.z;
      _314 = _306;
      _315 = _307;
      _316 = _308;
      _317 = _310;
      _318 = _311;
      _319 = _312;
   }
   else
   {
      _314 = TEXCOORD_4.x;
      _315 = TEXCOORD_4.y;
      _316 = TEXCOORD_4.z;
      _317 = TEXCOORD_5.x;
      _318 = TEXCOORD_5.y;
      _319 = TEXCOORD_5.z;
   }
   float _324 = -0.0f - (c_light_vec[1].x);
   float _325 = -0.0f - (c_light_vec[1].y);
   float _326 = -0.0f - (c_light_vec[1].z);
   float _327 = dot(float3(_143, _144, _145), float3(_324, _325, _326));
   float _328 = saturate(_327);
   float _333 = saturate(_158);
   float _334 = (c_light_col[0].x) * _333;
   float _335 = (c_light_col[0].y) * _333;
   float _336 = (c_light_col[0].z) * _333;
   float _341 = (c_light_col[1].x) * _328;
   float _342 = (c_light_col[1].y) * _328;
   float _343 = (c_light_col[1].z) * _328;
   float _352 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _353 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _354 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _355 = _334 * _255;
   float _356 = _335 * _256;
   float _357 = _336 * _257;
   float _358 = _341 * _243;
   float _359 = _342 * _243;
   float _360 = _343 * _243;
   float _361 = _358 + _355;
   float _362 = _359 + _356;
   float _363 = _360 + _357;
   float _364 = _352 * _361;
   float _365 = _353 * _362;
   float _366 = _354 * _363;
   float _367 = _295 + c_scene_ambient.x;
   float _368 = _367 + _364;
   float _369 = _297 + c_scene_ambient.y;
   float _370 = _369 + _365;
   float _371 = _299 + c_scene_ambient.z;
   float _372 = _371 + _366;
   float _373 = _368 * _258;
   float _374 = _370 * _259;
   float _375 = _372 * _260;
   float _376 = _314 * _281;
   float _377 = _315 * _282;
   float _378 = _316 * _283;
   float _379 = _373 + _376;
   float _380 = _374 + _377;
   float _381 = _375 + _378;
   float _386 = (c_light_vec[0].x) + _148;
   float _387 = (c_light_vec[0].y) + _149;
   float _388 = (c_light_vec[0].z) + _150;
   float _389 = dot(float3(_386, _387, _388), float3(_386, _387, _388));
   float _390 = rsqrt(_389);
   float _391 = _386 * _390;
   float _392 = -0.0f - _391;
   float _393 = _387 * _390;
   float _394 = -0.0f - _393;
   float _395 = _388 * _390;
   float _396 = -0.0f - _395;
   float _397 = dot(float3(_392, _394, _396), float3(_143, _144, _145));
   float _398 = saturate(_397);
   float _399 = _149 + -0.30000001192092896f;
   float _400 = dot(float3(_148, _399, _150), float3(_148, _399, _150));
   float _401 = rsqrt(_400);
   float _402 = _148 * _401;
   float _403 = -0.0f - _402;
   float _404 = _399 * _401;
   float _405 = -0.0f - _404;
   float _406 = _150 * _401;
   float _407 = -0.0f - _406;
   float _408 = dot(float3(_403, _405, _407), float3(_143, _144, _145));
   float _409 = saturate(_408);
   float _412 = log2(_398);
   float _413 = log2(_409);
   float _414 = _412 * c_mat_power;
   float _415 = _413 * c_mat_power;
   float _416 = exp2(_414);
   float _417 = exp2(_415);
   float _420 = _416 * (c_light_col[0].x);
   float _421 = _416 * (c_light_col[0].y);
   float _422 = _416 * (c_light_col[0].z);
   float _423 = _417 * 2.0f;
   float _424 = _423 * (c_light_vec[5].x);
   float _425 = _424 * (c_light_col[1].x);
   float _426 = _424 * (c_light_col[1].y);
   float _427 = _424 * (c_light_col[1].z);
   float _428 = _420 * _255;
   float _429 = _421 * _256;
   float _430 = _422 * _257;
   float _431 = _425 * _328;
   float _432 = _426 * _328;
   float _433 = _427 * _328;
   float _434 = _431 + _428;
   float _435 = _432 + _429;
   float _436 = _433 + _430;
   float _443 = c_mat_specular.x * c_specular_scale.x;
   float _444 = _443 * _258;
   float _445 = _444 * _434;
   float _446 = c_mat_specular.y * c_specular_scale.y;
   float _447 = _446 * _259;
   float _448 = _447 * _435;
   float _449 = c_mat_specular.z * c_specular_scale.z;
   float _450 = _449 * _260;
   float _451 = _450 * _436;
   float _452 = _317 * _281;
   float _453 = _452 * _417;
   float _454 = _318 * _282;
   float _455 = _454 * _417;
   float _456 = _319 * _283;
   float _457 = _456 * _417;
   float _458 = _445 + _453;
   float _459 = _448 + _455;
   float _460 = _451 + _457;
   if (!_301)
   {
      float _462 = _243 * _225;
      float _463 = _243 * _226;
      float _464 = _243 * _227;
      float _465 = _379 + _462;
      float _466 = _380 + _463;
      float _467 = _381 + _464;
      float _468 = _243 * _228;
      float _469 = _458 + _468;
      float _470 = _459 + _468;
      float _471 = _460 + _468;
      _473 = _465;
      _474 = _466;
      _475 = _467;
      _476 = _469;
      _477 = _470;
      _478 = _471;
   }
   else
   {
      _473 = _379;
      _474 = _380;
      _475 = _381;
      _476 = _458;
      _477 = _459;
      _478 = _460;
   }
   int _479 = c_bool & 128;
   bool _480 = (_479 != 0);
   float _481 = select(_480, 0.0f, _476);
   float _482 = select(_480, 0.0f, _477);
   float _483 = select(_480, 0.0f, _478);
   float _484 = _473 * _90;
   float _485 = _474 * _91;
   float _486 = _475 * _92;
   float _487 = _481 * _99;
   float _488 = _482 * _100;
   float _489 = _483 * _101;
   int _490 = c_bool & 32768;
   bool _491 = (_490 == 0);
   if (!_491)
   {
      float _495 = (c_ps_user[11].x) * _90;
      float _496 = (c_ps_user[11].x) * _91;
      float _497 = (c_ps_user[11].x) * _92;
      float _499 = _143 + 1.0f;
      float _500 = _144 + 1.0f;
      float _501 = _145 + 1.0f;
      float _502 = _499 * 0.5f;
      float _503 = _500 * 0.5f;
      float _504 = _501 * 0.5f;
      float _505 = saturate(_502);
      float _506 = saturate(_503);
      float _507 = saturate(_504);
      float _508 = (c_ps_user[11].y) * _505;
      float _509 = _506 * (c_ps_user[11].y);
      float _510 = _507 * (c_ps_user[11].y);
      float _512 = (c_ps_user[11].z) * _99;
      float _513 = (c_ps_user[11].z) * _100;
      float _514 = (c_ps_user[11].z) * _101;
      float _516 = c_mat_power * 0.009999999776482582f;
      float _517 = _516 * (c_ps_user[11].w);
      float _520 = (c_ps_user[12].x) * _255;
      float _521 = (c_ps_user[12].x) * _256;
      float _522 = (c_ps_user[12].x) * _257;
      float _524 = (c_ps_user[12].y) * _258;
      float _525 = (c_ps_user[12].y) * _259;
      float _526 = (c_ps_user[12].y) * _260;
      float _528 = (c_ps_user[12].z) * _281;
      float _529 = (c_ps_user[12].z) * _282;
      float _530 = (c_ps_user[12].z) * _283;
      float _536 = -0.0f - _148;
      float _537 = -0.0f - _149;
      float _538 = -0.0f - _150;
      float _539 = dot(float3(_536, _537, _538), float3(_143, _144, _145));
      float _540 = 1.0f - _539;
      float _541 = _540 * _540;
      float _542 = _540 * 0.9722222089767456f;
      float _543 = _541 * _541;
      float _544 = _543 * _542;
      float _545 = _544 + 0.02777777425944805f;
      float _546 = _545 * (c_ps_user[13].z);
      float _548 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _549 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _550 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _553 = (c_ps_user[14].x) * _295;
      float _554 = (c_ps_user[14].x) * _297;
      float _555 = (c_ps_user[14].x) * _299;
      float _557 = (c_ps_user[14].y) * _334;
      float _558 = (c_ps_user[14].y) * _335;
      float _559 = (c_ps_user[14].y) * _336;
      float _561 = (c_ps_user[14].z) * _341;
      float _562 = (c_ps_user[14].z) * _342;
      float _563 = (c_ps_user[14].z) * _343;
      float _565 = (c_ps_user[14].w) * _314;
      float _566 = (c_ps_user[14].w) * _315;
      float _567 = (c_ps_user[14].w) * _316;
      float _570 = (c_ps_user[15].x) * _420;
      float _571 = (c_ps_user[15].x) * _421;
      float _572 = (c_ps_user[15].x) * _422;
      float _574 = _443 * _425;
      float _575 = _574 * (c_ps_user[15].y);
      float _576 = _446 * _426;
      float _577 = _576 * (c_ps_user[15].y);
      float _578 = _449 * _427;
      float _579 = _578 * (c_ps_user[15].y);
      float _581 = (c_ps_user[15].z) * _317;
      float _582 = (c_ps_user[15].z) * _318;
      float _583 = (c_ps_user[15].z) * _319;
      float _584 = _575 + _581;
      float _585 = _584 * _417;
      float _586 = _557 + (c_ps_user[13].x);
      float _587 = _586 + _561;
      float _588 = c_diffuse_scale.x * _587;
      float _589 = _588 * c_mat_diffuse.x;
      float _590 = (c_ps_user[13].y) + _512;
      float _591 = _590 + _570;
      float _592 = c_mat_specular.x * _591;
      float _593 = _592 * c_specular_scale.x;
      float _594 = _508 + _495;
      float _595 = _594 + _517;
      float _596 = _595 + (c_ps_user[12].w);
      float _597 = _596 + _520;
      float _598 = _597 + _524;
      float _599 = _598 + _528;
      float _600 = _599 + _548;
      float _601 = _600 + _553;
      float _602 = _601 + _565;
      float _603 = _602 + _546;
      float _604 = _603 + _585;
      float _605 = _604 + _589;
      float _606 = _605 + _593;
      float _607 = _577 + _582;
      float _608 = _607 * _417;
      float _609 = _558 + (c_ps_user[13].x);
      float _610 = _609 + _562;
      float _611 = c_diffuse_scale.y * _610;
      float _612 = _611 * c_mat_diffuse.y;
      float _613 = (c_ps_user[13].y) + _513;
      float _614 = _613 + _571;
      float _615 = c_mat_specular.y * _614;
      float _616 = _615 * c_specular_scale.y;
      float _617 = _509 + _496;
      float _618 = _617 + _517;
      float _619 = _618 + (c_ps_user[12].w);
      float _620 = _619 + _521;
      float _621 = _620 + _525;
      float _622 = _621 + _529;
      float _623 = _622 + _549;
      float _624 = _623 + _554;
      float _625 = _624 + _566;
      float _626 = _625 + _546;
      float _627 = _626 + _608;
      float _628 = _627 + _612;
      float _629 = _628 + _616;
      float _630 = _579 + _583;
      float _631 = _630 * _417;
      float _632 = _559 + (c_ps_user[13].x);
      float _633 = _632 + _563;
      float _634 = c_diffuse_scale.z * _633;
      float _635 = _634 * c_mat_diffuse.z;
      float _636 = (c_ps_user[13].y) + _514;
      float _637 = _636 + _572;
      float _638 = c_mat_specular.z * _637;
      float _639 = _638 * c_specular_scale.z;
      float _640 = _517 + _497;
      float _641 = _640 + _510;
      float _642 = _641 + (c_ps_user[12].w);
      float _643 = _642 + _522;
      float _644 = _643 + _526;
      float _645 = _644 + _530;
      float _646 = _645 + _550;
      float _647 = _646 + _555;
      float _648 = _647 + _567;
      float _649 = _648 + _546;
      float _650 = _649 + _631;
      float _651 = _650 + _635;
      float _652 = _651 + _639;
      _654 = _606;
      _655 = _629;
      _656 = _652;
      _657 = 0.0f;
      _658 = 0.0f;
      _659 = 0.0f;
   }
   else
   {
      _654 = _484;
      _655 = _485;
      _656 = _486;
      _657 = _487;
      _658 = _488;
      _659 = _489;
   }
   float _660 = _657 + _654;
   float _661 = _658 + _655;
   float _662 = _659 + _656;
   float _667 = c_mat_opacity * _50.w;
   float _668 = _667 * c_global_opacity;
   float _669 = _668 * c_opacity_scale;
   int _670 = c_bool & 16;
   bool _671 = (_670 == 0);
   if (!_671)
   {
      float _675 = c_tone_control.x * _660;
      float _676 = c_tone_control.x * _661;
      float _677 = c_tone_control.x * _662;
      float _679 = log2(_675);
      float _680 = log2(_676);
      float _681 = log2(_677);
      float _682 = _679 * c_tone_control.y;
      float _683 = _680 * c_tone_control.y;
      float _684 = _681 * c_tone_control.y;
      float _685 = exp2(_682);
      float _686 = exp2(_683);
      float _687 = exp2(_684);
      _735 = _685;
      _736 = _686;
      _737 = _687;
   }
   else
   {
      if (_491)
      {
         float _692 = c_tone_control.x * _660;
         float _693 = c_tone_control.x * _661;
         float _694 = c_tone_control.x * _662;
         float _695 = _692 * 2.0f;
         float _696 = _693 * 2.0f;
         float _697 = _694 * 2.0f;
         float _698 = _692 * 0.30000001192092896f;
         float _699 = _693 * 0.30000001192092896f;
         float _700 = _694 * 0.30000001192092896f;
         float _701 = _698 + 0.05000000074505806f;
         float _702 = _699 + 0.05000000074505806f;
         float _703 = _700 + 0.05000000074505806f;
         float _704 = _701 * _695;
         float _705 = _702 * _696;
         float _706 = _703 * _697;
         float _707 = _704 + 0.004000000189989805f;
         float _708 = _705 + 0.004000000189989805f;
         float _709 = _706 + 0.004000000189989805f;
         float _710 = _698 + 0.5f;
         float _711 = _699 + 0.5f;
         float _712 = _700 + 0.5f;
         float _713 = _710 * _695;
         float _714 = _711 * _696;
         float _715 = _712 * _697;
         float _716 = _713 + 0.06000000238418579f;
         float _717 = _714 + 0.06000000238418579f;
         float _718 = _715 + 0.06000000238418579f;
         float _719 = _707 / _716;
         float _720 = _708 / _717;
         float _721 = _709 / _718;
         float _722 = _719 + -0.06666666269302368f;
         float _723 = _720 + -0.06666666269302368f;
         float _724 = _721 + -0.06666666269302368f;
         float _725 = _722 * 1.3790643215179443f;
         float _726 = _723 * 1.3790643215179443f;
         float _727 = _724 * 1.3790643215179443f;
         float _728 = YKR_MaterialCurve(_725, _692);
         float _729 = YKR_MaterialCurve(_726, _693);
         float _730 = YKR_MaterialCurve(_727, _694);
         float _731 = sqrt(_728);
         float _732 = sqrt(_729);
         float _733 = sqrt(_730);
         _735 = _731;
         _736 = _732;
         _737 = _733;
      }
      else
      {
         _735 = _660;
         _736 = _661;
         _737 = _662;
      }
   }
   int _738 = c_bool & 4;
   bool _739 = (_738 == 0);
   if (_739)
   {
      float _741 = saturate(COLOR_1.x);
      float _746 = (c_fog_param[0].x) - _735;
      float _747 = (c_fog_param[0].y) - _736;
      float _748 = (c_fog_param[0].z) - _737;
      float _749 = _746 * _741;
      float _750 = _747 * _741;
      float _751 = _748 * _741;
      _753 = _749;
      _754 = _750;
      _755 = _751;
   }
   else
   {
      _753 = COLOR_1.x;
      _754 = COLOR_1.y;
      _755 = COLOR_1.z;
   }
   float _756 = _753 + _735;
   float _757 = _754 + _736;
   float _758 = _755 + _737;
   float _764 = (c_ps_col_scl[0].x) * _756;
   float _765 = (c_ps_col_scl[0].y) * _757;
   float _766 = (c_ps_col_scl[0].z) * _758;
   float _767 = (c_ps_col_scl[0].w) * _669;
   SV_Target.x = _764;
   SV_Target.y = _765;
   SV_Target.z = _766;
   SV_Target.w = _767;
   int _770 = c_bool & 8;
   bool _771 = (_770 != 0);
   float _772 = select(_771, _756, _764);
   float _773 = select(_771, _757, _765);
   float _774 = select(_771, _758, _766);
   float _775 = select(_771, _669, _767);
   SV_Target.x = _772;
   SV_Target.y = _773;
   SV_Target.z = _774;
   SV_Target.w = _775;
   return SV_Target;
}
