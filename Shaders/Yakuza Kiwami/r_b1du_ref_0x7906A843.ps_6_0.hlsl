// Yakuza Kiwami material r_b1du[ref] (0x7906A843, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s6c_s : register(s6);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _43 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _48 = _43.x * _43.x;
   float _49 = _43.y * _43.y;
   float _50 = _43.z * _43.z;
   float _54 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _55 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _56 = s3_t.Sample(s3_s, float2(_54, _55));
   float _59 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _60 = rsqrt(_59);
   float _61 = _60 * TEXCOORD_2.x;
   float _62 = _60 * TEXCOORD_2.y;
   float _63 = _60 * TEXCOORD_2.z;
   float _64 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _65 = rsqrt(_64);
   float _66 = _65 * TEXCOORD_3.x;
   float _67 = _65 * TEXCOORD_3.y;
   float _68 = _65 * TEXCOORD_3.z;
   float _73 = -0.0f - (c_light_vec[0].x);
   float _74 = -0.0f - (c_light_vec[0].y);
   float _75 = -0.0f - (c_light_vec[0].z);
   float _76 = dot(float3(_61, _62, _63), float3(_73, _74, _75));
   float _80 = c_rt_size.z * SV_Position.x;
   float _81 = c_rt_size.w * SV_Position.y;
   int _84 = c_bool & 8;
   bool _85 = (_84 == 0);
   float _92;
   float _93;
   float _94;
   float _106;
   float _107;
   float _143;
   float _144;
   float _145;
   float _146;
   float _160;
   float _161;
   float _196;
   float _197;
   float _198;
   float _232;
   float _233;
   float _234;
   float _235;
   float _236;
   float _237;
   float _391;
   float _392;
   float _393;
   float _394;
   float _395;
   float _396;
   float _568;
   float _569;
   float _570;
   float _571;
   float _572;
   float _573;
   float _693;
   float _694;
   float _695;
   float _711;
   float _712;
   float _713;
   if (!_85)
   {
      float4 _87 = s13_t.Sample(s13_s, float2(_80, _81));
      _92 = _87.x;
      _93 = _87.y;
      _94 = _87.w;
   }
   else
   {
      _92 = 1.0f;
      _93 = 1.0f;
      _94 = 1.0f;
   }
   if (!_85)
   {
      float _99 = (c_sdw_param[0].x) * _92;
      float _100 = (c_sdw_param[0].z) * _93;
      float _103 = _99 + (c_sdw_param[0].y);
      float _104 = _100 + (c_sdw_param[0].w);
      _106 = _103;
      _107 = _104;
   }
   else
   {
      _106 = _92;
      _107 = _93;
   }
   float _111 = c_rt_size.z * SV_Position.x;
   float _112 = c_rt_size.w * SV_Position.y;
   float4 _113 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_111, _112)));
   int _120 = c_bool & 16384;
   bool _121 = (_120 == 0);
   if (!_121)
   {
      float _123 = _113.x * 2.0f;
      float _124 = _113.y * 2.0f;
      float _125 = _113.z * 2.0f;
      float _126 = _113.w * 2.0f;
      _143 = _123;
      _144 = _124;
      _145 = _125;
      _146 = _126;
   }
   else
   {
      int _128 = c_bool & 8192;
      bool _129 = (_128 == 0);
      if (!_129)
      {
         float _131 = _113.x * 2.0f;
         float _132 = _113.y * 2.0f;
         float _133 = _113.z * 2.0f;
         float _134 = _113.w * 2.0f;
         _143 = _131;
         _144 = _132;
         _145 = _133;
         _146 = _134;
      }
      else
      {
         float _138 = _113.x / c_tone_control.z;
         float _139 = _113.y / c_tone_control.z;
         float _140 = _113.z / c_tone_control.z;
         float _141 = _113.w / c_tone_control.z;
         _143 = _138;
         _144 = _139;
         _145 = _140;
         _146 = _141;
      }
   }
   int _147 = c_bool & 8;
   bool _148 = (_147 == 0);
   if (!_148)
   {
      float _153 = (c_sdw_param[0].x) * _106;
      float _154 = (c_sdw_param[0].z) * _107;
      float _157 = _153 + (c_sdw_param[0].y);
      float _158 = _154 + (c_sdw_param[0].w);
      _160 = _157;
      _161 = _158;
   }
   else
   {
      _160 = _106;
      _161 = _107;
   }
   float _162 = _161 * _160;
   float _167 = 1.0f - (c_light_col[5].x);
   float _168 = 1.0f - (c_light_col[5].y);
   float _169 = 1.0f - (c_light_col[5].z);
   float _170 = _167 * _162;
   float _171 = _168 * _162;
   float _172 = _169 * _162;
   float _173 = _170 + (c_light_col[5].x);
   float _174 = _171 + (c_light_col[5].y);
   float _175 = _172 + (c_light_col[5].z);
   float _176 = _94 * COLOR.x;
   float _177 = _94 * COLOR.y;
   float _178 = _94 * COLOR.z;
   if (!_148)
   {
      float _182 = (c_sdw_param[2].x) * _173;
      float _183 = (c_sdw_param[2].x) * _174;
      float _184 = (c_sdw_param[2].x) * _175;
      float _186 = _182 + (c_sdw_param[2].y);
      float _187 = _183 + (c_sdw_param[2].y);
      float _188 = _184 + (c_sdw_param[2].y);
      float _191 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _192 = _186 * _191;
      float _193 = _187 * _191;
      float _194 = _188 * _191;
      _196 = _192;
      _197 = _193;
      _198 = _194;
   }
   else
   {
      _196 = _173;
      _197 = _174;
      _198 = _175;
   }
   float _199 = _196 * _94;
   float _200 = _197 * _94;
   float _201 = _198 * _94;
   float _212 = c_mat_emissive.x * _48;
   float _213 = _212 * c_emissive_scale;
   float _214 = c_mat_emissive.y * _49;
   float _215 = _214 * c_emissive_scale;
   float _216 = c_mat_emissive.z * _50;
   float _217 = _216 * c_emissive_scale;
   int _218 = c_bool & 2048;
   bool _219 = (_218 == 0);
   if (_219)
   {
      float _221 = _161 * _143;
      float _222 = _161 * _144;
      float _223 = _161 * _145;
      float _224 = _221 + TEXCOORD_4.x;
      float _225 = _222 + TEXCOORD_4.y;
      float _226 = _223 + TEXCOORD_4.z;
      float _227 = _161 * _146;
      float _228 = _227 + TEXCOORD_5.x;
      float _229 = _227 + TEXCOORD_5.y;
      float _230 = _227 + TEXCOORD_5.z;
      _232 = _224;
      _233 = _225;
      _234 = _226;
      _235 = _228;
      _236 = _229;
      _237 = _230;
   }
   else
   {
      _232 = TEXCOORD_4.x;
      _233 = TEXCOORD_4.y;
      _234 = TEXCOORD_4.z;
      _235 = TEXCOORD_5.x;
      _236 = TEXCOORD_5.y;
      _237 = TEXCOORD_5.z;
   }
   float _242 = -0.0f - (c_light_vec[1].x);
   float _243 = -0.0f - (c_light_vec[1].y);
   float _244 = -0.0f - (c_light_vec[1].z);
   float _245 = dot(float3(_61, _62, _63), float3(_242, _243, _244));
   float _246 = saturate(_245);
   float _251 = saturate(_76);
   float _252 = (c_light_col[0].x) * _251;
   float _253 = (c_light_col[0].y) * _251;
   float _254 = (c_light_col[0].z) * _251;
   float _259 = (c_light_col[1].x) * _246;
   float _260 = (c_light_col[1].y) * _246;
   float _261 = (c_light_col[1].z) * _246;
   float _270 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _271 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _272 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _273 = _252 * _173;
   float _274 = _253 * _174;
   float _275 = _254 * _175;
   float _276 = _259 * _161;
   float _277 = _260 * _161;
   float _278 = _261 * _161;
   float _279 = _276 + _273;
   float _280 = _277 + _274;
   float _281 = _278 + _275;
   float _282 = _270 * _279;
   float _283 = _271 * _280;
   float _284 = _272 * _281;
   float _285 = _213 + c_scene_ambient.x;
   float _286 = _285 + _282;
   float _287 = _215 + c_scene_ambient.y;
   float _288 = _287 + _283;
   float _289 = _217 + c_scene_ambient.z;
   float _290 = _289 + _284;
   float _291 = _286 * _176;
   float _292 = _288 * _177;
   float _293 = _290 * _178;
   float _294 = _232 * _199;
   float _295 = _233 * _200;
   float _296 = _234 * _201;
   float _297 = _291 + _294;
   float _298 = _292 + _295;
   float _299 = _293 + _296;
   float _304 = (c_light_vec[0].x) + _66;
   float _305 = (c_light_vec[0].y) + _67;
   float _306 = (c_light_vec[0].z) + _68;
   float _307 = dot(float3(_304, _305, _306), float3(_304, _305, _306));
   float _308 = rsqrt(_307);
   float _309 = _304 * _308;
   float _310 = -0.0f - _309;
   float _311 = _305 * _308;
   float _312 = -0.0f - _311;
   float _313 = _306 * _308;
   float _314 = -0.0f - _313;
   float _315 = dot(float3(_310, _312, _314), float3(_61, _62, _63));
   float _316 = saturate(_315);
   float _317 = _67 + -0.30000001192092896f;
   float _318 = dot(float3(_66, _317, _68), float3(_66, _317, _68));
   float _319 = rsqrt(_318);
   float _320 = _66 * _319;
   float _321 = -0.0f - _320;
   float _322 = _317 * _319;
   float _323 = -0.0f - _322;
   float _324 = _68 * _319;
   float _325 = -0.0f - _324;
   float _326 = dot(float3(_321, _323, _325), float3(_61, _62, _63));
   float _327 = saturate(_326);
   float _330 = log2(_316);
   float _331 = log2(_327);
   float _332 = _330 * c_mat_power;
   float _333 = _331 * c_mat_power;
   float _334 = exp2(_332);
   float _335 = exp2(_333);
   float _338 = _334 * (c_light_col[0].x);
   float _339 = _334 * (c_light_col[0].y);
   float _340 = _334 * (c_light_col[0].z);
   float _341 = _335 * 2.0f;
   float _342 = _341 * (c_light_vec[5].x);
   float _343 = _342 * (c_light_col[1].x);
   float _344 = _342 * (c_light_col[1].y);
   float _345 = _342 * (c_light_col[1].z);
   float _346 = _338 * _173;
   float _347 = _339 * _174;
   float _348 = _340 * _175;
   float _349 = _343 * _246;
   float _350 = _344 * _246;
   float _351 = _345 * _246;
   float _352 = _349 + _346;
   float _353 = _350 + _347;
   float _354 = _351 + _348;
   float _361 = c_mat_specular.x * c_specular_scale.x;
   float _362 = _361 * _176;
   float _363 = _362 * _352;
   float _364 = c_mat_specular.y * c_specular_scale.y;
   float _365 = _364 * _177;
   float _366 = _365 * _353;
   float _367 = c_mat_specular.z * c_specular_scale.z;
   float _368 = _367 * _178;
   float _369 = _368 * _354;
   float _370 = _235 * _199;
   float _371 = _370 * _335;
   float _372 = _236 * _200;
   float _373 = _372 * _335;
   float _374 = _237 * _201;
   float _375 = _374 * _335;
   float _376 = _363 + _371;
   float _377 = _366 + _373;
   float _378 = _369 + _375;
   if (!_219)
   {
      float _380 = _161 * _143;
      float _381 = _161 * _144;
      float _382 = _161 * _145;
      float _383 = _297 + _380;
      float _384 = _298 + _381;
      float _385 = _299 + _382;
      float _386 = _161 * _146;
      float _387 = _376 + _386;
      float _388 = _377 + _386;
      float _389 = _378 + _386;
      _391 = _383;
      _392 = _384;
      _393 = _385;
      _394 = _387;
      _395 = _388;
      _396 = _389;
   }
   else
   {
      _391 = _297;
      _392 = _298;
      _393 = _299;
      _394 = _376;
      _395 = _377;
      _396 = _378;
   }
   int _397 = c_bool & 128;
   bool _398 = (_397 != 0);
   float _399 = select(_398, 0.0f, _394);
   float _400 = select(_398, 0.0f, _395);
   float _401 = select(_398, 0.0f, _396);
   float _402 = _391 * _48;
   float _403 = _392 * _49;
   float _404 = _393 * _50;
   float _405 = _399 * _56.y;
   float _406 = _400 * _56.y;
   float _407 = _401 * _56.y;
   int _408 = c_bool & 32768;
   bool _409 = (_408 == 0);
   if (!_409)
   {
      float _413 = (c_ps_user[11].x) * _48;
      float _414 = (c_ps_user[11].x) * _49;
      float _415 = (c_ps_user[11].x) * _50;
      float _417 = _61 + 1.0f;
      float _418 = _62 + 1.0f;
      float _419 = _63 + 1.0f;
      float _420 = _417 * 0.5f;
      float _421 = _418 * 0.5f;
      float _422 = _419 * 0.5f;
      float _423 = saturate(_420);
      float _424 = saturate(_421);
      float _425 = saturate(_422);
      float _426 = (c_ps_user[11].y) * _423;
      float _427 = _424 * (c_ps_user[11].y);
      float _428 = _425 * (c_ps_user[11].y);
      float _430 = (c_ps_user[11].z) * _56.y;
      float _432 = c_mat_power * 0.009999999776482582f;
      float _433 = _432 * (c_ps_user[11].w);
      float _436 = (c_ps_user[12].x) * _173;
      float _437 = (c_ps_user[12].x) * _174;
      float _438 = (c_ps_user[12].x) * _175;
      float _440 = (c_ps_user[12].y) * _176;
      float _441 = (c_ps_user[12].y) * _177;
      float _442 = (c_ps_user[12].y) * _178;
      float _444 = (c_ps_user[12].z) * _199;
      float _445 = (c_ps_user[12].z) * _200;
      float _446 = (c_ps_user[12].z) * _201;
      float _452 = -0.0f - _66;
      float _453 = -0.0f - _67;
      float _454 = -0.0f - _68;
      float _455 = dot(float3(_452, _453, _454), float3(_61, _62, _63));
      float _456 = 1.0f - _455;
      float _457 = _456 * _456;
      float _458 = _456 * 0.9722222089767456f;
      float _459 = _457 * _457;
      float _460 = _459 * _458;
      float _461 = _460 + 0.02777777425944805f;
      float _462 = _461 * (c_ps_user[13].z);
      float _464 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _465 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _466 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _469 = (c_ps_user[14].x) * _213;
      float _470 = (c_ps_user[14].x) * _215;
      float _471 = (c_ps_user[14].x) * _217;
      float _473 = (c_ps_user[14].y) * _252;
      float _474 = (c_ps_user[14].y) * _253;
      float _475 = (c_ps_user[14].y) * _254;
      float _477 = (c_ps_user[14].z) * _259;
      float _478 = (c_ps_user[14].z) * _260;
      float _479 = (c_ps_user[14].z) * _261;
      float _481 = (c_ps_user[14].w) * _232;
      float _482 = (c_ps_user[14].w) * _233;
      float _483 = (c_ps_user[14].w) * _234;
      float _486 = (c_ps_user[15].x) * _338;
      float _487 = (c_ps_user[15].x) * _339;
      float _488 = (c_ps_user[15].x) * _340;
      float _490 = _361 * _343;
      float _491 = _490 * (c_ps_user[15].y);
      float _492 = _364 * _344;
      float _493 = _492 * (c_ps_user[15].y);
      float _494 = _367 * _345;
      float _495 = _494 * (c_ps_user[15].y);
      float _497 = (c_ps_user[15].z) * _235;
      float _498 = (c_ps_user[15].z) * _236;
      float _499 = (c_ps_user[15].z) * _237;
      float _500 = _491 + _497;
      float _501 = _500 * _335;
      float _502 = _473 + (c_ps_user[13].x);
      float _503 = _502 + _477;
      float _504 = c_diffuse_scale.x * _503;
      float _505 = _504 * c_mat_diffuse.x;
      float _506 = (c_ps_user[13].y) + _430;
      float _507 = _506 + _486;
      float _508 = c_mat_specular.x * _507;
      float _509 = _508 * c_specular_scale.x;
      float _510 = _426 + _413;
      float _511 = _510 + _433;
      float _512 = _511 + (c_ps_user[12].w);
      float _513 = _512 + _436;
      float _514 = _513 + _440;
      float _515 = _514 + _444;
      float _516 = _515 + _464;
      float _517 = _516 + _469;
      float _518 = _517 + _481;
      float _519 = _518 + _462;
      float _520 = _519 + _501;
      float _521 = _520 + _505;
      float _522 = _521 + _509;
      float _523 = _493 + _498;
      float _524 = _523 * _335;
      float _525 = _474 + (c_ps_user[13].x);
      float _526 = _525 + _478;
      float _527 = c_diffuse_scale.y * _526;
      float _528 = _527 * c_mat_diffuse.y;
      float _529 = _506 + _487;
      float _530 = c_mat_specular.y * _529;
      float _531 = _530 * c_specular_scale.y;
      float _532 = _427 + _414;
      float _533 = _532 + _433;
      float _534 = _533 + (c_ps_user[12].w);
      float _535 = _534 + _437;
      float _536 = _535 + _441;
      float _537 = _536 + _445;
      float _538 = _537 + _465;
      float _539 = _538 + _470;
      float _540 = _539 + _482;
      float _541 = _540 + _462;
      float _542 = _541 + _524;
      float _543 = _542 + _528;
      float _544 = _543 + _531;
      float _545 = _495 + _499;
      float _546 = _545 * _335;
      float _547 = _475 + (c_ps_user[13].x);
      float _548 = _547 + _479;
      float _549 = c_diffuse_scale.z * _548;
      float _550 = _549 * c_mat_diffuse.z;
      float _551 = _506 + _488;
      float _552 = c_mat_specular.z * _551;
      float _553 = _552 * c_specular_scale.z;
      float _554 = _433 + _415;
      float _555 = _554 + _428;
      float _556 = _555 + (c_ps_user[12].w);
      float _557 = _556 + _438;
      float _558 = _557 + _442;
      float _559 = _558 + _446;
      float _560 = _559 + _466;
      float _561 = _560 + _471;
      float _562 = _561 + _483;
      float _563 = _562 + _462;
      float _564 = _563 + _546;
      float _565 = _564 + _550;
      float _566 = _565 + _553;
      _568 = _522;
      _569 = _544;
      _570 = _566;
      _571 = 0.0f;
      _572 = 0.0f;
      _573 = 0.0f;
   }
   else
   {
      _568 = _402;
      _569 = _403;
      _570 = _404;
      _571 = _405;
      _572 = _406;
      _573 = _407;
   }
   float _574 = _571 + _568;
   float _575 = _572 + _569;
   float _576 = _573 + _570;
   float _577 = dot(float3(_66, _67, _68), float3(_61, _62, _63));
   float _578 = _577 * 2.0f;
   float _579 = _578 * _61;
   float _580 = _578 * _62;
   float _581 = _578 * _63;
   float _582 = _66 - _579;
   float _583 = _67 - _580;
   float _584 = _68 - _581;
   float _585 = -0.0f - _582;
   float4 _586 = s6c_t.Sample(s6c_s, float3(_585, _583, _584));
   float _593 = _107 * _106;
   float _594 = _76 + 0.75f;
   float _595 = saturate(_594);
   float _596 = _593 * _595;
   float _597 = max(_596, (c_light_vec[5].y));
   float _598 = max(_586.w, 0.003921568859368563f);
   float _599 = 0.25f / _598;
   float _600 = _599 * _586.x;
   float _601 = _599 * _586.y;
   float _602 = _599 * _586.z;
   float _603 = _56.x * TEXCOORD_2.w;
   float _604 = _603 * _597;
   float _605 = _600 * _600;
   float _606 = _605 * _604;
   float _607 = _601 * _601;
   float _608 = _607 * _604;
   float _609 = _602 * _602;
   float _610 = _609 * _604;
   float _611 = _574 + _606;
   float _612 = _575 + _608;
   float _613 = _576 + _610;
   float _620 = _43.w * COLOR.w;
   float _621 = _620 * c_mat_opacity;
   float _622 = _621 * c_opacity_scale;
   float _623 = _622 * c_global_opacity;
   int _626 = c_bool & 16;
   bool _627 = (_626 == 0);
   if (!_627)
   {
      float _631 = c_tone_control.x * _611;
      float _632 = c_tone_control.x * _612;
      float _633 = c_tone_control.x * _613;
      float _635 = log2(_631);
      float _636 = log2(_632);
      float _637 = log2(_633);
      float _638 = _635 * c_tone_control.y;
      float _639 = _636 * c_tone_control.y;
      float _640 = _637 * c_tone_control.y;
      float _641 = exp2(_638);
      float _642 = exp2(_639);
      float _643 = exp2(_640);
      _693 = _641;
      _694 = _642;
      _695 = _643;
   }
   else
   {
      int _645 = c_bool & 32768;
      bool _646 = (_645 == 0);
      if (_646)
      {
         float _650 = c_tone_control.x * _611;
         float _651 = c_tone_control.x * _612;
         float _652 = c_tone_control.x * _613;
         float _653 = _650 * 2.0f;
         float _654 = _651 * 2.0f;
         float _655 = _652 * 2.0f;
         float _656 = _650 * 0.30000001192092896f;
         float _657 = _651 * 0.30000001192092896f;
         float _658 = _652 * 0.30000001192092896f;
         float _659 = _656 + 0.05000000074505806f;
         float _660 = _657 + 0.05000000074505806f;
         float _661 = _658 + 0.05000000074505806f;
         float _662 = _659 * _653;
         float _663 = _660 * _654;
         float _664 = _661 * _655;
         float _665 = _662 + 0.004000000189989805f;
         float _666 = _663 + 0.004000000189989805f;
         float _667 = _664 + 0.004000000189989805f;
         float _668 = _656 + 0.5f;
         float _669 = _657 + 0.5f;
         float _670 = _658 + 0.5f;
         float _671 = _668 * _653;
         float _672 = _669 * _654;
         float _673 = _670 * _655;
         float _674 = _671 + 0.06000000238418579f;
         float _675 = _672 + 0.06000000238418579f;
         float _676 = _673 + 0.06000000238418579f;
         float _677 = _665 / _674;
         float _678 = _666 / _675;
         float _679 = _667 / _676;
         float _680 = _677 + -0.06666666269302368f;
         float _681 = _678 + -0.06666666269302368f;
         float _682 = _679 + -0.06666666269302368f;
         float _683 = _680 * 1.3790643215179443f;
         float _684 = _681 * 1.3790643215179443f;
         float _685 = _682 * 1.3790643215179443f;
         float _686 = YKR_MaterialCurve(_683, _650);
         float _687 = YKR_MaterialCurve(_684, _651);
         float _688 = YKR_MaterialCurve(_685, _652);
         float _689 = sqrt(_686);
         float _690 = sqrt(_687);
         float _691 = sqrt(_688);
         _693 = _689;
         _694 = _690;
         _695 = _691;
      }
      else
      {
         _693 = _611;
         _694 = _612;
         _695 = _613;
      }
   }
   int _696 = c_bool & 4;
   bool _697 = (_696 == 0);
   if (_697)
   {
      float _699 = saturate(COLOR_1.x);
      float _704 = (c_fog_param[0].x) - _693;
      float _705 = (c_fog_param[0].y) - _694;
      float _706 = (c_fog_param[0].z) - _695;
      float _707 = _704 * _699;
      float _708 = _705 * _699;
      float _709 = _706 * _699;
      _711 = _707;
      _712 = _708;
      _713 = _709;
   }
   else
   {
      _711 = COLOR_1.x;
      _712 = COLOR_1.y;
      _713 = COLOR_1.z;
   }
   float _714 = _711 + _693;
   float _715 = _712 + _694;
   float _716 = _713 + _695;
   float _722 = (c_ps_col_scl[0].x) * _714;
   float _723 = (c_ps_col_scl[0].y) * _715;
   float _724 = (c_ps_col_scl[0].z) * _716;
   float _725 = (c_ps_col_scl[0].w) * _623;
   SV_Target.x = _722;
   SV_Target.y = _723;
   SV_Target.z = _724;
   SV_Target.w = _725;
   int _728 = c_bool & 8;
   bool _729 = (_728 != 0);
   float _730 = select(_729, _714, _722);
   float _731 = select(_729, _715, _723);
   float _732 = select(_729, _716, _724);
   float _733 = select(_729, _623, _725);
   SV_Target.x = _730;
   SV_Target.y = _731;
   SV_Target.z = _732;
   SV_Target.w = _733;
   return SV_Target;
}
