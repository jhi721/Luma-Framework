// Yakuza Kiwami material r_o1du2t_m(vr)3d_m4d[ref] (0x6C300A94, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float4 TEXCOORD_1 : TEXCOORD1,
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
   float4 _59 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _64 = _59.x * _59.x;
   float _65 = _59.y * _59.y;
   float _66 = _59.z * _59.z;
   float _70 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _72 = s3_t.Sample(s3_s, float2(_70, _71));
   float _78 = (c_attribute_ps[0].z) * TEXCOORD.z;
   float _79 = (c_attribute_ps[0].w) * TEXCOORD.w;
   float4 _80 = s5_t.Sample(s5_s, float2(_78, _79));
   float4 _83 = s1_t.Sample(s1_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _87 = _83.x * _83.x;
   float _88 = _83.y * _83.y;
   float _89 = _83.z * _83.z;
   float4 _90 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _93 = 1.0f - _87;
   float _94 = 1.0f - _88;
   float _95 = _93 * COLOR_1.w;
   float _96 = _94 * COLOR_1.w;
   float _97 = 1.0f - _95;
   float _98 = 1.0f - _96;
   float _99 = _98 * _72.y;
   float _100 = 1.0f - _89;
   float _101 = _100 * COLOR_1.w;
   float _102 = 1.0f - _101;
   float _103 = _64 * _97;
   float _104 = _65 * _98;
   float _105 = _66 * _102;
   float _106 = _80.x * 2.0f;
   float _107 = _80.y * 2.0f;
   float _108 = _106 + -1.0f;
   float _109 = _107 + -1.0f;
   float _110 = dot(float2(_108, _109), float2(_108, _109));
   float _111 = 1.0f - _110;
   float _112 = saturate(_111);
   float _113 = sqrt(_112);
   float _114 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _115 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _116 = _114 - _115;
   float _117 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _118 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _119 = _117 - _118;
   float _120 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _121 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _122 = _120 - _121;
   float _123 = _108 * TEXCOORD_6.x;
   float _124 = _108 * TEXCOORD_6.y;
   float _125 = _108 * TEXCOORD_6.z;
   float _126 = _109 * TEXCOORD_6.w;
   float _127 = _126 * _116;
   float _128 = _126 * _119;
   float _129 = _126 * _122;
   float _130 = _127 + _123;
   float _131 = _128 + _124;
   float _132 = _129 + _125;
   float _133 = _113 * TEXCOORD_2.x;
   float _134 = _113 * TEXCOORD_2.y;
   float _135 = _113 * TEXCOORD_2.z;
   float _136 = _130 + _133;
   float _137 = _131 + _134;
   float _138 = _132 + _135;
   float _139 = dot(float3(_136, _137, _138), float3(_136, _137, _138));
   float _140 = rsqrt(_139);
   float _141 = _136 * _140;
   float _142 = _137 * _140;
   float _143 = _138 * _140;
   float _144 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _145 = rsqrt(_144);
   float _146 = _145 * TEXCOORD_3.x;
   float _147 = _145 * TEXCOORD_3.y;
   float _148 = _145 * TEXCOORD_3.z;
   float _153 = -0.0f - (c_light_vec[0].x);
   float _154 = -0.0f - (c_light_vec[0].y);
   float _155 = -0.0f - (c_light_vec[0].z);
   float _156 = dot(float3(_141, _142, _143), float3(_153, _154, _155));
   float _160 = c_rt_size.z * SV_Position.x;
   float _161 = c_rt_size.w * SV_Position.y;
   int _164 = c_bool & 8;
   bool _165 = (_164 == 0);
   float _172;
   float _173;
   float _174;
   float _186;
   float _187;
   float _242;
   float _243;
   float _244;
   float _245;
   float _259;
   float _260;
   float _306;
   float _307;
   float _308;
   float _343;
   float _344;
   float _345;
   float _346;
   float _347;
   float _348;
   float _502;
   float _503;
   float _504;
   float _505;
   float _506;
   float _507;
   float _679;
   float _680;
   float _681;
   float _682;
   float _683;
   float _684;
   float _808;
   float _809;
   float _810;
   float _826;
   float _827;
   float _828;
   if (!_165)
   {
      float4 _167 = s13_t.Sample(s13_s, float2(_160, _161));
      _172 = _167.x;
      _173 = _167.y;
      _174 = _167.w;
   }
   else
   {
      _172 = 1.0f;
      _173 = 1.0f;
      _174 = 1.0f;
   }
   if (!_165)
   {
      float _179 = (c_sdw_param[0].x) * _172;
      float _180 = (c_sdw_param[0].z) * _173;
      float _183 = _179 + (c_sdw_param[0].y);
      float _184 = _180 + (c_sdw_param[0].w);
      _186 = _183;
      _187 = _184;
   }
   else
   {
      _186 = _172;
      _187 = _173;
   }
   float _194 = (c_sdw_param[1].x) * _90.y;
   float _196 = _194 + (c_sdw_param[1].y);
   float _197 = _196 * _90.x;
   float _198 = 1.0f - (c_light_col[5].x);
   float _199 = 1.0f - (c_light_col[5].y);
   float _200 = 1.0f - (c_light_col[5].z);
   float _201 = _197 * _198;
   float _202 = _197 * _199;
   float _203 = _197 * _200;
   float _204 = _201 + (c_light_col[5].x);
   float _205 = _202 + (c_light_col[5].y);
   float _206 = _203 + (c_light_col[5].z);
   float _210 = c_rt_size.z * SV_Position.x;
   float _211 = c_rt_size.w * SV_Position.y;
   float4 _212 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_210, _211)));
   int _219 = c_bool & 16384;
   bool _220 = (_219 == 0);
   if (!_220)
   {
      float _222 = _212.x * 2.0f;
      float _223 = _212.y * 2.0f;
      float _224 = _212.z * 2.0f;
      float _225 = _212.w * 2.0f;
      _242 = _222;
      _243 = _223;
      _244 = _224;
      _245 = _225;
   }
   else
   {
      int _227 = c_bool & 8192;
      bool _228 = (_227 == 0);
      if (!_228)
      {
         float _230 = _212.x * 2.0f;
         float _231 = _212.y * 2.0f;
         float _232 = _212.z * 2.0f;
         float _233 = _212.w * 2.0f;
         _242 = _230;
         _243 = _231;
         _244 = _232;
         _245 = _233;
      }
      else
      {
         float _237 = _212.x / c_tone_control.z;
         float _238 = _212.y / c_tone_control.z;
         float _239 = _212.z / c_tone_control.z;
         float _240 = _212.w / c_tone_control.z;
         _242 = _237;
         _243 = _238;
         _244 = _239;
         _245 = _240;
      }
   }
   int _246 = c_bool & 8;
   bool _247 = (_246 == 0);
   if (!_247)
   {
      float _252 = (c_sdw_param[0].x) * _186;
      float _253 = (c_sdw_param[0].z) * _187;
      float _256 = _252 + (c_sdw_param[0].y);
      float _257 = _253 + (c_sdw_param[0].w);
      _259 = _256;
      _260 = _257;
   }
   else
   {
      _259 = _186;
      _260 = _187;
   }
   float _261 = _260 * _259;
   float _266 = 1.0f - (c_light_col[5].x);
   float _267 = 1.0f - (c_light_col[5].y);
   float _268 = 1.0f - (c_light_col[5].z);
   float _269 = _266 * _261;
   float _270 = _267 * _261;
   float _271 = _268 * _261;
   float _272 = _269 + (c_light_col[5].x);
   float _273 = _270 + (c_light_col[5].y);
   float _274 = _271 + (c_light_col[5].z);
   float _275 = _174 * COLOR.x;
   float _276 = _275 * _204;
   float _277 = _174 * COLOR.y;
   float _278 = _277 * _205;
   float _279 = _174 * COLOR.z;
   float _280 = _279 * _206;
   float _281 = _272 * _204;
   float _282 = _273 * _205;
   float _283 = _274 * _206;
   if (!_247)
   {
      float _287 = (c_sdw_param[2].x) * _272;
      float _288 = (c_sdw_param[2].x) * _273;
      float _289 = (c_sdw_param[2].x) * _274;
      float _291 = _287 + (c_sdw_param[2].y);
      float _292 = _288 + (c_sdw_param[2].y);
      float _293 = _289 + (c_sdw_param[2].y);
      float _295 = (c_sdw_param[2].z) * _204;
      float _296 = (c_sdw_param[2].z) * _205;
      float _297 = (c_sdw_param[2].z) * _206;
      float _299 = _295 + (c_sdw_param[2].w);
      float _300 = _296 + (c_sdw_param[2].w);
      float _301 = _297 + (c_sdw_param[2].w);
      float _302 = _299 * _291;
      float _303 = _300 * _292;
      float _304 = _301 * _293;
      _306 = _302;
      _307 = _303;
      _308 = _304;
   }
   else
   {
      _306 = _281;
      _307 = _282;
      _308 = _283;
   }
   float _309 = _174 * _90.x;
   float _310 = _306 * _309;
   float _311 = _307 * _309;
   float _312 = _308 * _309;
   float _323 = c_mat_emissive.x * _103;
   float _324 = _323 * c_emissive_scale;
   float _325 = c_mat_emissive.y * _104;
   float _326 = _325 * c_emissive_scale;
   float _327 = c_mat_emissive.z * _105;
   float _328 = _327 * c_emissive_scale;
   int _329 = c_bool & 2048;
   bool _330 = (_329 == 0);
   if (_330)
   {
      float _332 = _260 * _242;
      float _333 = _260 * _243;
      float _334 = _260 * _244;
      float _335 = _332 + TEXCOORD_4.x;
      float _336 = _333 + TEXCOORD_4.y;
      float _337 = _334 + TEXCOORD_4.z;
      float _338 = _260 * _245;
      float _339 = _338 + TEXCOORD_5.x;
      float _340 = _338 + TEXCOORD_5.y;
      float _341 = _338 + TEXCOORD_5.z;
      _343 = _335;
      _344 = _336;
      _345 = _337;
      _346 = _339;
      _347 = _340;
      _348 = _341;
   }
   else
   {
      _343 = TEXCOORD_4.x;
      _344 = TEXCOORD_4.y;
      _345 = TEXCOORD_4.z;
      _346 = TEXCOORD_5.x;
      _347 = TEXCOORD_5.y;
      _348 = TEXCOORD_5.z;
   }
   float _353 = -0.0f - (c_light_vec[1].x);
   float _354 = -0.0f - (c_light_vec[1].y);
   float _355 = -0.0f - (c_light_vec[1].z);
   float _356 = dot(float3(_141, _142, _143), float3(_353, _354, _355));
   float _357 = saturate(_356);
   float _362 = saturate(_156);
   float _363 = (c_light_col[0].x) * _362;
   float _364 = (c_light_col[0].y) * _362;
   float _365 = (c_light_col[0].z) * _362;
   float _370 = (c_light_col[1].x) * _357;
   float _371 = (c_light_col[1].y) * _357;
   float _372 = (c_light_col[1].z) * _357;
   float _381 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _382 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _383 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _384 = _363 * _272;
   float _385 = _364 * _273;
   float _386 = _365 * _274;
   float _387 = _370 * _260;
   float _388 = _371 * _260;
   float _389 = _372 * _260;
   float _390 = _387 + _384;
   float _391 = _388 + _385;
   float _392 = _389 + _386;
   float _393 = _381 * _390;
   float _394 = _382 * _391;
   float _395 = _383 * _392;
   float _396 = _324 + c_scene_ambient.x;
   float _397 = _396 + _393;
   float _398 = _326 + c_scene_ambient.y;
   float _399 = _398 + _394;
   float _400 = _328 + c_scene_ambient.z;
   float _401 = _400 + _395;
   float _402 = _397 * _276;
   float _403 = _399 * _278;
   float _404 = _401 * _280;
   float _405 = _343 * _310;
   float _406 = _344 * _311;
   float _407 = _345 * _312;
   float _408 = _402 + _405;
   float _409 = _403 + _406;
   float _410 = _404 + _407;
   float _415 = (c_light_vec[0].x) + _146;
   float _416 = (c_light_vec[0].y) + _147;
   float _417 = (c_light_vec[0].z) + _148;
   float _418 = dot(float3(_415, _416, _417), float3(_415, _416, _417));
   float _419 = rsqrt(_418);
   float _420 = _415 * _419;
   float _421 = -0.0f - _420;
   float _422 = _416 * _419;
   float _423 = -0.0f - _422;
   float _424 = _417 * _419;
   float _425 = -0.0f - _424;
   float _426 = dot(float3(_421, _423, _425), float3(_141, _142, _143));
   float _427 = saturate(_426);
   float _428 = _147 + -0.30000001192092896f;
   float _429 = dot(float3(_146, _428, _148), float3(_146, _428, _148));
   float _430 = rsqrt(_429);
   float _431 = _146 * _430;
   float _432 = -0.0f - _431;
   float _433 = _428 * _430;
   float _434 = -0.0f - _433;
   float _435 = _148 * _430;
   float _436 = -0.0f - _435;
   float _437 = dot(float3(_432, _434, _436), float3(_141, _142, _143));
   float _438 = saturate(_437);
   float _441 = log2(_427);
   float _442 = log2(_438);
   float _443 = _441 * c_mat_power;
   float _444 = _442 * c_mat_power;
   float _445 = exp2(_443);
   float _446 = exp2(_444);
   float _449 = _445 * (c_light_col[0].x);
   float _450 = _445 * (c_light_col[0].y);
   float _451 = _445 * (c_light_col[0].z);
   float _452 = _446 * 2.0f;
   float _453 = _452 * (c_light_vec[5].x);
   float _454 = _453 * (c_light_col[1].x);
   float _455 = _453 * (c_light_col[1].y);
   float _456 = _453 * (c_light_col[1].z);
   float _457 = _449 * _272;
   float _458 = _450 * _273;
   float _459 = _451 * _274;
   float _460 = _454 * _357;
   float _461 = _455 * _357;
   float _462 = _456 * _357;
   float _463 = _460 + _457;
   float _464 = _461 + _458;
   float _465 = _462 + _459;
   float _472 = c_mat_specular.x * c_specular_scale.x;
   float _473 = _472 * _276;
   float _474 = _473 * _463;
   float _475 = c_mat_specular.y * c_specular_scale.y;
   float _476 = _475 * _278;
   float _477 = _476 * _464;
   float _478 = c_mat_specular.z * c_specular_scale.z;
   float _479 = _478 * _280;
   float _480 = _479 * _465;
   float _481 = _346 * _310;
   float _482 = _481 * _446;
   float _483 = _347 * _311;
   float _484 = _483 * _446;
   float _485 = _348 * _312;
   float _486 = _485 * _446;
   float _487 = _474 + _482;
   float _488 = _477 + _484;
   float _489 = _480 + _486;
   if (!_330)
   {
      float _491 = _260 * _242;
      float _492 = _260 * _243;
      float _493 = _260 * _244;
      float _494 = _408 + _491;
      float _495 = _409 + _492;
      float _496 = _410 + _493;
      float _497 = _260 * _245;
      float _498 = _487 + _497;
      float _499 = _488 + _497;
      float _500 = _489 + _497;
      _502 = _494;
      _503 = _495;
      _504 = _496;
      _505 = _498;
      _506 = _499;
      _507 = _500;
   }
   else
   {
      _502 = _408;
      _503 = _409;
      _504 = _410;
      _505 = _487;
      _506 = _488;
      _507 = _489;
   }
   int _508 = c_bool & 128;
   bool _509 = (_508 != 0);
   float _510 = select(_509, 0.0f, _505);
   float _511 = select(_509, 0.0f, _506);
   float _512 = select(_509, 0.0f, _507);
   float _513 = _502 * _103;
   float _514 = _503 * _104;
   float _515 = _504 * _105;
   float _516 = _510 * _99;
   float _517 = _511 * _99;
   float _518 = _512 * _99;
   int _519 = c_bool & 32768;
   bool _520 = (_519 == 0);
   if (!_520)
   {
      float _524 = (c_ps_user[11].x) * _103;
      float _525 = (c_ps_user[11].x) * _104;
      float _526 = (c_ps_user[11].x) * _105;
      float _528 = _141 + 1.0f;
      float _529 = _142 + 1.0f;
      float _530 = _143 + 1.0f;
      float _531 = _528 * 0.5f;
      float _532 = _529 * 0.5f;
      float _533 = _530 * 0.5f;
      float _534 = saturate(_531);
      float _535 = saturate(_532);
      float _536 = saturate(_533);
      float _537 = (c_ps_user[11].y) * _534;
      float _538 = _535 * (c_ps_user[11].y);
      float _539 = _536 * (c_ps_user[11].y);
      float _541 = (c_ps_user[11].z) * _99;
      float _543 = c_mat_power * 0.009999999776482582f;
      float _544 = _543 * (c_ps_user[11].w);
      float _547 = (c_ps_user[12].x) * _272;
      float _548 = (c_ps_user[12].x) * _273;
      float _549 = (c_ps_user[12].x) * _274;
      float _551 = (c_ps_user[12].y) * _276;
      float _552 = (c_ps_user[12].y) * _278;
      float _553 = (c_ps_user[12].y) * _280;
      float _555 = (c_ps_user[12].z) * _310;
      float _556 = (c_ps_user[12].z) * _311;
      float _557 = (c_ps_user[12].z) * _312;
      float _563 = -0.0f - _146;
      float _564 = -0.0f - _147;
      float _565 = -0.0f - _148;
      float _566 = dot(float3(_563, _564, _565), float3(_141, _142, _143));
      float _567 = 1.0f - _566;
      float _568 = _567 * _567;
      float _569 = _567 * 0.9722222089767456f;
      float _570 = _568 * _568;
      float _571 = _570 * _569;
      float _572 = _571 + 0.02777777425944805f;
      float _573 = _572 * (c_ps_user[13].z);
      float _575 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _576 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _577 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _580 = (c_ps_user[14].x) * _324;
      float _581 = (c_ps_user[14].x) * _326;
      float _582 = (c_ps_user[14].x) * _328;
      float _584 = (c_ps_user[14].y) * _363;
      float _585 = (c_ps_user[14].y) * _364;
      float _586 = (c_ps_user[14].y) * _365;
      float _588 = (c_ps_user[14].z) * _370;
      float _589 = (c_ps_user[14].z) * _371;
      float _590 = (c_ps_user[14].z) * _372;
      float _592 = (c_ps_user[14].w) * _343;
      float _593 = (c_ps_user[14].w) * _344;
      float _594 = (c_ps_user[14].w) * _345;
      float _597 = (c_ps_user[15].x) * _449;
      float _598 = (c_ps_user[15].x) * _450;
      float _599 = (c_ps_user[15].x) * _451;
      float _601 = _472 * _454;
      float _602 = _601 * (c_ps_user[15].y);
      float _603 = _475 * _455;
      float _604 = _603 * (c_ps_user[15].y);
      float _605 = _478 * _456;
      float _606 = _605 * (c_ps_user[15].y);
      float _608 = (c_ps_user[15].z) * _346;
      float _609 = (c_ps_user[15].z) * _347;
      float _610 = (c_ps_user[15].z) * _348;
      float _611 = _602 + _608;
      float _612 = _611 * _446;
      float _613 = _584 + (c_ps_user[13].x);
      float _614 = _613 + _588;
      float _615 = c_diffuse_scale.x * _614;
      float _616 = _615 * c_mat_diffuse.x;
      float _617 = (c_ps_user[13].y) + _541;
      float _618 = _617 + _597;
      float _619 = c_mat_specular.x * _618;
      float _620 = _619 * c_specular_scale.x;
      float _621 = _537 + _524;
      float _622 = _621 + _544;
      float _623 = _622 + (c_ps_user[12].w);
      float _624 = _623 + _547;
      float _625 = _624 + _551;
      float _626 = _625 + _555;
      float _627 = _626 + _575;
      float _628 = _627 + _580;
      float _629 = _628 + _592;
      float _630 = _629 + _573;
      float _631 = _630 + _612;
      float _632 = _631 + _616;
      float _633 = _632 + _620;
      float _634 = _604 + _609;
      float _635 = _634 * _446;
      float _636 = _585 + (c_ps_user[13].x);
      float _637 = _636 + _589;
      float _638 = c_diffuse_scale.y * _637;
      float _639 = _638 * c_mat_diffuse.y;
      float _640 = _617 + _598;
      float _641 = c_mat_specular.y * _640;
      float _642 = _641 * c_specular_scale.y;
      float _643 = _538 + _525;
      float _644 = _643 + _544;
      float _645 = _644 + (c_ps_user[12].w);
      float _646 = _645 + _548;
      float _647 = _646 + _552;
      float _648 = _647 + _556;
      float _649 = _648 + _576;
      float _650 = _649 + _581;
      float _651 = _650 + _593;
      float _652 = _651 + _573;
      float _653 = _652 + _635;
      float _654 = _653 + _639;
      float _655 = _654 + _642;
      float _656 = _606 + _610;
      float _657 = _656 * _446;
      float _658 = _586 + (c_ps_user[13].x);
      float _659 = _658 + _590;
      float _660 = c_diffuse_scale.z * _659;
      float _661 = _660 * c_mat_diffuse.z;
      float _662 = _617 + _599;
      float _663 = c_mat_specular.z * _662;
      float _664 = _663 * c_specular_scale.z;
      float _665 = _544 + _526;
      float _666 = _665 + _539;
      float _667 = _666 + (c_ps_user[12].w);
      float _668 = _667 + _549;
      float _669 = _668 + _553;
      float _670 = _669 + _557;
      float _671 = _670 + _577;
      float _672 = _671 + _582;
      float _673 = _672 + _594;
      float _674 = _673 + _573;
      float _675 = _674 + _657;
      float _676 = _675 + _661;
      float _677 = _676 + _664;
      _679 = _633;
      _680 = _655;
      _681 = _677;
      _682 = 0.0f;
      _683 = 0.0f;
      _684 = 0.0f;
   }
   else
   {
      _679 = _513;
      _680 = _514;
      _681 = _515;
      _682 = _516;
      _683 = _517;
      _684 = _518;
   }
   float _685 = _682 + _679;
   float _686 = _683 + _680;
   float _687 = _684 + _681;
   float _688 = dot(float3(_146, _147, _148), float3(_141, _142, _143));
   float _689 = _688 * 2.0f;
   float _690 = _689 * _141;
   float _691 = _689 * _142;
   float _692 = _689 * _143;
   float _693 = _146 - _690;
   float _694 = _147 - _691;
   float _695 = _148 - _692;
   float _696 = -0.0f - _693;
   float4 _697 = s6c_t.Sample(s6c_s, float3(_696, _694, _695));
   float _704 = _187 * _186;
   float _705 = _156 + 0.75f;
   float _706 = saturate(_705);
   float _707 = _704 * _706;
   float _708 = max(_707, (c_light_vec[5].y));
   float _709 = max(_697.w, 0.003921568859368563f);
   float _710 = 0.25f / _709;
   float _711 = _710 * _697.x;
   float _712 = _710 * _697.y;
   float _713 = _710 * _697.z;
   float _714 = _72.x * TEXCOORD_2.w;
   float _715 = _714 * _97;
   float _716 = _715 * _90.x;
   float _717 = _708 * _716;
   float _718 = _717 * _204;
   float _719 = _711 * _711;
   float _720 = _719 * _718;
   float _721 = _717 * _205;
   float _722 = _712 * _712;
   float _723 = _722 * _721;
   float _724 = _717 * _206;
   float _725 = _713 * _713;
   float _726 = _725 * _724;
   float _727 = _685 + _720;
   float _728 = _686 + _723;
   float _729 = _687 + _726;
   float _736 = c_mat_opacity * _59.w;
   float _737 = _736 * c_opacity_scale;
   float _738 = _737 * c_global_opacity;
   int _741 = c_bool & 16;
   bool _742 = (_741 == 0);
   if (!_742)
   {
      float _746 = c_tone_control.x * _727;
      float _747 = c_tone_control.x * _728;
      float _748 = c_tone_control.x * _729;
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
         float _765 = c_tone_control.x * _727;
         float _766 = c_tone_control.x * _728;
         float _767 = c_tone_control.x * _729;
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
         _808 = _727;
         _809 = _728;
         _810 = _729;
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
   float _837 = (c_ps_col_scl[0].x) * _829;
   float _838 = (c_ps_col_scl[0].y) * _830;
   float _839 = (c_ps_col_scl[0].z) * _831;
   float _840 = (c_ps_col_scl[0].w) * _738;
   SV_Target.x = _837;
   SV_Target.y = _838;
   SV_Target.z = _839;
   SV_Target.w = _840;
   int _843 = c_bool & 8;
   bool _844 = (_843 != 0);
   float _845 = select(_844, _829, _837);
   float _846 = select(_844, _830, _838);
   float _847 = select(_844, _831, _839);
   float _848 = select(_844, _738, _840);
   SV_Target.x = _845;
   SV_Target.y = _846;
   SV_Target.z = _847;
   SV_Target.w = _848;
   return SV_Target;
}
