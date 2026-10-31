// Yakuza Kiwami material r_b1dut_m4d (0x138B191D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

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
   float _68 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _69 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _70 = s5_t.Sample(s5_s, float2(_68, _69));
   float4 _73 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _76 = _70.x * 2.0f;
   float _77 = _70.y * 2.0f;
   float _78 = _76 + -1.0f;
   float _79 = _77 + -1.0f;
   float _80 = dot(float2(_78, _79), float2(_78, _79));
   float _81 = 1.0f - _80;
   float _82 = saturate(_81);
   float _83 = sqrt(_82);
   float _84 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _85 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _86 = _84 - _85;
   float _87 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _88 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _89 = _87 - _88;
   float _90 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _91 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _92 = _90 - _91;
   float _93 = _78 * TEXCOORD_6.x;
   float _94 = _78 * TEXCOORD_6.y;
   float _95 = _78 * TEXCOORD_6.z;
   float _96 = _79 * TEXCOORD_6.w;
   float _97 = _96 * _86;
   float _98 = _96 * _89;
   float _99 = _96 * _92;
   float _100 = _97 + _93;
   float _101 = _98 + _94;
   float _102 = _99 + _95;
   float _103 = _83 * TEXCOORD_2.x;
   float _104 = _83 * TEXCOORD_2.y;
   float _105 = _83 * TEXCOORD_2.z;
   float _106 = _100 + _103;
   float _107 = _101 + _104;
   float _108 = _102 + _105;
   float _109 = dot(float3(_106, _107, _108), float3(_106, _107, _108));
   float _110 = rsqrt(_109);
   float _111 = _106 * _110;
   float _112 = _107 * _110;
   float _113 = _108 * _110;
   float _114 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _115 = rsqrt(_114);
   float _116 = _115 * TEXCOORD_3.x;
   float _117 = _115 * TEXCOORD_3.y;
   float _118 = _115 * TEXCOORD_3.z;
   float _123 = -0.0f - (c_light_vec[0].x);
   float _124 = -0.0f - (c_light_vec[0].y);
   float _125 = -0.0f - (c_light_vec[0].z);
   float _126 = dot(float3(_111, _112, _113), float3(_123, _124, _125));
   float _130 = c_rt_size.z * SV_Position.x;
   float _131 = c_rt_size.w * SV_Position.y;
   int _134 = c_bool & 8;
   bool _135 = (_134 == 0);
   float _142;
   float _143;
   float _144;
   float _156;
   float _157;
   float _212;
   float _213;
   float _214;
   float _215;
   float _229;
   float _230;
   float _276;
   float _277;
   float _278;
   float _313;
   float _314;
   float _315;
   float _316;
   float _317;
   float _318;
   float _472;
   float _473;
   float _474;
   float _475;
   float _476;
   float _477;
   float _649;
   float _650;
   float _651;
   float _652;
   float _653;
   float _654;
   float _731;
   float _732;
   float _733;
   float _749;
   float _750;
   float _751;
   if (!_135)
   {
      float4 _137 = s13_t.Sample(s13_s, float2(_130, _131));
      _142 = _137.x;
      _143 = _137.y;
      _144 = _137.w;
   }
   else
   {
      _142 = 1.0f;
      _143 = 1.0f;
      _144 = 1.0f;
   }
   if (!_135)
   {
      float _149 = (c_sdw_param[0].x) * _142;
      float _150 = (c_sdw_param[0].z) * _143;
      float _153 = _149 + (c_sdw_param[0].y);
      float _154 = _150 + (c_sdw_param[0].w);
      _156 = _153;
      _157 = _154;
   }
   else
   {
      _156 = _142;
      _157 = _143;
   }
   float _164 = (c_sdw_param[1].x) * _73.y;
   float _166 = _164 + (c_sdw_param[1].y);
   float _167 = _166 * _73.x;
   float _168 = 1.0f - (c_light_col[5].x);
   float _169 = 1.0f - (c_light_col[5].y);
   float _170 = 1.0f - (c_light_col[5].z);
   float _171 = _167 * _168;
   float _172 = _167 * _169;
   float _173 = _167 * _170;
   float _174 = _171 + (c_light_col[5].x);
   float _175 = _172 + (c_light_col[5].y);
   float _176 = _173 + (c_light_col[5].z);
   float _180 = c_rt_size.z * SV_Position.x;
   float _181 = c_rt_size.w * SV_Position.y;
   float4 _182 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_180, _181)));
   int _189 = c_bool & 16384;
   bool _190 = (_189 == 0);
   if (!_190)
   {
      float _192 = _182.x * 2.0f;
      float _193 = _182.y * 2.0f;
      float _194 = _182.z * 2.0f;
      float _195 = _182.w * 2.0f;
      _212 = _192;
      _213 = _193;
      _214 = _194;
      _215 = _195;
   }
   else
   {
      int _197 = c_bool & 8192;
      bool _198 = (_197 == 0);
      if (!_198)
      {
         float _200 = _182.x * 2.0f;
         float _201 = _182.y * 2.0f;
         float _202 = _182.z * 2.0f;
         float _203 = _182.w * 2.0f;
         _212 = _200;
         _213 = _201;
         _214 = _202;
         _215 = _203;
      }
      else
      {
         float _207 = _182.x / c_tone_control.z;
         float _208 = _182.y / c_tone_control.z;
         float _209 = _182.z / c_tone_control.z;
         float _210 = _182.w / c_tone_control.z;
         _212 = _207;
         _213 = _208;
         _214 = _209;
         _215 = _210;
      }
   }
   int _216 = c_bool & 8;
   bool _217 = (_216 == 0);
   if (!_217)
   {
      float _222 = (c_sdw_param[0].x) * _156;
      float _223 = (c_sdw_param[0].z) * _157;
      float _226 = _222 + (c_sdw_param[0].y);
      float _227 = _223 + (c_sdw_param[0].w);
      _229 = _226;
      _230 = _227;
   }
   else
   {
      _229 = _156;
      _230 = _157;
   }
   float _231 = _230 * _229;
   float _236 = 1.0f - (c_light_col[5].x);
   float _237 = 1.0f - (c_light_col[5].y);
   float _238 = 1.0f - (c_light_col[5].z);
   float _239 = _236 * _231;
   float _240 = _237 * _231;
   float _241 = _238 * _231;
   float _242 = _239 + (c_light_col[5].x);
   float _243 = _240 + (c_light_col[5].y);
   float _244 = _241 + (c_light_col[5].z);
   float _245 = _144 * COLOR.x;
   float _246 = _245 * _174;
   float _247 = _144 * COLOR.y;
   float _248 = _247 * _175;
   float _249 = _144 * COLOR.z;
   float _250 = _249 * _176;
   float _251 = _242 * _174;
   float _252 = _243 * _175;
   float _253 = _244 * _176;
   if (!_217)
   {
      float _257 = (c_sdw_param[2].x) * _242;
      float _258 = (c_sdw_param[2].x) * _243;
      float _259 = (c_sdw_param[2].x) * _244;
      float _261 = _257 + (c_sdw_param[2].y);
      float _262 = _258 + (c_sdw_param[2].y);
      float _263 = _259 + (c_sdw_param[2].y);
      float _265 = (c_sdw_param[2].z) * _174;
      float _266 = (c_sdw_param[2].z) * _175;
      float _267 = (c_sdw_param[2].z) * _176;
      float _269 = _265 + (c_sdw_param[2].w);
      float _270 = _266 + (c_sdw_param[2].w);
      float _271 = _267 + (c_sdw_param[2].w);
      float _272 = _269 * _261;
      float _273 = _270 * _262;
      float _274 = _271 * _263;
      _276 = _272;
      _277 = _273;
      _278 = _274;
   }
   else
   {
      _276 = _251;
      _277 = _252;
      _278 = _253;
   }
   float _279 = _144 * _73.x;
   float _280 = _276 * _279;
   float _281 = _277 * _279;
   float _282 = _278 * _279;
   float _293 = c_mat_emissive.x * _55;
   float _294 = _293 * c_emissive_scale;
   float _295 = c_mat_emissive.y * _56;
   float _296 = _295 * c_emissive_scale;
   float _297 = c_mat_emissive.z * _57;
   float _298 = _297 * c_emissive_scale;
   int _299 = c_bool & 2048;
   bool _300 = (_299 == 0);
   if (_300)
   {
      float _302 = _230 * _212;
      float _303 = _230 * _213;
      float _304 = _230 * _214;
      float _305 = _302 + TEXCOORD_4.x;
      float _306 = _303 + TEXCOORD_4.y;
      float _307 = _304 + TEXCOORD_4.z;
      float _308 = _230 * _215;
      float _309 = _308 + TEXCOORD_5.x;
      float _310 = _308 + TEXCOORD_5.y;
      float _311 = _308 + TEXCOORD_5.z;
      _313 = _309;
      _314 = _310;
      _315 = _311;
      _316 = _305;
      _317 = _306;
      _318 = _307;
   }
   else
   {
      _313 = TEXCOORD_5.x;
      _314 = TEXCOORD_5.y;
      _315 = TEXCOORD_5.z;
      _316 = TEXCOORD_4.x;
      _317 = TEXCOORD_4.y;
      _318 = TEXCOORD_4.z;
   }
   float _323 = -0.0f - (c_light_vec[1].x);
   float _324 = -0.0f - (c_light_vec[1].y);
   float _325 = -0.0f - (c_light_vec[1].z);
   float _326 = dot(float3(_111, _112, _113), float3(_323, _324, _325));
   float _327 = saturate(_326);
   float _332 = saturate(_126);
   float _333 = (c_light_col[0].x) * _332;
   float _334 = (c_light_col[0].y) * _332;
   float _335 = (c_light_col[0].z) * _332;
   float _340 = (c_light_col[1].x) * _327;
   float _341 = (c_light_col[1].y) * _327;
   float _342 = (c_light_col[1].z) * _327;
   float _351 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _352 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _353 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _354 = _333 * _242;
   float _355 = _334 * _243;
   float _356 = _335 * _244;
   float _357 = _340 * _230;
   float _358 = _341 * _230;
   float _359 = _342 * _230;
   float _360 = _357 + _354;
   float _361 = _358 + _355;
   float _362 = _359 + _356;
   float _363 = _351 * _360;
   float _364 = _352 * _361;
   float _365 = _353 * _362;
   float _366 = _294 + c_scene_ambient.x;
   float _367 = _366 + _363;
   float _368 = _296 + c_scene_ambient.y;
   float _369 = _368 + _364;
   float _370 = _298 + c_scene_ambient.z;
   float _371 = _370 + _365;
   float _372 = _367 * _246;
   float _373 = _369 * _248;
   float _374 = _371 * _250;
   float _375 = _316 * _280;
   float _376 = _317 * _281;
   float _377 = _318 * _282;
   float _378 = _372 + _375;
   float _379 = _373 + _376;
   float _380 = _374 + _377;
   float _385 = (c_light_vec[0].x) + _116;
   float _386 = (c_light_vec[0].y) + _117;
   float _387 = (c_light_vec[0].z) + _118;
   float _388 = dot(float3(_385, _386, _387), float3(_385, _386, _387));
   float _389 = rsqrt(_388);
   float _390 = _385 * _389;
   float _391 = -0.0f - _390;
   float _392 = _386 * _389;
   float _393 = -0.0f - _392;
   float _394 = _387 * _389;
   float _395 = -0.0f - _394;
   float _396 = dot(float3(_391, _393, _395), float3(_111, _112, _113));
   float _397 = saturate(_396);
   float _398 = _117 + -0.30000001192092896f;
   float _399 = dot(float3(_116, _398, _118), float3(_116, _398, _118));
   float _400 = rsqrt(_399);
   float _401 = _116 * _400;
   float _402 = -0.0f - _401;
   float _403 = _398 * _400;
   float _404 = -0.0f - _403;
   float _405 = _118 * _400;
   float _406 = -0.0f - _405;
   float _407 = dot(float3(_402, _404, _406), float3(_111, _112, _113));
   float _408 = saturate(_407);
   float _411 = log2(_397);
   float _412 = log2(_408);
   float _413 = _411 * c_mat_power;
   float _414 = _412 * c_mat_power;
   float _415 = exp2(_413);
   float _416 = exp2(_414);
   float _419 = _415 * (c_light_col[0].x);
   float _420 = _415 * (c_light_col[0].y);
   float _421 = _415 * (c_light_col[0].z);
   float _422 = _416 * 2.0f;
   float _423 = _422 * (c_light_vec[5].x);
   float _424 = _423 * (c_light_col[1].x);
   float _425 = _423 * (c_light_col[1].y);
   float _426 = _423 * (c_light_col[1].z);
   float _427 = _419 * _242;
   float _428 = _420 * _243;
   float _429 = _421 * _244;
   float _430 = _424 * _327;
   float _431 = _425 * _327;
   float _432 = _426 * _327;
   float _433 = _430 + _427;
   float _434 = _431 + _428;
   float _435 = _432 + _429;
   float _442 = c_mat_specular.x * c_specular_scale.x;
   float _443 = _442 * _246;
   float _444 = _443 * _433;
   float _445 = c_mat_specular.y * c_specular_scale.y;
   float _446 = _445 * _248;
   float _447 = _446 * _434;
   float _448 = c_mat_specular.z * c_specular_scale.z;
   float _449 = _448 * _250;
   float _450 = _449 * _435;
   float _451 = _313 * _280;
   float _452 = _451 * _416;
   float _453 = _314 * _281;
   float _454 = _453 * _416;
   float _455 = _315 * _282;
   float _456 = _455 * _416;
   float _457 = _444 + _452;
   float _458 = _447 + _454;
   float _459 = _450 + _456;
   if (!_300)
   {
      float _461 = _230 * _212;
      float _462 = _230 * _213;
      float _463 = _230 * _214;
      float _464 = _378 + _461;
      float _465 = _379 + _462;
      float _466 = _380 + _463;
      float _467 = _230 * _215;
      float _468 = _457 + _467;
      float _469 = _458 + _467;
      float _470 = _459 + _467;
      _472 = _464;
      _473 = _465;
      _474 = _466;
      _475 = _468;
      _476 = _469;
      _477 = _470;
   }
   else
   {
      _472 = _378;
      _473 = _379;
      _474 = _380;
      _475 = _457;
      _476 = _458;
      _477 = _459;
   }
   int _478 = c_bool & 128;
   bool _479 = (_478 != 0);
   float _480 = select(_479, 0.0f, _475);
   float _481 = select(_479, 0.0f, _476);
   float _482 = select(_479, 0.0f, _477);
   float _483 = _472 * _55;
   float _484 = _473 * _56;
   float _485 = _474 * _57;
   float _486 = _480 * _63.y;
   float _487 = _481 * _63.y;
   float _488 = _482 * _63.y;
   int _489 = c_bool & 32768;
   bool _490 = (_489 == 0);
   if (!_490)
   {
      float _494 = (c_ps_user[11].x) * _55;
      float _495 = (c_ps_user[11].x) * _56;
      float _496 = (c_ps_user[11].x) * _57;
      float _498 = _111 + 1.0f;
      float _499 = _112 + 1.0f;
      float _500 = _113 + 1.0f;
      float _501 = _498 * 0.5f;
      float _502 = _499 * 0.5f;
      float _503 = _500 * 0.5f;
      float _504 = saturate(_501);
      float _505 = saturate(_502);
      float _506 = saturate(_503);
      float _507 = (c_ps_user[11].y) * _504;
      float _508 = _505 * (c_ps_user[11].y);
      float _509 = _506 * (c_ps_user[11].y);
      float _511 = (c_ps_user[11].z) * _63.y;
      float _513 = c_mat_power * 0.009999999776482582f;
      float _514 = _513 * (c_ps_user[11].w);
      float _517 = (c_ps_user[12].x) * _242;
      float _518 = (c_ps_user[12].x) * _243;
      float _519 = (c_ps_user[12].x) * _244;
      float _521 = (c_ps_user[12].y) * _246;
      float _522 = (c_ps_user[12].y) * _248;
      float _523 = (c_ps_user[12].y) * _250;
      float _525 = (c_ps_user[12].z) * _280;
      float _526 = (c_ps_user[12].z) * _281;
      float _527 = (c_ps_user[12].z) * _282;
      float _533 = -0.0f - _116;
      float _534 = -0.0f - _117;
      float _535 = -0.0f - _118;
      float _536 = dot(float3(_533, _534, _535), float3(_111, _112, _113));
      float _537 = 1.0f - _536;
      float _538 = _537 * _537;
      float _539 = _537 * 0.9722222089767456f;
      float _540 = _538 * _538;
      float _541 = _540 * _539;
      float _542 = _541 + 0.02777777425944805f;
      float _543 = _542 * (c_ps_user[13].z);
      float _545 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _546 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _547 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _550 = (c_ps_user[14].x) * _294;
      float _551 = (c_ps_user[14].x) * _296;
      float _552 = (c_ps_user[14].x) * _298;
      float _554 = (c_ps_user[14].y) * _333;
      float _555 = (c_ps_user[14].y) * _334;
      float _556 = (c_ps_user[14].y) * _335;
      float _558 = (c_ps_user[14].z) * _340;
      float _559 = (c_ps_user[14].z) * _341;
      float _560 = (c_ps_user[14].z) * _342;
      float _562 = (c_ps_user[14].w) * _316;
      float _563 = (c_ps_user[14].w) * _317;
      float _564 = (c_ps_user[14].w) * _318;
      float _567 = (c_ps_user[15].x) * _419;
      float _568 = (c_ps_user[15].x) * _420;
      float _569 = (c_ps_user[15].x) * _421;
      float _571 = _442 * _424;
      float _572 = _571 * (c_ps_user[15].y);
      float _573 = _445 * _425;
      float _574 = _573 * (c_ps_user[15].y);
      float _575 = _448 * _426;
      float _576 = _575 * (c_ps_user[15].y);
      float _578 = (c_ps_user[15].z) * _313;
      float _579 = (c_ps_user[15].z) * _314;
      float _580 = (c_ps_user[15].z) * _315;
      float _581 = _572 + _578;
      float _582 = _581 * _416;
      float _583 = _554 + (c_ps_user[13].x);
      float _584 = _583 + _558;
      float _585 = c_diffuse_scale.x * _584;
      float _586 = _585 * c_mat_diffuse.x;
      float _587 = (c_ps_user[13].y) + _511;
      float _588 = _587 + _567;
      float _589 = c_mat_specular.x * _588;
      float _590 = _589 * c_specular_scale.x;
      float _591 = _507 + _494;
      float _592 = _591 + _514;
      float _593 = _592 + (c_ps_user[12].w);
      float _594 = _593 + _517;
      float _595 = _594 + _521;
      float _596 = _595 + _525;
      float _597 = _596 + _545;
      float _598 = _597 + _550;
      float _599 = _598 + _562;
      float _600 = _599 + _543;
      float _601 = _600 + _582;
      float _602 = _601 + _586;
      float _603 = _602 + _590;
      float _604 = _574 + _579;
      float _605 = _604 * _416;
      float _606 = _555 + (c_ps_user[13].x);
      float _607 = _606 + _559;
      float _608 = c_diffuse_scale.y * _607;
      float _609 = _608 * c_mat_diffuse.y;
      float _610 = _587 + _568;
      float _611 = c_mat_specular.y * _610;
      float _612 = _611 * c_specular_scale.y;
      float _613 = _508 + _495;
      float _614 = _613 + _514;
      float _615 = _614 + (c_ps_user[12].w);
      float _616 = _615 + _518;
      float _617 = _616 + _522;
      float _618 = _617 + _526;
      float _619 = _618 + _546;
      float _620 = _619 + _551;
      float _621 = _620 + _563;
      float _622 = _621 + _543;
      float _623 = _622 + _605;
      float _624 = _623 + _609;
      float _625 = _624 + _612;
      float _626 = _576 + _580;
      float _627 = _626 * _416;
      float _628 = _556 + (c_ps_user[13].x);
      float _629 = _628 + _560;
      float _630 = c_diffuse_scale.z * _629;
      float _631 = _630 * c_mat_diffuse.z;
      float _632 = _587 + _569;
      float _633 = c_mat_specular.z * _632;
      float _634 = _633 * c_specular_scale.z;
      float _635 = _514 + _496;
      float _636 = _635 + _509;
      float _637 = _636 + (c_ps_user[12].w);
      float _638 = _637 + _519;
      float _639 = _638 + _523;
      float _640 = _639 + _527;
      float _641 = _640 + _547;
      float _642 = _641 + _552;
      float _643 = _642 + _564;
      float _644 = _643 + _543;
      float _645 = _644 + _627;
      float _646 = _645 + _631;
      float _647 = _646 + _634;
      _649 = _603;
      _650 = _625;
      _651 = _647;
      _652 = 0.0f;
      _653 = 0.0f;
      _654 = 0.0f;
   }
   else
   {
      _649 = _483;
      _650 = _484;
      _651 = _485;
      _652 = _486;
      _653 = _487;
      _654 = _488;
   }
   float _655 = _652 + _649;
   float _656 = _653 + _650;
   float _657 = _654 + _651;
   float _662 = _50.w * COLOR.w;
   float _663 = _662 * c_mat_opacity;
   float _664 = _663 * c_global_opacity;
   float _665 = _664 * c_opacity_scale;
   int _666 = c_bool & 16;
   bool _667 = (_666 == 0);
   if (!_667)
   {
      float _671 = c_tone_control.x * _655;
      float _672 = c_tone_control.x * _656;
      float _673 = c_tone_control.x * _657;
      float _675 = log2(_671);
      float _676 = log2(_672);
      float _677 = log2(_673);
      float _678 = _675 * c_tone_control.y;
      float _679 = _676 * c_tone_control.y;
      float _680 = _677 * c_tone_control.y;
      float _681 = exp2(_678);
      float _682 = exp2(_679);
      float _683 = exp2(_680);
      _731 = _681;
      _732 = _682;
      _733 = _683;
   }
   else
   {
      if (_490)
      {
         float _688 = c_tone_control.x * _655;
         float _689 = c_tone_control.x * _656;
         float _690 = c_tone_control.x * _657;
         float _691 = _688 * 2.0f;
         float _692 = _689 * 2.0f;
         float _693 = _690 * 2.0f;
         float _694 = _688 * 0.30000001192092896f;
         float _695 = _689 * 0.30000001192092896f;
         float _696 = _690 * 0.30000001192092896f;
         float _697 = _694 + 0.05000000074505806f;
         float _698 = _695 + 0.05000000074505806f;
         float _699 = _696 + 0.05000000074505806f;
         float _700 = _697 * _691;
         float _701 = _698 * _692;
         float _702 = _699 * _693;
         float _703 = _700 + 0.004000000189989805f;
         float _704 = _701 + 0.004000000189989805f;
         float _705 = _702 + 0.004000000189989805f;
         float _706 = _694 + 0.5f;
         float _707 = _695 + 0.5f;
         float _708 = _696 + 0.5f;
         float _709 = _706 * _691;
         float _710 = _707 * _692;
         float _711 = _708 * _693;
         float _712 = _709 + 0.06000000238418579f;
         float _713 = _710 + 0.06000000238418579f;
         float _714 = _711 + 0.06000000238418579f;
         float _715 = _703 / _712;
         float _716 = _704 / _713;
         float _717 = _705 / _714;
         float _718 = _715 + -0.06666666269302368f;
         float _719 = _716 + -0.06666666269302368f;
         float _720 = _717 + -0.06666666269302368f;
         float _721 = _718 * 1.3790643215179443f;
         float _722 = _719 * 1.3790643215179443f;
         float _723 = _720 * 1.3790643215179443f;
         float _724 = YKR_MaterialCurve(_721, _688);
         float _725 = YKR_MaterialCurve(_722, _689);
         float _726 = YKR_MaterialCurve(_723, _690);
         float _727 = sqrt(_724);
         float _728 = sqrt(_725);
         float _729 = sqrt(_726);
         _731 = _727;
         _732 = _728;
         _733 = _729;
      }
      else
      {
         _731 = _655;
         _732 = _656;
         _733 = _657;
      }
   }
   int _734 = c_bool & 4;
   bool _735 = (_734 == 0);
   if (_735)
   {
      float _737 = saturate(COLOR_1.x);
      float _742 = (c_fog_param[0].x) - _731;
      float _743 = (c_fog_param[0].y) - _732;
      float _744 = (c_fog_param[0].z) - _733;
      float _745 = _742 * _737;
      float _746 = _743 * _737;
      float _747 = _744 * _737;
      _749 = _745;
      _750 = _746;
      _751 = _747;
   }
   else
   {
      _749 = COLOR_1.x;
      _750 = COLOR_1.y;
      _751 = COLOR_1.z;
   }
   float _752 = _749 + _731;
   float _753 = _750 + _732;
   float _754 = _751 + _733;
   float _757 = _665 - c_mat_punchthrough_ref;
   bool _758 = (_757 < 0.0f);
   if (_758)
      discard;
   float _764 = (c_ps_col_scl[0].x) * _752;
   float _765 = (c_ps_col_scl[0].y) * _753;
   float _766 = (c_ps_col_scl[0].z) * _754;
   float _767 = (c_ps_col_scl[0].w) * _665;
   SV_Target.x = _764;
   SV_Target.y = _765;
   SV_Target.z = _766;
   SV_Target.w = _767;
   int _770 = c_bool & 8;
   bool _771 = (_770 != 0);
   float _772 = select(_771, _752, _764);
   float _773 = select(_771, _753, _765);
   float _774 = select(_771, _754, _766);
   float _775 = select(_771, _665, _767);
   SV_Target.x = _772;
   SV_Target.y = _773;
   SV_Target.z = _774;
   SV_Target.w = _775;
   return SV_Target;
}
