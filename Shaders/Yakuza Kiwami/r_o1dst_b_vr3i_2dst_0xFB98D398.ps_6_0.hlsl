// Yakuza Kiwami material r_o1dst_b(vr3i)2dst (0xFB98D398, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s2_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

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
   float4 _58 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _63 = _58.x * _58.x;
   float _64 = _58.y * _58.y;
   float _65 = _58.z * _58.z;
   float _69 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _70 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _71 = s3_t.Sample(s3_s, float2(_69, _70));
   float _78 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _79 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _80 = s5_t.Sample(s5_s, float2(_78, _79));
   float4 _83 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _88 = _83.x * _83.x;
   float _89 = _83.y * _83.y;
   float _90 = _83.z * _83.z;
   float _94 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _95 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _96 = s4_t.Sample(s4_s, float2(_94, _95));
   float _103 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _104 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _105 = s6_t.Sample(s6_s, float2(_103, _104));
   float4 _108 = s2_t.Sample(s2_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _110 = _83.w * COLOR_1.w;
   float _111 = _110 * _108.y;
   float _112 = _88 - _63;
   float _113 = _89 - _64;
   float _114 = _90 - _65;
   float _115 = _111 * _112;
   float _116 = _111 * _113;
   float _117 = _111 * _114;
   float _118 = _115 + _63;
   float _119 = _116 + _64;
   float _120 = _117 + _65;
   float _121 = _96.x - _71.x;
   float _122 = _96.y - _71.y;
   float _123 = _96.z - _71.z;
   float _124 = _111 * _121;
   float _125 = _111 * _122;
   float _126 = _111 * _123;
   float _127 = _124 + _71.x;
   float _128 = _125 + _71.y;
   float _129 = _126 + _71.z;
   float _130 = _105.x - _80.x;
   float _131 = _105.y - _80.y;
   float _132 = _111 * _130;
   float _133 = _111 * _131;
   float _134 = _132 + _80.x;
   float _135 = _133 + _80.y;
   float _136 = _134 * 2.0f;
   float _137 = _135 * 2.0f;
   float _138 = _136 + -1.0f;
   float _139 = _137 + -1.0f;
   float _140 = dot(float2(_138, _139), float2(_138, _139));
   float _141 = 1.0f - _140;
   float _142 = saturate(_141);
   float _143 = sqrt(_142);
   float _144 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _145 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _146 = _144 - _145;
   float _147 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _148 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _149 = _147 - _148;
   float _150 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _151 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _152 = _150 - _151;
   float _153 = _138 * TEXCOORD_6.x;
   float _154 = _138 * TEXCOORD_6.y;
   float _155 = _138 * TEXCOORD_6.z;
   float _156 = _139 * TEXCOORD_6.w;
   float _157 = _156 * _146;
   float _158 = _156 * _149;
   float _159 = _156 * _152;
   float _160 = _143 * TEXCOORD_2.x;
   float _161 = _143 * TEXCOORD_2.y;
   float _162 = _143 * TEXCOORD_2.z;
   float _163 = _153 + _160;
   float _164 = _163 + _157;
   float _165 = _154 + _161;
   float _166 = _165 + _158;
   float _167 = _155 + _162;
   float _168 = _167 + _159;
   float _169 = dot(float3(_164, _166, _168), float3(_164, _166, _168));
   float _170 = rsqrt(_169);
   float _171 = _164 * _170;
   float _172 = _166 * _170;
   float _173 = _168 * _170;
   float _174 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _175 = rsqrt(_174);
   float _176 = _175 * TEXCOORD_3.x;
   float _177 = _175 * TEXCOORD_3.y;
   float _178 = _175 * TEXCOORD_3.z;
   float _183 = -0.0f - (c_light_vec[0].x);
   float _184 = -0.0f - (c_light_vec[0].y);
   float _185 = -0.0f - (c_light_vec[0].z);
   float _186 = dot(float3(_171, _172, _173), float3(_183, _184, _185));
   float _190 = c_rt_size.z * SV_Position.x;
   float _191 = c_rt_size.w * SV_Position.y;
   int _194 = c_bool & 8;
   bool _195 = (_194 == 0);
   float _202;
   float _203;
   float _204;
   float _216;
   float _217;
   float _253;
   float _254;
   float _255;
   float _256;
   float _270;
   float _271;
   float _306;
   float _307;
   float _308;
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
   float _682;
   float _683;
   float _684;
   float _685;
   float _686;
   float _687;
   float _763;
   float _764;
   float _765;
   float _781;
   float _782;
   float _783;
   if (!_195)
   {
      float4 _197 = s13_t.Sample(s13_s, float2(_190, _191));
      _202 = _197.x;
      _203 = _197.y;
      _204 = _197.w;
   }
   else
   {
      _202 = 1.0f;
      _203 = 1.0f;
      _204 = 1.0f;
   }
   if (!_195)
   {
      float _209 = (c_sdw_param[0].x) * _202;
      float _210 = (c_sdw_param[0].z) * _203;
      float _213 = _209 + (c_sdw_param[0].y);
      float _214 = _210 + (c_sdw_param[0].w);
      _216 = _213;
      _217 = _214;
   }
   else
   {
      _216 = _202;
      _217 = _203;
   }
   float _221 = c_rt_size.z * SV_Position.x;
   float _222 = c_rt_size.w * SV_Position.y;
   float4 _223 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_221, _222)));
   int _230 = c_bool & 16384;
   bool _231 = (_230 == 0);
   if (!_231)
   {
      float _233 = _223.x * 2.0f;
      float _234 = _223.y * 2.0f;
      float _235 = _223.z * 2.0f;
      float _236 = _223.w * 2.0f;
      _253 = _233;
      _254 = _234;
      _255 = _235;
      _256 = _236;
   }
   else
   {
      int _238 = c_bool & 8192;
      bool _239 = (_238 == 0);
      if (!_239)
      {
         float _241 = _223.x * 2.0f;
         float _242 = _223.y * 2.0f;
         float _243 = _223.z * 2.0f;
         float _244 = _223.w * 2.0f;
         _253 = _241;
         _254 = _242;
         _255 = _243;
         _256 = _244;
      }
      else
      {
         float _248 = _223.x / c_tone_control.z;
         float _249 = _223.y / c_tone_control.z;
         float _250 = _223.z / c_tone_control.z;
         float _251 = _223.w / c_tone_control.z;
         _253 = _248;
         _254 = _249;
         _255 = _250;
         _256 = _251;
      }
   }
   int _257 = c_bool & 8;
   bool _258 = (_257 == 0);
   if (!_258)
   {
      float _263 = (c_sdw_param[0].x) * _216;
      float _264 = (c_sdw_param[0].z) * _217;
      float _267 = _263 + (c_sdw_param[0].y);
      float _268 = _264 + (c_sdw_param[0].w);
      _270 = _267;
      _271 = _268;
   }
   else
   {
      _270 = _216;
      _271 = _217;
   }
   float _272 = _271 * _270;
   float _277 = 1.0f - (c_light_col[5].x);
   float _278 = 1.0f - (c_light_col[5].y);
   float _279 = 1.0f - (c_light_col[5].z);
   float _280 = _277 * _272;
   float _281 = _278 * _272;
   float _282 = _279 * _272;
   float _283 = _280 + (c_light_col[5].x);
   float _284 = _281 + (c_light_col[5].y);
   float _285 = _282 + (c_light_col[5].z);
   float _286 = _204 * COLOR.x;
   float _287 = _204 * COLOR.y;
   float _288 = _204 * COLOR.z;
   if (!_258)
   {
      float _292 = (c_sdw_param[2].x) * _283;
      float _293 = (c_sdw_param[2].x) * _284;
      float _294 = (c_sdw_param[2].x) * _285;
      float _296 = _292 + (c_sdw_param[2].y);
      float _297 = _293 + (c_sdw_param[2].y);
      float _298 = _294 + (c_sdw_param[2].y);
      float _301 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _302 = _296 * _301;
      float _303 = _297 * _301;
      float _304 = _298 * _301;
      _306 = _302;
      _307 = _303;
      _308 = _304;
   }
   else
   {
      _306 = _283;
      _307 = _284;
      _308 = _285;
   }
   float _309 = _306 * _204;
   float _310 = _307 * _204;
   float _311 = _308 * _204;
   float _322 = c_mat_emissive.x * _118;
   float _323 = _322 * c_emissive_scale;
   float _324 = c_mat_emissive.y * _119;
   float _325 = _324 * c_emissive_scale;
   float _326 = c_mat_emissive.z * _120;
   float _327 = _326 * c_emissive_scale;
   int _328 = c_bool & 2048;
   bool _329 = (_328 == 0);
   if (_329)
   {
      float _331 = _271 * _253;
      float _332 = _271 * _254;
      float _333 = _271 * _255;
      float _334 = _331 + TEXCOORD_4.x;
      float _335 = _332 + TEXCOORD_4.y;
      float _336 = _333 + TEXCOORD_4.z;
      float _337 = _271 * _256;
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
   float _355 = dot(float3(_171, _172, _173), float3(_352, _353, _354));
   float _356 = saturate(_355);
   float _361 = saturate(_186);
   float _362 = (c_light_col[0].x) * _361;
   float _363 = (c_light_col[0].y) * _361;
   float _364 = (c_light_col[0].z) * _361;
   float _369 = (c_light_col[1].x) * _356;
   float _370 = (c_light_col[1].y) * _356;
   float _371 = (c_light_col[1].z) * _356;
   float _380 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _381 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _382 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _383 = _362 * _283;
   float _384 = _363 * _284;
   float _385 = _364 * _285;
   float _386 = _369 * _271;
   float _387 = _370 * _271;
   float _388 = _371 * _271;
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
   float _401 = _396 * _286;
   float _402 = _398 * _287;
   float _403 = _400 * _288;
   float _404 = _342 * _309;
   float _405 = _343 * _310;
   float _406 = _344 * _311;
   float _407 = _401 + _404;
   float _408 = _402 + _405;
   float _409 = _403 + _406;
   float _414 = (c_light_vec[0].x) + _176;
   float _415 = (c_light_vec[0].y) + _177;
   float _416 = (c_light_vec[0].z) + _178;
   float _417 = dot(float3(_414, _415, _416), float3(_414, _415, _416));
   float _418 = rsqrt(_417);
   float _419 = _414 * _418;
   float _420 = -0.0f - _419;
   float _421 = _415 * _418;
   float _422 = -0.0f - _421;
   float _423 = _416 * _418;
   float _424 = -0.0f - _423;
   float _425 = dot(float3(_420, _422, _424), float3(_171, _172, _173));
   float _426 = saturate(_425);
   float _427 = _177 + -0.30000001192092896f;
   float _428 = dot(float3(_176, _427, _178), float3(_176, _427, _178));
   float _429 = rsqrt(_428);
   float _430 = _176 * _429;
   float _431 = -0.0f - _430;
   float _432 = _427 * _429;
   float _433 = -0.0f - _432;
   float _434 = _178 * _429;
   float _435 = -0.0f - _434;
   float _436 = dot(float3(_431, _433, _435), float3(_171, _172, _173));
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
   float _456 = _448 * _283;
   float _457 = _449 * _284;
   float _458 = _450 * _285;
   float _459 = _453 * _356;
   float _460 = _454 * _356;
   float _461 = _455 * _356;
   float _462 = _459 + _456;
   float _463 = _460 + _457;
   float _464 = _461 + _458;
   float _471 = c_mat_specular.x * c_specular_scale.x;
   float _472 = _471 * _286;
   float _473 = _472 * _462;
   float _474 = c_mat_specular.y * c_specular_scale.y;
   float _475 = _474 * _287;
   float _476 = _475 * _463;
   float _477 = c_mat_specular.z * c_specular_scale.z;
   float _478 = _477 * _288;
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
      float _490 = _271 * _253;
      float _491 = _271 * _254;
      float _492 = _271 * _255;
      float _493 = _407 + _490;
      float _494 = _408 + _491;
      float _495 = _409 + _492;
      float _496 = _271 * _256;
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
   float _512 = _501 * _118;
   float _513 = _502 * _119;
   float _514 = _503 * _120;
   float _515 = _509 * _127;
   float _516 = _510 * _128;
   float _517 = _511 * _129;
   int _518 = c_bool & 32768;
   bool _519 = (_518 == 0);
   if (!_519)
   {
      float _523 = (c_ps_user[11].x) * _118;
      float _524 = (c_ps_user[11].x) * _119;
      float _525 = (c_ps_user[11].x) * _120;
      float _527 = _171 + 1.0f;
      float _528 = _172 + 1.0f;
      float _529 = _173 + 1.0f;
      float _530 = _527 * 0.5f;
      float _531 = _528 * 0.5f;
      float _532 = _529 * 0.5f;
      float _533 = saturate(_530);
      float _534 = saturate(_531);
      float _535 = saturate(_532);
      float _536 = (c_ps_user[11].y) * _533;
      float _537 = _534 * (c_ps_user[11].y);
      float _538 = _535 * (c_ps_user[11].y);
      float _540 = (c_ps_user[11].z) * _127;
      float _541 = (c_ps_user[11].z) * _128;
      float _542 = (c_ps_user[11].z) * _129;
      float _544 = c_mat_power * 0.009999999776482582f;
      float _545 = _544 * (c_ps_user[11].w);
      float _548 = (c_ps_user[12].x) * _283;
      float _549 = (c_ps_user[12].x) * _284;
      float _550 = (c_ps_user[12].x) * _285;
      float _552 = (c_ps_user[12].y) * _286;
      float _553 = (c_ps_user[12].y) * _287;
      float _554 = (c_ps_user[12].y) * _288;
      float _556 = (c_ps_user[12].z) * _309;
      float _557 = (c_ps_user[12].z) * _310;
      float _558 = (c_ps_user[12].z) * _311;
      float _564 = -0.0f - _176;
      float _565 = -0.0f - _177;
      float _566 = -0.0f - _178;
      float _567 = dot(float3(_564, _565, _566), float3(_171, _172, _173));
      float _568 = 1.0f - _567;
      float _569 = _568 * _568;
      float _570 = _568 * 0.9722222089767456f;
      float _571 = _569 * _569;
      float _572 = _571 * _570;
      float _573 = _572 + 0.02777777425944805f;
      float _574 = _573 * (c_ps_user[13].z);
      float _576 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _577 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _578 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _581 = (c_ps_user[14].x) * _323;
      float _582 = (c_ps_user[14].x) * _325;
      float _583 = (c_ps_user[14].x) * _327;
      float _585 = (c_ps_user[14].y) * _362;
      float _586 = (c_ps_user[14].y) * _363;
      float _587 = (c_ps_user[14].y) * _364;
      float _589 = (c_ps_user[14].z) * _369;
      float _590 = (c_ps_user[14].z) * _370;
      float _591 = (c_ps_user[14].z) * _371;
      float _593 = (c_ps_user[14].w) * _342;
      float _594 = (c_ps_user[14].w) * _343;
      float _595 = (c_ps_user[14].w) * _344;
      float _598 = (c_ps_user[15].x) * _448;
      float _599 = (c_ps_user[15].x) * _449;
      float _600 = (c_ps_user[15].x) * _450;
      float _602 = _471 * _453;
      float _603 = _602 * (c_ps_user[15].y);
      float _604 = _474 * _454;
      float _605 = _604 * (c_ps_user[15].y);
      float _606 = _477 * _455;
      float _607 = _606 * (c_ps_user[15].y);
      float _609 = (c_ps_user[15].z) * _345;
      float _610 = (c_ps_user[15].z) * _346;
      float _611 = (c_ps_user[15].z) * _347;
      float _612 = _603 + _609;
      float _613 = _612 * _445;
      float _614 = _585 + (c_ps_user[13].x);
      float _615 = _614 + _589;
      float _616 = c_diffuse_scale.x * _615;
      float _617 = _616 * c_mat_diffuse.x;
      float _618 = (c_ps_user[13].y) + _540;
      float _619 = _618 + _598;
      float _620 = c_mat_specular.x * _619;
      float _621 = _620 * c_specular_scale.x;
      float _622 = _536 + _523;
      float _623 = _622 + _545;
      float _624 = _623 + (c_ps_user[12].w);
      float _625 = _624 + _548;
      float _626 = _625 + _552;
      float _627 = _626 + _556;
      float _628 = _627 + _576;
      float _629 = _628 + _581;
      float _630 = _629 + _593;
      float _631 = _630 + _574;
      float _632 = _631 + _613;
      float _633 = _632 + _617;
      float _634 = _633 + _621;
      float _635 = _605 + _610;
      float _636 = _635 * _445;
      float _637 = _586 + (c_ps_user[13].x);
      float _638 = _637 + _590;
      float _639 = c_diffuse_scale.y * _638;
      float _640 = _639 * c_mat_diffuse.y;
      float _641 = (c_ps_user[13].y) + _541;
      float _642 = _641 + _599;
      float _643 = c_mat_specular.y * _642;
      float _644 = _643 * c_specular_scale.y;
      float _645 = _537 + _524;
      float _646 = _645 + _545;
      float _647 = _646 + (c_ps_user[12].w);
      float _648 = _647 + _549;
      float _649 = _648 + _553;
      float _650 = _649 + _557;
      float _651 = _650 + _577;
      float _652 = _651 + _582;
      float _653 = _652 + _594;
      float _654 = _653 + _574;
      float _655 = _654 + _636;
      float _656 = _655 + _640;
      float _657 = _656 + _644;
      float _658 = _607 + _611;
      float _659 = _658 * _445;
      float _660 = _587 + (c_ps_user[13].x);
      float _661 = _660 + _591;
      float _662 = c_diffuse_scale.z * _661;
      float _663 = _662 * c_mat_diffuse.z;
      float _664 = (c_ps_user[13].y) + _542;
      float _665 = _664 + _600;
      float _666 = c_mat_specular.z * _665;
      float _667 = _666 * c_specular_scale.z;
      float _668 = _545 + _525;
      float _669 = _668 + _538;
      float _670 = _669 + (c_ps_user[12].w);
      float _671 = _670 + _550;
      float _672 = _671 + _554;
      float _673 = _672 + _558;
      float _674 = _673 + _578;
      float _675 = _674 + _583;
      float _676 = _675 + _595;
      float _677 = _676 + _574;
      float _678 = _677 + _659;
      float _679 = _678 + _663;
      float _680 = _679 + _667;
      _682 = _634;
      _683 = _657;
      _684 = _680;
      _685 = 0.0f;
      _686 = 0.0f;
      _687 = 0.0f;
   }
   else
   {
      _682 = _512;
      _683 = _513;
      _684 = _514;
      _685 = _515;
      _686 = _516;
      _687 = _517;
   }
   float _688 = _685 + _682;
   float _689 = _686 + _683;
   float _690 = _687 + _684;
   float _695 = c_mat_opacity * _58.w;
   float _696 = _695 * c_global_opacity;
   float _697 = _696 * c_opacity_scale;
   int _698 = c_bool & 16;
   bool _699 = (_698 == 0);
   if (!_699)
   {
      float _703 = c_tone_control.x * _688;
      float _704 = c_tone_control.x * _689;
      float _705 = c_tone_control.x * _690;
      float _707 = log2(_703);
      float _708 = log2(_704);
      float _709 = log2(_705);
      float _710 = _707 * c_tone_control.y;
      float _711 = _708 * c_tone_control.y;
      float _712 = _709 * c_tone_control.y;
      float _713 = exp2(_710);
      float _714 = exp2(_711);
      float _715 = exp2(_712);
      _763 = _713;
      _764 = _714;
      _765 = _715;
   }
   else
   {
      if (_519)
      {
         float _720 = c_tone_control.x * _688;
         float _721 = c_tone_control.x * _689;
         float _722 = c_tone_control.x * _690;
         float _723 = _720 * 2.0f;
         float _724 = _721 * 2.0f;
         float _725 = _722 * 2.0f;
         float _726 = _720 * 0.30000001192092896f;
         float _727 = _721 * 0.30000001192092896f;
         float _728 = _722 * 0.30000001192092896f;
         float _729 = _726 + 0.05000000074505806f;
         float _730 = _727 + 0.05000000074505806f;
         float _731 = _728 + 0.05000000074505806f;
         float _732 = _729 * _723;
         float _733 = _730 * _724;
         float _734 = _731 * _725;
         float _735 = _732 + 0.004000000189989805f;
         float _736 = _733 + 0.004000000189989805f;
         float _737 = _734 + 0.004000000189989805f;
         float _738 = _726 + 0.5f;
         float _739 = _727 + 0.5f;
         float _740 = _728 + 0.5f;
         float _741 = _738 * _723;
         float _742 = _739 * _724;
         float _743 = _740 * _725;
         float _744 = _741 + 0.06000000238418579f;
         float _745 = _742 + 0.06000000238418579f;
         float _746 = _743 + 0.06000000238418579f;
         float _747 = _735 / _744;
         float _748 = _736 / _745;
         float _749 = _737 / _746;
         float _750 = _747 + -0.06666666269302368f;
         float _751 = _748 + -0.06666666269302368f;
         float _752 = _749 + -0.06666666269302368f;
         float _753 = _750 * 1.3790643215179443f;
         float _754 = _751 * 1.3790643215179443f;
         float _755 = _752 * 1.3790643215179443f;
         float _756 = YKR_MaterialCurve(_753, _720);
         float _757 = YKR_MaterialCurve(_754, _721);
         float _758 = YKR_MaterialCurve(_755, _722);
         float _759 = sqrt(_756);
         float _760 = sqrt(_757);
         float _761 = sqrt(_758);
         _763 = _759;
         _764 = _760;
         _765 = _761;
      }
      else
      {
         _763 = _688;
         _764 = _689;
         _765 = _690;
      }
   }
   int _766 = c_bool & 4;
   bool _767 = (_766 == 0);
   if (_767)
   {
      float _769 = saturate(COLOR_1.x);
      float _774 = (c_fog_param[0].x) - _763;
      float _775 = (c_fog_param[0].y) - _764;
      float _776 = (c_fog_param[0].z) - _765;
      float _777 = _774 * _769;
      float _778 = _775 * _769;
      float _779 = _776 * _769;
      _781 = _777;
      _782 = _778;
      _783 = _779;
   }
   else
   {
      _781 = COLOR_1.x;
      _782 = COLOR_1.y;
      _783 = COLOR_1.z;
   }
   float _784 = _781 + _763;
   float _785 = _782 + _764;
   float _786 = _783 + _765;
   float _792 = (c_ps_col_scl[0].x) * _784;
   float _793 = (c_ps_col_scl[0].y) * _785;
   float _794 = (c_ps_col_scl[0].z) * _786;
   float _795 = (c_ps_col_scl[0].w) * _697;
   SV_Target.x = _792;
   SV_Target.y = _793;
   SV_Target.z = _794;
   SV_Target.w = _795;
   int _798 = c_bool & 8;
   bool _799 = (_798 != 0);
   float _800 = select(_799, _784, _792);
   float _801 = select(_799, _785, _793);
   float _802 = select(_799, _786, _794);
   float _803 = select(_799, _697, _795);
   SV_Target.x = _800;
   SV_Target.y = _801;
   SV_Target.z = _802;
   SV_Target.w = _803;
   return SV_Target;
}
