// Yakuza Kiwami material r_b1dut_m(vr)2d_m4d[ref] (0xD96EB6D1, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
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
   float4 _58 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _63 = _58.x * _58.x;
   float _64 = _58.y * _58.y;
   float _65 = _58.z * _58.z;
   float _69 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _70 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _71 = s3_t.Sample(s3_s, float2(_69, _70));
   float _77 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _78 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _79 = s5_t.Sample(s5_s, float2(_77, _78));
   float4 _82 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _86 = _82.x * _82.x;
   float _87 = _82.y * _82.y;
   float _88 = _82.z * _82.z;
   float4 _89 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _92 = 1.0f - _86;
   float _93 = 1.0f - _87;
   float _94 = _92 * COLOR_1.w;
   float _95 = _93 * COLOR_1.w;
   float _96 = 1.0f - _94;
   float _97 = 1.0f - _95;
   float _98 = _97 * _71.y;
   float _99 = 1.0f - _88;
   float _100 = _99 * COLOR_1.w;
   float _101 = 1.0f - _100;
   float _102 = _63 * _96;
   float _103 = _64 * _97;
   float _104 = _65 * _101;
   float _105 = _79.x * 2.0f;
   float _106 = _79.y * 2.0f;
   float _107 = _105 + -1.0f;
   float _108 = _106 + -1.0f;
   float _109 = dot(float2(_107, _108), float2(_107, _108));
   float _110 = 1.0f - _109;
   float _111 = saturate(_110);
   float _112 = sqrt(_111);
   float _113 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _114 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _115 = _113 - _114;
   float _116 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _117 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _118 = _116 - _117;
   float _119 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _120 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _121 = _119 - _120;
   float _122 = _107 * TEXCOORD_6.x;
   float _123 = _107 * TEXCOORD_6.y;
   float _124 = _107 * TEXCOORD_6.z;
   float _125 = _108 * TEXCOORD_6.w;
   float _126 = _125 * _115;
   float _127 = _125 * _118;
   float _128 = _125 * _121;
   float _129 = _126 + _122;
   float _130 = _127 + _123;
   float _131 = _128 + _124;
   float _132 = _112 * TEXCOORD_2.x;
   float _133 = _112 * TEXCOORD_2.y;
   float _134 = _112 * TEXCOORD_2.z;
   float _135 = _129 + _132;
   float _136 = _130 + _133;
   float _137 = _131 + _134;
   float _138 = dot(float3(_135, _136, _137), float3(_135, _136, _137));
   float _139 = rsqrt(_138);
   float _140 = _135 * _139;
   float _141 = _136 * _139;
   float _142 = _137 * _139;
   float _143 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _144 = rsqrt(_143);
   float _145 = _144 * TEXCOORD_3.x;
   float _146 = _144 * TEXCOORD_3.y;
   float _147 = _144 * TEXCOORD_3.z;
   float _152 = -0.0f - (c_light_vec[0].x);
   float _153 = -0.0f - (c_light_vec[0].y);
   float _154 = -0.0f - (c_light_vec[0].z);
   float _155 = dot(float3(_140, _141, _142), float3(_152, _153, _154));
   float _159 = c_rt_size.z * SV_Position.x;
   float _160 = c_rt_size.w * SV_Position.y;
   int _163 = c_bool & 8;
   bool _164 = (_163 == 0);
   float _171;
   float _172;
   float _173;
   float _185;
   float _186;
   float _241;
   float _242;
   float _243;
   float _244;
   float _258;
   float _259;
   float _305;
   float _306;
   float _307;
   float _342;
   float _343;
   float _344;
   float _345;
   float _346;
   float _347;
   float _501;
   float _502;
   float _503;
   float _504;
   float _505;
   float _506;
   float _678;
   float _679;
   float _680;
   float _681;
   float _682;
   float _683;
   float _808;
   float _809;
   float _810;
   float _826;
   float _827;
   float _828;
   if (!_164)
   {
      float4 _166 = s13_t.Sample(s13_s, float2(_159, _160));
      _171 = _166.x;
      _172 = _166.y;
      _173 = _166.w;
   }
   else
   {
      _171 = 1.0f;
      _172 = 1.0f;
      _173 = 1.0f;
   }
   if (!_164)
   {
      float _178 = (c_sdw_param[0].x) * _171;
      float _179 = (c_sdw_param[0].z) * _172;
      float _182 = _178 + (c_sdw_param[0].y);
      float _183 = _179 + (c_sdw_param[0].w);
      _185 = _182;
      _186 = _183;
   }
   else
   {
      _185 = _171;
      _186 = _172;
   }
   float _193 = (c_sdw_param[1].x) * _89.y;
   float _195 = _193 + (c_sdw_param[1].y);
   float _196 = _195 * _89.x;
   float _197 = 1.0f - (c_light_col[5].x);
   float _198 = 1.0f - (c_light_col[5].y);
   float _199 = 1.0f - (c_light_col[5].z);
   float _200 = _196 * _197;
   float _201 = _196 * _198;
   float _202 = _196 * _199;
   float _203 = _200 + (c_light_col[5].x);
   float _204 = _201 + (c_light_col[5].y);
   float _205 = _202 + (c_light_col[5].z);
   float _209 = c_rt_size.z * SV_Position.x;
   float _210 = c_rt_size.w * SV_Position.y;
   float4 _211 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_209, _210)));
   int _218 = c_bool & 16384;
   bool _219 = (_218 == 0);
   if (!_219)
   {
      float _221 = _211.x * 2.0f;
      float _222 = _211.y * 2.0f;
      float _223 = _211.z * 2.0f;
      float _224 = _211.w * 2.0f;
      _241 = _221;
      _242 = _222;
      _243 = _223;
      _244 = _224;
   }
   else
   {
      int _226 = c_bool & 8192;
      bool _227 = (_226 == 0);
      if (!_227)
      {
         float _229 = _211.x * 2.0f;
         float _230 = _211.y * 2.0f;
         float _231 = _211.z * 2.0f;
         float _232 = _211.w * 2.0f;
         _241 = _229;
         _242 = _230;
         _243 = _231;
         _244 = _232;
      }
      else
      {
         float _236 = _211.x / c_tone_control.z;
         float _237 = _211.y / c_tone_control.z;
         float _238 = _211.z / c_tone_control.z;
         float _239 = _211.w / c_tone_control.z;
         _241 = _236;
         _242 = _237;
         _243 = _238;
         _244 = _239;
      }
   }
   int _245 = c_bool & 8;
   bool _246 = (_245 == 0);
   if (!_246)
   {
      float _251 = (c_sdw_param[0].x) * _185;
      float _252 = (c_sdw_param[0].z) * _186;
      float _255 = _251 + (c_sdw_param[0].y);
      float _256 = _252 + (c_sdw_param[0].w);
      _258 = _255;
      _259 = _256;
   }
   else
   {
      _258 = _185;
      _259 = _186;
   }
   float _260 = _259 * _258;
   float _265 = 1.0f - (c_light_col[5].x);
   float _266 = 1.0f - (c_light_col[5].y);
   float _267 = 1.0f - (c_light_col[5].z);
   float _268 = _265 * _260;
   float _269 = _266 * _260;
   float _270 = _267 * _260;
   float _271 = _268 + (c_light_col[5].x);
   float _272 = _269 + (c_light_col[5].y);
   float _273 = _270 + (c_light_col[5].z);
   float _274 = _173 * COLOR.x;
   float _275 = _274 * _203;
   float _276 = _173 * COLOR.y;
   float _277 = _276 * _204;
   float _278 = _173 * COLOR.z;
   float _279 = _278 * _205;
   float _280 = _271 * _203;
   float _281 = _272 * _204;
   float _282 = _273 * _205;
   if (!_246)
   {
      float _286 = (c_sdw_param[2].x) * _271;
      float _287 = (c_sdw_param[2].x) * _272;
      float _288 = (c_sdw_param[2].x) * _273;
      float _290 = _286 + (c_sdw_param[2].y);
      float _291 = _287 + (c_sdw_param[2].y);
      float _292 = _288 + (c_sdw_param[2].y);
      float _294 = (c_sdw_param[2].z) * _203;
      float _295 = (c_sdw_param[2].z) * _204;
      float _296 = (c_sdw_param[2].z) * _205;
      float _298 = _294 + (c_sdw_param[2].w);
      float _299 = _295 + (c_sdw_param[2].w);
      float _300 = _296 + (c_sdw_param[2].w);
      float _301 = _298 * _290;
      float _302 = _299 * _291;
      float _303 = _300 * _292;
      _305 = _301;
      _306 = _302;
      _307 = _303;
   }
   else
   {
      _305 = _280;
      _306 = _281;
      _307 = _282;
   }
   float _308 = _173 * _89.x;
   float _309 = _305 * _308;
   float _310 = _306 * _308;
   float _311 = _307 * _308;
   float _322 = c_mat_emissive.x * _102;
   float _323 = _322 * c_emissive_scale;
   float _324 = c_mat_emissive.y * _103;
   float _325 = _324 * c_emissive_scale;
   float _326 = c_mat_emissive.z * _104;
   float _327 = _326 * c_emissive_scale;
   int _328 = c_bool & 2048;
   bool _329 = (_328 == 0);
   if (_329)
   {
      float _331 = _259 * _241;
      float _332 = _259 * _242;
      float _333 = _259 * _243;
      float _334 = _331 + TEXCOORD_4.x;
      float _335 = _332 + TEXCOORD_4.y;
      float _336 = _333 + TEXCOORD_4.z;
      float _337 = _259 * _244;
      float _338 = _337 + TEXCOORD_5.x;
      float _339 = _337 + TEXCOORD_5.y;
      float _340 = _337 + TEXCOORD_5.z;
      _342 = _334;
      _343 = _335;
      _344 = _336;
      _345 = _338;
      _346 = _339;
      _347 = _340;
   }
   else
   {
      _342 = TEXCOORD_4.x;
      _343 = TEXCOORD_4.y;
      _344 = TEXCOORD_4.z;
      _345 = TEXCOORD_5.x;
      _346 = TEXCOORD_5.y;
      _347 = TEXCOORD_5.z;
   }
   float _352 = -0.0f - (c_light_vec[1].x);
   float _353 = -0.0f - (c_light_vec[1].y);
   float _354 = -0.0f - (c_light_vec[1].z);
   float _355 = dot(float3(_140, _141, _142), float3(_352, _353, _354));
   float _356 = saturate(_355);
   float _361 = saturate(_155);
   float _362 = (c_light_col[0].x) * _361;
   float _363 = (c_light_col[0].y) * _361;
   float _364 = (c_light_col[0].z) * _361;
   float _369 = (c_light_col[1].x) * _356;
   float _370 = (c_light_col[1].y) * _356;
   float _371 = (c_light_col[1].z) * _356;
   float _380 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _381 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _382 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _383 = _362 * _271;
   float _384 = _363 * _272;
   float _385 = _364 * _273;
   float _386 = _369 * _259;
   float _387 = _370 * _259;
   float _388 = _371 * _259;
   float _389 = _386 + _383;
   float _390 = _387 + _384;
   float _391 = _388 + _385;
   float _392 = _380 * _389;
   float _393 = _381 * _390;
   float _394 = _382 * _391;
   float _395 = _323 + c_scene_ambient.x;
   float _396 = _395 + _392;
   float _397 = _325 + c_scene_ambient.y;
   float _398 = _397 + _393;
   float _399 = _327 + c_scene_ambient.z;
   float _400 = _399 + _394;
   float _401 = _396 * _275;
   float _402 = _398 * _277;
   float _403 = _400 * _279;
   float _404 = _342 * _309;
   float _405 = _343 * _310;
   float _406 = _344 * _311;
   float _407 = _401 + _404;
   float _408 = _402 + _405;
   float _409 = _403 + _406;
   float _414 = (c_light_vec[0].x) + _145;
   float _415 = (c_light_vec[0].y) + _146;
   float _416 = (c_light_vec[0].z) + _147;
   float _417 = dot(float3(_414, _415, _416), float3(_414, _415, _416));
   float _418 = rsqrt(_417);
   float _419 = _414 * _418;
   float _420 = -0.0f - _419;
   float _421 = _415 * _418;
   float _422 = -0.0f - _421;
   float _423 = _416 * _418;
   float _424 = -0.0f - _423;
   float _425 = dot(float3(_420, _422, _424), float3(_140, _141, _142));
   float _426 = saturate(_425);
   float _427 = _146 + -0.30000001192092896f;
   float _428 = dot(float3(_145, _427, _147), float3(_145, _427, _147));
   float _429 = rsqrt(_428);
   float _430 = _145 * _429;
   float _431 = -0.0f - _430;
   float _432 = _427 * _429;
   float _433 = -0.0f - _432;
   float _434 = _147 * _429;
   float _435 = -0.0f - _434;
   float _436 = dot(float3(_431, _433, _435), float3(_140, _141, _142));
   float _437 = saturate(_436);
   float _440 = log2(_426);
   float _441 = log2(_437);
   float _442 = _440 * c_mat_power;
   float _443 = _441 * c_mat_power;
   float _444 = exp2(_442);
   float _445 = exp2(_443);
   float _448 = _444 * (c_light_col[0].x);
   float _449 = _444 * (c_light_col[0].y);
   float _450 = _444 * (c_light_col[0].z);
   float _451 = _445 * 2.0f;
   float _452 = _451 * (c_light_vec[5].x);
   float _453 = _452 * (c_light_col[1].x);
   float _454 = _452 * (c_light_col[1].y);
   float _455 = _452 * (c_light_col[1].z);
   float _456 = _448 * _271;
   float _457 = _449 * _272;
   float _458 = _450 * _273;
   float _459 = _453 * _356;
   float _460 = _454 * _356;
   float _461 = _455 * _356;
   float _462 = _459 + _456;
   float _463 = _460 + _457;
   float _464 = _461 + _458;
   float _471 = c_mat_specular.x * c_specular_scale.x;
   float _472 = _471 * _275;
   float _473 = _472 * _462;
   float _474 = c_mat_specular.y * c_specular_scale.y;
   float _475 = _474 * _277;
   float _476 = _475 * _463;
   float _477 = c_mat_specular.z * c_specular_scale.z;
   float _478 = _477 * _279;
   float _479 = _478 * _464;
   float _480 = _345 * _309;
   float _481 = _480 * _445;
   float _482 = _346 * _310;
   float _483 = _482 * _445;
   float _484 = _347 * _311;
   float _485 = _484 * _445;
   float _486 = _473 + _481;
   float _487 = _476 + _483;
   float _488 = _479 + _485;
   if (!_329)
   {
      float _490 = _259 * _241;
      float _491 = _259 * _242;
      float _492 = _259 * _243;
      float _493 = _407 + _490;
      float _494 = _408 + _491;
      float _495 = _409 + _492;
      float _496 = _259 * _244;
      float _497 = _486 + _496;
      float _498 = _487 + _496;
      float _499 = _488 + _496;
      _501 = _493;
      _502 = _494;
      _503 = _495;
      _504 = _497;
      _505 = _498;
      _506 = _499;
   }
   else
   {
      _501 = _407;
      _502 = _408;
      _503 = _409;
      _504 = _486;
      _505 = _487;
      _506 = _488;
   }
   int _507 = c_bool & 128;
   bool _508 = (_507 != 0);
   float _509 = select(_508, 0.0f, _504);
   float _510 = select(_508, 0.0f, _505);
   float _511 = select(_508, 0.0f, _506);
   float _512 = _501 * _102;
   float _513 = _502 * _103;
   float _514 = _503 * _104;
   float _515 = _509 * _98;
   float _516 = _510 * _98;
   float _517 = _511 * _98;
   int _518 = c_bool & 32768;
   bool _519 = (_518 == 0);
   if (!_519)
   {
      float _523 = (c_ps_user[11].x) * _102;
      float _524 = (c_ps_user[11].x) * _103;
      float _525 = (c_ps_user[11].x) * _104;
      float _527 = _140 + 1.0f;
      float _528 = _141 + 1.0f;
      float _529 = _142 + 1.0f;
      float _530 = _527 * 0.5f;
      float _531 = _528 * 0.5f;
      float _532 = _529 * 0.5f;
      float _533 = saturate(_530);
      float _534 = saturate(_531);
      float _535 = saturate(_532);
      float _536 = (c_ps_user[11].y) * _533;
      float _537 = _534 * (c_ps_user[11].y);
      float _538 = _535 * (c_ps_user[11].y);
      float _540 = (c_ps_user[11].z) * _98;
      float _542 = c_mat_power * 0.009999999776482582f;
      float _543 = _542 * (c_ps_user[11].w);
      float _546 = (c_ps_user[12].x) * _271;
      float _547 = (c_ps_user[12].x) * _272;
      float _548 = (c_ps_user[12].x) * _273;
      float _550 = (c_ps_user[12].y) * _275;
      float _551 = (c_ps_user[12].y) * _277;
      float _552 = (c_ps_user[12].y) * _279;
      float _554 = (c_ps_user[12].z) * _309;
      float _555 = (c_ps_user[12].z) * _310;
      float _556 = (c_ps_user[12].z) * _311;
      float _562 = -0.0f - _145;
      float _563 = -0.0f - _146;
      float _564 = -0.0f - _147;
      float _565 = dot(float3(_562, _563, _564), float3(_140, _141, _142));
      float _566 = 1.0f - _565;
      float _567 = _566 * _566;
      float _568 = _566 * 0.9722222089767456f;
      float _569 = _567 * _567;
      float _570 = _569 * _568;
      float _571 = _570 + 0.02777777425944805f;
      float _572 = _571 * (c_ps_user[13].z);
      float _574 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _575 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _576 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _579 = (c_ps_user[14].x) * _323;
      float _580 = (c_ps_user[14].x) * _325;
      float _581 = (c_ps_user[14].x) * _327;
      float _583 = (c_ps_user[14].y) * _362;
      float _584 = (c_ps_user[14].y) * _363;
      float _585 = (c_ps_user[14].y) * _364;
      float _587 = (c_ps_user[14].z) * _369;
      float _588 = (c_ps_user[14].z) * _370;
      float _589 = (c_ps_user[14].z) * _371;
      float _591 = (c_ps_user[14].w) * _342;
      float _592 = (c_ps_user[14].w) * _343;
      float _593 = (c_ps_user[14].w) * _344;
      float _596 = (c_ps_user[15].x) * _448;
      float _597 = (c_ps_user[15].x) * _449;
      float _598 = (c_ps_user[15].x) * _450;
      float _600 = _471 * _453;
      float _601 = _600 * (c_ps_user[15].y);
      float _602 = _474 * _454;
      float _603 = _602 * (c_ps_user[15].y);
      float _604 = _477 * _455;
      float _605 = _604 * (c_ps_user[15].y);
      float _607 = (c_ps_user[15].z) * _345;
      float _608 = (c_ps_user[15].z) * _346;
      float _609 = (c_ps_user[15].z) * _347;
      float _610 = _601 + _607;
      float _611 = _610 * _445;
      float _612 = _583 + (c_ps_user[13].x);
      float _613 = _612 + _587;
      float _614 = c_diffuse_scale.x * _613;
      float _615 = _614 * c_mat_diffuse.x;
      float _616 = (c_ps_user[13].y) + _540;
      float _617 = _616 + _596;
      float _618 = c_mat_specular.x * _617;
      float _619 = _618 * c_specular_scale.x;
      float _620 = _536 + _523;
      float _621 = _620 + _543;
      float _622 = _621 + (c_ps_user[12].w);
      float _623 = _622 + _546;
      float _624 = _623 + _550;
      float _625 = _624 + _554;
      float _626 = _625 + _574;
      float _627 = _626 + _579;
      float _628 = _627 + _591;
      float _629 = _628 + _572;
      float _630 = _629 + _611;
      float _631 = _630 + _615;
      float _632 = _631 + _619;
      float _633 = _603 + _608;
      float _634 = _633 * _445;
      float _635 = _584 + (c_ps_user[13].x);
      float _636 = _635 + _588;
      float _637 = c_diffuse_scale.y * _636;
      float _638 = _637 * c_mat_diffuse.y;
      float _639 = _616 + _597;
      float _640 = c_mat_specular.y * _639;
      float _641 = _640 * c_specular_scale.y;
      float _642 = _537 + _524;
      float _643 = _642 + _543;
      float _644 = _643 + (c_ps_user[12].w);
      float _645 = _644 + _547;
      float _646 = _645 + _551;
      float _647 = _646 + _555;
      float _648 = _647 + _575;
      float _649 = _648 + _580;
      float _650 = _649 + _592;
      float _651 = _650 + _572;
      float _652 = _651 + _634;
      float _653 = _652 + _638;
      float _654 = _653 + _641;
      float _655 = _605 + _609;
      float _656 = _655 * _445;
      float _657 = _585 + (c_ps_user[13].x);
      float _658 = _657 + _589;
      float _659 = c_diffuse_scale.z * _658;
      float _660 = _659 * c_mat_diffuse.z;
      float _661 = _616 + _598;
      float _662 = c_mat_specular.z * _661;
      float _663 = _662 * c_specular_scale.z;
      float _664 = _543 + _525;
      float _665 = _664 + _538;
      float _666 = _665 + (c_ps_user[12].w);
      float _667 = _666 + _548;
      float _668 = _667 + _552;
      float _669 = _668 + _556;
      float _670 = _669 + _576;
      float _671 = _670 + _581;
      float _672 = _671 + _593;
      float _673 = _672 + _572;
      float _674 = _673 + _656;
      float _675 = _674 + _660;
      float _676 = _675 + _663;
      _678 = _632;
      _679 = _654;
      _680 = _676;
      _681 = 0.0f;
      _682 = 0.0f;
      _683 = 0.0f;
   }
   else
   {
      _678 = _512;
      _679 = _513;
      _680 = _514;
      _681 = _515;
      _682 = _516;
      _683 = _517;
   }
   float _684 = _681 + _678;
   float _685 = _682 + _679;
   float _686 = _683 + _680;
   float _687 = dot(float3(_145, _146, _147), float3(_140, _141, _142));
   float _688 = _687 * 2.0f;
   float _689 = _688 * _140;
   float _690 = _688 * _141;
   float _691 = _688 * _142;
   float _692 = _145 - _689;
   float _693 = _146 - _690;
   float _694 = _147 - _691;
   float _695 = -0.0f - _692;
   float4 _696 = s6c_t.Sample(s6c_s, float3(_695, _693, _694));
   float _703 = _186 * _185;
   float _704 = _155 + 0.75f;
   float _705 = saturate(_704);
   float _706 = _703 * _705;
   float _707 = max(_706, (c_light_vec[5].y));
   float _708 = max(_696.w, 0.003921568859368563f);
   float _709 = 0.25f / _708;
   float _710 = _709 * _696.x;
   float _711 = _709 * _696.y;
   float _712 = _709 * _696.z;
   float _713 = _71.x * TEXCOORD_2.w;
   float _714 = _713 * _96;
   float _715 = _714 * _89.x;
   float _716 = _707 * _715;
   float _717 = _716 * _203;
   float _718 = _710 * _710;
   float _719 = _718 * _717;
   float _720 = _716 * _204;
   float _721 = _711 * _711;
   float _722 = _721 * _720;
   float _723 = _716 * _205;
   float _724 = _712 * _712;
   float _725 = _724 * _723;
   float _726 = _684 + _719;
   float _727 = _685 + _722;
   float _728 = _686 + _725;
   float _735 = _58.w * COLOR.w;
   float _736 = _735 * c_mat_opacity;
   float _737 = _736 * c_opacity_scale;
   float _738 = _737 * c_global_opacity;
   int _741 = c_bool & 16;
   bool _742 = (_741 == 0);
   if (!_742)
   {
      float _746 = c_tone_control.x * _726;
      float _747 = c_tone_control.x * _727;
      float _748 = c_tone_control.x * _728;
      float _750 = log2(_746);
      float _751 = log2(_747);
      float _752 = log2(_748);
      float _753 = _750 * c_tone_control.y;
      float _754 = _751 * c_tone_control.y;
      float _755 = _752 * c_tone_control.y;
      float _756 = exp2(_753);
      float _757 = exp2(_754);
      float _758 = exp2(_755);
      _808 = _756;
      _809 = _757;
      _810 = _758;
   }
   else
   {
      int _760 = c_bool & 32768;
      bool _761 = (_760 == 0);
      if (_761)
      {
         float _765 = c_tone_control.x * _726;
         float _766 = c_tone_control.x * _727;
         float _767 = c_tone_control.x * _728;
         float _768 = _765 * 2.0f;
         float _769 = _766 * 2.0f;
         float _770 = _767 * 2.0f;
         float _771 = _765 * 0.30000001192092896f;
         float _772 = _766 * 0.30000001192092896f;
         float _773 = _767 * 0.30000001192092896f;
         float _774 = _771 + 0.05000000074505806f;
         float _775 = _772 + 0.05000000074505806f;
         float _776 = _773 + 0.05000000074505806f;
         float _777 = _774 * _768;
         float _778 = _775 * _769;
         float _779 = _776 * _770;
         float _780 = _777 + 0.004000000189989805f;
         float _781 = _778 + 0.004000000189989805f;
         float _782 = _779 + 0.004000000189989805f;
         float _783 = _771 + 0.5f;
         float _784 = _772 + 0.5f;
         float _785 = _773 + 0.5f;
         float _786 = _783 * _768;
         float _787 = _784 * _769;
         float _788 = _785 * _770;
         float _789 = _786 + 0.06000000238418579f;
         float _790 = _787 + 0.06000000238418579f;
         float _791 = _788 + 0.06000000238418579f;
         float _792 = _780 / _789;
         float _793 = _781 / _790;
         float _794 = _782 / _791;
         float _795 = _792 + -0.06666666269302368f;
         float _796 = _793 + -0.06666666269302368f;
         float _797 = _794 + -0.06666666269302368f;
         float _798 = _795 * 1.3790643215179443f;
         float _799 = _796 * 1.3790643215179443f;
         float _800 = _797 * 1.3790643215179443f;
         float _801 = YKR_MaterialCurve(_798, _765);
         float _802 = YKR_MaterialCurve(_799, _766);
         float _803 = YKR_MaterialCurve(_800, _767);
         float _804 = sqrt(_801);
         float _805 = sqrt(_802);
         float _806 = sqrt(_803);
         _808 = _804;
         _809 = _805;
         _810 = _806;
      }
      else
      {
         _808 = _726;
         _809 = _727;
         _810 = _728;
      }
   }
   int _811 = c_bool & 4;
   bool _812 = (_811 == 0);
   if (_812)
   {
      float _814 = saturate(COLOR_1.x);
      float _819 = (c_fog_param[0].x) - _808;
      float _820 = (c_fog_param[0].y) - _809;
      float _821 = (c_fog_param[0].z) - _810;
      float _822 = _819 * _814;
      float _823 = _820 * _814;
      float _824 = _821 * _814;
      _826 = _822;
      _827 = _823;
      _828 = _824;
   }
   else
   {
      _826 = COLOR_1.x;
      _827 = COLOR_1.y;
      _828 = COLOR_1.z;
   }
   float _829 = _826 + _808;
   float _830 = _827 + _809;
   float _831 = _828 + _810;
   float _834 = _738 - c_mat_punchthrough_ref;
   bool _835 = (_834 < 0.0f);
   if (_835)
      discard;
   float _841 = (c_ps_col_scl[0].x) * _829;
   float _842 = (c_ps_col_scl[0].y) * _830;
   float _843 = (c_ps_col_scl[0].z) * _831;
   float _844 = (c_ps_col_scl[0].w) * _738;
   SV_Target.x = _841;
   SV_Target.y = _842;
   SV_Target.z = _843;
   SV_Target.w = _844;
   int _847 = c_bool & 8;
   bool _848 = (_847 != 0);
   float _849 = select(_848, _829, _841);
   float _850 = select(_848, _830, _842);
   float _851 = select(_848, _831, _843);
   float _852 = select(_848, _738, _844);
   SV_Target.x = _849;
   SV_Target.y = _850;
   SV_Target.z = _851;
   SV_Target.w = _852;
   return SV_Target;
}
