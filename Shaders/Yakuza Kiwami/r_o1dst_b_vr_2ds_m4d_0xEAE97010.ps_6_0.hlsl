// Yakuza Kiwami material r_o1dst_b(vr)2ds_m4d (0xEAE97010, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _56 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _61 = _56.x * _56.x;
   float _62 = _56.y * _56.y;
   float _63 = _56.z * _56.z;
   float _67 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _68 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _69 = s3_t.Sample(s3_s, float2(_67, _68));
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
   float4 _98 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _101 = _81.w * COLOR_1.w;
   float _102 = _86 - _61;
   float _103 = _87 - _62;
   float _104 = _88 - _63;
   float _105 = _102 * _101;
   float _106 = _103 * _101;
   float _107 = _104 * _101;
   float _108 = _105 + _61;
   float _109 = _106 + _62;
   float _110 = _107 + _63;
   float _111 = _94.x - _69.x;
   float _112 = _94.y - _69.y;
   float _113 = _94.z - _69.z;
   float _114 = _111 * _101;
   float _115 = _112 * _101;
   float _116 = _113 * _101;
   float _117 = _114 + _69.x;
   float _118 = _115 + _69.y;
   float _119 = _116 + _69.z;
   float _120 = 0.5f - _78.x;
   float _121 = 0.5f - _78.y;
   float _122 = _101 * _120;
   float _123 = _101 * _121;
   float _124 = _122 + _78.x;
   float _125 = _123 + _78.y;
   float _126 = _124 * 2.0f;
   float _127 = _125 * 2.0f;
   float _128 = _126 + -1.0f;
   float _129 = _127 + -1.0f;
   float _130 = dot(float2(_128, _129), float2(_128, _129));
   float _131 = 1.0f - _130;
   float _132 = saturate(_131);
   float _133 = sqrt(_132);
   float _134 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _135 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _136 = _134 - _135;
   float _137 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _138 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _139 = _137 - _138;
   float _140 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _141 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _142 = _140 - _141;
   float _143 = _128 * TEXCOORD_6.x;
   float _144 = _128 * TEXCOORD_6.y;
   float _145 = _128 * TEXCOORD_6.z;
   float _146 = _129 * TEXCOORD_6.w;
   float _147 = _146 * _136;
   float _148 = _146 * _139;
   float _149 = _146 * _142;
   float _150 = _147 + _143;
   float _151 = _148 + _144;
   float _152 = _149 + _145;
   float _153 = _133 * TEXCOORD_2.x;
   float _154 = _133 * TEXCOORD_2.y;
   float _155 = _133 * TEXCOORD_2.z;
   float _156 = _150 + _153;
   float _157 = _151 + _154;
   float _158 = _152 + _155;
   float _159 = dot(float3(_156, _157, _158), float3(_156, _157, _158));
   float _160 = rsqrt(_159);
   float _161 = _156 * _160;
   float _162 = _157 * _160;
   float _163 = _158 * _160;
   float _164 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _165 = rsqrt(_164);
   float _166 = _165 * TEXCOORD_3.x;
   float _167 = _165 * TEXCOORD_3.y;
   float _168 = _165 * TEXCOORD_3.z;
   float _173 = -0.0f - (c_light_vec[0].x);
   float _174 = -0.0f - (c_light_vec[0].y);
   float _175 = -0.0f - (c_light_vec[0].z);
   float _176 = dot(float3(_161, _162, _163), float3(_173, _174, _175));
   float _180 = c_rt_size.z * SV_Position.x;
   float _181 = c_rt_size.w * SV_Position.y;
   int _184 = c_bool & 8;
   bool _185 = (_184 == 0);
   float _192;
   float _193;
   float _194;
   float _206;
   float _207;
   float _262;
   float _263;
   float _264;
   float _265;
   float _279;
   float _280;
   float _326;
   float _327;
   float _328;
   float _363;
   float _364;
   float _365;
   float _366;
   float _367;
   float _368;
   float _522;
   float _523;
   float _524;
   float _525;
   float _526;
   float _527;
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
   if (!_185)
   {
      float4 _187 = s13_t.Sample(s13_s, float2(_180, _181));
      _192 = _187.x;
      _193 = _187.y;
      _194 = _187.w;
   }
   else
   {
      _192 = 1.0f;
      _193 = 1.0f;
      _194 = 1.0f;
   }
   if (!_185)
   {
      float _199 = (c_sdw_param[0].x) * _192;
      float _200 = (c_sdw_param[0].z) * _193;
      float _203 = _199 + (c_sdw_param[0].y);
      float _204 = _200 + (c_sdw_param[0].w);
      _206 = _203;
      _207 = _204;
   }
   else
   {
      _206 = _192;
      _207 = _193;
   }
   float _214 = (c_sdw_param[1].x) * _98.y;
   float _216 = _214 + (c_sdw_param[1].y);
   float _217 = _216 * _98.x;
   float _218 = 1.0f - (c_light_col[5].x);
   float _219 = 1.0f - (c_light_col[5].y);
   float _220 = 1.0f - (c_light_col[5].z);
   float _221 = _217 * _218;
   float _222 = _217 * _219;
   float _223 = _217 * _220;
   float _224 = _221 + (c_light_col[5].x);
   float _225 = _222 + (c_light_col[5].y);
   float _226 = _223 + (c_light_col[5].z);
   float _230 = c_rt_size.z * SV_Position.x;
   float _231 = c_rt_size.w * SV_Position.y;
   float4 _232 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_230, _231)));
   int _239 = c_bool & 16384;
   bool _240 = (_239 == 0);
   if (!_240)
   {
      float _242 = _232.x * 2.0f;
      float _243 = _232.y * 2.0f;
      float _244 = _232.z * 2.0f;
      float _245 = _232.w * 2.0f;
      _262 = _242;
      _263 = _243;
      _264 = _244;
      _265 = _245;
   }
   else
   {
      int _247 = c_bool & 8192;
      bool _248 = (_247 == 0);
      if (!_248)
      {
         float _250 = _232.x * 2.0f;
         float _251 = _232.y * 2.0f;
         float _252 = _232.z * 2.0f;
         float _253 = _232.w * 2.0f;
         _262 = _250;
         _263 = _251;
         _264 = _252;
         _265 = _253;
      }
      else
      {
         float _257 = _232.x / c_tone_control.z;
         float _258 = _232.y / c_tone_control.z;
         float _259 = _232.z / c_tone_control.z;
         float _260 = _232.w / c_tone_control.z;
         _262 = _257;
         _263 = _258;
         _264 = _259;
         _265 = _260;
      }
   }
   int _266 = c_bool & 8;
   bool _267 = (_266 == 0);
   if (!_267)
   {
      float _272 = (c_sdw_param[0].x) * _206;
      float _273 = (c_sdw_param[0].z) * _207;
      float _276 = _272 + (c_sdw_param[0].y);
      float _277 = _273 + (c_sdw_param[0].w);
      _279 = _276;
      _280 = _277;
   }
   else
   {
      _279 = _206;
      _280 = _207;
   }
   float _281 = _280 * _279;
   float _286 = 1.0f - (c_light_col[5].x);
   float _287 = 1.0f - (c_light_col[5].y);
   float _288 = 1.0f - (c_light_col[5].z);
   float _289 = _286 * _281;
   float _290 = _287 * _281;
   float _291 = _288 * _281;
   float _292 = _289 + (c_light_col[5].x);
   float _293 = _290 + (c_light_col[5].y);
   float _294 = _291 + (c_light_col[5].z);
   float _295 = _194 * COLOR.x;
   float _296 = _295 * _224;
   float _297 = _194 * COLOR.y;
   float _298 = _297 * _225;
   float _299 = _194 * COLOR.z;
   float _300 = _299 * _226;
   float _301 = _292 * _224;
   float _302 = _293 * _225;
   float _303 = _294 * _226;
   if (!_267)
   {
      float _307 = (c_sdw_param[2].x) * _292;
      float _308 = (c_sdw_param[2].x) * _293;
      float _309 = (c_sdw_param[2].x) * _294;
      float _311 = _307 + (c_sdw_param[2].y);
      float _312 = _308 + (c_sdw_param[2].y);
      float _313 = _309 + (c_sdw_param[2].y);
      float _315 = (c_sdw_param[2].z) * _224;
      float _316 = (c_sdw_param[2].z) * _225;
      float _317 = (c_sdw_param[2].z) * _226;
      float _319 = _315 + (c_sdw_param[2].w);
      float _320 = _316 + (c_sdw_param[2].w);
      float _321 = _317 + (c_sdw_param[2].w);
      float _322 = _319 * _311;
      float _323 = _320 * _312;
      float _324 = _321 * _313;
      _326 = _322;
      _327 = _323;
      _328 = _324;
   }
   else
   {
      _326 = _301;
      _327 = _302;
      _328 = _303;
   }
   float _329 = _194 * _98.x;
   float _330 = _326 * _329;
   float _331 = _327 * _329;
   float _332 = _328 * _329;
   float _343 = c_mat_emissive.x * _108;
   float _344 = _343 * c_emissive_scale;
   float _345 = c_mat_emissive.y * _109;
   float _346 = _345 * c_emissive_scale;
   float _347 = c_mat_emissive.z * _110;
   float _348 = _347 * c_emissive_scale;
   int _349 = c_bool & 2048;
   bool _350 = (_349 == 0);
   if (_350)
   {
      float _352 = _280 * _262;
      float _353 = _280 * _263;
      float _354 = _280 * _264;
      float _355 = _352 + TEXCOORD_4.x;
      float _356 = _353 + TEXCOORD_4.y;
      float _357 = _354 + TEXCOORD_4.z;
      float _358 = _280 * _265;
      float _359 = _358 + TEXCOORD_5.x;
      float _360 = _358 + TEXCOORD_5.y;
      float _361 = _358 + TEXCOORD_5.z;
      _363 = _355;
      _364 = _356;
      _365 = _357;
      _366 = _359;
      _367 = _360;
      _368 = _361;
   }
   else
   {
      _363 = TEXCOORD_4.x;
      _364 = TEXCOORD_4.y;
      _365 = TEXCOORD_4.z;
      _366 = TEXCOORD_5.x;
      _367 = TEXCOORD_5.y;
      _368 = TEXCOORD_5.z;
   }
   float _373 = -0.0f - (c_light_vec[1].x);
   float _374 = -0.0f - (c_light_vec[1].y);
   float _375 = -0.0f - (c_light_vec[1].z);
   float _376 = dot(float3(_161, _162, _163), float3(_373, _374, _375));
   float _377 = saturate(_376);
   float _382 = saturate(_176);
   float _383 = (c_light_col[0].x) * _382;
   float _384 = (c_light_col[0].y) * _382;
   float _385 = (c_light_col[0].z) * _382;
   float _390 = (c_light_col[1].x) * _377;
   float _391 = (c_light_col[1].y) * _377;
   float _392 = (c_light_col[1].z) * _377;
   float _401 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _402 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _403 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _404 = _383 * _292;
   float _405 = _384 * _293;
   float _406 = _385 * _294;
   float _407 = _390 * _280;
   float _408 = _391 * _280;
   float _409 = _392 * _280;
   float _410 = _407 + _404;
   float _411 = _408 + _405;
   float _412 = _409 + _406;
   float _413 = _401 * _410;
   float _414 = _402 * _411;
   float _415 = _403 * _412;
   float _416 = _344 + c_scene_ambient.x;
   float _417 = _416 + _413;
   float _418 = _346 + c_scene_ambient.y;
   float _419 = _418 + _414;
   float _420 = _348 + c_scene_ambient.z;
   float _421 = _420 + _415;
   float _422 = _417 * _296;
   float _423 = _419 * _298;
   float _424 = _421 * _300;
   float _425 = _363 * _330;
   float _426 = _364 * _331;
   float _427 = _365 * _332;
   float _428 = _422 + _425;
   float _429 = _423 + _426;
   float _430 = _424 + _427;
   float _435 = (c_light_vec[0].x) + _166;
   float _436 = (c_light_vec[0].y) + _167;
   float _437 = (c_light_vec[0].z) + _168;
   float _438 = dot(float3(_435, _436, _437), float3(_435, _436, _437));
   float _439 = rsqrt(_438);
   float _440 = _435 * _439;
   float _441 = -0.0f - _440;
   float _442 = _436 * _439;
   float _443 = -0.0f - _442;
   float _444 = _437 * _439;
   float _445 = -0.0f - _444;
   float _446 = dot(float3(_441, _443, _445), float3(_161, _162, _163));
   float _447 = saturate(_446);
   float _448 = _167 + -0.30000001192092896f;
   float _449 = dot(float3(_166, _448, _168), float3(_166, _448, _168));
   float _450 = rsqrt(_449);
   float _451 = _166 * _450;
   float _452 = -0.0f - _451;
   float _453 = _448 * _450;
   float _454 = -0.0f - _453;
   float _455 = _168 * _450;
   float _456 = -0.0f - _455;
   float _457 = dot(float3(_452, _454, _456), float3(_161, _162, _163));
   float _458 = saturate(_457);
   float _461 = log2(_447);
   float _462 = log2(_458);
   float _463 = _461 * c_mat_power;
   float _464 = _462 * c_mat_power;
   float _465 = exp2(_463);
   float _466 = exp2(_464);
   float _469 = _465 * (c_light_col[0].x);
   float _470 = _465 * (c_light_col[0].y);
   float _471 = _465 * (c_light_col[0].z);
   float _472 = _466 * 2.0f;
   float _473 = _472 * (c_light_vec[5].x);
   float _474 = _473 * (c_light_col[1].x);
   float _475 = _473 * (c_light_col[1].y);
   float _476 = _473 * (c_light_col[1].z);
   float _477 = _469 * _292;
   float _478 = _470 * _293;
   float _479 = _471 * _294;
   float _480 = _474 * _377;
   float _481 = _475 * _377;
   float _482 = _476 * _377;
   float _483 = _480 + _477;
   float _484 = _481 + _478;
   float _485 = _482 + _479;
   float _492 = c_mat_specular.x * c_specular_scale.x;
   float _493 = _492 * _296;
   float _494 = _493 * _483;
   float _495 = c_mat_specular.y * c_specular_scale.y;
   float _496 = _495 * _298;
   float _497 = _496 * _484;
   float _498 = c_mat_specular.z * c_specular_scale.z;
   float _499 = _498 * _300;
   float _500 = _499 * _485;
   float _501 = _366 * _330;
   float _502 = _501 * _466;
   float _503 = _367 * _331;
   float _504 = _503 * _466;
   float _505 = _368 * _332;
   float _506 = _505 * _466;
   float _507 = _494 + _502;
   float _508 = _497 + _504;
   float _509 = _500 + _506;
   if (!_350)
   {
      float _511 = _280 * _262;
      float _512 = _280 * _263;
      float _513 = _280 * _264;
      float _514 = _428 + _511;
      float _515 = _429 + _512;
      float _516 = _430 + _513;
      float _517 = _280 * _265;
      float _518 = _507 + _517;
      float _519 = _508 + _517;
      float _520 = _509 + _517;
      _522 = _514;
      _523 = _515;
      _524 = _516;
      _525 = _518;
      _526 = _519;
      _527 = _520;
   }
   else
   {
      _522 = _428;
      _523 = _429;
      _524 = _430;
      _525 = _507;
      _526 = _508;
      _527 = _509;
   }
   int _528 = c_bool & 128;
   bool _529 = (_528 != 0);
   float _530 = select(_529, 0.0f, _525);
   float _531 = select(_529, 0.0f, _526);
   float _532 = select(_529, 0.0f, _527);
   float _533 = _522 * _108;
   float _534 = _523 * _109;
   float _535 = _524 * _110;
   float _536 = _530 * _117;
   float _537 = _531 * _118;
   float _538 = _532 * _119;
   int _539 = c_bool & 32768;
   bool _540 = (_539 == 0);
   if (!_540)
   {
      float _544 = (c_ps_user[11].x) * _108;
      float _545 = (c_ps_user[11].x) * _109;
      float _546 = (c_ps_user[11].x) * _110;
      float _548 = _161 + 1.0f;
      float _549 = _162 + 1.0f;
      float _550 = _163 + 1.0f;
      float _551 = _548 * 0.5f;
      float _552 = _549 * 0.5f;
      float _553 = _550 * 0.5f;
      float _554 = saturate(_551);
      float _555 = saturate(_552);
      float _556 = saturate(_553);
      float _557 = (c_ps_user[11].y) * _554;
      float _558 = _555 * (c_ps_user[11].y);
      float _559 = _556 * (c_ps_user[11].y);
      float _561 = (c_ps_user[11].z) * _117;
      float _562 = (c_ps_user[11].z) * _118;
      float _563 = (c_ps_user[11].z) * _119;
      float _565 = c_mat_power * 0.009999999776482582f;
      float _566 = _565 * (c_ps_user[11].w);
      float _569 = (c_ps_user[12].x) * _292;
      float _570 = (c_ps_user[12].x) * _293;
      float _571 = (c_ps_user[12].x) * _294;
      float _573 = (c_ps_user[12].y) * _296;
      float _574 = (c_ps_user[12].y) * _298;
      float _575 = (c_ps_user[12].y) * _300;
      float _577 = (c_ps_user[12].z) * _330;
      float _578 = (c_ps_user[12].z) * _331;
      float _579 = (c_ps_user[12].z) * _332;
      float _585 = -0.0f - _166;
      float _586 = -0.0f - _167;
      float _587 = -0.0f - _168;
      float _588 = dot(float3(_585, _586, _587), float3(_161, _162, _163));
      float _589 = 1.0f - _588;
      float _590 = _589 * _589;
      float _591 = _589 * 0.9722222089767456f;
      float _592 = _590 * _590;
      float _593 = _592 * _591;
      float _594 = _593 + 0.02777777425944805f;
      float _595 = _594 * (c_ps_user[13].z);
      float _597 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _598 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _599 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _602 = (c_ps_user[14].x) * _344;
      float _603 = (c_ps_user[14].x) * _346;
      float _604 = (c_ps_user[14].x) * _348;
      float _606 = (c_ps_user[14].y) * _383;
      float _607 = (c_ps_user[14].y) * _384;
      float _608 = (c_ps_user[14].y) * _385;
      float _610 = (c_ps_user[14].z) * _390;
      float _611 = (c_ps_user[14].z) * _391;
      float _612 = (c_ps_user[14].z) * _392;
      float _614 = (c_ps_user[14].w) * _363;
      float _615 = (c_ps_user[14].w) * _364;
      float _616 = (c_ps_user[14].w) * _365;
      float _619 = (c_ps_user[15].x) * _469;
      float _620 = (c_ps_user[15].x) * _470;
      float _621 = (c_ps_user[15].x) * _471;
      float _623 = _492 * _474;
      float _624 = _623 * (c_ps_user[15].y);
      float _625 = _495 * _475;
      float _626 = _625 * (c_ps_user[15].y);
      float _627 = _498 * _476;
      float _628 = _627 * (c_ps_user[15].y);
      float _630 = (c_ps_user[15].z) * _366;
      float _631 = (c_ps_user[15].z) * _367;
      float _632 = (c_ps_user[15].z) * _368;
      float _633 = _624 + _630;
      float _634 = _633 * _466;
      float _635 = _606 + (c_ps_user[13].x);
      float _636 = _635 + _610;
      float _637 = c_diffuse_scale.x * _636;
      float _638 = _637 * c_mat_diffuse.x;
      float _639 = (c_ps_user[13].y) + _561;
      float _640 = _639 + _619;
      float _641 = c_mat_specular.x * _640;
      float _642 = _641 * c_specular_scale.x;
      float _643 = _557 + _544;
      float _644 = _643 + _566;
      float _645 = _644 + (c_ps_user[12].w);
      float _646 = _645 + _569;
      float _647 = _646 + _573;
      float _648 = _647 + _577;
      float _649 = _648 + _597;
      float _650 = _649 + _602;
      float _651 = _650 + _614;
      float _652 = _651 + _595;
      float _653 = _652 + _634;
      float _654 = _653 + _638;
      float _655 = _654 + _642;
      float _656 = _626 + _631;
      float _657 = _656 * _466;
      float _658 = _607 + (c_ps_user[13].x);
      float _659 = _658 + _611;
      float _660 = c_diffuse_scale.y * _659;
      float _661 = _660 * c_mat_diffuse.y;
      float _662 = (c_ps_user[13].y) + _562;
      float _663 = _662 + _620;
      float _664 = c_mat_specular.y * _663;
      float _665 = _664 * c_specular_scale.y;
      float _666 = _558 + _545;
      float _667 = _666 + _566;
      float _668 = _667 + (c_ps_user[12].w);
      float _669 = _668 + _570;
      float _670 = _669 + _574;
      float _671 = _670 + _578;
      float _672 = _671 + _598;
      float _673 = _672 + _603;
      float _674 = _673 + _615;
      float _675 = _674 + _595;
      float _676 = _675 + _657;
      float _677 = _676 + _661;
      float _678 = _677 + _665;
      float _679 = _628 + _632;
      float _680 = _679 * _466;
      float _681 = _608 + (c_ps_user[13].x);
      float _682 = _681 + _612;
      float _683 = c_diffuse_scale.z * _682;
      float _684 = _683 * c_mat_diffuse.z;
      float _685 = (c_ps_user[13].y) + _563;
      float _686 = _685 + _621;
      float _687 = c_mat_specular.z * _686;
      float _688 = _687 * c_specular_scale.z;
      float _689 = _566 + _546;
      float _690 = _689 + _559;
      float _691 = _690 + (c_ps_user[12].w);
      float _692 = _691 + _571;
      float _693 = _692 + _575;
      float _694 = _693 + _579;
      float _695 = _694 + _599;
      float _696 = _695 + _604;
      float _697 = _696 + _616;
      float _698 = _697 + _595;
      float _699 = _698 + _680;
      float _700 = _699 + _684;
      float _701 = _700 + _688;
      _703 = _655;
      _704 = _678;
      _705 = _701;
      _706 = 0.0f;
      _707 = 0.0f;
      _708 = 0.0f;
   }
   else
   {
      _703 = _533;
      _704 = _534;
      _705 = _535;
      _706 = _536;
      _707 = _537;
      _708 = _538;
   }
   float _709 = _706 + _703;
   float _710 = _707 + _704;
   float _711 = _708 + _705;
   float _716 = c_mat_opacity * _56.w;
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
      if (_540)
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
