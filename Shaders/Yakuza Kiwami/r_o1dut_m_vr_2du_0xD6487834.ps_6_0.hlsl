// Yakuza Kiwami material r_o1dut_m(vr)2du (0xD6487834, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s4_s : register(s4);

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
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float _63 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _64 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _65 = s3_t.Sample(s3_s, float2(_63, _64));
   float _70 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _72 = s5_t.Sample(s5_s, float2(_70, _71));
   float4 _75 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _80 = _75.x * _75.x;
   float _81 = _75.y * _75.y;
   float _82 = _75.z * _75.z;
   float _86 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _87 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _88 = s4_t.Sample(s4_s, float2(_86, _87));
   float _90 = _75.w * COLOR_1.w;
   float _91 = 1.0f - _88.y;
   float _92 = _91 * _90;
   float _93 = 1.0f - _92;
   float _94 = _93 * _65.y;
   float _95 = 1.0f - _80;
   float _96 = 1.0f - _81;
   float _97 = 1.0f - _82;
   float _98 = _95 * _90;
   float _99 = _96 * _90;
   float _100 = _97 * _90;
   float _101 = 1.0f - _98;
   float _102 = 1.0f - _99;
   float _103 = 1.0f - _100;
   float _104 = _57 * _101;
   float _105 = _58 * _102;
   float _106 = _59 * _103;
   float _107 = _72.x * 2.0f;
   float _108 = _72.y * 2.0f;
   float _109 = _107 + -1.0f;
   float _110 = _108 + -1.0f;
   float _111 = dot(float2(_109, _110), float2(_109, _110));
   float _112 = 1.0f - _111;
   float _113 = saturate(_112);
   float _114 = sqrt(_113);
   float _115 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _116 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _117 = _115 - _116;
   float _118 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _119 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _120 = _118 - _119;
   float _121 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _122 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _123 = _121 - _122;
   float _124 = _109 * TEXCOORD_6.x;
   float _125 = _109 * TEXCOORD_6.y;
   float _126 = _109 * TEXCOORD_6.z;
   float _127 = _110 * TEXCOORD_6.w;
   float _128 = _127 * _117;
   float _129 = _127 * _120;
   float _130 = _127 * _123;
   float _131 = _128 + _124;
   float _132 = _129 + _125;
   float _133 = _130 + _126;
   float _134 = _114 * TEXCOORD_2.x;
   float _135 = _114 * TEXCOORD_2.y;
   float _136 = _114 * TEXCOORD_2.z;
   float _137 = _131 + _134;
   float _138 = _132 + _135;
   float _139 = _133 + _136;
   float _140 = dot(float3(_137, _138, _139), float3(_137, _138, _139));
   float _141 = rsqrt(_140);
   float _142 = _137 * _141;
   float _143 = _138 * _141;
   float _144 = _139 * _141;
   float _145 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _146 = rsqrt(_145);
   float _147 = _146 * TEXCOORD_3.x;
   float _148 = _146 * TEXCOORD_3.y;
   float _149 = _146 * TEXCOORD_3.z;
   float _154 = -0.0f - (c_light_vec[0].x);
   float _155 = -0.0f - (c_light_vec[0].y);
   float _156 = -0.0f - (c_light_vec[0].z);
   float _157 = dot(float3(_142, _143, _144), float3(_154, _155, _156));
   float _161 = c_rt_size.z * SV_Position.x;
   float _162 = c_rt_size.w * SV_Position.y;
   int _165 = c_bool & 8;
   bool _166 = (_165 == 0);
   float _173;
   float _174;
   float _175;
   float _187;
   float _188;
   float _224;
   float _225;
   float _226;
   float _227;
   float _241;
   float _242;
   float _277;
   float _278;
   float _279;
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
   float _730;
   float _731;
   float _732;
   float _748;
   float _749;
   float _750;
   if (!_166)
   {
      float4 _168 = s13_t.Sample(s13_s, float2(_161, _162));
      _173 = _168.x;
      _174 = _168.y;
      _175 = _168.w;
   }
   else
   {
      _173 = 1.0f;
      _174 = 1.0f;
      _175 = 1.0f;
   }
   if (!_166)
   {
      float _180 = (c_sdw_param[0].x) * _173;
      float _181 = (c_sdw_param[0].z) * _174;
      float _184 = _180 + (c_sdw_param[0].y);
      float _185 = _181 + (c_sdw_param[0].w);
      _187 = _184;
      _188 = _185;
   }
   else
   {
      _187 = _173;
      _188 = _174;
   }
   float _192 = c_rt_size.z * SV_Position.x;
   float _193 = c_rt_size.w * SV_Position.y;
   float4 _194 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_192, _193)));
   int _201 = c_bool & 16384;
   bool _202 = (_201 == 0);
   if (!_202)
   {
      float _204 = _194.x * 2.0f;
      float _205 = _194.y * 2.0f;
      float _206 = _194.z * 2.0f;
      float _207 = _194.w * 2.0f;
      _224 = _204;
      _225 = _205;
      _226 = _206;
      _227 = _207;
   }
   else
   {
      int _209 = c_bool & 8192;
      bool _210 = (_209 == 0);
      if (!_210)
      {
         float _212 = _194.x * 2.0f;
         float _213 = _194.y * 2.0f;
         float _214 = _194.z * 2.0f;
         float _215 = _194.w * 2.0f;
         _224 = _212;
         _225 = _213;
         _226 = _214;
         _227 = _215;
      }
      else
      {
         float _219 = _194.x / c_tone_control.z;
         float _220 = _194.y / c_tone_control.z;
         float _221 = _194.z / c_tone_control.z;
         float _222 = _194.w / c_tone_control.z;
         _224 = _219;
         _225 = _220;
         _226 = _221;
         _227 = _222;
      }
   }
   int _228 = c_bool & 8;
   bool _229 = (_228 == 0);
   if (!_229)
   {
      float _234 = (c_sdw_param[0].x) * _187;
      float _235 = (c_sdw_param[0].z) * _188;
      float _238 = _234 + (c_sdw_param[0].y);
      float _239 = _235 + (c_sdw_param[0].w);
      _241 = _238;
      _242 = _239;
   }
   else
   {
      _241 = _187;
      _242 = _188;
   }
   float _243 = _242 * _241;
   float _248 = 1.0f - (c_light_col[5].x);
   float _249 = 1.0f - (c_light_col[5].y);
   float _250 = 1.0f - (c_light_col[5].z);
   float _251 = _248 * _243;
   float _252 = _249 * _243;
   float _253 = _250 * _243;
   float _254 = _251 + (c_light_col[5].x);
   float _255 = _252 + (c_light_col[5].y);
   float _256 = _253 + (c_light_col[5].z);
   float _257 = _175 * COLOR.x;
   float _258 = _175 * COLOR.y;
   float _259 = _175 * COLOR.z;
   if (!_229)
   {
      float _263 = (c_sdw_param[2].x) * _254;
      float _264 = (c_sdw_param[2].x) * _255;
      float _265 = (c_sdw_param[2].x) * _256;
      float _267 = _263 + (c_sdw_param[2].y);
      float _268 = _264 + (c_sdw_param[2].y);
      float _269 = _265 + (c_sdw_param[2].y);
      float _272 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _273 = _267 * _272;
      float _274 = _268 * _272;
      float _275 = _269 * _272;
      _277 = _273;
      _278 = _274;
      _279 = _275;
   }
   else
   {
      _277 = _254;
      _278 = _255;
      _279 = _256;
   }
   float _280 = _277 * _175;
   float _281 = _278 * _175;
   float _282 = _279 * _175;
   float _293 = c_mat_emissive.x * _104;
   float _294 = _293 * c_emissive_scale;
   float _295 = c_mat_emissive.y * _105;
   float _296 = _295 * c_emissive_scale;
   float _297 = c_mat_emissive.z * _106;
   float _298 = _297 * c_emissive_scale;
   int _299 = c_bool & 2048;
   bool _300 = (_299 == 0);
   if (_300)
   {
      float _302 = _242 * _224;
      float _303 = _242 * _225;
      float _304 = _242 * _226;
      float _305 = _302 + TEXCOORD_4.x;
      float _306 = _303 + TEXCOORD_4.y;
      float _307 = _304 + TEXCOORD_4.z;
      float _308 = _242 * _227;
      float _309 = _308 + TEXCOORD_5.x;
      float _310 = _308 + TEXCOORD_5.y;
      float _311 = _308 + TEXCOORD_5.z;
      _313 = _305;
      _314 = _306;
      _315 = _307;
      _316 = _309;
      _317 = _310;
      _318 = _311;
   }
   else
   {
      _313 = TEXCOORD_4.x;
      _314 = TEXCOORD_4.y;
      _315 = TEXCOORD_4.z;
      _316 = TEXCOORD_5.x;
      _317 = TEXCOORD_5.y;
      _318 = TEXCOORD_5.z;
   }
   float _323 = -0.0f - (c_light_vec[1].x);
   float _324 = -0.0f - (c_light_vec[1].y);
   float _325 = -0.0f - (c_light_vec[1].z);
   float _326 = dot(float3(_142, _143, _144), float3(_323, _324, _325));
   float _327 = saturate(_326);
   float _332 = saturate(_157);
   float _333 = (c_light_col[0].x) * _332;
   float _334 = (c_light_col[0].y) * _332;
   float _335 = (c_light_col[0].z) * _332;
   float _340 = (c_light_col[1].x) * _327;
   float _341 = (c_light_col[1].y) * _327;
   float _342 = (c_light_col[1].z) * _327;
   float _351 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _352 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _353 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _354 = _333 * _254;
   float _355 = _334 * _255;
   float _356 = _335 * _256;
   float _357 = _340 * _242;
   float _358 = _341 * _242;
   float _359 = _342 * _242;
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
   float _372 = _367 * _257;
   float _373 = _369 * _258;
   float _374 = _371 * _259;
   float _375 = _313 * _280;
   float _376 = _314 * _281;
   float _377 = _315 * _282;
   float _378 = _372 + _375;
   float _379 = _373 + _376;
   float _380 = _374 + _377;
   float _385 = (c_light_vec[0].x) + _147;
   float _386 = (c_light_vec[0].y) + _148;
   float _387 = (c_light_vec[0].z) + _149;
   float _388 = dot(float3(_385, _386, _387), float3(_385, _386, _387));
   float _389 = rsqrt(_388);
   float _390 = _385 * _389;
   float _391 = -0.0f - _390;
   float _392 = _386 * _389;
   float _393 = -0.0f - _392;
   float _394 = _387 * _389;
   float _395 = -0.0f - _394;
   float _396 = dot(float3(_391, _393, _395), float3(_142, _143, _144));
   float _397 = saturate(_396);
   float _398 = _148 + -0.30000001192092896f;
   float _399 = dot(float3(_147, _398, _149), float3(_147, _398, _149));
   float _400 = rsqrt(_399);
   float _401 = _147 * _400;
   float _402 = -0.0f - _401;
   float _403 = _398 * _400;
   float _404 = -0.0f - _403;
   float _405 = _149 * _400;
   float _406 = -0.0f - _405;
   float _407 = dot(float3(_402, _404, _406), float3(_142, _143, _144));
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
   float _427 = _419 * _254;
   float _428 = _420 * _255;
   float _429 = _421 * _256;
   float _430 = _424 * _327;
   float _431 = _425 * _327;
   float _432 = _426 * _327;
   float _433 = _430 + _427;
   float _434 = _431 + _428;
   float _435 = _432 + _429;
   float _442 = c_mat_specular.x * c_specular_scale.x;
   float _443 = _442 * _257;
   float _444 = _443 * _433;
   float _445 = c_mat_specular.y * c_specular_scale.y;
   float _446 = _445 * _258;
   float _447 = _446 * _434;
   float _448 = c_mat_specular.z * c_specular_scale.z;
   float _449 = _448 * _259;
   float _450 = _449 * _435;
   float _451 = _316 * _280;
   float _452 = _451 * _416;
   float _453 = _317 * _281;
   float _454 = _453 * _416;
   float _455 = _318 * _282;
   float _456 = _455 * _416;
   float _457 = _444 + _452;
   float _458 = _447 + _454;
   float _459 = _450 + _456;
   if (!_300)
   {
      float _461 = _242 * _224;
      float _462 = _242 * _225;
      float _463 = _242 * _226;
      float _464 = _378 + _461;
      float _465 = _379 + _462;
      float _466 = _380 + _463;
      float _467 = _242 * _227;
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
   float _483 = _472 * _104;
   float _484 = _473 * _105;
   float _485 = _474 * _106;
   float _486 = _480 * _94;
   float _487 = _481 * _94;
   float _488 = _482 * _94;
   int _489 = c_bool & 32768;
   bool _490 = (_489 == 0);
   if (!_490)
   {
      float _494 = (c_ps_user[11].x) * _104;
      float _495 = (c_ps_user[11].x) * _105;
      float _496 = (c_ps_user[11].x) * _106;
      float _498 = _142 + 1.0f;
      float _499 = _143 + 1.0f;
      float _500 = _144 + 1.0f;
      float _501 = _498 * 0.5f;
      float _502 = _499 * 0.5f;
      float _503 = _500 * 0.5f;
      float _504 = saturate(_501);
      float _505 = saturate(_502);
      float _506 = saturate(_503);
      float _507 = (c_ps_user[11].y) * _504;
      float _508 = _505 * (c_ps_user[11].y);
      float _509 = _506 * (c_ps_user[11].y);
      float _511 = (c_ps_user[11].z) * _94;
      float _513 = c_mat_power * 0.009999999776482582f;
      float _514 = _513 * (c_ps_user[11].w);
      float _517 = (c_ps_user[12].x) * _254;
      float _518 = (c_ps_user[12].x) * _255;
      float _519 = (c_ps_user[12].x) * _256;
      float _521 = (c_ps_user[12].y) * _257;
      float _522 = (c_ps_user[12].y) * _258;
      float _523 = (c_ps_user[12].y) * _259;
      float _525 = (c_ps_user[12].z) * _280;
      float _526 = (c_ps_user[12].z) * _281;
      float _527 = (c_ps_user[12].z) * _282;
      float _533 = -0.0f - _147;
      float _534 = -0.0f - _148;
      float _535 = -0.0f - _149;
      float _536 = dot(float3(_533, _534, _535), float3(_142, _143, _144));
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
      float _562 = (c_ps_user[14].w) * _313;
      float _563 = (c_ps_user[14].w) * _314;
      float _564 = (c_ps_user[14].w) * _315;
      float _567 = (c_ps_user[15].x) * _419;
      float _568 = (c_ps_user[15].x) * _420;
      float _569 = (c_ps_user[15].x) * _421;
      float _571 = _442 * _424;
      float _572 = _571 * (c_ps_user[15].y);
      float _573 = _445 * _425;
      float _574 = _573 * (c_ps_user[15].y);
      float _575 = _448 * _426;
      float _576 = _575 * (c_ps_user[15].y);
      float _578 = (c_ps_user[15].z) * _316;
      float _579 = (c_ps_user[15].z) * _317;
      float _580 = (c_ps_user[15].z) * _318;
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
   float _662 = c_mat_opacity * _52.w;
   float _663 = _662 * c_global_opacity;
   float _664 = _663 * c_opacity_scale;
   int _665 = c_bool & 16;
   bool _666 = (_665 == 0);
   if (!_666)
   {
      float _670 = c_tone_control.x * _655;
      float _671 = c_tone_control.x * _656;
      float _672 = c_tone_control.x * _657;
      float _674 = log2(_670);
      float _675 = log2(_671);
      float _676 = log2(_672);
      float _677 = _674 * c_tone_control.y;
      float _678 = _675 * c_tone_control.y;
      float _679 = _676 * c_tone_control.y;
      float _680 = exp2(_677);
      float _681 = exp2(_678);
      float _682 = exp2(_679);
      _730 = _680;
      _731 = _681;
      _732 = _682;
   }
   else
   {
      if (_490)
      {
         float _687 = c_tone_control.x * _655;
         float _688 = c_tone_control.x * _656;
         float _689 = c_tone_control.x * _657;
         float _690 = _687 * 2.0f;
         float _691 = _688 * 2.0f;
         float _692 = _689 * 2.0f;
         float _693 = _687 * 0.30000001192092896f;
         float _694 = _688 * 0.30000001192092896f;
         float _695 = _689 * 0.30000001192092896f;
         float _696 = _693 + 0.05000000074505806f;
         float _697 = _694 + 0.05000000074505806f;
         float _698 = _695 + 0.05000000074505806f;
         float _699 = _696 * _690;
         float _700 = _697 * _691;
         float _701 = _698 * _692;
         float _702 = _699 + 0.004000000189989805f;
         float _703 = _700 + 0.004000000189989805f;
         float _704 = _701 + 0.004000000189989805f;
         float _705 = _693 + 0.5f;
         float _706 = _694 + 0.5f;
         float _707 = _695 + 0.5f;
         float _708 = _705 * _690;
         float _709 = _706 * _691;
         float _710 = _707 * _692;
         float _711 = _708 + 0.06000000238418579f;
         float _712 = _709 + 0.06000000238418579f;
         float _713 = _710 + 0.06000000238418579f;
         float _714 = _702 / _711;
         float _715 = _703 / _712;
         float _716 = _704 / _713;
         float _717 = _714 + -0.06666666269302368f;
         float _718 = _715 + -0.06666666269302368f;
         float _719 = _716 + -0.06666666269302368f;
         float _720 = _717 * 1.3790643215179443f;
         float _721 = _718 * 1.3790643215179443f;
         float _722 = _719 * 1.3790643215179443f;
         float _723 = YKR_MaterialCurve(_720, _687);
         float _724 = YKR_MaterialCurve(_721, _688);
         float _725 = YKR_MaterialCurve(_722, _689);
         float _726 = sqrt(_723);
         float _727 = sqrt(_724);
         float _728 = sqrt(_725);
         _730 = _726;
         _731 = _727;
         _732 = _728;
      }
      else
      {
         _730 = _655;
         _731 = _656;
         _732 = _657;
      }
   }
   int _733 = c_bool & 4;
   bool _734 = (_733 == 0);
   if (_734)
   {
      float _736 = saturate(COLOR_1.x);
      float _741 = (c_fog_param[0].x) - _730;
      float _742 = (c_fog_param[0].y) - _731;
      float _743 = (c_fog_param[0].z) - _732;
      float _744 = _741 * _736;
      float _745 = _742 * _736;
      float _746 = _743 * _736;
      _748 = _744;
      _749 = _745;
      _750 = _746;
   }
   else
   {
      _748 = COLOR_1.x;
      _749 = COLOR_1.y;
      _750 = COLOR_1.z;
   }
   float _751 = _748 + _730;
   float _752 = _749 + _731;
   float _753 = _750 + _732;
   float _759 = (c_ps_col_scl[0].x) * _751;
   float _760 = (c_ps_col_scl[0].y) * _752;
   float _761 = (c_ps_col_scl[0].z) * _753;
   float _762 = (c_ps_col_scl[0].w) * _664;
   SV_Target.x = _759;
   SV_Target.y = _760;
   SV_Target.z = _761;
   SV_Target.w = _762;
   int _765 = c_bool & 8;
   bool _766 = (_765 != 0);
   float _767 = select(_766, _751, _759);
   float _768 = select(_766, _752, _760);
   float _769 = select(_766, _753, _761);
   float _770 = select(_766, _664, _762);
   SV_Target.x = _767;
   SV_Target.y = _768;
   SV_Target.z = _769;
   SV_Target.w = _770;
   return SV_Target;
}
