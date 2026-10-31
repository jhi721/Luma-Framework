// Yakuza Kiwami material r_o1dut_m(vr)2dut (0x296EE1E9, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _101 = 1.0f - _90.y;
   float _102 = _101 * _100;
   float _103 = 1.0f - _102;
   float _104 = _103 * _67.y;
   float _105 = 1.0f - _82;
   float _106 = 1.0f - _83;
   float _107 = 1.0f - _84;
   float _108 = _105 * _100;
   float _109 = _106 * _100;
   float _110 = _107 * _100;
   float _111 = 1.0f - _108;
   float _112 = 1.0f - _109;
   float _113 = 1.0f - _110;
   float _114 = _59 * _111;
   float _115 = _60 * _112;
   float _116 = _61 * _113;
   float _117 = _97.x - _74.x;
   float _118 = _97.y - _74.y;
   float _119 = _117 * _100;
   float _120 = _118 * _100;
   float _121 = _119 + _74.x;
   float _122 = _120 + _74.y;
   float _123 = _121 * 2.0f;
   float _124 = _122 * 2.0f;
   float _125 = _123 + -1.0f;
   float _126 = _124 + -1.0f;
   float _127 = dot(float2(_125, _126), float2(_125, _126));
   float _128 = 1.0f - _127;
   float _129 = saturate(_128);
   float _130 = sqrt(_129);
   float _131 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _132 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _133 = _131 - _132;
   float _134 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _135 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _136 = _134 - _135;
   float _137 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _138 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _139 = _137 - _138;
   float _140 = _125 * TEXCOORD_6.x;
   float _141 = _125 * TEXCOORD_6.y;
   float _142 = _125 * TEXCOORD_6.z;
   float _143 = _126 * TEXCOORD_6.w;
   float _144 = _143 * _133;
   float _145 = _143 * _136;
   float _146 = _143 * _139;
   float _147 = _130 * TEXCOORD_2.x;
   float _148 = _130 * TEXCOORD_2.y;
   float _149 = _130 * TEXCOORD_2.z;
   float _150 = _140 + _147;
   float _151 = _150 + _144;
   float _152 = _141 + _148;
   float _153 = _152 + _145;
   float _154 = _142 + _149;
   float _155 = _154 + _146;
   float _156 = dot(float3(_151, _153, _155), float3(_151, _153, _155));
   float _157 = rsqrt(_156);
   float _158 = _151 * _157;
   float _159 = _153 * _157;
   float _160 = _155 * _157;
   float _161 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _162 = rsqrt(_161);
   float _163 = _162 * TEXCOORD_3.x;
   float _164 = _162 * TEXCOORD_3.y;
   float _165 = _162 * TEXCOORD_3.z;
   float _170 = -0.0f - (c_light_vec[0].x);
   float _171 = -0.0f - (c_light_vec[0].y);
   float _172 = -0.0f - (c_light_vec[0].z);
   float _173 = dot(float3(_158, _159, _160), float3(_170, _171, _172));
   float _177 = c_rt_size.z * SV_Position.x;
   float _178 = c_rt_size.w * SV_Position.y;
   int _181 = c_bool & 8;
   bool _182 = (_181 == 0);
   float _189;
   float _190;
   float _191;
   float _203;
   float _204;
   float _240;
   float _241;
   float _242;
   float _243;
   float _257;
   float _258;
   float _293;
   float _294;
   float _295;
   float _329;
   float _330;
   float _331;
   float _332;
   float _333;
   float _334;
   float _488;
   float _489;
   float _490;
   float _491;
   float _492;
   float _493;
   float _665;
   float _666;
   float _667;
   float _668;
   float _669;
   float _670;
   float _746;
   float _747;
   float _748;
   float _764;
   float _765;
   float _766;
   if (!_182)
   {
      float4 _184 = s13_t.Sample(s13_s, float2(_177, _178));
      _189 = _184.x;
      _190 = _184.y;
      _191 = _184.w;
   }
   else
   {
      _189 = 1.0f;
      _190 = 1.0f;
      _191 = 1.0f;
   }
   if (!_182)
   {
      float _196 = (c_sdw_param[0].x) * _189;
      float _197 = (c_sdw_param[0].z) * _190;
      float _200 = _196 + (c_sdw_param[0].y);
      float _201 = _197 + (c_sdw_param[0].w);
      _203 = _200;
      _204 = _201;
   }
   else
   {
      _203 = _189;
      _204 = _190;
   }
   float _208 = c_rt_size.z * SV_Position.x;
   float _209 = c_rt_size.w * SV_Position.y;
   float4 _210 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_208, _209)));
   int _217 = c_bool & 16384;
   bool _218 = (_217 == 0);
   if (!_218)
   {
      float _220 = _210.x * 2.0f;
      float _221 = _210.y * 2.0f;
      float _222 = _210.z * 2.0f;
      float _223 = _210.w * 2.0f;
      _240 = _220;
      _241 = _221;
      _242 = _222;
      _243 = _223;
   }
   else
   {
      int _225 = c_bool & 8192;
      bool _226 = (_225 == 0);
      if (!_226)
      {
         float _228 = _210.x * 2.0f;
         float _229 = _210.y * 2.0f;
         float _230 = _210.z * 2.0f;
         float _231 = _210.w * 2.0f;
         _240 = _228;
         _241 = _229;
         _242 = _230;
         _243 = _231;
      }
      else
      {
         float _235 = _210.x / c_tone_control.z;
         float _236 = _210.y / c_tone_control.z;
         float _237 = _210.z / c_tone_control.z;
         float _238 = _210.w / c_tone_control.z;
         _240 = _235;
         _241 = _236;
         _242 = _237;
         _243 = _238;
      }
   }
   int _244 = c_bool & 8;
   bool _245 = (_244 == 0);
   if (!_245)
   {
      float _250 = (c_sdw_param[0].x) * _203;
      float _251 = (c_sdw_param[0].z) * _204;
      float _254 = _250 + (c_sdw_param[0].y);
      float _255 = _251 + (c_sdw_param[0].w);
      _257 = _254;
      _258 = _255;
   }
   else
   {
      _257 = _203;
      _258 = _204;
   }
   float _259 = _258 * _257;
   float _264 = 1.0f - (c_light_col[5].x);
   float _265 = 1.0f - (c_light_col[5].y);
   float _266 = 1.0f - (c_light_col[5].z);
   float _267 = _264 * _259;
   float _268 = _265 * _259;
   float _269 = _266 * _259;
   float _270 = _267 + (c_light_col[5].x);
   float _271 = _268 + (c_light_col[5].y);
   float _272 = _269 + (c_light_col[5].z);
   float _273 = _191 * COLOR.x;
   float _274 = _191 * COLOR.y;
   float _275 = _191 * COLOR.z;
   if (!_245)
   {
      float _279 = (c_sdw_param[2].x) * _270;
      float _280 = (c_sdw_param[2].x) * _271;
      float _281 = (c_sdw_param[2].x) * _272;
      float _283 = _279 + (c_sdw_param[2].y);
      float _284 = _280 + (c_sdw_param[2].y);
      float _285 = _281 + (c_sdw_param[2].y);
      float _288 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _289 = _283 * _288;
      float _290 = _284 * _288;
      float _291 = _285 * _288;
      _293 = _289;
      _294 = _290;
      _295 = _291;
   }
   else
   {
      _293 = _270;
      _294 = _271;
      _295 = _272;
   }
   float _296 = _293 * _191;
   float _297 = _294 * _191;
   float _298 = _295 * _191;
   float _309 = c_mat_emissive.x * _114;
   float _310 = _309 * c_emissive_scale;
   float _311 = c_mat_emissive.y * _115;
   float _312 = _311 * c_emissive_scale;
   float _313 = c_mat_emissive.z * _116;
   float _314 = _313 * c_emissive_scale;
   int _315 = c_bool & 2048;
   bool _316 = (_315 == 0);
   if (_316)
   {
      float _318 = _258 * _240;
      float _319 = _258 * _241;
      float _320 = _258 * _242;
      float _321 = _318 + TEXCOORD_4.x;
      float _322 = _319 + TEXCOORD_4.y;
      float _323 = _320 + TEXCOORD_4.z;
      float _324 = _258 * _243;
      float _325 = _324 + TEXCOORD_5.x;
      float _326 = _324 + TEXCOORD_5.y;
      float _327 = _324 + TEXCOORD_5.z;
      _329 = _321;
      _330 = _322;
      _331 = _323;
      _332 = _325;
      _333 = _326;
      _334 = _327;
   }
   else
   {
      _329 = TEXCOORD_4.x;
      _330 = TEXCOORD_4.y;
      _331 = TEXCOORD_4.z;
      _332 = TEXCOORD_5.x;
      _333 = TEXCOORD_5.y;
      _334 = TEXCOORD_5.z;
   }
   float _339 = -0.0f - (c_light_vec[1].x);
   float _340 = -0.0f - (c_light_vec[1].y);
   float _341 = -0.0f - (c_light_vec[1].z);
   float _342 = dot(float3(_158, _159, _160), float3(_339, _340, _341));
   float _343 = saturate(_342);
   float _348 = saturate(_173);
   float _349 = (c_light_col[0].x) * _348;
   float _350 = (c_light_col[0].y) * _348;
   float _351 = (c_light_col[0].z) * _348;
   float _356 = (c_light_col[1].x) * _343;
   float _357 = (c_light_col[1].y) * _343;
   float _358 = (c_light_col[1].z) * _343;
   float _367 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _368 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _369 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _370 = _349 * _270;
   float _371 = _350 * _271;
   float _372 = _351 * _272;
   float _373 = _356 * _258;
   float _374 = _357 * _258;
   float _375 = _358 * _258;
   float _376 = _373 + _370;
   float _377 = _374 + _371;
   float _378 = _375 + _372;
   float _379 = _367 * _376;
   float _380 = _368 * _377;
   float _381 = _369 * _378;
   float _382 = _310 + c_scene_ambient.x;
   float _383 = _382 + _379;
   float _384 = _312 + c_scene_ambient.y;
   float _385 = _384 + _380;
   float _386 = _314 + c_scene_ambient.z;
   float _387 = _386 + _381;
   float _388 = _383 * _273;
   float _389 = _385 * _274;
   float _390 = _387 * _275;
   float _391 = _329 * _296;
   float _392 = _330 * _297;
   float _393 = _331 * _298;
   float _394 = _388 + _391;
   float _395 = _389 + _392;
   float _396 = _390 + _393;
   float _401 = (c_light_vec[0].x) + _163;
   float _402 = (c_light_vec[0].y) + _164;
   float _403 = (c_light_vec[0].z) + _165;
   float _404 = dot(float3(_401, _402, _403), float3(_401, _402, _403));
   float _405 = rsqrt(_404);
   float _406 = _401 * _405;
   float _407 = -0.0f - _406;
   float _408 = _402 * _405;
   float _409 = -0.0f - _408;
   float _410 = _403 * _405;
   float _411 = -0.0f - _410;
   float _412 = dot(float3(_407, _409, _411), float3(_158, _159, _160));
   float _413 = saturate(_412);
   float _414 = _164 + -0.30000001192092896f;
   float _415 = dot(float3(_163, _414, _165), float3(_163, _414, _165));
   float _416 = rsqrt(_415);
   float _417 = _163 * _416;
   float _418 = -0.0f - _417;
   float _419 = _414 * _416;
   float _420 = -0.0f - _419;
   float _421 = _165 * _416;
   float _422 = -0.0f - _421;
   float _423 = dot(float3(_418, _420, _422), float3(_158, _159, _160));
   float _424 = saturate(_423);
   float _427 = log2(_413);
   float _428 = log2(_424);
   float _429 = _427 * c_mat_power;
   float _430 = _428 * c_mat_power;
   float _431 = exp2(_429);
   float _432 = exp2(_430);
   float _435 = _431 * (c_light_col[0].x);
   float _436 = _431 * (c_light_col[0].y);
   float _437 = _431 * (c_light_col[0].z);
   float _438 = _432 * 2.0f;
   float _439 = _438 * (c_light_vec[5].x);
   float _440 = _439 * (c_light_col[1].x);
   float _441 = _439 * (c_light_col[1].y);
   float _442 = _439 * (c_light_col[1].z);
   float _443 = _435 * _270;
   float _444 = _436 * _271;
   float _445 = _437 * _272;
   float _446 = _440 * _343;
   float _447 = _441 * _343;
   float _448 = _442 * _343;
   float _449 = _446 + _443;
   float _450 = _447 + _444;
   float _451 = _448 + _445;
   float _458 = c_mat_specular.x * c_specular_scale.x;
   float _459 = _458 * _273;
   float _460 = _459 * _449;
   float _461 = c_mat_specular.y * c_specular_scale.y;
   float _462 = _461 * _274;
   float _463 = _462 * _450;
   float _464 = c_mat_specular.z * c_specular_scale.z;
   float _465 = _464 * _275;
   float _466 = _465 * _451;
   float _467 = _332 * _296;
   float _468 = _467 * _432;
   float _469 = _333 * _297;
   float _470 = _469 * _432;
   float _471 = _334 * _298;
   float _472 = _471 * _432;
   float _473 = _460 + _468;
   float _474 = _463 + _470;
   float _475 = _466 + _472;
   if (!_316)
   {
      float _477 = _258 * _240;
      float _478 = _258 * _241;
      float _479 = _258 * _242;
      float _480 = _394 + _477;
      float _481 = _395 + _478;
      float _482 = _396 + _479;
      float _483 = _258 * _243;
      float _484 = _473 + _483;
      float _485 = _474 + _483;
      float _486 = _475 + _483;
      _488 = _480;
      _489 = _481;
      _490 = _482;
      _491 = _484;
      _492 = _485;
      _493 = _486;
   }
   else
   {
      _488 = _394;
      _489 = _395;
      _490 = _396;
      _491 = _473;
      _492 = _474;
      _493 = _475;
   }
   int _494 = c_bool & 128;
   bool _495 = (_494 != 0);
   float _496 = select(_495, 0.0f, _491);
   float _497 = select(_495, 0.0f, _492);
   float _498 = select(_495, 0.0f, _493);
   float _499 = _488 * _114;
   float _500 = _489 * _115;
   float _501 = _490 * _116;
   float _502 = _496 * _104;
   float _503 = _497 * _104;
   float _504 = _498 * _104;
   int _505 = c_bool & 32768;
   bool _506 = (_505 == 0);
   if (!_506)
   {
      float _510 = (c_ps_user[11].x) * _114;
      float _511 = (c_ps_user[11].x) * _115;
      float _512 = (c_ps_user[11].x) * _116;
      float _514 = _158 + 1.0f;
      float _515 = _159 + 1.0f;
      float _516 = _160 + 1.0f;
      float _517 = _514 * 0.5f;
      float _518 = _515 * 0.5f;
      float _519 = _516 * 0.5f;
      float _520 = saturate(_517);
      float _521 = saturate(_518);
      float _522 = saturate(_519);
      float _523 = (c_ps_user[11].y) * _520;
      float _524 = _521 * (c_ps_user[11].y);
      float _525 = _522 * (c_ps_user[11].y);
      float _527 = (c_ps_user[11].z) * _104;
      float _529 = c_mat_power * 0.009999999776482582f;
      float _530 = _529 * (c_ps_user[11].w);
      float _533 = (c_ps_user[12].x) * _270;
      float _534 = (c_ps_user[12].x) * _271;
      float _535 = (c_ps_user[12].x) * _272;
      float _537 = (c_ps_user[12].y) * _273;
      float _538 = (c_ps_user[12].y) * _274;
      float _539 = (c_ps_user[12].y) * _275;
      float _541 = (c_ps_user[12].z) * _296;
      float _542 = (c_ps_user[12].z) * _297;
      float _543 = (c_ps_user[12].z) * _298;
      float _549 = -0.0f - _163;
      float _550 = -0.0f - _164;
      float _551 = -0.0f - _165;
      float _552 = dot(float3(_549, _550, _551), float3(_158, _159, _160));
      float _553 = 1.0f - _552;
      float _554 = _553 * _553;
      float _555 = _553 * 0.9722222089767456f;
      float _556 = _554 * _554;
      float _557 = _556 * _555;
      float _558 = _557 + 0.02777777425944805f;
      float _559 = _558 * (c_ps_user[13].z);
      float _561 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _562 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _563 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _566 = (c_ps_user[14].x) * _310;
      float _567 = (c_ps_user[14].x) * _312;
      float _568 = (c_ps_user[14].x) * _314;
      float _570 = (c_ps_user[14].y) * _349;
      float _571 = (c_ps_user[14].y) * _350;
      float _572 = (c_ps_user[14].y) * _351;
      float _574 = (c_ps_user[14].z) * _356;
      float _575 = (c_ps_user[14].z) * _357;
      float _576 = (c_ps_user[14].z) * _358;
      float _578 = (c_ps_user[14].w) * _329;
      float _579 = (c_ps_user[14].w) * _330;
      float _580 = (c_ps_user[14].w) * _331;
      float _583 = (c_ps_user[15].x) * _435;
      float _584 = (c_ps_user[15].x) * _436;
      float _585 = (c_ps_user[15].x) * _437;
      float _587 = _458 * _440;
      float _588 = _587 * (c_ps_user[15].y);
      float _589 = _461 * _441;
      float _590 = _589 * (c_ps_user[15].y);
      float _591 = _464 * _442;
      float _592 = _591 * (c_ps_user[15].y);
      float _594 = (c_ps_user[15].z) * _332;
      float _595 = (c_ps_user[15].z) * _333;
      float _596 = (c_ps_user[15].z) * _334;
      float _597 = _588 + _594;
      float _598 = _597 * _432;
      float _599 = _570 + (c_ps_user[13].x);
      float _600 = _599 + _574;
      float _601 = c_diffuse_scale.x * _600;
      float _602 = _601 * c_mat_diffuse.x;
      float _603 = (c_ps_user[13].y) + _527;
      float _604 = _603 + _583;
      float _605 = c_mat_specular.x * _604;
      float _606 = _605 * c_specular_scale.x;
      float _607 = _523 + _510;
      float _608 = _607 + _530;
      float _609 = _608 + (c_ps_user[12].w);
      float _610 = _609 + _533;
      float _611 = _610 + _537;
      float _612 = _611 + _541;
      float _613 = _612 + _561;
      float _614 = _613 + _566;
      float _615 = _614 + _578;
      float _616 = _615 + _559;
      float _617 = _616 + _598;
      float _618 = _617 + _602;
      float _619 = _618 + _606;
      float _620 = _590 + _595;
      float _621 = _620 * _432;
      float _622 = _571 + (c_ps_user[13].x);
      float _623 = _622 + _575;
      float _624 = c_diffuse_scale.y * _623;
      float _625 = _624 * c_mat_diffuse.y;
      float _626 = _603 + _584;
      float _627 = c_mat_specular.y * _626;
      float _628 = _627 * c_specular_scale.y;
      float _629 = _524 + _511;
      float _630 = _629 + _530;
      float _631 = _630 + (c_ps_user[12].w);
      float _632 = _631 + _534;
      float _633 = _632 + _538;
      float _634 = _633 + _542;
      float _635 = _634 + _562;
      float _636 = _635 + _567;
      float _637 = _636 + _579;
      float _638 = _637 + _559;
      float _639 = _638 + _621;
      float _640 = _639 + _625;
      float _641 = _640 + _628;
      float _642 = _592 + _596;
      float _643 = _642 * _432;
      float _644 = _572 + (c_ps_user[13].x);
      float _645 = _644 + _576;
      float _646 = c_diffuse_scale.z * _645;
      float _647 = _646 * c_mat_diffuse.z;
      float _648 = _603 + _585;
      float _649 = c_mat_specular.z * _648;
      float _650 = _649 * c_specular_scale.z;
      float _651 = _530 + _512;
      float _652 = _651 + _525;
      float _653 = _652 + (c_ps_user[12].w);
      float _654 = _653 + _535;
      float _655 = _654 + _539;
      float _656 = _655 + _543;
      float _657 = _656 + _563;
      float _658 = _657 + _568;
      float _659 = _658 + _580;
      float _660 = _659 + _559;
      float _661 = _660 + _643;
      float _662 = _661 + _647;
      float _663 = _662 + _650;
      _665 = _619;
      _666 = _641;
      _667 = _663;
      _668 = 0.0f;
      _669 = 0.0f;
      _670 = 0.0f;
   }
   else
   {
      _665 = _499;
      _666 = _500;
      _667 = _501;
      _668 = _502;
      _669 = _503;
      _670 = _504;
   }
   float _671 = _668 + _665;
   float _672 = _669 + _666;
   float _673 = _670 + _667;
   float _678 = c_mat_opacity * _54.w;
   float _679 = _678 * c_global_opacity;
   float _680 = _679 * c_opacity_scale;
   int _681 = c_bool & 16;
   bool _682 = (_681 == 0);
   if (!_682)
   {
      float _686 = c_tone_control.x * _671;
      float _687 = c_tone_control.x * _672;
      float _688 = c_tone_control.x * _673;
      float _690 = log2(_686);
      float _691 = log2(_687);
      float _692 = log2(_688);
      float _693 = _690 * c_tone_control.y;
      float _694 = _691 * c_tone_control.y;
      float _695 = _692 * c_tone_control.y;
      float _696 = exp2(_693);
      float _697 = exp2(_694);
      float _698 = exp2(_695);
      _746 = _696;
      _747 = _697;
      _748 = _698;
   }
   else
   {
      if (_506)
      {
         float _703 = c_tone_control.x * _671;
         float _704 = c_tone_control.x * _672;
         float _705 = c_tone_control.x * _673;
         float _706 = _703 * 2.0f;
         float _707 = _704 * 2.0f;
         float _708 = _705 * 2.0f;
         float _709 = _703 * 0.30000001192092896f;
         float _710 = _704 * 0.30000001192092896f;
         float _711 = _705 * 0.30000001192092896f;
         float _712 = _709 + 0.05000000074505806f;
         float _713 = _710 + 0.05000000074505806f;
         float _714 = _711 + 0.05000000074505806f;
         float _715 = _712 * _706;
         float _716 = _713 * _707;
         float _717 = _714 * _708;
         float _718 = _715 + 0.004000000189989805f;
         float _719 = _716 + 0.004000000189989805f;
         float _720 = _717 + 0.004000000189989805f;
         float _721 = _709 + 0.5f;
         float _722 = _710 + 0.5f;
         float _723 = _711 + 0.5f;
         float _724 = _721 * _706;
         float _725 = _722 * _707;
         float _726 = _723 * _708;
         float _727 = _724 + 0.06000000238418579f;
         float _728 = _725 + 0.06000000238418579f;
         float _729 = _726 + 0.06000000238418579f;
         float _730 = _718 / _727;
         float _731 = _719 / _728;
         float _732 = _720 / _729;
         float _733 = _730 + -0.06666666269302368f;
         float _734 = _731 + -0.06666666269302368f;
         float _735 = _732 + -0.06666666269302368f;
         float _736 = _733 * 1.3790643215179443f;
         float _737 = _734 * 1.3790643215179443f;
         float _738 = _735 * 1.3790643215179443f;
         float _739 = YKR_MaterialCurve(_736, _703);
         float _740 = YKR_MaterialCurve(_737, _704);
         float _741 = YKR_MaterialCurve(_738, _705);
         float _742 = sqrt(_739);
         float _743 = sqrt(_740);
         float _744 = sqrt(_741);
         _746 = _742;
         _747 = _743;
         _748 = _744;
      }
      else
      {
         _746 = _671;
         _747 = _672;
         _748 = _673;
      }
   }
   int _749 = c_bool & 4;
   bool _750 = (_749 == 0);
   if (_750)
   {
      float _752 = saturate(COLOR_1.x);
      float _757 = (c_fog_param[0].x) - _746;
      float _758 = (c_fog_param[0].y) - _747;
      float _759 = (c_fog_param[0].z) - _748;
      float _760 = _757 * _752;
      float _761 = _758 * _752;
      float _762 = _759 * _752;
      _764 = _760;
      _765 = _761;
      _766 = _762;
   }
   else
   {
      _764 = COLOR_1.x;
      _765 = COLOR_1.y;
      _766 = COLOR_1.z;
   }
   float _767 = _764 + _746;
   float _768 = _765 + _747;
   float _769 = _766 + _748;
   float _775 = (c_ps_col_scl[0].x) * _767;
   float _776 = (c_ps_col_scl[0].y) * _768;
   float _777 = (c_ps_col_scl[0].z) * _769;
   float _778 = (c_ps_col_scl[0].w) * _680;
   SV_Target.x = _775;
   SV_Target.y = _776;
   SV_Target.z = _777;
   SV_Target.w = _778;
   int _781 = c_bool & 8;
   bool _782 = (_781 != 0);
   float _783 = select(_782, _767, _775);
   float _784 = select(_782, _768, _776);
   float _785 = select(_782, _769, _777);
   float _786 = select(_782, _680, _778);
   SV_Target.x = _783;
   SV_Target.y = _784;
   SV_Target.z = _785;
   SV_Target.w = _786;
   return SV_Target;
}
