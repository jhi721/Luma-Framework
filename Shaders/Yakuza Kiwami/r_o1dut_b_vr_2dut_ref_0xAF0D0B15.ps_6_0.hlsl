// Yakuza Kiwami material r_o1dut_b(vr)2dut[ref] (0xAF0D0B15, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

TextureCube<float4> s2c_t : register(t2);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

SamplerState s2c_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _62 = _57.x * _57.x;
   float _63 = _57.y * _57.y;
   float _64 = _57.z * _57.z;
   float _68 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _69 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _70 = s3_t.Sample(s3_s, float2(_68, _69));
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
   float _100 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _101 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _102 = s6_t.Sample(s6_s, float2(_100, _101));
   float _105 = _81.w * COLOR_1.w;
   float _106 = _94.x - _70.x;
   float _107 = _94.y - _70.y;
   float _108 = _106 * _105;
   float _109 = _107 * _105;
   float _110 = _108 + _70.x;
   float _111 = _109 + _70.y;
   float _112 = _86 - _62;
   float _113 = _87 - _63;
   float _114 = _88 - _64;
   float _115 = _112 * _105;
   float _116 = _113 * _105;
   float _117 = _114 * _105;
   float _118 = _115 + _62;
   float _119 = _116 + _63;
   float _120 = _117 + _64;
   float _121 = _102.x - _78.x;
   float _122 = _102.y - _78.y;
   float _123 = _121 * _105;
   float _124 = _122 * _105;
   float _125 = _123 + _78.x;
   float _126 = _124 + _78.y;
   float _127 = _125 * 2.0f;
   float _128 = _126 * 2.0f;
   float _129 = _127 + -1.0f;
   float _130 = _128 + -1.0f;
   float _131 = dot(float2(_129, _130), float2(_129, _130));
   float _132 = 1.0f - _131;
   float _133 = saturate(_132);
   float _134 = sqrt(_133);
   float _135 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _136 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _137 = _135 - _136;
   float _138 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _139 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _140 = _138 - _139;
   float _141 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _142 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _143 = _141 - _142;
   float _144 = _129 * TEXCOORD_6.x;
   float _145 = _129 * TEXCOORD_6.y;
   float _146 = _129 * TEXCOORD_6.z;
   float _147 = _130 * TEXCOORD_6.w;
   float _148 = _147 * _137;
   float _149 = _147 * _140;
   float _150 = _147 * _143;
   float _151 = _134 * TEXCOORD_2.x;
   float _152 = _134 * TEXCOORD_2.y;
   float _153 = _134 * TEXCOORD_2.z;
   float _154 = _144 + _151;
   float _155 = _154 + _148;
   float _156 = _145 + _152;
   float _157 = _156 + _149;
   float _158 = _146 + _153;
   float _159 = _158 + _150;
   float _160 = dot(float3(_155, _157, _159), float3(_155, _157, _159));
   float _161 = rsqrt(_160);
   float _162 = _155 * _161;
   float _163 = _157 * _161;
   float _164 = _159 * _161;
   float _165 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _166 = rsqrt(_165);
   float _167 = _166 * TEXCOORD_3.x;
   float _168 = _166 * TEXCOORD_3.y;
   float _169 = _166 * TEXCOORD_3.z;
   float _174 = -0.0f - (c_light_vec[0].x);
   float _175 = -0.0f - (c_light_vec[0].y);
   float _176 = -0.0f - (c_light_vec[0].z);
   float _177 = dot(float3(_162, _163, _164), float3(_174, _175, _176));
   float _181 = c_rt_size.z * SV_Position.x;
   float _182 = c_rt_size.w * SV_Position.y;
   int _185 = c_bool & 8;
   bool _186 = (_185 == 0);
   float _193;
   float _194;
   float _195;
   float _207;
   float _208;
   float _244;
   float _245;
   float _246;
   float _247;
   float _261;
   float _262;
   float _297;
   float _298;
   float _299;
   float _333;
   float _334;
   float _335;
   float _336;
   float _337;
   float _338;
   float _492;
   float _493;
   float _494;
   float _495;
   float _496;
   float _497;
   float _669;
   float _670;
   float _671;
   float _672;
   float _673;
   float _674;
   float _793;
   float _794;
   float _795;
   float _811;
   float _812;
   float _813;
   if (!_186)
   {
      float4 _188 = s13_t.Sample(s13_s, float2(_181, _182));
      _193 = _188.x;
      _194 = _188.y;
      _195 = _188.w;
   }
   else
   {
      _193 = 1.0f;
      _194 = 1.0f;
      _195 = 1.0f;
   }
   if (!_186)
   {
      float _200 = (c_sdw_param[0].x) * _193;
      float _201 = (c_sdw_param[0].z) * _194;
      float _204 = _200 + (c_sdw_param[0].y);
      float _205 = _201 + (c_sdw_param[0].w);
      _207 = _204;
      _208 = _205;
   }
   else
   {
      _207 = _193;
      _208 = _194;
   }
   float _212 = c_rt_size.z * SV_Position.x;
   float _213 = c_rt_size.w * SV_Position.y;
   float4 _214 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_212, _213)));
   int _221 = c_bool & 16384;
   bool _222 = (_221 == 0);
   if (!_222)
   {
      float _224 = _214.x * 2.0f;
      float _225 = _214.y * 2.0f;
      float _226 = _214.z * 2.0f;
      float _227 = _214.w * 2.0f;
      _244 = _224;
      _245 = _225;
      _246 = _226;
      _247 = _227;
   }
   else
   {
      int _229 = c_bool & 8192;
      bool _230 = (_229 == 0);
      if (!_230)
      {
         float _232 = _214.x * 2.0f;
         float _233 = _214.y * 2.0f;
         float _234 = _214.z * 2.0f;
         float _235 = _214.w * 2.0f;
         _244 = _232;
         _245 = _233;
         _246 = _234;
         _247 = _235;
      }
      else
      {
         float _239 = _214.x / c_tone_control.z;
         float _240 = _214.y / c_tone_control.z;
         float _241 = _214.z / c_tone_control.z;
         float _242 = _214.w / c_tone_control.z;
         _244 = _239;
         _245 = _240;
         _246 = _241;
         _247 = _242;
      }
   }
   int _248 = c_bool & 8;
   bool _249 = (_248 == 0);
   if (!_249)
   {
      float _254 = (c_sdw_param[0].x) * _207;
      float _255 = (c_sdw_param[0].z) * _208;
      float _258 = _254 + (c_sdw_param[0].y);
      float _259 = _255 + (c_sdw_param[0].w);
      _261 = _258;
      _262 = _259;
   }
   else
   {
      _261 = _207;
      _262 = _208;
   }
   float _263 = _262 * _261;
   float _268 = 1.0f - (c_light_col[5].x);
   float _269 = 1.0f - (c_light_col[5].y);
   float _270 = 1.0f - (c_light_col[5].z);
   float _271 = _268 * _263;
   float _272 = _269 * _263;
   float _273 = _270 * _263;
   float _274 = _271 + (c_light_col[5].x);
   float _275 = _272 + (c_light_col[5].y);
   float _276 = _273 + (c_light_col[5].z);
   float _277 = _195 * COLOR.x;
   float _278 = _195 * COLOR.y;
   float _279 = _195 * COLOR.z;
   if (!_249)
   {
      float _283 = (c_sdw_param[2].x) * _274;
      float _284 = (c_sdw_param[2].x) * _275;
      float _285 = (c_sdw_param[2].x) * _276;
      float _287 = _283 + (c_sdw_param[2].y);
      float _288 = _284 + (c_sdw_param[2].y);
      float _289 = _285 + (c_sdw_param[2].y);
      float _292 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _293 = _287 * _292;
      float _294 = _288 * _292;
      float _295 = _289 * _292;
      _297 = _293;
      _298 = _294;
      _299 = _295;
   }
   else
   {
      _297 = _274;
      _298 = _275;
      _299 = _276;
   }
   float _300 = _297 * _195;
   float _301 = _298 * _195;
   float _302 = _299 * _195;
   float _313 = c_mat_emissive.x * _118;
   float _314 = _313 * c_emissive_scale;
   float _315 = c_mat_emissive.y * _119;
   float _316 = _315 * c_emissive_scale;
   float _317 = c_mat_emissive.z * _120;
   float _318 = _317 * c_emissive_scale;
   int _319 = c_bool & 2048;
   bool _320 = (_319 == 0);
   if (_320)
   {
      float _322 = _262 * _244;
      float _323 = _262 * _245;
      float _324 = _262 * _246;
      float _325 = _322 + TEXCOORD_4.x;
      float _326 = _323 + TEXCOORD_4.y;
      float _327 = _324 + TEXCOORD_4.z;
      float _328 = _262 * _247;
      float _329 = _328 + TEXCOORD_5.x;
      float _330 = _328 + TEXCOORD_5.y;
      float _331 = _328 + TEXCOORD_5.z;
      _333 = _325;
      _334 = _326;
      _335 = _327;
      _336 = _329;
      _337 = _330;
      _338 = _331;
   }
   else
   {
      _333 = TEXCOORD_4.x;
      _334 = TEXCOORD_4.y;
      _335 = TEXCOORD_4.z;
      _336 = TEXCOORD_5.x;
      _337 = TEXCOORD_5.y;
      _338 = TEXCOORD_5.z;
   }
   float _343 = -0.0f - (c_light_vec[1].x);
   float _344 = -0.0f - (c_light_vec[1].y);
   float _345 = -0.0f - (c_light_vec[1].z);
   float _346 = dot(float3(_162, _163, _164), float3(_343, _344, _345));
   float _347 = saturate(_346);
   float _352 = saturate(_177);
   float _353 = (c_light_col[0].x) * _352;
   float _354 = (c_light_col[0].y) * _352;
   float _355 = (c_light_col[0].z) * _352;
   float _360 = (c_light_col[1].x) * _347;
   float _361 = (c_light_col[1].y) * _347;
   float _362 = (c_light_col[1].z) * _347;
   float _371 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _372 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _373 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _374 = _353 * _274;
   float _375 = _354 * _275;
   float _376 = _355 * _276;
   float _377 = _360 * _262;
   float _378 = _361 * _262;
   float _379 = _362 * _262;
   float _380 = _377 + _374;
   float _381 = _378 + _375;
   float _382 = _379 + _376;
   float _383 = _371 * _380;
   float _384 = _372 * _381;
   float _385 = _373 * _382;
   float _386 = _314 + c_scene_ambient.x;
   float _387 = _386 + _383;
   float _388 = _316 + c_scene_ambient.y;
   float _389 = _388 + _384;
   float _390 = _318 + c_scene_ambient.z;
   float _391 = _390 + _385;
   float _392 = _387 * _277;
   float _393 = _389 * _278;
   float _394 = _391 * _279;
   float _395 = _333 * _300;
   float _396 = _334 * _301;
   float _397 = _335 * _302;
   float _398 = _392 + _395;
   float _399 = _393 + _396;
   float _400 = _394 + _397;
   float _405 = (c_light_vec[0].x) + _167;
   float _406 = (c_light_vec[0].y) + _168;
   float _407 = (c_light_vec[0].z) + _169;
   float _408 = dot(float3(_405, _406, _407), float3(_405, _406, _407));
   float _409 = rsqrt(_408);
   float _410 = _405 * _409;
   float _411 = -0.0f - _410;
   float _412 = _406 * _409;
   float _413 = -0.0f - _412;
   float _414 = _407 * _409;
   float _415 = -0.0f - _414;
   float _416 = dot(float3(_411, _413, _415), float3(_162, _163, _164));
   float _417 = saturate(_416);
   float _418 = _168 + -0.30000001192092896f;
   float _419 = dot(float3(_167, _418, _169), float3(_167, _418, _169));
   float _420 = rsqrt(_419);
   float _421 = _167 * _420;
   float _422 = -0.0f - _421;
   float _423 = _418 * _420;
   float _424 = -0.0f - _423;
   float _425 = _169 * _420;
   float _426 = -0.0f - _425;
   float _427 = dot(float3(_422, _424, _426), float3(_162, _163, _164));
   float _428 = saturate(_427);
   float _431 = log2(_417);
   float _432 = log2(_428);
   float _433 = _431 * c_mat_power;
   float _434 = _432 * c_mat_power;
   float _435 = exp2(_433);
   float _436 = exp2(_434);
   float _439 = _435 * (c_light_col[0].x);
   float _440 = _435 * (c_light_col[0].y);
   float _441 = _435 * (c_light_col[0].z);
   float _442 = _436 * 2.0f;
   float _443 = _442 * (c_light_vec[5].x);
   float _444 = _443 * (c_light_col[1].x);
   float _445 = _443 * (c_light_col[1].y);
   float _446 = _443 * (c_light_col[1].z);
   float _447 = _439 * _274;
   float _448 = _440 * _275;
   float _449 = _441 * _276;
   float _450 = _444 * _347;
   float _451 = _445 * _347;
   float _452 = _446 * _347;
   float _453 = _450 + _447;
   float _454 = _451 + _448;
   float _455 = _452 + _449;
   float _462 = c_mat_specular.x * c_specular_scale.x;
   float _463 = _462 * _277;
   float _464 = _463 * _453;
   float _465 = c_mat_specular.y * c_specular_scale.y;
   float _466 = _465 * _278;
   float _467 = _466 * _454;
   float _468 = c_mat_specular.z * c_specular_scale.z;
   float _469 = _468 * _279;
   float _470 = _469 * _455;
   float _471 = _336 * _300;
   float _472 = _471 * _436;
   float _473 = _337 * _301;
   float _474 = _473 * _436;
   float _475 = _338 * _302;
   float _476 = _475 * _436;
   float _477 = _464 + _472;
   float _478 = _467 + _474;
   float _479 = _470 + _476;
   if (!_320)
   {
      float _481 = _262 * _244;
      float _482 = _262 * _245;
      float _483 = _262 * _246;
      float _484 = _398 + _481;
      float _485 = _399 + _482;
      float _486 = _400 + _483;
      float _487 = _262 * _247;
      float _488 = _477 + _487;
      float _489 = _478 + _487;
      float _490 = _479 + _487;
      _492 = _484;
      _493 = _485;
      _494 = _486;
      _495 = _488;
      _496 = _489;
      _497 = _490;
   }
   else
   {
      _492 = _398;
      _493 = _399;
      _494 = _400;
      _495 = _477;
      _496 = _478;
      _497 = _479;
   }
   int _498 = c_bool & 128;
   bool _499 = (_498 != 0);
   float _500 = select(_499, 0.0f, _495);
   float _501 = select(_499, 0.0f, _496);
   float _502 = select(_499, 0.0f, _497);
   float _503 = _492 * _118;
   float _504 = _493 * _119;
   float _505 = _494 * _120;
   float _506 = _500 * _111;
   float _507 = _501 * _111;
   float _508 = _502 * _111;
   int _509 = c_bool & 32768;
   bool _510 = (_509 == 0);
   if (!_510)
   {
      float _514 = (c_ps_user[11].x) * _118;
      float _515 = (c_ps_user[11].x) * _119;
      float _516 = (c_ps_user[11].x) * _120;
      float _518 = _162 + 1.0f;
      float _519 = _163 + 1.0f;
      float _520 = _164 + 1.0f;
      float _521 = _518 * 0.5f;
      float _522 = _519 * 0.5f;
      float _523 = _520 * 0.5f;
      float _524 = saturate(_521);
      float _525 = saturate(_522);
      float _526 = saturate(_523);
      float _527 = (c_ps_user[11].y) * _524;
      float _528 = _525 * (c_ps_user[11].y);
      float _529 = _526 * (c_ps_user[11].y);
      float _531 = (c_ps_user[11].z) * _111;
      float _533 = c_mat_power * 0.009999999776482582f;
      float _534 = _533 * (c_ps_user[11].w);
      float _537 = (c_ps_user[12].x) * _274;
      float _538 = (c_ps_user[12].x) * _275;
      float _539 = (c_ps_user[12].x) * _276;
      float _541 = (c_ps_user[12].y) * _277;
      float _542 = (c_ps_user[12].y) * _278;
      float _543 = (c_ps_user[12].y) * _279;
      float _545 = (c_ps_user[12].z) * _300;
      float _546 = (c_ps_user[12].z) * _301;
      float _547 = (c_ps_user[12].z) * _302;
      float _553 = -0.0f - _167;
      float _554 = -0.0f - _168;
      float _555 = -0.0f - _169;
      float _556 = dot(float3(_553, _554, _555), float3(_162, _163, _164));
      float _557 = 1.0f - _556;
      float _558 = _557 * _557;
      float _559 = _557 * 0.9722222089767456f;
      float _560 = _558 * _558;
      float _561 = _560 * _559;
      float _562 = _561 + 0.02777777425944805f;
      float _563 = _562 * (c_ps_user[13].z);
      float _565 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _566 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _567 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _570 = (c_ps_user[14].x) * _314;
      float _571 = (c_ps_user[14].x) * _316;
      float _572 = (c_ps_user[14].x) * _318;
      float _574 = (c_ps_user[14].y) * _353;
      float _575 = (c_ps_user[14].y) * _354;
      float _576 = (c_ps_user[14].y) * _355;
      float _578 = (c_ps_user[14].z) * _360;
      float _579 = (c_ps_user[14].z) * _361;
      float _580 = (c_ps_user[14].z) * _362;
      float _582 = (c_ps_user[14].w) * _333;
      float _583 = (c_ps_user[14].w) * _334;
      float _584 = (c_ps_user[14].w) * _335;
      float _587 = (c_ps_user[15].x) * _439;
      float _588 = (c_ps_user[15].x) * _440;
      float _589 = (c_ps_user[15].x) * _441;
      float _591 = _462 * _444;
      float _592 = _591 * (c_ps_user[15].y);
      float _593 = _465 * _445;
      float _594 = _593 * (c_ps_user[15].y);
      float _595 = _468 * _446;
      float _596 = _595 * (c_ps_user[15].y);
      float _598 = (c_ps_user[15].z) * _336;
      float _599 = (c_ps_user[15].z) * _337;
      float _600 = (c_ps_user[15].z) * _338;
      float _601 = _592 + _598;
      float _602 = _601 * _436;
      float _603 = _574 + (c_ps_user[13].x);
      float _604 = _603 + _578;
      float _605 = c_diffuse_scale.x * _604;
      float _606 = _605 * c_mat_diffuse.x;
      float _607 = (c_ps_user[13].y) + _531;
      float _608 = _607 + _587;
      float _609 = c_mat_specular.x * _608;
      float _610 = _609 * c_specular_scale.x;
      float _611 = _527 + _514;
      float _612 = _611 + _534;
      float _613 = _612 + (c_ps_user[12].w);
      float _614 = _613 + _537;
      float _615 = _614 + _541;
      float _616 = _615 + _545;
      float _617 = _616 + _565;
      float _618 = _617 + _570;
      float _619 = _618 + _582;
      float _620 = _619 + _563;
      float _621 = _620 + _602;
      float _622 = _621 + _606;
      float _623 = _622 + _610;
      float _624 = _594 + _599;
      float _625 = _624 * _436;
      float _626 = _575 + (c_ps_user[13].x);
      float _627 = _626 + _579;
      float _628 = c_diffuse_scale.y * _627;
      float _629 = _628 * c_mat_diffuse.y;
      float _630 = _607 + _588;
      float _631 = c_mat_specular.y * _630;
      float _632 = _631 * c_specular_scale.y;
      float _633 = _528 + _515;
      float _634 = _633 + _534;
      float _635 = _634 + (c_ps_user[12].w);
      float _636 = _635 + _538;
      float _637 = _636 + _542;
      float _638 = _637 + _546;
      float _639 = _638 + _566;
      float _640 = _639 + _571;
      float _641 = _640 + _583;
      float _642 = _641 + _563;
      float _643 = _642 + _625;
      float _644 = _643 + _629;
      float _645 = _644 + _632;
      float _646 = _596 + _600;
      float _647 = _646 * _436;
      float _648 = _576 + (c_ps_user[13].x);
      float _649 = _648 + _580;
      float _650 = c_diffuse_scale.z * _649;
      float _651 = _650 * c_mat_diffuse.z;
      float _652 = _607 + _589;
      float _653 = c_mat_specular.z * _652;
      float _654 = _653 * c_specular_scale.z;
      float _655 = _534 + _516;
      float _656 = _655 + _529;
      float _657 = _656 + (c_ps_user[12].w);
      float _658 = _657 + _539;
      float _659 = _658 + _543;
      float _660 = _659 + _547;
      float _661 = _660 + _567;
      float _662 = _661 + _572;
      float _663 = _662 + _584;
      float _664 = _663 + _563;
      float _665 = _664 + _647;
      float _666 = _665 + _651;
      float _667 = _666 + _654;
      _669 = 0.0f;
      _670 = 0.0f;
      _671 = 0.0f;
      _672 = _623;
      _673 = _645;
      _674 = _667;
   }
   else
   {
      _669 = _506;
      _670 = _507;
      _671 = _508;
      _672 = _503;
      _673 = _504;
      _674 = _505;
   }
   float _675 = _672 + _669;
   float _676 = _673 + _670;
   float _677 = _674 + _671;
   float _678 = dot(float3(_167, _168, _169), float3(_162, _163, _164));
   float _679 = _678 * 2.0f;
   float _680 = _679 * _162;
   float _681 = _679 * _163;
   float _682 = _679 * _164;
   float _683 = _167 - _680;
   float _684 = _168 - _681;
   float _685 = _169 - _682;
   float _686 = -0.0f - _683;
   float4 _687 = s2c_t.Sample(s2c_s, float3(_686, _684, _685));
   float _694 = _208 * _207;
   float _695 = _177 + 0.75f;
   float _696 = saturate(_695);
   float _697 = _694 * _696;
   float _698 = max(_697, (c_light_vec[5].y));
   float _699 = max(_687.w, 0.003921568859368563f);
   float _700 = 0.25f / _699;
   float _701 = _700 * _687.x;
   float _702 = _700 * _687.y;
   float _703 = _700 * _687.z;
   float _704 = _110 * TEXCOORD_2.w;
   float _705 = _704 * _698;
   float _706 = _701 * _701;
   float _707 = _706 * _705;
   float _708 = _702 * _702;
   float _709 = _708 * _705;
   float _710 = _703 * _703;
   float _711 = _710 * _705;
   float _712 = _675 + _707;
   float _713 = _676 + _709;
   float _714 = _677 + _711;
   float _721 = c_mat_opacity * _57.w;
   float _722 = _721 * c_opacity_scale;
   float _723 = _722 * c_global_opacity;
   int _726 = c_bool & 16;
   bool _727 = (_726 == 0);
   if (!_727)
   {
      float _731 = c_tone_control.x * _712;
      float _732 = c_tone_control.x * _713;
      float _733 = c_tone_control.x * _714;
      float _735 = log2(_731);
      float _736 = log2(_732);
      float _737 = log2(_733);
      float _738 = _735 * c_tone_control.y;
      float _739 = _736 * c_tone_control.y;
      float _740 = _737 * c_tone_control.y;
      float _741 = exp2(_738);
      float _742 = exp2(_739);
      float _743 = exp2(_740);
      _793 = _741;
      _794 = _742;
      _795 = _743;
   }
   else
   {
      int _745 = c_bool & 32768;
      bool _746 = (_745 == 0);
      if (_746)
      {
         float _750 = c_tone_control.x * _712;
         float _751 = c_tone_control.x * _713;
         float _752 = c_tone_control.x * _714;
         float _753 = _750 * 2.0f;
         float _754 = _751 * 2.0f;
         float _755 = _752 * 2.0f;
         float _756 = _750 * 0.30000001192092896f;
         float _757 = _751 * 0.30000001192092896f;
         float _758 = _752 * 0.30000001192092896f;
         float _759 = _756 + 0.05000000074505806f;
         float _760 = _757 + 0.05000000074505806f;
         float _761 = _758 + 0.05000000074505806f;
         float _762 = _759 * _753;
         float _763 = _760 * _754;
         float _764 = _761 * _755;
         float _765 = _762 + 0.004000000189989805f;
         float _766 = _763 + 0.004000000189989805f;
         float _767 = _764 + 0.004000000189989805f;
         float _768 = _756 + 0.5f;
         float _769 = _757 + 0.5f;
         float _770 = _758 + 0.5f;
         float _771 = _768 * _753;
         float _772 = _769 * _754;
         float _773 = _770 * _755;
         float _774 = _771 + 0.06000000238418579f;
         float _775 = _772 + 0.06000000238418579f;
         float _776 = _773 + 0.06000000238418579f;
         float _777 = _765 / _774;
         float _778 = _766 / _775;
         float _779 = _767 / _776;
         float _780 = _777 + -0.06666666269302368f;
         float _781 = _778 + -0.06666666269302368f;
         float _782 = _779 + -0.06666666269302368f;
         float _783 = _780 * 1.3790643215179443f;
         float _784 = _781 * 1.3790643215179443f;
         float _785 = _782 * 1.3790643215179443f;
         float _786 = YKR_MaterialCurve(_783, _750);
         float _787 = YKR_MaterialCurve(_784, _751);
         float _788 = YKR_MaterialCurve(_785, _752);
         float _789 = sqrt(_786);
         float _790 = sqrt(_787);
         float _791 = sqrt(_788);
         _793 = _789;
         _794 = _790;
         _795 = _791;
      }
      else
      {
         _793 = _712;
         _794 = _713;
         _795 = _714;
      }
   }
   int _796 = c_bool & 4;
   bool _797 = (_796 == 0);
   if (_797)
   {
      float _799 = saturate(COLOR_1.x);
      float _804 = (c_fog_param[0].x) - _793;
      float _805 = (c_fog_param[0].y) - _794;
      float _806 = (c_fog_param[0].z) - _795;
      float _807 = _804 * _799;
      float _808 = _805 * _799;
      float _809 = _806 * _799;
      _811 = _807;
      _812 = _808;
      _813 = _809;
   }
   else
   {
      _811 = COLOR_1.x;
      _812 = COLOR_1.y;
      _813 = COLOR_1.z;
   }
   float _814 = _811 + _793;
   float _815 = _812 + _794;
   float _816 = _813 + _795;
   float _822 = (c_ps_col_scl[0].x) * _814;
   float _823 = (c_ps_col_scl[0].y) * _815;
   float _824 = (c_ps_col_scl[0].z) * _816;
   float _825 = (c_ps_col_scl[0].w) * _723;
   SV_Target.x = _822;
   SV_Target.y = _823;
   SV_Target.z = _824;
   SV_Target.w = _825;
   int _828 = c_bool & 8;
   bool _829 = (_828 != 0);
   float _830 = select(_829, _814, _822);
   float _831 = select(_829, _815, _823);
   float _832 = select(_829, _816, _824);
   float _833 = select(_829, _723, _825);
   SV_Target.x = _830;
   SV_Target.y = _831;
   SV_Target.z = _832;
   SV_Target.w = _833;
   return SV_Target;
}
