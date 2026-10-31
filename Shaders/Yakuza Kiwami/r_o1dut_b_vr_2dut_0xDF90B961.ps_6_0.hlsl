// Yakuza Kiwami material r_o1dut_b(vr)2dut (0xDF90B961, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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
   float _72 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _73 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _74 = s5_t.Sample(s5_s, float2(_72, _73));
   float4 _77 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _82 = _77.x * _77.x;
   float _83 = _77.y * _77.y;
   float _84 = _77.z * _77.z;
   float _88 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _89 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _90 = s4_t.Sample(s4_s, float2(_88, _89));
   float _95 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _96 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _97 = s6_t.Sample(s6_s, float2(_95, _96));
   float _100 = _77.w * COLOR_1.w;
   float _101 = _90.y - _67.y;
   float _102 = _101 * _100;
   float _103 = _102 + _67.y;
   float _104 = _82 - _59;
   float _105 = _83 - _60;
   float _106 = _84 - _61;
   float _107 = _104 * _100;
   float _108 = _105 * _100;
   float _109 = _106 * _100;
   float _110 = _107 + _59;
   float _111 = _108 + _60;
   float _112 = _109 + _61;
   float _113 = _97.x - _74.x;
   float _114 = _97.y - _74.y;
   float _115 = _113 * _100;
   float _116 = _114 * _100;
   float _117 = _115 + _74.x;
   float _118 = _116 + _74.y;
   float _119 = _117 * 2.0f;
   float _120 = _118 * 2.0f;
   float _121 = _119 + -1.0f;
   float _122 = _120 + -1.0f;
   float _123 = dot(float2(_121, _122), float2(_121, _122));
   float _124 = 1.0f - _123;
   float _125 = saturate(_124);
   float _126 = sqrt(_125);
   float _127 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _128 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _129 = _127 - _128;
   float _130 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _131 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _132 = _130 - _131;
   float _133 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _134 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _135 = _133 - _134;
   float _136 = _121 * TEXCOORD_6.x;
   float _137 = _121 * TEXCOORD_6.y;
   float _138 = _121 * TEXCOORD_6.z;
   float _139 = _122 * TEXCOORD_6.w;
   float _140 = _139 * _129;
   float _141 = _139 * _132;
   float _142 = _139 * _135;
   float _143 = _126 * TEXCOORD_2.x;
   float _144 = _126 * TEXCOORD_2.y;
   float _145 = _126 * TEXCOORD_2.z;
   float _146 = _136 + _143;
   float _147 = _146 + _140;
   float _148 = _137 + _144;
   float _149 = _148 + _141;
   float _150 = _138 + _145;
   float _151 = _150 + _142;
   float _152 = dot(float3(_147, _149, _151), float3(_147, _149, _151));
   float _153 = rsqrt(_152);
   float _154 = _147 * _153;
   float _155 = _149 * _153;
   float _156 = _151 * _153;
   float _157 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _158 = rsqrt(_157);
   float _159 = _158 * TEXCOORD_3.x;
   float _160 = _158 * TEXCOORD_3.y;
   float _161 = _158 * TEXCOORD_3.z;
   float _166 = -0.0f - (c_light_vec[0].x);
   float _167 = -0.0f - (c_light_vec[0].y);
   float _168 = -0.0f - (c_light_vec[0].z);
   float _169 = dot(float3(_154, _155, _156), float3(_166, _167, _168));
   float _173 = c_rt_size.z * SV_Position.x;
   float _174 = c_rt_size.w * SV_Position.y;
   int _177 = c_bool & 8;
   bool _178 = (_177 == 0);
   float _185;
   float _186;
   float _187;
   float _199;
   float _200;
   float _236;
   float _237;
   float _238;
   float _239;
   float _253;
   float _254;
   float _289;
   float _290;
   float _291;
   float _325;
   float _326;
   float _327;
   float _328;
   float _329;
   float _330;
   float _484;
   float _485;
   float _486;
   float _487;
   float _488;
   float _489;
   float _661;
   float _662;
   float _663;
   float _664;
   float _665;
   float _666;
   float _742;
   float _743;
   float _744;
   float _760;
   float _761;
   float _762;
   if (!_178)
   {
      float4 _180 = s13_t.Sample(s13_s, float2(_173, _174));
      _185 = _180.x;
      _186 = _180.y;
      _187 = _180.w;
   }
   else
   {
      _185 = 1.0f;
      _186 = 1.0f;
      _187 = 1.0f;
   }
   if (!_178)
   {
      float _192 = (c_sdw_param[0].x) * _185;
      float _193 = (c_sdw_param[0].z) * _186;
      float _196 = _192 + (c_sdw_param[0].y);
      float _197 = _193 + (c_sdw_param[0].w);
      _199 = _196;
      _200 = _197;
   }
   else
   {
      _199 = _185;
      _200 = _186;
   }
   float _204 = c_rt_size.z * SV_Position.x;
   float _205 = c_rt_size.w * SV_Position.y;
   float4 _206 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_204, _205)));
   int _213 = c_bool & 16384;
   bool _214 = (_213 == 0);
   if (!_214)
   {
      float _216 = _206.x * 2.0f;
      float _217 = _206.y * 2.0f;
      float _218 = _206.z * 2.0f;
      float _219 = _206.w * 2.0f;
      _236 = _216;
      _237 = _217;
      _238 = _218;
      _239 = _219;
   }
   else
   {
      int _221 = c_bool & 8192;
      bool _222 = (_221 == 0);
      if (!_222)
      {
         float _224 = _206.x * 2.0f;
         float _225 = _206.y * 2.0f;
         float _226 = _206.z * 2.0f;
         float _227 = _206.w * 2.0f;
         _236 = _224;
         _237 = _225;
         _238 = _226;
         _239 = _227;
      }
      else
      {
         float _231 = _206.x / c_tone_control.z;
         float _232 = _206.y / c_tone_control.z;
         float _233 = _206.z / c_tone_control.z;
         float _234 = _206.w / c_tone_control.z;
         _236 = _231;
         _237 = _232;
         _238 = _233;
         _239 = _234;
      }
   }
   int _240 = c_bool & 8;
   bool _241 = (_240 == 0);
   if (!_241)
   {
      float _246 = (c_sdw_param[0].x) * _199;
      float _247 = (c_sdw_param[0].z) * _200;
      float _250 = _246 + (c_sdw_param[0].y);
      float _251 = _247 + (c_sdw_param[0].w);
      _253 = _250;
      _254 = _251;
   }
   else
   {
      _253 = _199;
      _254 = _200;
   }
   float _255 = _254 * _253;
   float _260 = 1.0f - (c_light_col[5].x);
   float _261 = 1.0f - (c_light_col[5].y);
   float _262 = 1.0f - (c_light_col[5].z);
   float _263 = _260 * _255;
   float _264 = _261 * _255;
   float _265 = _262 * _255;
   float _266 = _263 + (c_light_col[5].x);
   float _267 = _264 + (c_light_col[5].y);
   float _268 = _265 + (c_light_col[5].z);
   float _269 = _187 * COLOR.x;
   float _270 = _187 * COLOR.y;
   float _271 = _187 * COLOR.z;
   if (!_241)
   {
      float _275 = (c_sdw_param[2].x) * _266;
      float _276 = (c_sdw_param[2].x) * _267;
      float _277 = (c_sdw_param[2].x) * _268;
      float _279 = _275 + (c_sdw_param[2].y);
      float _280 = _276 + (c_sdw_param[2].y);
      float _281 = _277 + (c_sdw_param[2].y);
      float _284 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _285 = _279 * _284;
      float _286 = _280 * _284;
      float _287 = _281 * _284;
      _289 = _285;
      _290 = _286;
      _291 = _287;
   }
   else
   {
      _289 = _266;
      _290 = _267;
      _291 = _268;
   }
   float _292 = _289 * _187;
   float _293 = _290 * _187;
   float _294 = _291 * _187;
   float _305 = c_mat_emissive.x * _110;
   float _306 = _305 * c_emissive_scale;
   float _307 = c_mat_emissive.y * _111;
   float _308 = _307 * c_emissive_scale;
   float _309 = c_mat_emissive.z * _112;
   float _310 = _309 * c_emissive_scale;
   int _311 = c_bool & 2048;
   bool _312 = (_311 == 0);
   if (_312)
   {
      float _314 = _254 * _236;
      float _315 = _254 * _237;
      float _316 = _254 * _238;
      float _317 = _314 + TEXCOORD_4.x;
      float _318 = _315 + TEXCOORD_4.y;
      float _319 = _316 + TEXCOORD_4.z;
      float _320 = _254 * _239;
      float _321 = _320 + TEXCOORD_5.x;
      float _322 = _320 + TEXCOORD_5.y;
      float _323 = _320 + TEXCOORD_5.z;
      _325 = _317;
      _326 = _318;
      _327 = _319;
      _328 = _321;
      _329 = _322;
      _330 = _323;
   }
   else
   {
      _325 = TEXCOORD_4.x;
      _326 = TEXCOORD_4.y;
      _327 = TEXCOORD_4.z;
      _328 = TEXCOORD_5.x;
      _329 = TEXCOORD_5.y;
      _330 = TEXCOORD_5.z;
   }
   float _335 = -0.0f - (c_light_vec[1].x);
   float _336 = -0.0f - (c_light_vec[1].y);
   float _337 = -0.0f - (c_light_vec[1].z);
   float _338 = dot(float3(_154, _155, _156), float3(_335, _336, _337));
   float _339 = saturate(_338);
   float _344 = saturate(_169);
   float _345 = (c_light_col[0].x) * _344;
   float _346 = (c_light_col[0].y) * _344;
   float _347 = (c_light_col[0].z) * _344;
   float _352 = (c_light_col[1].x) * _339;
   float _353 = (c_light_col[1].y) * _339;
   float _354 = (c_light_col[1].z) * _339;
   float _363 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _364 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _365 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _366 = _345 * _266;
   float _367 = _346 * _267;
   float _368 = _347 * _268;
   float _369 = _352 * _254;
   float _370 = _353 * _254;
   float _371 = _354 * _254;
   float _372 = _369 + _366;
   float _373 = _370 + _367;
   float _374 = _371 + _368;
   float _375 = _363 * _372;
   float _376 = _364 * _373;
   float _377 = _365 * _374;
   float _378 = _306 + c_scene_ambient.x;
   float _379 = _378 + _375;
   float _380 = _308 + c_scene_ambient.y;
   float _381 = _380 + _376;
   float _382 = _310 + c_scene_ambient.z;
   float _383 = _382 + _377;
   float _384 = _379 * _269;
   float _385 = _381 * _270;
   float _386 = _383 * _271;
   float _387 = _325 * _292;
   float _388 = _326 * _293;
   float _389 = _327 * _294;
   float _390 = _384 + _387;
   float _391 = _385 + _388;
   float _392 = _386 + _389;
   float _397 = (c_light_vec[0].x) + _159;
   float _398 = (c_light_vec[0].y) + _160;
   float _399 = (c_light_vec[0].z) + _161;
   float _400 = dot(float3(_397, _398, _399), float3(_397, _398, _399));
   float _401 = rsqrt(_400);
   float _402 = _397 * _401;
   float _403 = -0.0f - _402;
   float _404 = _398 * _401;
   float _405 = -0.0f - _404;
   float _406 = _399 * _401;
   float _407 = -0.0f - _406;
   float _408 = dot(float3(_403, _405, _407), float3(_154, _155, _156));
   float _409 = saturate(_408);
   float _410 = _160 + -0.30000001192092896f;
   float _411 = dot(float3(_159, _410, _161), float3(_159, _410, _161));
   float _412 = rsqrt(_411);
   float _413 = _159 * _412;
   float _414 = -0.0f - _413;
   float _415 = _410 * _412;
   float _416 = -0.0f - _415;
   float _417 = _161 * _412;
   float _418 = -0.0f - _417;
   float _419 = dot(float3(_414, _416, _418), float3(_154, _155, _156));
   float _420 = saturate(_419);
   float _423 = log2(_409);
   float _424 = log2(_420);
   float _425 = _423 * c_mat_power;
   float _426 = _424 * c_mat_power;
   float _427 = exp2(_425);
   float _428 = exp2(_426);
   float _431 = _427 * (c_light_col[0].x);
   float _432 = _427 * (c_light_col[0].y);
   float _433 = _427 * (c_light_col[0].z);
   float _434 = _428 * 2.0f;
   float _435 = _434 * (c_light_vec[5].x);
   float _436 = _435 * (c_light_col[1].x);
   float _437 = _435 * (c_light_col[1].y);
   float _438 = _435 * (c_light_col[1].z);
   float _439 = _431 * _266;
   float _440 = _432 * _267;
   float _441 = _433 * _268;
   float _442 = _436 * _339;
   float _443 = _437 * _339;
   float _444 = _438 * _339;
   float _445 = _442 + _439;
   float _446 = _443 + _440;
   float _447 = _444 + _441;
   float _454 = c_mat_specular.x * c_specular_scale.x;
   float _455 = _454 * _269;
   float _456 = _455 * _445;
   float _457 = c_mat_specular.y * c_specular_scale.y;
   float _458 = _457 * _270;
   float _459 = _458 * _446;
   float _460 = c_mat_specular.z * c_specular_scale.z;
   float _461 = _460 * _271;
   float _462 = _461 * _447;
   float _463 = _328 * _292;
   float _464 = _463 * _428;
   float _465 = _329 * _293;
   float _466 = _465 * _428;
   float _467 = _330 * _294;
   float _468 = _467 * _428;
   float _469 = _456 + _464;
   float _470 = _459 + _466;
   float _471 = _462 + _468;
   if (!_312)
   {
      float _473 = _254 * _236;
      float _474 = _254 * _237;
      float _475 = _254 * _238;
      float _476 = _390 + _473;
      float _477 = _391 + _474;
      float _478 = _392 + _475;
      float _479 = _254 * _239;
      float _480 = _469 + _479;
      float _481 = _470 + _479;
      float _482 = _471 + _479;
      _484 = _476;
      _485 = _477;
      _486 = _478;
      _487 = _480;
      _488 = _481;
      _489 = _482;
   }
   else
   {
      _484 = _390;
      _485 = _391;
      _486 = _392;
      _487 = _469;
      _488 = _470;
      _489 = _471;
   }
   int _490 = c_bool & 128;
   bool _491 = (_490 != 0);
   float _492 = select(_491, 0.0f, _487);
   float _493 = select(_491, 0.0f, _488);
   float _494 = select(_491, 0.0f, _489);
   float _495 = _484 * _110;
   float _496 = _485 * _111;
   float _497 = _486 * _112;
   float _498 = _492 * _103;
   float _499 = _493 * _103;
   float _500 = _494 * _103;
   int _501 = c_bool & 32768;
   bool _502 = (_501 == 0);
   if (!_502)
   {
      float _506 = (c_ps_user[11].x) * _110;
      float _507 = (c_ps_user[11].x) * _111;
      float _508 = (c_ps_user[11].x) * _112;
      float _510 = _154 + 1.0f;
      float _511 = _155 + 1.0f;
      float _512 = _156 + 1.0f;
      float _513 = _510 * 0.5f;
      float _514 = _511 * 0.5f;
      float _515 = _512 * 0.5f;
      float _516 = saturate(_513);
      float _517 = saturate(_514);
      float _518 = saturate(_515);
      float _519 = (c_ps_user[11].y) * _516;
      float _520 = _517 * (c_ps_user[11].y);
      float _521 = _518 * (c_ps_user[11].y);
      float _523 = (c_ps_user[11].z) * _103;
      float _525 = c_mat_power * 0.009999999776482582f;
      float _526 = _525 * (c_ps_user[11].w);
      float _529 = (c_ps_user[12].x) * _266;
      float _530 = (c_ps_user[12].x) * _267;
      float _531 = (c_ps_user[12].x) * _268;
      float _533 = (c_ps_user[12].y) * _269;
      float _534 = (c_ps_user[12].y) * _270;
      float _535 = (c_ps_user[12].y) * _271;
      float _537 = (c_ps_user[12].z) * _292;
      float _538 = (c_ps_user[12].z) * _293;
      float _539 = (c_ps_user[12].z) * _294;
      float _545 = -0.0f - _159;
      float _546 = -0.0f - _160;
      float _547 = -0.0f - _161;
      float _548 = dot(float3(_545, _546, _547), float3(_154, _155, _156));
      float _549 = 1.0f - _548;
      float _550 = _549 * _549;
      float _551 = _549 * 0.9722222089767456f;
      float _552 = _550 * _550;
      float _553 = _552 * _551;
      float _554 = _553 + 0.02777777425944805f;
      float _555 = _554 * (c_ps_user[13].z);
      float _557 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _558 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _559 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _562 = (c_ps_user[14].x) * _306;
      float _563 = (c_ps_user[14].x) * _308;
      float _564 = (c_ps_user[14].x) * _310;
      float _566 = (c_ps_user[14].y) * _345;
      float _567 = (c_ps_user[14].y) * _346;
      float _568 = (c_ps_user[14].y) * _347;
      float _570 = (c_ps_user[14].z) * _352;
      float _571 = (c_ps_user[14].z) * _353;
      float _572 = (c_ps_user[14].z) * _354;
      float _574 = (c_ps_user[14].w) * _325;
      float _575 = (c_ps_user[14].w) * _326;
      float _576 = (c_ps_user[14].w) * _327;
      float _579 = (c_ps_user[15].x) * _431;
      float _580 = (c_ps_user[15].x) * _432;
      float _581 = (c_ps_user[15].x) * _433;
      float _583 = _454 * _436;
      float _584 = _583 * (c_ps_user[15].y);
      float _585 = _457 * _437;
      float _586 = _585 * (c_ps_user[15].y);
      float _587 = _460 * _438;
      float _588 = _587 * (c_ps_user[15].y);
      float _590 = (c_ps_user[15].z) * _328;
      float _591 = (c_ps_user[15].z) * _329;
      float _592 = (c_ps_user[15].z) * _330;
      float _593 = _584 + _590;
      float _594 = _593 * _428;
      float _595 = _566 + (c_ps_user[13].x);
      float _596 = _595 + _570;
      float _597 = c_diffuse_scale.x * _596;
      float _598 = _597 * c_mat_diffuse.x;
      float _599 = (c_ps_user[13].y) + _523;
      float _600 = _599 + _579;
      float _601 = c_mat_specular.x * _600;
      float _602 = _601 * c_specular_scale.x;
      float _603 = _519 + _506;
      float _604 = _603 + _526;
      float _605 = _604 + (c_ps_user[12].w);
      float _606 = _605 + _529;
      float _607 = _606 + _533;
      float _608 = _607 + _537;
      float _609 = _608 + _557;
      float _610 = _609 + _562;
      float _611 = _610 + _574;
      float _612 = _611 + _555;
      float _613 = _612 + _594;
      float _614 = _613 + _598;
      float _615 = _614 + _602;
      float _616 = _586 + _591;
      float _617 = _616 * _428;
      float _618 = _567 + (c_ps_user[13].x);
      float _619 = _618 + _571;
      float _620 = c_diffuse_scale.y * _619;
      float _621 = _620 * c_mat_diffuse.y;
      float _622 = _599 + _580;
      float _623 = c_mat_specular.y * _622;
      float _624 = _623 * c_specular_scale.y;
      float _625 = _520 + _507;
      float _626 = _625 + _526;
      float _627 = _626 + (c_ps_user[12].w);
      float _628 = _627 + _530;
      float _629 = _628 + _534;
      float _630 = _629 + _538;
      float _631 = _630 + _558;
      float _632 = _631 + _563;
      float _633 = _632 + _575;
      float _634 = _633 + _555;
      float _635 = _634 + _617;
      float _636 = _635 + _621;
      float _637 = _636 + _624;
      float _638 = _588 + _592;
      float _639 = _638 * _428;
      float _640 = _568 + (c_ps_user[13].x);
      float _641 = _640 + _572;
      float _642 = c_diffuse_scale.z * _641;
      float _643 = _642 * c_mat_diffuse.z;
      float _644 = _599 + _581;
      float _645 = c_mat_specular.z * _644;
      float _646 = _645 * c_specular_scale.z;
      float _647 = _526 + _508;
      float _648 = _647 + _521;
      float _649 = _648 + (c_ps_user[12].w);
      float _650 = _649 + _531;
      float _651 = _650 + _535;
      float _652 = _651 + _539;
      float _653 = _652 + _559;
      float _654 = _653 + _564;
      float _655 = _654 + _576;
      float _656 = _655 + _555;
      float _657 = _656 + _639;
      float _658 = _657 + _643;
      float _659 = _658 + _646;
      _661 = _615;
      _662 = _637;
      _663 = _659;
      _664 = 0.0f;
      _665 = 0.0f;
      _666 = 0.0f;
   }
   else
   {
      _661 = _495;
      _662 = _496;
      _663 = _497;
      _664 = _498;
      _665 = _499;
      _666 = _500;
   }
   float _667 = _664 + _661;
   float _668 = _665 + _662;
   float _669 = _666 + _663;
   float _674 = c_mat_opacity * _54.w;
   float _675 = _674 * c_global_opacity;
   float _676 = _675 * c_opacity_scale;
   int _677 = c_bool & 16;
   bool _678 = (_677 == 0);
   if (!_678)
   {
      float _682 = c_tone_control.x * _667;
      float _683 = c_tone_control.x * _668;
      float _684 = c_tone_control.x * _669;
      float _686 = log2(_682);
      float _687 = log2(_683);
      float _688 = log2(_684);
      float _689 = _686 * c_tone_control.y;
      float _690 = _687 * c_tone_control.y;
      float _691 = _688 * c_tone_control.y;
      float _692 = exp2(_689);
      float _693 = exp2(_690);
      float _694 = exp2(_691);
      _742 = _692;
      _743 = _693;
      _744 = _694;
   }
   else
   {
      if (_502)
      {
         float _699 = c_tone_control.x * _667;
         float _700 = c_tone_control.x * _668;
         float _701 = c_tone_control.x * _669;
         float _702 = _699 * 2.0f;
         float _703 = _700 * 2.0f;
         float _704 = _701 * 2.0f;
         float _705 = _699 * 0.30000001192092896f;
         float _706 = _700 * 0.30000001192092896f;
         float _707 = _701 * 0.30000001192092896f;
         float _708 = _705 + 0.05000000074505806f;
         float _709 = _706 + 0.05000000074505806f;
         float _710 = _707 + 0.05000000074505806f;
         float _711 = _708 * _702;
         float _712 = _709 * _703;
         float _713 = _710 * _704;
         float _714 = _711 + 0.004000000189989805f;
         float _715 = _712 + 0.004000000189989805f;
         float _716 = _713 + 0.004000000189989805f;
         float _717 = _705 + 0.5f;
         float _718 = _706 + 0.5f;
         float _719 = _707 + 0.5f;
         float _720 = _717 * _702;
         float _721 = _718 * _703;
         float _722 = _719 * _704;
         float _723 = _720 + 0.06000000238418579f;
         float _724 = _721 + 0.06000000238418579f;
         float _725 = _722 + 0.06000000238418579f;
         float _726 = _714 / _723;
         float _727 = _715 / _724;
         float _728 = _716 / _725;
         float _729 = _726 + -0.06666666269302368f;
         float _730 = _727 + -0.06666666269302368f;
         float _731 = _728 + -0.06666666269302368f;
         float _732 = _729 * 1.3790643215179443f;
         float _733 = _730 * 1.3790643215179443f;
         float _734 = _731 * 1.3790643215179443f;
         float _735 = YKR_MaterialCurve(_732, _699);
         float _736 = YKR_MaterialCurve(_733, _700);
         float _737 = YKR_MaterialCurve(_734, _701);
         float _738 = sqrt(_735);
         float _739 = sqrt(_736);
         float _740 = sqrt(_737);
         _742 = _738;
         _743 = _739;
         _744 = _740;
      }
      else
      {
         _742 = _667;
         _743 = _668;
         _744 = _669;
      }
   }
   int _745 = c_bool & 4;
   bool _746 = (_745 == 0);
   if (_746)
   {
      float _748 = saturate(COLOR_1.x);
      float _753 = (c_fog_param[0].x) - _742;
      float _754 = (c_fog_param[0].y) - _743;
      float _755 = (c_fog_param[0].z) - _744;
      float _756 = _753 * _748;
      float _757 = _754 * _748;
      float _758 = _755 * _748;
      _760 = _756;
      _761 = _757;
      _762 = _758;
   }
   else
   {
      _760 = COLOR_1.x;
      _761 = COLOR_1.y;
      _762 = COLOR_1.z;
   }
   float _763 = _760 + _742;
   float _764 = _761 + _743;
   float _765 = _762 + _744;
   float _771 = (c_ps_col_scl[0].x) * _763;
   float _772 = (c_ps_col_scl[0].y) * _764;
   float _773 = (c_ps_col_scl[0].z) * _765;
   float _774 = (c_ps_col_scl[0].w) * _676;
   SV_Target.x = _771;
   SV_Target.y = _772;
   SV_Target.z = _773;
   SV_Target.w = _774;
   int _777 = c_bool & 8;
   bool _778 = (_777 != 0);
   float _779 = select(_778, _763, _771);
   float _780 = select(_778, _764, _772);
   float _781 = select(_778, _765, _773);
   float _782 = select(_778, _676, _774);
   SV_Target.x = _779;
   SV_Target.y = _780;
   SV_Target.z = _781;
   SV_Target.w = _782;
   return SV_Target;
}
