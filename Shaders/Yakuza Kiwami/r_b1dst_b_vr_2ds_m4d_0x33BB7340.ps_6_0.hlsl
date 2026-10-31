// Yakuza Kiwami material r_b1dst_b(vr)2ds_m4d (0x33BB7340, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s4_s : register(s4);

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
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _62 = _57.x * _57.x;
   float _63 = _57.y * _57.y;
   float _64 = _57.z * _57.z;
   float _68 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _69 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _70 = s3_t.Sample(s3_s, float2(_68, _69));
   float _77 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _78 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _79 = s5_t.Sample(s5_s, float2(_77, _78));
   float4 _82 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _87 = _82.x * _82.x;
   float _88 = _82.y * _82.y;
   float _89 = _82.z * _82.z;
   float _93 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _94 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _95 = s4_t.Sample(s4_s, float2(_93, _94));
   float4 _99 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _102 = _82.w * COLOR_1.w;
   float _103 = _87 - _62;
   float _104 = _88 - _63;
   float _105 = _89 - _64;
   float _106 = _103 * _102;
   float _107 = _104 * _102;
   float _108 = _105 * _102;
   float _109 = _106 + _62;
   float _110 = _107 + _63;
   float _111 = _108 + _64;
   float _112 = _95.x - _70.x;
   float _113 = _95.y - _70.y;
   float _114 = _95.z - _70.z;
   float _115 = _112 * _102;
   float _116 = _113 * _102;
   float _117 = _114 * _102;
   float _118 = _115 + _70.x;
   float _119 = _116 + _70.y;
   float _120 = _117 + _70.z;
   float _121 = 0.5f - _79.x;
   float _122 = 0.5f - _79.y;
   float _123 = _102 * _121;
   float _124 = _102 * _122;
   float _125 = _123 + _79.x;
   float _126 = _124 + _79.y;
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
   float _151 = _148 + _144;
   float _152 = _149 + _145;
   float _153 = _150 + _146;
   float _154 = _134 * TEXCOORD_2.x;
   float _155 = _134 * TEXCOORD_2.y;
   float _156 = _134 * TEXCOORD_2.z;
   float _157 = _151 + _154;
   float _158 = _152 + _155;
   float _159 = _153 + _156;
   float _160 = dot(float3(_157, _158, _159), float3(_157, _158, _159));
   float _161 = rsqrt(_160);
   float _162 = _157 * _161;
   float _163 = _158 * _161;
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
   float _263;
   float _264;
   float _265;
   float _266;
   float _280;
   float _281;
   float _327;
   float _328;
   float _329;
   float _364;
   float _365;
   float _366;
   float _367;
   float _368;
   float _369;
   float _523;
   float _524;
   float _525;
   float _526;
   float _527;
   float _528;
   float _704;
   float _705;
   float _706;
   float _707;
   float _708;
   float _709;
   float _786;
   float _787;
   float _788;
   float _804;
   float _805;
   float _806;
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
   float _215 = (c_sdw_param[1].x) * _99.y;
   float _217 = _215 + (c_sdw_param[1].y);
   float _218 = _217 * _99.x;
   float _219 = 1.0f - (c_light_col[5].x);
   float _220 = 1.0f - (c_light_col[5].y);
   float _221 = 1.0f - (c_light_col[5].z);
   float _222 = _218 * _219;
   float _223 = _218 * _220;
   float _224 = _218 * _221;
   float _225 = _222 + (c_light_col[5].x);
   float _226 = _223 + (c_light_col[5].y);
   float _227 = _224 + (c_light_col[5].z);
   float _231 = c_rt_size.z * SV_Position.x;
   float _232 = c_rt_size.w * SV_Position.y;
   float4 _233 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_231, _232)));
   int _240 = c_bool & 16384;
   bool _241 = (_240 == 0);
   if (!_241)
   {
      float _243 = _233.x * 2.0f;
      float _244 = _233.y * 2.0f;
      float _245 = _233.z * 2.0f;
      float _246 = _233.w * 2.0f;
      _263 = _243;
      _264 = _244;
      _265 = _245;
      _266 = _246;
   }
   else
   {
      int _248 = c_bool & 8192;
      bool _249 = (_248 == 0);
      if (!_249)
      {
         float _251 = _233.x * 2.0f;
         float _252 = _233.y * 2.0f;
         float _253 = _233.z * 2.0f;
         float _254 = _233.w * 2.0f;
         _263 = _251;
         _264 = _252;
         _265 = _253;
         _266 = _254;
      }
      else
      {
         float _258 = _233.x / c_tone_control.z;
         float _259 = _233.y / c_tone_control.z;
         float _260 = _233.z / c_tone_control.z;
         float _261 = _233.w / c_tone_control.z;
         _263 = _258;
         _264 = _259;
         _265 = _260;
         _266 = _261;
      }
   }
   int _267 = c_bool & 8;
   bool _268 = (_267 == 0);
   if (!_268)
   {
      float _273 = (c_sdw_param[0].x) * _207;
      float _274 = (c_sdw_param[0].z) * _208;
      float _277 = _273 + (c_sdw_param[0].y);
      float _278 = _274 + (c_sdw_param[0].w);
      _280 = _277;
      _281 = _278;
   }
   else
   {
      _280 = _207;
      _281 = _208;
   }
   float _282 = _281 * _280;
   float _287 = 1.0f - (c_light_col[5].x);
   float _288 = 1.0f - (c_light_col[5].y);
   float _289 = 1.0f - (c_light_col[5].z);
   float _290 = _287 * _282;
   float _291 = _288 * _282;
   float _292 = _289 * _282;
   float _293 = _290 + (c_light_col[5].x);
   float _294 = _291 + (c_light_col[5].y);
   float _295 = _292 + (c_light_col[5].z);
   float _296 = _195 * COLOR.x;
   float _297 = _296 * _225;
   float _298 = _195 * COLOR.y;
   float _299 = _298 * _226;
   float _300 = _195 * COLOR.z;
   float _301 = _300 * _227;
   float _302 = _293 * _225;
   float _303 = _294 * _226;
   float _304 = _295 * _227;
   if (!_268)
   {
      float _308 = (c_sdw_param[2].x) * _293;
      float _309 = (c_sdw_param[2].x) * _294;
      float _310 = (c_sdw_param[2].x) * _295;
      float _312 = _308 + (c_sdw_param[2].y);
      float _313 = _309 + (c_sdw_param[2].y);
      float _314 = _310 + (c_sdw_param[2].y);
      float _316 = (c_sdw_param[2].z) * _225;
      float _317 = (c_sdw_param[2].z) * _226;
      float _318 = (c_sdw_param[2].z) * _227;
      float _320 = _316 + (c_sdw_param[2].w);
      float _321 = _317 + (c_sdw_param[2].w);
      float _322 = _318 + (c_sdw_param[2].w);
      float _323 = _320 * _312;
      float _324 = _321 * _313;
      float _325 = _322 * _314;
      _327 = _323;
      _328 = _324;
      _329 = _325;
   }
   else
   {
      _327 = _302;
      _328 = _303;
      _329 = _304;
   }
   float _330 = _195 * _99.x;
   float _331 = _327 * _330;
   float _332 = _328 * _330;
   float _333 = _329 * _330;
   float _344 = c_mat_emissive.x * _109;
   float _345 = _344 * c_emissive_scale;
   float _346 = c_mat_emissive.y * _110;
   float _347 = _346 * c_emissive_scale;
   float _348 = c_mat_emissive.z * _111;
   float _349 = _348 * c_emissive_scale;
   int _350 = c_bool & 2048;
   bool _351 = (_350 == 0);
   if (_351)
   {
      float _353 = _281 * _263;
      float _354 = _281 * _264;
      float _355 = _281 * _265;
      float _356 = _353 + TEXCOORD_4.x;
      float _357 = _354 + TEXCOORD_4.y;
      float _358 = _355 + TEXCOORD_4.z;
      float _359 = _281 * _266;
      float _360 = _359 + TEXCOORD_5.x;
      float _361 = _359 + TEXCOORD_5.y;
      float _362 = _359 + TEXCOORD_5.z;
      _364 = _356;
      _365 = _357;
      _366 = _358;
      _367 = _360;
      _368 = _361;
      _369 = _362;
   }
   else
   {
      _364 = TEXCOORD_4.x;
      _365 = TEXCOORD_4.y;
      _366 = TEXCOORD_4.z;
      _367 = TEXCOORD_5.x;
      _368 = TEXCOORD_5.y;
      _369 = TEXCOORD_5.z;
   }
   float _374 = -0.0f - (c_light_vec[1].x);
   float _375 = -0.0f - (c_light_vec[1].y);
   float _376 = -0.0f - (c_light_vec[1].z);
   float _377 = dot(float3(_162, _163, _164), float3(_374, _375, _376));
   float _378 = saturate(_377);
   float _383 = saturate(_177);
   float _384 = (c_light_col[0].x) * _383;
   float _385 = (c_light_col[0].y) * _383;
   float _386 = (c_light_col[0].z) * _383;
   float _391 = (c_light_col[1].x) * _378;
   float _392 = (c_light_col[1].y) * _378;
   float _393 = (c_light_col[1].z) * _378;
   float _402 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _403 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _404 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _405 = _384 * _293;
   float _406 = _385 * _294;
   float _407 = _386 * _295;
   float _408 = _391 * _281;
   float _409 = _392 * _281;
   float _410 = _393 * _281;
   float _411 = _408 + _405;
   float _412 = _409 + _406;
   float _413 = _410 + _407;
   float _414 = _402 * _411;
   float _415 = _403 * _412;
   float _416 = _404 * _413;
   float _417 = _345 + c_scene_ambient.x;
   float _418 = _417 + _414;
   float _419 = _347 + c_scene_ambient.y;
   float _420 = _419 + _415;
   float _421 = _349 + c_scene_ambient.z;
   float _422 = _421 + _416;
   float _423 = _418 * _297;
   float _424 = _420 * _299;
   float _425 = _422 * _301;
   float _426 = _364 * _331;
   float _427 = _365 * _332;
   float _428 = _366 * _333;
   float _429 = _423 + _426;
   float _430 = _424 + _427;
   float _431 = _425 + _428;
   float _436 = (c_light_vec[0].x) + _167;
   float _437 = (c_light_vec[0].y) + _168;
   float _438 = (c_light_vec[0].z) + _169;
   float _439 = dot(float3(_436, _437, _438), float3(_436, _437, _438));
   float _440 = rsqrt(_439);
   float _441 = _436 * _440;
   float _442 = -0.0f - _441;
   float _443 = _437 * _440;
   float _444 = -0.0f - _443;
   float _445 = _438 * _440;
   float _446 = -0.0f - _445;
   float _447 = dot(float3(_442, _444, _446), float3(_162, _163, _164));
   float _448 = saturate(_447);
   float _449 = _168 + -0.30000001192092896f;
   float _450 = dot(float3(_167, _449, _169), float3(_167, _449, _169));
   float _451 = rsqrt(_450);
   float _452 = _167 * _451;
   float _453 = -0.0f - _452;
   float _454 = _449 * _451;
   float _455 = -0.0f - _454;
   float _456 = _169 * _451;
   float _457 = -0.0f - _456;
   float _458 = dot(float3(_453, _455, _457), float3(_162, _163, _164));
   float _459 = saturate(_458);
   float _462 = log2(_448);
   float _463 = log2(_459);
   float _464 = _462 * c_mat_power;
   float _465 = _463 * c_mat_power;
   float _466 = exp2(_464);
   float _467 = exp2(_465);
   float _470 = _466 * (c_light_col[0].x);
   float _471 = _466 * (c_light_col[0].y);
   float _472 = _466 * (c_light_col[0].z);
   float _473 = _467 * 2.0f;
   float _474 = _473 * (c_light_vec[5].x);
   float _475 = _474 * (c_light_col[1].x);
   float _476 = _474 * (c_light_col[1].y);
   float _477 = _474 * (c_light_col[1].z);
   float _478 = _470 * _293;
   float _479 = _471 * _294;
   float _480 = _472 * _295;
   float _481 = _475 * _378;
   float _482 = _476 * _378;
   float _483 = _477 * _378;
   float _484 = _481 + _478;
   float _485 = _482 + _479;
   float _486 = _483 + _480;
   float _493 = c_mat_specular.x * c_specular_scale.x;
   float _494 = _493 * _297;
   float _495 = _494 * _484;
   float _496 = c_mat_specular.y * c_specular_scale.y;
   float _497 = _496 * _299;
   float _498 = _497 * _485;
   float _499 = c_mat_specular.z * c_specular_scale.z;
   float _500 = _499 * _301;
   float _501 = _500 * _486;
   float _502 = _367 * _331;
   float _503 = _502 * _467;
   float _504 = _368 * _332;
   float _505 = _504 * _467;
   float _506 = _369 * _333;
   float _507 = _506 * _467;
   float _508 = _495 + _503;
   float _509 = _498 + _505;
   float _510 = _501 + _507;
   if (!_351)
   {
      float _512 = _281 * _263;
      float _513 = _281 * _264;
      float _514 = _281 * _265;
      float _515 = _429 + _512;
      float _516 = _430 + _513;
      float _517 = _431 + _514;
      float _518 = _281 * _266;
      float _519 = _508 + _518;
      float _520 = _509 + _518;
      float _521 = _510 + _518;
      _523 = _515;
      _524 = _516;
      _525 = _517;
      _526 = _519;
      _527 = _520;
      _528 = _521;
   }
   else
   {
      _523 = _429;
      _524 = _430;
      _525 = _431;
      _526 = _508;
      _527 = _509;
      _528 = _510;
   }
   int _529 = c_bool & 128;
   bool _530 = (_529 != 0);
   float _531 = select(_530, 0.0f, _526);
   float _532 = select(_530, 0.0f, _527);
   float _533 = select(_530, 0.0f, _528);
   float _534 = _523 * _109;
   float _535 = _524 * _110;
   float _536 = _525 * _111;
   float _537 = _531 * _118;
   float _538 = _532 * _119;
   float _539 = _533 * _120;
   int _540 = c_bool & 32768;
   bool _541 = (_540 == 0);
   if (!_541)
   {
      float _545 = (c_ps_user[11].x) * _109;
      float _546 = (c_ps_user[11].x) * _110;
      float _547 = (c_ps_user[11].x) * _111;
      float _549 = _162 + 1.0f;
      float _550 = _163 + 1.0f;
      float _551 = _164 + 1.0f;
      float _552 = _549 * 0.5f;
      float _553 = _550 * 0.5f;
      float _554 = _551 * 0.5f;
      float _555 = saturate(_552);
      float _556 = saturate(_553);
      float _557 = saturate(_554);
      float _558 = (c_ps_user[11].y) * _555;
      float _559 = _556 * (c_ps_user[11].y);
      float _560 = _557 * (c_ps_user[11].y);
      float _562 = (c_ps_user[11].z) * _118;
      float _563 = (c_ps_user[11].z) * _119;
      float _564 = (c_ps_user[11].z) * _120;
      float _566 = c_mat_power * 0.009999999776482582f;
      float _567 = _566 * (c_ps_user[11].w);
      float _570 = (c_ps_user[12].x) * _293;
      float _571 = (c_ps_user[12].x) * _294;
      float _572 = (c_ps_user[12].x) * _295;
      float _574 = (c_ps_user[12].y) * _297;
      float _575 = (c_ps_user[12].y) * _299;
      float _576 = (c_ps_user[12].y) * _301;
      float _578 = (c_ps_user[12].z) * _331;
      float _579 = (c_ps_user[12].z) * _332;
      float _580 = (c_ps_user[12].z) * _333;
      float _586 = -0.0f - _167;
      float _587 = -0.0f - _168;
      float _588 = -0.0f - _169;
      float _589 = dot(float3(_586, _587, _588), float3(_162, _163, _164));
      float _590 = 1.0f - _589;
      float _591 = _590 * _590;
      float _592 = _590 * 0.9722222089767456f;
      float _593 = _591 * _591;
      float _594 = _593 * _592;
      float _595 = _594 + 0.02777777425944805f;
      float _596 = _595 * (c_ps_user[13].z);
      float _598 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _599 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _600 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _603 = (c_ps_user[14].x) * _345;
      float _604 = (c_ps_user[14].x) * _347;
      float _605 = (c_ps_user[14].x) * _349;
      float _607 = (c_ps_user[14].y) * _384;
      float _608 = (c_ps_user[14].y) * _385;
      float _609 = (c_ps_user[14].y) * _386;
      float _611 = (c_ps_user[14].z) * _391;
      float _612 = (c_ps_user[14].z) * _392;
      float _613 = (c_ps_user[14].z) * _393;
      float _615 = (c_ps_user[14].w) * _364;
      float _616 = (c_ps_user[14].w) * _365;
      float _617 = (c_ps_user[14].w) * _366;
      float _620 = (c_ps_user[15].x) * _470;
      float _621 = (c_ps_user[15].x) * _471;
      float _622 = (c_ps_user[15].x) * _472;
      float _624 = _493 * _475;
      float _625 = _624 * (c_ps_user[15].y);
      float _626 = _496 * _476;
      float _627 = _626 * (c_ps_user[15].y);
      float _628 = _499 * _477;
      float _629 = _628 * (c_ps_user[15].y);
      float _631 = (c_ps_user[15].z) * _367;
      float _632 = (c_ps_user[15].z) * _368;
      float _633 = (c_ps_user[15].z) * _369;
      float _634 = _625 + _631;
      float _635 = _634 * _467;
      float _636 = _607 + (c_ps_user[13].x);
      float _637 = _636 + _611;
      float _638 = c_diffuse_scale.x * _637;
      float _639 = _638 * c_mat_diffuse.x;
      float _640 = (c_ps_user[13].y) + _562;
      float _641 = _640 + _620;
      float _642 = c_mat_specular.x * _641;
      float _643 = _642 * c_specular_scale.x;
      float _644 = _558 + _545;
      float _645 = _644 + _567;
      float _646 = _645 + (c_ps_user[12].w);
      float _647 = _646 + _570;
      float _648 = _647 + _574;
      float _649 = _648 + _578;
      float _650 = _649 + _598;
      float _651 = _650 + _603;
      float _652 = _651 + _615;
      float _653 = _652 + _596;
      float _654 = _653 + _635;
      float _655 = _654 + _639;
      float _656 = _655 + _643;
      float _657 = _627 + _632;
      float _658 = _657 * _467;
      float _659 = _608 + (c_ps_user[13].x);
      float _660 = _659 + _612;
      float _661 = c_diffuse_scale.y * _660;
      float _662 = _661 * c_mat_diffuse.y;
      float _663 = (c_ps_user[13].y) + _563;
      float _664 = _663 + _621;
      float _665 = c_mat_specular.y * _664;
      float _666 = _665 * c_specular_scale.y;
      float _667 = _559 + _546;
      float _668 = _667 + _567;
      float _669 = _668 + (c_ps_user[12].w);
      float _670 = _669 + _571;
      float _671 = _670 + _575;
      float _672 = _671 + _579;
      float _673 = _672 + _599;
      float _674 = _673 + _604;
      float _675 = _674 + _616;
      float _676 = _675 + _596;
      float _677 = _676 + _658;
      float _678 = _677 + _662;
      float _679 = _678 + _666;
      float _680 = _629 + _633;
      float _681 = _680 * _467;
      float _682 = _609 + (c_ps_user[13].x);
      float _683 = _682 + _613;
      float _684 = c_diffuse_scale.z * _683;
      float _685 = _684 * c_mat_diffuse.z;
      float _686 = (c_ps_user[13].y) + _564;
      float _687 = _686 + _622;
      float _688 = c_mat_specular.z * _687;
      float _689 = _688 * c_specular_scale.z;
      float _690 = _567 + _547;
      float _691 = _690 + _560;
      float _692 = _691 + (c_ps_user[12].w);
      float _693 = _692 + _572;
      float _694 = _693 + _576;
      float _695 = _694 + _580;
      float _696 = _695 + _600;
      float _697 = _696 + _605;
      float _698 = _697 + _617;
      float _699 = _698 + _596;
      float _700 = _699 + _681;
      float _701 = _700 + _685;
      float _702 = _701 + _689;
      _704 = _656;
      _705 = _679;
      _706 = _702;
      _707 = 0.0f;
      _708 = 0.0f;
      _709 = 0.0f;
   }
   else
   {
      _704 = _534;
      _705 = _535;
      _706 = _536;
      _707 = _537;
      _708 = _538;
      _709 = _539;
   }
   float _710 = _707 + _704;
   float _711 = _708 + _705;
   float _712 = _709 + _706;
   float _717 = _57.w * COLOR.w;
   float _718 = _717 * c_mat_opacity;
   float _719 = _718 * c_global_opacity;
   float _720 = _719 * c_opacity_scale;
   int _721 = c_bool & 16;
   bool _722 = (_721 == 0);
   if (!_722)
   {
      float _726 = c_tone_control.x * _710;
      float _727 = c_tone_control.x * _711;
      float _728 = c_tone_control.x * _712;
      float _730 = log2(_726);
      float _731 = log2(_727);
      float _732 = log2(_728);
      float _733 = _730 * c_tone_control.y;
      float _734 = _731 * c_tone_control.y;
      float _735 = _732 * c_tone_control.y;
      float _736 = exp2(_733);
      float _737 = exp2(_734);
      float _738 = exp2(_735);
      _786 = _736;
      _787 = _737;
      _788 = _738;
   }
   else
   {
      if (_541)
      {
         float _743 = c_tone_control.x * _710;
         float _744 = c_tone_control.x * _711;
         float _745 = c_tone_control.x * _712;
         float _746 = _743 * 2.0f;
         float _747 = _744 * 2.0f;
         float _748 = _745 * 2.0f;
         float _749 = _743 * 0.30000001192092896f;
         float _750 = _744 * 0.30000001192092896f;
         float _751 = _745 * 0.30000001192092896f;
         float _752 = _749 + 0.05000000074505806f;
         float _753 = _750 + 0.05000000074505806f;
         float _754 = _751 + 0.05000000074505806f;
         float _755 = _752 * _746;
         float _756 = _753 * _747;
         float _757 = _754 * _748;
         float _758 = _755 + 0.004000000189989805f;
         float _759 = _756 + 0.004000000189989805f;
         float _760 = _757 + 0.004000000189989805f;
         float _761 = _749 + 0.5f;
         float _762 = _750 + 0.5f;
         float _763 = _751 + 0.5f;
         float _764 = _761 * _746;
         float _765 = _762 * _747;
         float _766 = _763 * _748;
         float _767 = _764 + 0.06000000238418579f;
         float _768 = _765 + 0.06000000238418579f;
         float _769 = _766 + 0.06000000238418579f;
         float _770 = _758 / _767;
         float _771 = _759 / _768;
         float _772 = _760 / _769;
         float _773 = _770 + -0.06666666269302368f;
         float _774 = _771 + -0.06666666269302368f;
         float _775 = _772 + -0.06666666269302368f;
         float _776 = _773 * 1.3790643215179443f;
         float _777 = _774 * 1.3790643215179443f;
         float _778 = _775 * 1.3790643215179443f;
         float _779 = YKR_MaterialCurve(_776, _743);
         float _780 = YKR_MaterialCurve(_777, _744);
         float _781 = YKR_MaterialCurve(_778, _745);
         float _782 = sqrt(_779);
         float _783 = sqrt(_780);
         float _784 = sqrt(_781);
         _786 = _782;
         _787 = _783;
         _788 = _784;
      }
      else
      {
         _786 = _710;
         _787 = _711;
         _788 = _712;
      }
   }
   int _789 = c_bool & 4;
   bool _790 = (_789 == 0);
   if (_790)
   {
      float _792 = saturate(COLOR_1.x);
      float _797 = (c_fog_param[0].x) - _786;
      float _798 = (c_fog_param[0].y) - _787;
      float _799 = (c_fog_param[0].z) - _788;
      float _800 = _797 * _792;
      float _801 = _798 * _792;
      float _802 = _799 * _792;
      _804 = _800;
      _805 = _801;
      _806 = _802;
   }
   else
   {
      _804 = COLOR_1.x;
      _805 = COLOR_1.y;
      _806 = COLOR_1.z;
   }
   float _807 = _804 + _786;
   float _808 = _805 + _787;
   float _809 = _806 + _788;
   float _815 = (c_ps_col_scl[0].x) * _807;
   float _816 = (c_ps_col_scl[0].y) * _808;
   float _817 = (c_ps_col_scl[0].z) * _809;
   float _818 = (c_ps_col_scl[0].w) * _720;
   SV_Target.x = _815;
   SV_Target.y = _816;
   SV_Target.z = _817;
   SV_Target.w = _818;
   int _821 = c_bool & 8;
   bool _822 = (_821 != 0);
   float _823 = select(_822, _807, _815);
   float _824 = select(_822, _808, _816);
   float _825 = select(_822, _809, _817);
   float _826 = select(_822, _720, _818);
   SV_Target.x = _823;
   SV_Target.y = _824;
   SV_Target.z = _825;
   SV_Target.w = _826;
   return SV_Target;
}
