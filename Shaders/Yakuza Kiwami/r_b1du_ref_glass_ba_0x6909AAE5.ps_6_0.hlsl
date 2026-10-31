// Yakuza Kiwami material r_b1du[ref][glass][ba] (0x6909AAE5, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _60 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _61 = rsqrt(_60);
   float _62 = _61 * TEXCOORD_2.x;
   float _63 = _61 * TEXCOORD_2.y;
   float _64 = _61 * TEXCOORD_2.z;
   float _65 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _66 = rsqrt(_65);
   float _67 = _66 * TEXCOORD_3.x;
   float _68 = _66 * TEXCOORD_3.y;
   float _69 = _66 * TEXCOORD_3.z;
   float _74 = -0.0f - (c_light_vec[0].x);
   float _75 = -0.0f - (c_light_vec[0].y);
   float _76 = -0.0f - (c_light_vec[0].z);
   float _77 = dot(float3(_62, _63, _64), float3(_74, _75, _76));
   float _81 = c_rt_size.z * SV_Position.x;
   float _82 = c_rt_size.w * SV_Position.y;
   int _85 = c_bool & 8;
   bool _86 = (_85 == 0);
   float _93;
   float _94;
   float _95;
   float _107;
   float _108;
   float _144;
   float _145;
   float _146;
   float _147;
   float _161;
   float _162;
   float _197;
   float _198;
   float _199;
   float _233;
   float _234;
   float _235;
   float _236;
   float _237;
   float _238;
   float _392;
   float _393;
   float _394;
   float _395;
   float _396;
   float _397;
   float _569;
   float _570;
   float _571;
   float _572;
   float _573;
   float _574;
   float _703;
   float _704;
   float _705;
   float _721;
   float _722;
   float _723;
   if (!_86)
   {
      float4 _88 = s13_t.Sample(s13_s, float2(_81, _82));
      _93 = _88.x;
      _94 = _88.y;
      _95 = _88.w;
   }
   else
   {
      _93 = 1.0f;
      _94 = 1.0f;
      _95 = 1.0f;
   }
   if (!_86)
   {
      float _100 = (c_sdw_param[0].x) * _93;
      float _101 = (c_sdw_param[0].z) * _94;
      float _104 = _100 + (c_sdw_param[0].y);
      float _105 = _101 + (c_sdw_param[0].w);
      _107 = _104;
      _108 = _105;
   }
   else
   {
      _107 = _93;
      _108 = _94;
   }
   float _112 = c_rt_size.z * SV_Position.x;
   float _113 = c_rt_size.w * SV_Position.y;
   float4 _114 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_112, _113)));
   int _121 = c_bool & 16384;
   bool _122 = (_121 == 0);
   if (!_122)
   {
      float _124 = _114.x * 2.0f;
      float _125 = _114.y * 2.0f;
      float _126 = _114.z * 2.0f;
      float _127 = _114.w * 2.0f;
      _144 = _124;
      _145 = _125;
      _146 = _126;
      _147 = _127;
   }
   else
   {
      int _129 = c_bool & 8192;
      bool _130 = (_129 == 0);
      if (!_130)
      {
         float _132 = _114.x * 2.0f;
         float _133 = _114.y * 2.0f;
         float _134 = _114.z * 2.0f;
         float _135 = _114.w * 2.0f;
         _144 = _132;
         _145 = _133;
         _146 = _134;
         _147 = _135;
      }
      else
      {
         float _139 = _114.x / c_tone_control.z;
         float _140 = _114.y / c_tone_control.z;
         float _141 = _114.z / c_tone_control.z;
         float _142 = _114.w / c_tone_control.z;
         _144 = _139;
         _145 = _140;
         _146 = _141;
         _147 = _142;
      }
   }
   int _148 = c_bool & 8;
   bool _149 = (_148 == 0);
   if (!_149)
   {
      float _154 = (c_sdw_param[0].x) * _107;
      float _155 = (c_sdw_param[0].z) * _108;
      float _158 = _154 + (c_sdw_param[0].y);
      float _159 = _155 + (c_sdw_param[0].w);
      _161 = _158;
      _162 = _159;
   }
   else
   {
      _161 = _107;
      _162 = _108;
   }
   float _163 = _162 * _161;
   float _168 = 1.0f - (c_light_col[5].x);
   float _169 = 1.0f - (c_light_col[5].y);
   float _170 = 1.0f - (c_light_col[5].z);
   float _171 = _168 * _163;
   float _172 = _169 * _163;
   float _173 = _170 * _163;
   float _174 = _171 + (c_light_col[5].x);
   float _175 = _172 + (c_light_col[5].y);
   float _176 = _173 + (c_light_col[5].z);
   float _177 = _95 * COLOR.x;
   float _178 = _95 * COLOR.y;
   float _179 = _95 * COLOR.z;
   if (!_149)
   {
      float _183 = (c_sdw_param[2].x) * _174;
      float _184 = (c_sdw_param[2].x) * _175;
      float _185 = (c_sdw_param[2].x) * _176;
      float _187 = _183 + (c_sdw_param[2].y);
      float _188 = _184 + (c_sdw_param[2].y);
      float _189 = _185 + (c_sdw_param[2].y);
      float _192 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _193 = _187 * _192;
      float _194 = _188 * _192;
      float _195 = _189 * _192;
      _197 = _193;
      _198 = _194;
      _199 = _195;
   }
   else
   {
      _197 = _174;
      _198 = _175;
      _199 = _176;
   }
   float _200 = _197 * _95;
   float _201 = _198 * _95;
   float _202 = _199 * _95;
   float _213 = c_mat_emissive.x * _48;
   float _214 = _213 * c_emissive_scale;
   float _215 = c_mat_emissive.y * _49;
   float _216 = _215 * c_emissive_scale;
   float _217 = c_mat_emissive.z * _50;
   float _218 = _217 * c_emissive_scale;
   int _219 = c_bool & 2048;
   bool _220 = (_219 == 0);
   if (_220)
   {
      float _222 = _162 * _144;
      float _223 = _162 * _145;
      float _224 = _162 * _146;
      float _225 = _222 + TEXCOORD_4.x;
      float _226 = _223 + TEXCOORD_4.y;
      float _227 = _224 + TEXCOORD_4.z;
      float _228 = _162 * _147;
      float _229 = _228 + TEXCOORD_5.x;
      float _230 = _228 + TEXCOORD_5.y;
      float _231 = _228 + TEXCOORD_5.z;
      _233 = _225;
      _234 = _226;
      _235 = _227;
      _236 = _229;
      _237 = _230;
      _238 = _231;
   }
   else
   {
      _233 = TEXCOORD_4.x;
      _234 = TEXCOORD_4.y;
      _235 = TEXCOORD_4.z;
      _236 = TEXCOORD_5.x;
      _237 = TEXCOORD_5.y;
      _238 = TEXCOORD_5.z;
   }
   float _243 = -0.0f - (c_light_vec[1].x);
   float _244 = -0.0f - (c_light_vec[1].y);
   float _245 = -0.0f - (c_light_vec[1].z);
   float _246 = dot(float3(_62, _63, _64), float3(_243, _244, _245));
   float _247 = saturate(_246);
   float _252 = saturate(_77);
   float _253 = (c_light_col[0].x) * _252;
   float _254 = (c_light_col[0].y) * _252;
   float _255 = (c_light_col[0].z) * _252;
   float _260 = (c_light_col[1].x) * _247;
   float _261 = (c_light_col[1].y) * _247;
   float _262 = (c_light_col[1].z) * _247;
   float _271 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _272 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _273 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _274 = _253 * _174;
   float _275 = _254 * _175;
   float _276 = _255 * _176;
   float _277 = _260 * _162;
   float _278 = _261 * _162;
   float _279 = _262 * _162;
   float _280 = _277 + _274;
   float _281 = _278 + _275;
   float _282 = _279 + _276;
   float _283 = _271 * _280;
   float _284 = _272 * _281;
   float _285 = _273 * _282;
   float _286 = _214 + c_scene_ambient.x;
   float _287 = _286 + _283;
   float _288 = _216 + c_scene_ambient.y;
   float _289 = _288 + _284;
   float _290 = _218 + c_scene_ambient.z;
   float _291 = _290 + _285;
   float _292 = _287 * _177;
   float _293 = _289 * _178;
   float _294 = _291 * _179;
   float _295 = _233 * _200;
   float _296 = _234 * _201;
   float _297 = _235 * _202;
   float _298 = _292 + _295;
   float _299 = _293 + _296;
   float _300 = _294 + _297;
   float _305 = (c_light_vec[0].x) + _67;
   float _306 = (c_light_vec[0].y) + _68;
   float _307 = (c_light_vec[0].z) + _69;
   float _308 = dot(float3(_305, _306, _307), float3(_305, _306, _307));
   float _309 = rsqrt(_308);
   float _310 = _305 * _309;
   float _311 = -0.0f - _310;
   float _312 = _306 * _309;
   float _313 = -0.0f - _312;
   float _314 = _307 * _309;
   float _315 = -0.0f - _314;
   float _316 = dot(float3(_311, _313, _315), float3(_62, _63, _64));
   float _317 = saturate(_316);
   float _318 = _68 + -0.30000001192092896f;
   float _319 = dot(float3(_67, _318, _69), float3(_67, _318, _69));
   float _320 = rsqrt(_319);
   float _321 = _67 * _320;
   float _322 = -0.0f - _321;
   float _323 = _318 * _320;
   float _324 = -0.0f - _323;
   float _325 = _69 * _320;
   float _326 = -0.0f - _325;
   float _327 = dot(float3(_322, _324, _326), float3(_62, _63, _64));
   float _328 = saturate(_327);
   float _331 = log2(_317);
   float _332 = log2(_328);
   float _333 = _331 * c_mat_power;
   float _334 = _332 * c_mat_power;
   float _335 = exp2(_333);
   float _336 = exp2(_334);
   float _339 = _335 * (c_light_col[0].x);
   float _340 = _335 * (c_light_col[0].y);
   float _341 = _335 * (c_light_col[0].z);
   float _342 = _336 * 2.0f;
   float _343 = _342 * (c_light_vec[5].x);
   float _344 = _343 * (c_light_col[1].x);
   float _345 = _343 * (c_light_col[1].y);
   float _346 = _343 * (c_light_col[1].z);
   float _347 = _339 * _174;
   float _348 = _340 * _175;
   float _349 = _341 * _176;
   float _350 = _344 * _247;
   float _351 = _345 * _247;
   float _352 = _346 * _247;
   float _353 = _350 + _347;
   float _354 = _351 + _348;
   float _355 = _352 + _349;
   float _362 = c_mat_specular.x * c_specular_scale.x;
   float _363 = _362 * _177;
   float _364 = _363 * _353;
   float _365 = c_mat_specular.y * c_specular_scale.y;
   float _366 = _365 * _178;
   float _367 = _366 * _354;
   float _368 = c_mat_specular.z * c_specular_scale.z;
   float _369 = _368 * _179;
   float _370 = _369 * _355;
   float _371 = _236 * _200;
   float _372 = _371 * _336;
   float _373 = _237 * _201;
   float _374 = _373 * _336;
   float _375 = _238 * _202;
   float _376 = _375 * _336;
   float _377 = _364 + _372;
   float _378 = _367 + _374;
   float _379 = _370 + _376;
   if (!_220)
   {
      float _381 = _162 * _144;
      float _382 = _162 * _145;
      float _383 = _162 * _146;
      float _384 = _298 + _381;
      float _385 = _299 + _382;
      float _386 = _300 + _383;
      float _387 = _162 * _147;
      float _388 = _377 + _387;
      float _389 = _378 + _387;
      float _390 = _379 + _387;
      _392 = _384;
      _393 = _385;
      _394 = _386;
      _395 = _388;
      _396 = _389;
      _397 = _390;
   }
   else
   {
      _392 = _298;
      _393 = _299;
      _394 = _300;
      _395 = _377;
      _396 = _378;
      _397 = _379;
   }
   int _398 = c_bool & 128;
   bool _399 = (_398 != 0);
   float _400 = select(_399, 0.0f, _395);
   float _401 = select(_399, 0.0f, _396);
   float _402 = select(_399, 0.0f, _397);
   float _403 = _392 * _48;
   float _404 = _393 * _49;
   float _405 = _394 * _50;
   float _406 = _400 * _56.y;
   float _407 = _401 * _56.y;
   float _408 = _402 * _56.y;
   int _409 = c_bool & 32768;
   bool _410 = (_409 == 0);
   if (!_410)
   {
      float _414 = (c_ps_user[11].x) * _48;
      float _415 = (c_ps_user[11].x) * _49;
      float _416 = (c_ps_user[11].x) * _50;
      float _418 = _62 + 1.0f;
      float _419 = _63 + 1.0f;
      float _420 = _64 + 1.0f;
      float _421 = _418 * 0.5f;
      float _422 = _419 * 0.5f;
      float _423 = _420 * 0.5f;
      float _424 = saturate(_421);
      float _425 = saturate(_422);
      float _426 = saturate(_423);
      float _427 = (c_ps_user[11].y) * _424;
      float _428 = _425 * (c_ps_user[11].y);
      float _429 = _426 * (c_ps_user[11].y);
      float _431 = (c_ps_user[11].z) * _56.y;
      float _433 = c_mat_power * 0.009999999776482582f;
      float _434 = _433 * (c_ps_user[11].w);
      float _437 = (c_ps_user[12].x) * _174;
      float _438 = (c_ps_user[12].x) * _175;
      float _439 = (c_ps_user[12].x) * _176;
      float _441 = (c_ps_user[12].y) * _177;
      float _442 = (c_ps_user[12].y) * _178;
      float _443 = (c_ps_user[12].y) * _179;
      float _445 = (c_ps_user[12].z) * _200;
      float _446 = (c_ps_user[12].z) * _201;
      float _447 = (c_ps_user[12].z) * _202;
      float _453 = -0.0f - _67;
      float _454 = -0.0f - _68;
      float _455 = -0.0f - _69;
      float _456 = dot(float3(_453, _454, _455), float3(_62, _63, _64));
      float _457 = 1.0f - _456;
      float _458 = _457 * _457;
      float _459 = _457 * 0.9722222089767456f;
      float _460 = _458 * _458;
      float _461 = _460 * _459;
      float _462 = _461 + 0.02777777425944805f;
      float _463 = _462 * (c_ps_user[13].z);
      float _465 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _466 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _467 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _470 = (c_ps_user[14].x) * _214;
      float _471 = (c_ps_user[14].x) * _216;
      float _472 = (c_ps_user[14].x) * _218;
      float _474 = (c_ps_user[14].y) * _253;
      float _475 = (c_ps_user[14].y) * _254;
      float _476 = (c_ps_user[14].y) * _255;
      float _478 = (c_ps_user[14].z) * _260;
      float _479 = (c_ps_user[14].z) * _261;
      float _480 = (c_ps_user[14].z) * _262;
      float _482 = (c_ps_user[14].w) * _233;
      float _483 = (c_ps_user[14].w) * _234;
      float _484 = (c_ps_user[14].w) * _235;
      float _487 = (c_ps_user[15].x) * _339;
      float _488 = (c_ps_user[15].x) * _340;
      float _489 = (c_ps_user[15].x) * _341;
      float _491 = _362 * _344;
      float _492 = _491 * (c_ps_user[15].y);
      float _493 = _365 * _345;
      float _494 = _493 * (c_ps_user[15].y);
      float _495 = _368 * _346;
      float _496 = _495 * (c_ps_user[15].y);
      float _498 = (c_ps_user[15].z) * _236;
      float _499 = (c_ps_user[15].z) * _237;
      float _500 = (c_ps_user[15].z) * _238;
      float _501 = _492 + _498;
      float _502 = _501 * _336;
      float _503 = _474 + (c_ps_user[13].x);
      float _504 = _503 + _478;
      float _505 = c_diffuse_scale.x * _504;
      float _506 = _505 * c_mat_diffuse.x;
      float _507 = (c_ps_user[13].y) + _431;
      float _508 = _507 + _487;
      float _509 = c_mat_specular.x * _508;
      float _510 = _509 * c_specular_scale.x;
      float _511 = _427 + _414;
      float _512 = _511 + _434;
      float _513 = _512 + (c_ps_user[12].w);
      float _514 = _513 + _437;
      float _515 = _514 + _441;
      float _516 = _515 + _445;
      float _517 = _516 + _465;
      float _518 = _517 + _470;
      float _519 = _518 + _482;
      float _520 = _519 + _463;
      float _521 = _520 + _502;
      float _522 = _521 + _506;
      float _523 = _522 + _510;
      float _524 = _494 + _499;
      float _525 = _524 * _336;
      float _526 = _475 + (c_ps_user[13].x);
      float _527 = _526 + _479;
      float _528 = c_diffuse_scale.y * _527;
      float _529 = _528 * c_mat_diffuse.y;
      float _530 = _507 + _488;
      float _531 = c_mat_specular.y * _530;
      float _532 = _531 * c_specular_scale.y;
      float _533 = _428 + _415;
      float _534 = _533 + _434;
      float _535 = _534 + (c_ps_user[12].w);
      float _536 = _535 + _438;
      float _537 = _536 + _442;
      float _538 = _537 + _446;
      float _539 = _538 + _466;
      float _540 = _539 + _471;
      float _541 = _540 + _483;
      float _542 = _541 + _463;
      float _543 = _542 + _525;
      float _544 = _543 + _529;
      float _545 = _544 + _532;
      float _546 = _496 + _500;
      float _547 = _546 * _336;
      float _548 = _476 + (c_ps_user[13].x);
      float _549 = _548 + _480;
      float _550 = c_diffuse_scale.z * _549;
      float _551 = _550 * c_mat_diffuse.z;
      float _552 = _507 + _489;
      float _553 = c_mat_specular.z * _552;
      float _554 = _553 * c_specular_scale.z;
      float _555 = _434 + _416;
      float _556 = _555 + _429;
      float _557 = _556 + (c_ps_user[12].w);
      float _558 = _557 + _439;
      float _559 = _558 + _443;
      float _560 = _559 + _447;
      float _561 = _560 + _467;
      float _562 = _561 + _472;
      float _563 = _562 + _484;
      float _564 = _563 + _463;
      float _565 = _564 + _547;
      float _566 = _565 + _551;
      float _567 = _566 + _554;
      _569 = _523;
      _570 = _545;
      _571 = _567;
      _572 = 0.0f;
      _573 = 0.0f;
      _574 = 0.0f;
   }
   else
   {
      _569 = _403;
      _570 = _404;
      _571 = _405;
      _572 = _406;
      _573 = _407;
      _574 = _408;
   }
   float _575 = _572 + _569;
   float _576 = _573 + _570;
   float _577 = _574 + _571;
   float _578 = dot(float3(_67, _68, _69), float3(_62, _63, _64));
   float _579 = _578 * 2.0f;
   float _580 = _579 * _62;
   float _581 = _579 * _63;
   float _582 = _579 * _64;
   float _583 = _67 - _580;
   float _584 = _68 - _581;
   float _585 = _69 - _582;
   float _586 = -0.0f - _583;
   float4 _587 = s6c_t.Sample(s6c_s, float3(_586, _584, _585));
   float _594 = _108 * _107;
   float _595 = _77 + 0.75f;
   float _596 = saturate(_595);
   float _597 = _594 * _596;
   float _598 = max(_597, (c_light_vec[5].y));
   float _599 = max(_587.w, 0.003921568859368563f);
   float _600 = 0.25f / _599;
   float _601 = _600 * _587.x;
   float _602 = _600 * _587.y;
   float _603 = _600 * _587.z;
   float _604 = _56.x * TEXCOORD_2.w;
   float _605 = _604 * _598;
   float _606 = _601 * _601;
   float _607 = _606 * _605;
   float _608 = _602 * _602;
   float _609 = _608 * _605;
   float _610 = _603 * _603;
   float _611 = _610 * _605;
   float _612 = _575 + _607;
   float _613 = _576 + _609;
   float _614 = _577 + _611;
   float _621 = _43.w * COLOR.w;
   float _622 = _621 * _56.z;
   float _623 = _622 * c_mat_opacity;
   float _624 = _623 * c_opacity_scale;
   float _625 = _624 * c_global_opacity;
   float _626 = _607 * 4.0f;
   float _627 = _609 * 4.0f;
   float _628 = _611 * 4.0f;
   float _629 = _626 + _572;
   float _630 = _627 + _573;
   float _631 = _628 + _574;
   float _632 = dot(float3(_629, _630, _631), float3(0.33000001311302185f, 0.33000001311302185f, 0.33000001311302185f));
   float _633 = _625 + _632;
   int _636 = c_bool & 16;
   bool _637 = (_636 == 0);
   if (!_637)
   {
      float _641 = c_tone_control.x * _612;
      float _642 = c_tone_control.x * _613;
      float _643 = c_tone_control.x * _614;
      float _645 = log2(_641);
      float _646 = log2(_642);
      float _647 = log2(_643);
      float _648 = _645 * c_tone_control.y;
      float _649 = _646 * c_tone_control.y;
      float _650 = _647 * c_tone_control.y;
      float _651 = exp2(_648);
      float _652 = exp2(_649);
      float _653 = exp2(_650);
      _703 = _651;
      _704 = _652;
      _705 = _653;
   }
   else
   {
      int _655 = c_bool & 32768;
      bool _656 = (_655 == 0);
      if (_656)
      {
         float _660 = c_tone_control.x * _612;
         float _661 = c_tone_control.x * _613;
         float _662 = c_tone_control.x * _614;
         float _663 = _660 * 2.0f;
         float _664 = _661 * 2.0f;
         float _665 = _662 * 2.0f;
         float _666 = _660 * 0.30000001192092896f;
         float _667 = _661 * 0.30000001192092896f;
         float _668 = _662 * 0.30000001192092896f;
         float _669 = _666 + 0.05000000074505806f;
         float _670 = _667 + 0.05000000074505806f;
         float _671 = _668 + 0.05000000074505806f;
         float _672 = _669 * _663;
         float _673 = _670 * _664;
         float _674 = _671 * _665;
         float _675 = _672 + 0.004000000189989805f;
         float _676 = _673 + 0.004000000189989805f;
         float _677 = _674 + 0.004000000189989805f;
         float _678 = _666 + 0.5f;
         float _679 = _667 + 0.5f;
         float _680 = _668 + 0.5f;
         float _681 = _678 * _663;
         float _682 = _679 * _664;
         float _683 = _680 * _665;
         float _684 = _681 + 0.06000000238418579f;
         float _685 = _682 + 0.06000000238418579f;
         float _686 = _683 + 0.06000000238418579f;
         float _687 = _675 / _684;
         float _688 = _676 / _685;
         float _689 = _677 / _686;
         float _690 = _687 + -0.06666666269302368f;
         float _691 = _688 + -0.06666666269302368f;
         float _692 = _689 + -0.06666666269302368f;
         float _693 = _690 * 1.3790643215179443f;
         float _694 = _691 * 1.3790643215179443f;
         float _695 = _692 * 1.3790643215179443f;
         float _696 = YKR_MaterialCurve(_693, _660);
         float _697 = YKR_MaterialCurve(_694, _661);
         float _698 = YKR_MaterialCurve(_695, _662);
         float _699 = sqrt(_696);
         float _700 = sqrt(_697);
         float _701 = sqrt(_698);
         _703 = _699;
         _704 = _700;
         _705 = _701;
      }
      else
      {
         _703 = _612;
         _704 = _613;
         _705 = _614;
      }
   }
   int _706 = c_bool & 4;
   bool _707 = (_706 == 0);
   if (_707)
   {
      float _709 = saturate(COLOR_1.x);
      float _714 = (c_fog_param[0].x) - _703;
      float _715 = (c_fog_param[0].y) - _704;
      float _716 = (c_fog_param[0].z) - _705;
      float _717 = _714 * _709;
      float _718 = _715 * _709;
      float _719 = _716 * _709;
      _721 = _717;
      _722 = _718;
      _723 = _719;
   }
   else
   {
      _721 = COLOR_1.x;
      _722 = COLOR_1.y;
      _723 = COLOR_1.z;
   }
   float _724 = _721 + _703;
   float _725 = _722 + _704;
   float _726 = _723 + _705;
   float _732 = (c_ps_col_scl[0].x) * _724;
   float _733 = (c_ps_col_scl[0].y) * _725;
   float _734 = (c_ps_col_scl[0].z) * _726;
   float _735 = (c_ps_col_scl[0].w) * _633;
   SV_Target.x = _732;
   SV_Target.y = _733;
   SV_Target.z = _734;
   SV_Target.w = _735;
   int _738 = c_bool & 8;
   bool _739 = (_738 != 0);
   float _740 = select(_739, _724, _732);
   float _741 = select(_739, _725, _733);
   float _742 = select(_739, _726, _734);
   float _743 = select(_739, _633, _735);
   SV_Target.x = _740;
   SV_Target.y = _741;
   SV_Target.z = _742;
   SV_Target.w = _743;
   return SV_Target;
}
