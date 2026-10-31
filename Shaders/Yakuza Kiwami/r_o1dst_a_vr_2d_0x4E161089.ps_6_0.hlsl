// Yakuza Kiwami material r_o1dst_a(vr)2d (0x4E161089, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

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
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _61 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _62 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _63 = s3_t.Sample(s3_s, float2(_61, _62));
   float _70 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _72 = s5_t.Sample(s5_s, float2(_70, _71));
   float4 _75 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _79 = _72.x * 2.0f;
   float _80 = _72.y * 2.0f;
   float _81 = _79 + -1.0f;
   float _82 = _80 + -1.0f;
   float _83 = dot(float2(_81, _82), float2(_81, _82));
   float _84 = 1.0f - _83;
   float _85 = saturate(_84);
   float _86 = sqrt(_85);
   float _87 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _88 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _89 = _87 - _88;
   float _90 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _91 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _92 = _90 - _91;
   float _93 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _94 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _95 = _93 - _94;
   float _96 = _81 * TEXCOORD_6.x;
   float _97 = _81 * TEXCOORD_6.y;
   float _98 = _81 * TEXCOORD_6.z;
   float _99 = _82 * TEXCOORD_6.w;
   float _100 = _99 * _89;
   float _101 = _99 * _92;
   float _102 = _99 * _95;
   float _103 = _100 + _96;
   float _104 = _101 + _97;
   float _105 = _102 + _98;
   float _106 = _86 * TEXCOORD_2.x;
   float _107 = _86 * TEXCOORD_2.y;
   float _108 = _86 * TEXCOORD_2.z;
   float _109 = _103 + _106;
   float _110 = _104 + _107;
   float _111 = _105 + _108;
   float _112 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _113 = rsqrt(_112);
   float _114 = _109 * _113;
   float _115 = _110 * _113;
   float _116 = _111 * _113;
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
   float _625;
   float _626;
   float _627;
   float _628;
   float _629;
   float _630;
   float _715;
   float _716;
   float _717;
   float _733;
   float _734;
   float _735;
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
   float _265 = c_mat_emissive.x * _55;
   float _266 = _265 * c_emissive_scale;
   float _267 = c_mat_emissive.y * _56;
   float _268 = _267 * c_emissive_scale;
   float _269 = c_mat_emissive.z * _57;
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
   float _455 = _444 * _55;
   float _456 = _445 * _56;
   float _457 = _446 * _57;
   float _458 = _452 * _63.x;
   float _459 = _453 * _63.y;
   float _460 = _454 * _63.z;
   int _461 = c_bool & 32768;
   bool _462 = (_461 == 0);
   if (!_462)
   {
      float _466 = (c_ps_user[11].x) * _55;
      float _467 = (c_ps_user[11].x) * _56;
      float _468 = (c_ps_user[11].x) * _57;
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
      float _483 = (c_ps_user[11].z) * _63.x;
      float _484 = (c_ps_user[11].z) * _63.y;
      float _485 = (c_ps_user[11].z) * _63.z;
      float _487 = c_mat_power * 0.009999999776482582f;
      float _488 = _487 * (c_ps_user[11].w);
      float _491 = (c_ps_user[12].x) * _226;
      float _492 = (c_ps_user[12].x) * _227;
      float _493 = (c_ps_user[12].x) * _228;
      float _495 = (c_ps_user[12].y) * _229;
      float _496 = (c_ps_user[12].y) * _230;
      float _497 = (c_ps_user[12].y) * _231;
      float _499 = (c_ps_user[12].z) * _252;
      float _500 = (c_ps_user[12].z) * _253;
      float _501 = (c_ps_user[12].z) * _254;
      float _507 = -0.0f - _119;
      float _508 = -0.0f - _120;
      float _509 = -0.0f - _121;
      float _510 = dot(float3(_507, _508, _509), float3(_114, _115, _116));
      float _511 = 1.0f - _510;
      float _512 = _511 * _511;
      float _513 = _511 * 0.9722222089767456f;
      float _514 = _512 * _512;
      float _515 = _514 * _513;
      float _516 = _515 + 0.02777777425944805f;
      float _517 = _516 * (c_ps_user[13].z);
      float _519 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _520 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _521 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _524 = (c_ps_user[14].x) * _266;
      float _525 = (c_ps_user[14].x) * _268;
      float _526 = (c_ps_user[14].x) * _270;
      float _528 = (c_ps_user[14].y) * _305;
      float _529 = (c_ps_user[14].y) * _306;
      float _530 = (c_ps_user[14].y) * _307;
      float _532 = (c_ps_user[14].z) * _312;
      float _533 = (c_ps_user[14].z) * _313;
      float _534 = (c_ps_user[14].z) * _314;
      float _536 = (c_ps_user[14].w) * _285;
      float _537 = (c_ps_user[14].w) * _286;
      float _538 = (c_ps_user[14].w) * _287;
      float _541 = (c_ps_user[15].x) * _391;
      float _542 = (c_ps_user[15].x) * _392;
      float _543 = (c_ps_user[15].x) * _393;
      float _545 = _414 * _396;
      float _546 = _545 * (c_ps_user[15].y);
      float _547 = _417 * _397;
      float _548 = _547 * (c_ps_user[15].y);
      float _549 = _420 * _398;
      float _550 = _549 * (c_ps_user[15].y);
      float _552 = (c_ps_user[15].z) * _288;
      float _553 = (c_ps_user[15].z) * _289;
      float _554 = (c_ps_user[15].z) * _290;
      float _555 = _546 + _552;
      float _556 = _555 * _388;
      float _557 = _528 + (c_ps_user[13].x);
      float _558 = _557 + _532;
      float _559 = c_diffuse_scale.x * _558;
      float _560 = _559 * c_mat_diffuse.x;
      float _561 = (c_ps_user[13].y) + _483;
      float _562 = _561 + _541;
      float _563 = c_mat_specular.x * _562;
      float _564 = _563 * c_specular_scale.x;
      float _565 = _479 + _466;
      float _566 = _565 + _488;
      float _567 = _566 + (c_ps_user[12].w);
      float _568 = _567 + _491;
      float _569 = _568 + _495;
      float _570 = _569 + _499;
      float _571 = _570 + _519;
      float _572 = _571 + _524;
      float _573 = _572 + _536;
      float _574 = _573 + _517;
      float _575 = _574 + _556;
      float _576 = _575 + _560;
      float _577 = _576 + _564;
      float _578 = _548 + _553;
      float _579 = _578 * _388;
      float _580 = _529 + (c_ps_user[13].x);
      float _581 = _580 + _533;
      float _582 = c_diffuse_scale.y * _581;
      float _583 = _582 * c_mat_diffuse.y;
      float _584 = (c_ps_user[13].y) + _484;
      float _585 = _584 + _542;
      float _586 = c_mat_specular.y * _585;
      float _587 = _586 * c_specular_scale.y;
      float _588 = _480 + _467;
      float _589 = _588 + _488;
      float _590 = _589 + (c_ps_user[12].w);
      float _591 = _590 + _492;
      float _592 = _591 + _496;
      float _593 = _592 + _500;
      float _594 = _593 + _520;
      float _595 = _594 + _525;
      float _596 = _595 + _537;
      float _597 = _596 + _517;
      float _598 = _597 + _579;
      float _599 = _598 + _583;
      float _600 = _599 + _587;
      float _601 = _550 + _554;
      float _602 = _601 * _388;
      float _603 = _530 + (c_ps_user[13].x);
      float _604 = _603 + _534;
      float _605 = c_diffuse_scale.z * _604;
      float _606 = _605 * c_mat_diffuse.z;
      float _607 = (c_ps_user[13].y) + _485;
      float _608 = _607 + _543;
      float _609 = c_mat_specular.z * _608;
      float _610 = _609 * c_specular_scale.z;
      float _611 = _488 + _468;
      float _612 = _611 + _481;
      float _613 = _612 + (c_ps_user[12].w);
      float _614 = _613 + _493;
      float _615 = _614 + _497;
      float _616 = _615 + _501;
      float _617 = _616 + _521;
      float _618 = _617 + _526;
      float _619 = _618 + _538;
      float _620 = _619 + _517;
      float _621 = _620 + _602;
      float _622 = _621 + _606;
      float _623 = _622 + _610;
      _625 = _577;
      _626 = _600;
      _627 = _623;
      _628 = 0.0f;
      _629 = 0.0f;
      _630 = 0.0f;
   }
   else
   {
      _625 = _455;
      _626 = _456;
      _627 = _457;
      _628 = _458;
      _629 = _459;
      _630 = _460;
   }
   float _631 = _75.x * _75.x;
   float _632 = _631 * COLOR_1.w;
   float _633 = _75.y * _75.y;
   float _634 = _633 * COLOR_1.w;
   float _635 = _75.z * _75.z;
   float _636 = _635 * COLOR_1.w;
   float _637 = _625 + _632;
   float _638 = _637 + _628;
   float _639 = _626 + _634;
   float _640 = _639 + _629;
   float _641 = _627 + _636;
   float _642 = _641 + _630;
   float _647 = c_mat_opacity * _50.w;
   float _648 = _647 * c_global_opacity;
   float _649 = _648 * c_opacity_scale;
   int _650 = c_bool & 16;
   bool _651 = (_650 == 0);
   if (!_651)
   {
      float _655 = c_tone_control.x * _638;
      float _656 = c_tone_control.x * _640;
      float _657 = c_tone_control.x * _642;
      float _659 = log2(_655);
      float _660 = log2(_656);
      float _661 = log2(_657);
      float _662 = _659 * c_tone_control.y;
      float _663 = _660 * c_tone_control.y;
      float _664 = _661 * c_tone_control.y;
      float _665 = exp2(_662);
      float _666 = exp2(_663);
      float _667 = exp2(_664);
      _715 = _665;
      _716 = _666;
      _717 = _667;
   }
   else
   {
      if (_462)
      {
         float _672 = c_tone_control.x * _638;
         float _673 = c_tone_control.x * _640;
         float _674 = c_tone_control.x * _642;
         float _675 = _672 * 2.0f;
         float _676 = _673 * 2.0f;
         float _677 = _674 * 2.0f;
         float _678 = _672 * 0.30000001192092896f;
         float _679 = _673 * 0.30000001192092896f;
         float _680 = _674 * 0.30000001192092896f;
         float _681 = _678 + 0.05000000074505806f;
         float _682 = _679 + 0.05000000074505806f;
         float _683 = _680 + 0.05000000074505806f;
         float _684 = _681 * _675;
         float _685 = _682 * _676;
         float _686 = _683 * _677;
         float _687 = _684 + 0.004000000189989805f;
         float _688 = _685 + 0.004000000189989805f;
         float _689 = _686 + 0.004000000189989805f;
         float _690 = _678 + 0.5f;
         float _691 = _679 + 0.5f;
         float _692 = _680 + 0.5f;
         float _693 = _690 * _675;
         float _694 = _691 * _676;
         float _695 = _692 * _677;
         float _696 = _693 + 0.06000000238418579f;
         float _697 = _694 + 0.06000000238418579f;
         float _698 = _695 + 0.06000000238418579f;
         float _699 = _687 / _696;
         float _700 = _688 / _697;
         float _701 = _689 / _698;
         float _702 = _699 + -0.06666666269302368f;
         float _703 = _700 + -0.06666666269302368f;
         float _704 = _701 + -0.06666666269302368f;
         float _705 = _702 * 1.3790643215179443f;
         float _706 = _703 * 1.3790643215179443f;
         float _707 = _704 * 1.3790643215179443f;
         float _708 = YKR_MaterialCurve(_705, _672);
         float _709 = YKR_MaterialCurve(_706, _673);
         float _710 = YKR_MaterialCurve(_707, _674);
         float _711 = sqrt(_708);
         float _712 = sqrt(_709);
         float _713 = sqrt(_710);
         _715 = _711;
         _716 = _712;
         _717 = _713;
      }
      else
      {
         _715 = _638;
         _716 = _640;
         _717 = _642;
      }
   }
   int _718 = c_bool & 4;
   bool _719 = (_718 == 0);
   if (_719)
   {
      float _721 = saturate(COLOR_1.x);
      float _726 = (c_fog_param[0].x) - _715;
      float _727 = (c_fog_param[0].y) - _716;
      float _728 = (c_fog_param[0].z) - _717;
      float _729 = _726 * _721;
      float _730 = _727 * _721;
      float _731 = _728 * _721;
      _733 = _729;
      _734 = _730;
      _735 = _731;
   }
   else
   {
      _733 = COLOR_1.x;
      _734 = COLOR_1.y;
      _735 = COLOR_1.z;
   }
   float _736 = _733 + _715;
   float _737 = _734 + _716;
   float _738 = _735 + _717;
   float _744 = (c_ps_col_scl[0].x) * _736;
   float _745 = (c_ps_col_scl[0].y) * _737;
   float _746 = (c_ps_col_scl[0].z) * _738;
   float _747 = (c_ps_col_scl[0].w) * _649;
   SV_Target.x = _744;
   SV_Target.y = _745;
   SV_Target.z = _746;
   SV_Target.w = _747;
   int _750 = c_bool & 8;
   bool _751 = (_750 != 0);
   float _752 = select(_751, _736, _744);
   float _753 = select(_751, _737, _745);
   float _754 = select(_751, _738, _746);
   float _755 = select(_751, _649, _747);
   SV_Target.x = _752;
   SV_Target.y = _753;
   SV_Target.z = _754;
   SV_Target.w = _755;
   return SV_Target;
}
