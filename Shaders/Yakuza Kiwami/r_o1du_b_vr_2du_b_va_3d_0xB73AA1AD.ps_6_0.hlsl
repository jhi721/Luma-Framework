// Yakuza Kiwami material r_o1du_b(vr)2du_b(va)3d (0xB73AA1AD, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _51 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _51.x * _51.x;
   float _57 = _51.y * _51.y;
   float _58 = _51.z * _51.z;
   float _62 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _63 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _64 = s3_t.Sample(s3_s, float2(_62, _63));
   float4 _66 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _71 = _66.x * _66.x;
   float _72 = _66.y * _66.y;
   float _73 = _66.z * _66.z;
   float _77 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _78 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _79 = s4_t.Sample(s4_s, float2(_77, _78));
   float4 _81 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _85 = _81.x * _81.x;
   float _86 = _81.y * _81.y;
   float _87 = _81.z * _81.z;
   float _88 = _66.w * COLOR_1.w;
   float _89 = _79.y - _64.y;
   float _90 = _89 * _88;
   float _91 = _90 + _64.y;
   float _92 = _71 - _56;
   float _93 = _72 - _57;
   float _94 = _73 - _58;
   float _95 = _92 * _88;
   float _96 = _93 * _88;
   float _97 = _94 * _88;
   float _98 = _95 + _56;
   float _99 = _96 + _57;
   float _100 = _97 + _58;
   float _101 = _85 - _98;
   float _102 = _86 - _99;
   float _103 = _87 - _100;
   float _104 = _101 * COLOR.w;
   float _105 = _102 * COLOR.w;
   float _106 = _103 * COLOR.w;
   float _107 = _104 + _98;
   float _108 = _105 + _99;
   float _109 = _106 + _100;
   float _110 = _91 * COLOR.w;
   float _111 = _91 - _110;
   float _112 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _113 = rsqrt(_112);
   float _114 = _113 * TEXCOORD_2.x;
   float _115 = _113 * TEXCOORD_2.y;
   float _116 = _113 * TEXCOORD_2.z;
   float _117 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _118 = rsqrt(_117);
   float _119 = _118 * TEXCOORD_3.x;
   float _120 = _118 * TEXCOORD_3.y;
   float _121 = _118 * TEXCOORD_3.z;
   float _126 = -0.0f - (c_light_vec[0].x);
   float _127 = -0.0f - (c_light_vec[0].y);
   float _128 = -0.0f - (c_light_vec[0].z);
   float _129 = dot(float3(_114, _115, _116), float3(_126, _127, _128));
   float _133 = c_rt_size.z * SV_Position.x;
   float _134 = c_rt_size.w * SV_Position.y;
   int _137 = c_bool & 8;
   bool _138 = (_137 == 0);
   float _145;
   float _146;
   float _147;
   float _159;
   float _160;
   float _196;
   float _197;
   float _198;
   float _199;
   float _213;
   float _214;
   float _249;
   float _250;
   float _251;
   float _285;
   float _286;
   float _287;
   float _288;
   float _289;
   float _290;
   float _444;
   float _445;
   float _446;
   float _447;
   float _448;
   float _449;
   float _621;
   float _622;
   float _623;
   float _624;
   float _625;
   float _626;
   float _702;
   float _703;
   float _704;
   float _720;
   float _721;
   float _722;
   if (!_138)
   {
      float4 _140 = s13_t.Sample(s13_s, float2(_133, _134));
      _145 = _140.x;
      _146 = _140.y;
      _147 = _140.w;
   }
   else
   {
      _145 = 1.0f;
      _146 = 1.0f;
      _147 = 1.0f;
   }
   if (!_138)
   {
      float _152 = (c_sdw_param[0].x) * _145;
      float _153 = (c_sdw_param[0].z) * _146;
      float _156 = _152 + (c_sdw_param[0].y);
      float _157 = _153 + (c_sdw_param[0].w);
      _159 = _156;
      _160 = _157;
   }
   else
   {
      _159 = _145;
      _160 = _146;
   }
   float _164 = c_rt_size.z * SV_Position.x;
   float _165 = c_rt_size.w * SV_Position.y;
   float4 _166 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_164, _165)));
   int _173 = c_bool & 16384;
   bool _174 = (_173 == 0);
   if (!_174)
   {
      float _176 = _166.x * 2.0f;
      float _177 = _166.y * 2.0f;
      float _178 = _166.z * 2.0f;
      float _179 = _166.w * 2.0f;
      _196 = _176;
      _197 = _177;
      _198 = _178;
      _199 = _179;
   }
   else
   {
      int _181 = c_bool & 8192;
      bool _182 = (_181 == 0);
      if (!_182)
      {
         float _184 = _166.x * 2.0f;
         float _185 = _166.y * 2.0f;
         float _186 = _166.z * 2.0f;
         float _187 = _166.w * 2.0f;
         _196 = _184;
         _197 = _185;
         _198 = _186;
         _199 = _187;
      }
      else
      {
         float _191 = _166.x / c_tone_control.z;
         float _192 = _166.y / c_tone_control.z;
         float _193 = _166.z / c_tone_control.z;
         float _194 = _166.w / c_tone_control.z;
         _196 = _191;
         _197 = _192;
         _198 = _193;
         _199 = _194;
      }
   }
   int _200 = c_bool & 8;
   bool _201 = (_200 == 0);
   if (!_201)
   {
      float _206 = (c_sdw_param[0].x) * _159;
      float _207 = (c_sdw_param[0].z) * _160;
      float _210 = _206 + (c_sdw_param[0].y);
      float _211 = _207 + (c_sdw_param[0].w);
      _213 = _210;
      _214 = _211;
   }
   else
   {
      _213 = _159;
      _214 = _160;
   }
   float _215 = _214 * _213;
   float _220 = 1.0f - (c_light_col[5].x);
   float _221 = 1.0f - (c_light_col[5].y);
   float _222 = 1.0f - (c_light_col[5].z);
   float _223 = _220 * _215;
   float _224 = _221 * _215;
   float _225 = _222 * _215;
   float _226 = _223 + (c_light_col[5].x);
   float _227 = _224 + (c_light_col[5].y);
   float _228 = _225 + (c_light_col[5].z);
   float _229 = _147 * COLOR.x;
   float _230 = _147 * COLOR.y;
   float _231 = _147 * COLOR.z;
   if (!_201)
   {
      float _235 = (c_sdw_param[2].x) * _226;
      float _236 = (c_sdw_param[2].x) * _227;
      float _237 = (c_sdw_param[2].x) * _228;
      float _239 = _235 + (c_sdw_param[2].y);
      float _240 = _236 + (c_sdw_param[2].y);
      float _241 = _237 + (c_sdw_param[2].y);
      float _244 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _245 = _239 * _244;
      float _246 = _240 * _244;
      float _247 = _241 * _244;
      _249 = _245;
      _250 = _246;
      _251 = _247;
   }
   else
   {
      _249 = _226;
      _250 = _227;
      _251 = _228;
   }
   float _252 = _249 * _147;
   float _253 = _250 * _147;
   float _254 = _251 * _147;
   float _265 = c_mat_emissive.x * _107;
   float _266 = _265 * c_emissive_scale;
   float _267 = c_mat_emissive.y * _108;
   float _268 = _267 * c_emissive_scale;
   float _269 = c_mat_emissive.z * _109;
   float _270 = _269 * c_emissive_scale;
   int _271 = c_bool & 2048;
   bool _272 = (_271 == 0);
   if (_272)
   {
      float _274 = _214 * _196;
      float _275 = _214 * _197;
      float _276 = _214 * _198;
      float _277 = _274 + TEXCOORD_4.x;
      float _278 = _275 + TEXCOORD_4.y;
      float _279 = _276 + TEXCOORD_4.z;
      float _280 = _214 * _199;
      float _281 = _280 + TEXCOORD_5.x;
      float _282 = _280 + TEXCOORD_5.y;
      float _283 = _280 + TEXCOORD_5.z;
      _285 = _277;
      _286 = _278;
      _287 = _279;
      _288 = _281;
      _289 = _282;
      _290 = _283;
   }
   else
   {
      _285 = TEXCOORD_4.x;
      _286 = TEXCOORD_4.y;
      _287 = TEXCOORD_4.z;
      _288 = TEXCOORD_5.x;
      _289 = TEXCOORD_5.y;
      _290 = TEXCOORD_5.z;
   }
   float _295 = -0.0f - (c_light_vec[1].x);
   float _296 = -0.0f - (c_light_vec[1].y);
   float _297 = -0.0f - (c_light_vec[1].z);
   float _298 = dot(float3(_114, _115, _116), float3(_295, _296, _297));
   float _299 = saturate(_298);
   float _304 = saturate(_129);
   float _305 = (c_light_col[0].x) * _304;
   float _306 = (c_light_col[0].y) * _304;
   float _307 = (c_light_col[0].z) * _304;
   float _312 = (c_light_col[1].x) * _299;
   float _313 = (c_light_col[1].y) * _299;
   float _314 = (c_light_col[1].z) * _299;
   float _323 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _324 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _325 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _326 = _305 * _226;
   float _327 = _306 * _227;
   float _328 = _307 * _228;
   float _329 = _312 * _214;
   float _330 = _313 * _214;
   float _331 = _314 * _214;
   float _332 = _329 + _326;
   float _333 = _330 + _327;
   float _334 = _331 + _328;
   float _335 = _323 * _332;
   float _336 = _324 * _333;
   float _337 = _325 * _334;
   float _338 = _266 + c_scene_ambient.x;
   float _339 = _338 + _335;
   float _340 = _268 + c_scene_ambient.y;
   float _341 = _340 + _336;
   float _342 = _270 + c_scene_ambient.z;
   float _343 = _342 + _337;
   float _344 = _339 * _229;
   float _345 = _341 * _230;
   float _346 = _343 * _231;
   float _347 = _285 * _252;
   float _348 = _286 * _253;
   float _349 = _287 * _254;
   float _350 = _344 + _347;
   float _351 = _345 + _348;
   float _352 = _346 + _349;
   float _357 = (c_light_vec[0].x) + _119;
   float _358 = (c_light_vec[0].y) + _120;
   float _359 = (c_light_vec[0].z) + _121;
   float _360 = dot(float3(_357, _358, _359), float3(_357, _358, _359));
   float _361 = rsqrt(_360);
   float _362 = _357 * _361;
   float _363 = -0.0f - _362;
   float _364 = _358 * _361;
   float _365 = -0.0f - _364;
   float _366 = _359 * _361;
   float _367 = -0.0f - _366;
   float _368 = dot(float3(_363, _365, _367), float3(_114, _115, _116));
   float _369 = saturate(_368);
   float _370 = _120 + -0.30000001192092896f;
   float _371 = dot(float3(_119, _370, _121), float3(_119, _370, _121));
   float _372 = rsqrt(_371);
   float _373 = _119 * _372;
   float _374 = -0.0f - _373;
   float _375 = _370 * _372;
   float _376 = -0.0f - _375;
   float _377 = _121 * _372;
   float _378 = -0.0f - _377;
   float _379 = dot(float3(_374, _376, _378), float3(_114, _115, _116));
   float _380 = saturate(_379);
   float _383 = log2(_369);
   float _384 = log2(_380);
   float _385 = _383 * c_mat_power;
   float _386 = _384 * c_mat_power;
   float _387 = exp2(_385);
   float _388 = exp2(_386);
   float _391 = _387 * (c_light_col[0].x);
   float _392 = _387 * (c_light_col[0].y);
   float _393 = _387 * (c_light_col[0].z);
   float _394 = _388 * 2.0f;
   float _395 = _394 * (c_light_vec[5].x);
   float _396 = _395 * (c_light_col[1].x);
   float _397 = _395 * (c_light_col[1].y);
   float _398 = _395 * (c_light_col[1].z);
   float _399 = _391 * _226;
   float _400 = _392 * _227;
   float _401 = _393 * _228;
   float _402 = _396 * _299;
   float _403 = _397 * _299;
   float _404 = _398 * _299;
   float _405 = _402 + _399;
   float _406 = _403 + _400;
   float _407 = _404 + _401;
   float _414 = c_mat_specular.x * c_specular_scale.x;
   float _415 = _414 * _229;
   float _416 = _415 * _405;
   float _417 = c_mat_specular.y * c_specular_scale.y;
   float _418 = _417 * _230;
   float _419 = _418 * _406;
   float _420 = c_mat_specular.z * c_specular_scale.z;
   float _421 = _420 * _231;
   float _422 = _421 * _407;
   float _423 = _288 * _252;
   float _424 = _423 * _388;
   float _425 = _289 * _253;
   float _426 = _425 * _388;
   float _427 = _290 * _254;
   float _428 = _427 * _388;
   float _429 = _416 + _424;
   float _430 = _419 + _426;
   float _431 = _422 + _428;
   if (!_272)
   {
      float _433 = _214 * _196;
      float _434 = _214 * _197;
      float _435 = _214 * _198;
      float _436 = _350 + _433;
      float _437 = _351 + _434;
      float _438 = _352 + _435;
      float _439 = _214 * _199;
      float _440 = _429 + _439;
      float _441 = _430 + _439;
      float _442 = _431 + _439;
      _444 = _436;
      _445 = _437;
      _446 = _438;
      _447 = _440;
      _448 = _441;
      _449 = _442;
   }
   else
   {
      _444 = _350;
      _445 = _351;
      _446 = _352;
      _447 = _429;
      _448 = _430;
      _449 = _431;
   }
   int _450 = c_bool & 128;
   bool _451 = (_450 != 0);
   float _452 = select(_451, 0.0f, _447);
   float _453 = select(_451, 0.0f, _448);
   float _454 = select(_451, 0.0f, _449);
   float _455 = _444 * _107;
   float _456 = _445 * _108;
   float _457 = _446 * _109;
   float _458 = _452 * _111;
   float _459 = _453 * _111;
   float _460 = _454 * _111;
   int _461 = c_bool & 32768;
   bool _462 = (_461 == 0);
   if (!_462)
   {
      float _466 = (c_ps_user[11].x) * _107;
      float _467 = (c_ps_user[11].x) * _108;
      float _468 = (c_ps_user[11].x) * _109;
      float _470 = _114 + 1.0f;
      float _471 = _115 + 1.0f;
      float _472 = _116 + 1.0f;
      float _473 = _470 * 0.5f;
      float _474 = _471 * 0.5f;
      float _475 = _472 * 0.5f;
      float _476 = saturate(_473);
      float _477 = saturate(_474);
      float _478 = saturate(_475);
      float _479 = (c_ps_user[11].y) * _476;
      float _480 = _477 * (c_ps_user[11].y);
      float _481 = _478 * (c_ps_user[11].y);
      float _483 = (c_ps_user[11].z) * _111;
      float _485 = c_mat_power * 0.009999999776482582f;
      float _486 = _485 * (c_ps_user[11].w);
      float _489 = (c_ps_user[12].x) * _226;
      float _490 = (c_ps_user[12].x) * _227;
      float _491 = (c_ps_user[12].x) * _228;
      float _493 = (c_ps_user[12].y) * _229;
      float _494 = (c_ps_user[12].y) * _230;
      float _495 = (c_ps_user[12].y) * _231;
      float _497 = (c_ps_user[12].z) * _252;
      float _498 = (c_ps_user[12].z) * _253;
      float _499 = (c_ps_user[12].z) * _254;
      float _505 = -0.0f - _119;
      float _506 = -0.0f - _120;
      float _507 = -0.0f - _121;
      float _508 = dot(float3(_505, _506, _507), float3(_114, _115, _116));
      float _509 = 1.0f - _508;
      float _510 = _509 * _509;
      float _511 = _509 * 0.9722222089767456f;
      float _512 = _510 * _510;
      float _513 = _512 * _511;
      float _514 = _513 + 0.02777777425944805f;
      float _515 = _514 * (c_ps_user[13].z);
      float _517 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _518 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _519 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _522 = (c_ps_user[14].x) * _266;
      float _523 = (c_ps_user[14].x) * _268;
      float _524 = (c_ps_user[14].x) * _270;
      float _526 = (c_ps_user[14].y) * _305;
      float _527 = (c_ps_user[14].y) * _306;
      float _528 = (c_ps_user[14].y) * _307;
      float _530 = (c_ps_user[14].z) * _312;
      float _531 = (c_ps_user[14].z) * _313;
      float _532 = (c_ps_user[14].z) * _314;
      float _534 = (c_ps_user[14].w) * _285;
      float _535 = (c_ps_user[14].w) * _286;
      float _536 = (c_ps_user[14].w) * _287;
      float _539 = (c_ps_user[15].x) * _391;
      float _540 = (c_ps_user[15].x) * _392;
      float _541 = (c_ps_user[15].x) * _393;
      float _543 = _414 * _396;
      float _544 = _543 * (c_ps_user[15].y);
      float _545 = _417 * _397;
      float _546 = _545 * (c_ps_user[15].y);
      float _547 = _420 * _398;
      float _548 = _547 * (c_ps_user[15].y);
      float _550 = (c_ps_user[15].z) * _288;
      float _551 = (c_ps_user[15].z) * _289;
      float _552 = (c_ps_user[15].z) * _290;
      float _553 = _544 + _550;
      float _554 = _553 * _388;
      float _555 = _526 + (c_ps_user[13].x);
      float _556 = _555 + _530;
      float _557 = c_diffuse_scale.x * _556;
      float _558 = _557 * c_mat_diffuse.x;
      float _559 = (c_ps_user[13].y) + _483;
      float _560 = _559 + _539;
      float _561 = c_mat_specular.x * _560;
      float _562 = _561 * c_specular_scale.x;
      float _563 = _479 + _466;
      float _564 = _563 + _486;
      float _565 = _564 + (c_ps_user[12].w);
      float _566 = _565 + _489;
      float _567 = _566 + _493;
      float _568 = _567 + _497;
      float _569 = _568 + _517;
      float _570 = _569 + _522;
      float _571 = _570 + _534;
      float _572 = _571 + _515;
      float _573 = _572 + _554;
      float _574 = _573 + _558;
      float _575 = _574 + _562;
      float _576 = _546 + _551;
      float _577 = _576 * _388;
      float _578 = _527 + (c_ps_user[13].x);
      float _579 = _578 + _531;
      float _580 = c_diffuse_scale.y * _579;
      float _581 = _580 * c_mat_diffuse.y;
      float _582 = _559 + _540;
      float _583 = c_mat_specular.y * _582;
      float _584 = _583 * c_specular_scale.y;
      float _585 = _480 + _467;
      float _586 = _585 + _486;
      float _587 = _586 + (c_ps_user[12].w);
      float _588 = _587 + _490;
      float _589 = _588 + _494;
      float _590 = _589 + _498;
      float _591 = _590 + _518;
      float _592 = _591 + _523;
      float _593 = _592 + _535;
      float _594 = _593 + _515;
      float _595 = _594 + _577;
      float _596 = _595 + _581;
      float _597 = _596 + _584;
      float _598 = _548 + _552;
      float _599 = _598 * _388;
      float _600 = _528 + (c_ps_user[13].x);
      float _601 = _600 + _532;
      float _602 = c_diffuse_scale.z * _601;
      float _603 = _602 * c_mat_diffuse.z;
      float _604 = _559 + _541;
      float _605 = c_mat_specular.z * _604;
      float _606 = _605 * c_specular_scale.z;
      float _607 = _486 + _468;
      float _608 = _607 + _481;
      float _609 = _608 + (c_ps_user[12].w);
      float _610 = _609 + _491;
      float _611 = _610 + _495;
      float _612 = _611 + _499;
      float _613 = _612 + _519;
      float _614 = _613 + _524;
      float _615 = _614 + _536;
      float _616 = _615 + _515;
      float _617 = _616 + _599;
      float _618 = _617 + _603;
      float _619 = _618 + _606;
      _621 = _575;
      _622 = _597;
      _623 = _619;
      _624 = 0.0f;
      _625 = 0.0f;
      _626 = 0.0f;
   }
   else
   {
      _621 = _455;
      _622 = _456;
      _623 = _457;
      _624 = _458;
      _625 = _459;
      _626 = _460;
   }
   float _627 = _624 + _621;
   float _628 = _625 + _622;
   float _629 = _626 + _623;
   float _634 = c_mat_opacity * _51.w;
   float _635 = _634 * c_global_opacity;
   float _636 = _635 * c_opacity_scale;
   int _637 = c_bool & 16;
   bool _638 = (_637 == 0);
   if (!_638)
   {
      float _642 = c_tone_control.x * _627;
      float _643 = c_tone_control.x * _628;
      float _644 = c_tone_control.x * _629;
      float _646 = log2(_642);
      float _647 = log2(_643);
      float _648 = log2(_644);
      float _649 = _646 * c_tone_control.y;
      float _650 = _647 * c_tone_control.y;
      float _651 = _648 * c_tone_control.y;
      float _652 = exp2(_649);
      float _653 = exp2(_650);
      float _654 = exp2(_651);
      _702 = _652;
      _703 = _653;
      _704 = _654;
   }
   else
   {
      if (_462)
      {
         float _659 = c_tone_control.x * _627;
         float _660 = c_tone_control.x * _628;
         float _661 = c_tone_control.x * _629;
         float _662 = _659 * 2.0f;
         float _663 = _660 * 2.0f;
         float _664 = _661 * 2.0f;
         float _665 = _659 * 0.30000001192092896f;
         float _666 = _660 * 0.30000001192092896f;
         float _667 = _661 * 0.30000001192092896f;
         float _668 = _665 + 0.05000000074505806f;
         float _669 = _666 + 0.05000000074505806f;
         float _670 = _667 + 0.05000000074505806f;
         float _671 = _668 * _662;
         float _672 = _669 * _663;
         float _673 = _670 * _664;
         float _674 = _671 + 0.004000000189989805f;
         float _675 = _672 + 0.004000000189989805f;
         float _676 = _673 + 0.004000000189989805f;
         float _677 = _665 + 0.5f;
         float _678 = _666 + 0.5f;
         float _679 = _667 + 0.5f;
         float _680 = _677 * _662;
         float _681 = _678 * _663;
         float _682 = _679 * _664;
         float _683 = _680 + 0.06000000238418579f;
         float _684 = _681 + 0.06000000238418579f;
         float _685 = _682 + 0.06000000238418579f;
         float _686 = _674 / _683;
         float _687 = _675 / _684;
         float _688 = _676 / _685;
         float _689 = _686 + -0.06666666269302368f;
         float _690 = _687 + -0.06666666269302368f;
         float _691 = _688 + -0.06666666269302368f;
         float _692 = _689 * 1.3790643215179443f;
         float _693 = _690 * 1.3790643215179443f;
         float _694 = _691 * 1.3790643215179443f;
         float _695 = YKR_MaterialCurve(_692, _659);
         float _696 = YKR_MaterialCurve(_693, _660);
         float _697 = YKR_MaterialCurve(_694, _661);
         float _698 = sqrt(_695);
         float _699 = sqrt(_696);
         float _700 = sqrt(_697);
         _702 = _698;
         _703 = _699;
         _704 = _700;
      }
      else
      {
         _702 = _627;
         _703 = _628;
         _704 = _629;
      }
   }
   int _705 = c_bool & 4;
   bool _706 = (_705 == 0);
   if (_706)
   {
      float _708 = saturate(COLOR_1.x);
      float _713 = (c_fog_param[0].x) - _702;
      float _714 = (c_fog_param[0].y) - _703;
      float _715 = (c_fog_param[0].z) - _704;
      float _716 = _713 * _708;
      float _717 = _714 * _708;
      float _718 = _715 * _708;
      _720 = _716;
      _721 = _717;
      _722 = _718;
   }
   else
   {
      _720 = COLOR_1.x;
      _721 = COLOR_1.y;
      _722 = COLOR_1.z;
   }
   float _723 = _720 + _702;
   float _724 = _721 + _703;
   float _725 = _722 + _704;
   float _731 = (c_ps_col_scl[0].x) * _723;
   float _732 = (c_ps_col_scl[0].y) * _724;
   float _733 = (c_ps_col_scl[0].z) * _725;
   float _734 = (c_ps_col_scl[0].w) * _636;
   SV_Target.x = _731;
   SV_Target.y = _732;
   SV_Target.z = _733;
   SV_Target.w = _734;
   int _737 = c_bool & 8;
   bool _738 = (_737 != 0);
   float _739 = select(_738, _723, _731);
   float _740 = select(_738, _724, _732);
   float _741 = select(_738, _725, _733);
   float _742 = select(_738, _636, _734);
   SV_Target.x = _739;
   SV_Target.y = _740;
   SV_Target.z = _741;
   SV_Target.w = _742;
   return SV_Target;
}
