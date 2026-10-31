// Yakuza Kiwami material r_o1m2t_m3d (0xEE5F5F8D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s2_s : register(s2);

SamplerState s5_s : register(s5);

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
   float4 _49 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _54 = _49.x * _49.x;
   float _55 = _49.y * _49.y;
   float _56 = _49.z * _49.z;
   float4 _57 = s5_t.Sample(s5_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _60 = s2_t.Sample(s2_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _64 = _57.x * 2.0f;
   float _65 = _57.y * 2.0f;
   float _66 = _64 + -1.0f;
   float _67 = _65 + -1.0f;
   float _68 = dot(float2(_66, _67), float2(_66, _67));
   float _69 = 1.0f - _68;
   float _70 = saturate(_69);
   float _71 = sqrt(_70);
   float _72 = _54 * _60.x;
   float _73 = _55 * _60.y;
   float _74 = _56 * _60.z;
   float _75 = _60.x * _49.w;
   float _76 = _60.y * _49.w;
   float _77 = _60.z * _49.w;
   float _78 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _79 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _80 = _78 - _79;
   float _81 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _82 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _83 = _81 - _82;
   float _84 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _85 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _86 = _84 - _85;
   float _87 = _66 * TEXCOORD_6.x;
   float _88 = _66 * TEXCOORD_6.y;
   float _89 = _66 * TEXCOORD_6.z;
   float _90 = _67 * TEXCOORD_6.w;
   float _91 = _90 * _80;
   float _92 = _90 * _83;
   float _93 = _90 * _86;
   float _94 = _91 + _87;
   float _95 = _92 + _88;
   float _96 = _93 + _89;
   float _97 = _71 * TEXCOORD_2.x;
   float _98 = _71 * TEXCOORD_2.y;
   float _99 = _71 * TEXCOORD_2.z;
   float _100 = _94 + _97;
   float _101 = _95 + _98;
   float _102 = _96 + _99;
   float _103 = dot(float3(_100, _101, _102), float3(_100, _101, _102));
   float _104 = rsqrt(_103);
   float _105 = _100 * _104;
   float _106 = _101 * _104;
   float _107 = _102 * _104;
   float _108 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _109 = rsqrt(_108);
   float _110 = _109 * TEXCOORD_3.x;
   float _111 = _109 * TEXCOORD_3.y;
   float _112 = _109 * TEXCOORD_3.z;
   float _117 = -0.0f - (c_light_vec[0].x);
   float _118 = -0.0f - (c_light_vec[0].y);
   float _119 = -0.0f - (c_light_vec[0].z);
   float _120 = dot(float3(_105, _106, _107), float3(_117, _118, _119));
   float _124 = c_rt_size.z * SV_Position.x;
   float _125 = c_rt_size.w * SV_Position.y;
   int _128 = c_bool & 8;
   bool _129 = (_128 == 0);
   float _136;
   float _137;
   float _138;
   float _150;
   float _151;
   float _187;
   float _188;
   float _189;
   float _190;
   float _204;
   float _205;
   float _240;
   float _241;
   float _242;
   float _276;
   float _277;
   float _278;
   float _279;
   float _280;
   float _281;
   float _435;
   float _436;
   float _437;
   float _438;
   float _439;
   float _440;
   float _616;
   float _617;
   float _618;
   float _619;
   float _620;
   float _621;
   float _696;
   float _697;
   float _698;
   float _714;
   float _715;
   float _716;
   if (!_129)
   {
      float4 _131 = s13_t.Sample(s13_s, float2(_124, _125));
      _136 = _131.x;
      _137 = _131.y;
      _138 = _131.w;
   }
   else
   {
      _136 = 1.0f;
      _137 = 1.0f;
      _138 = 1.0f;
   }
   if (!_129)
   {
      float _143 = (c_sdw_param[0].x) * _136;
      float _144 = (c_sdw_param[0].z) * _137;
      float _147 = _143 + (c_sdw_param[0].y);
      float _148 = _144 + (c_sdw_param[0].w);
      _150 = _147;
      _151 = _148;
   }
   else
   {
      _150 = _136;
      _151 = _137;
   }
   float _155 = c_rt_size.z * SV_Position.x;
   float _156 = c_rt_size.w * SV_Position.y;
   float4 _157 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_155, _156)));
   int _164 = c_bool & 16384;
   bool _165 = (_164 == 0);
   if (!_165)
   {
      float _167 = _157.x * 2.0f;
      float _168 = _157.y * 2.0f;
      float _169 = _157.z * 2.0f;
      float _170 = _157.w * 2.0f;
      _187 = _167;
      _188 = _168;
      _189 = _169;
      _190 = _170;
   }
   else
   {
      int _172 = c_bool & 8192;
      bool _173 = (_172 == 0);
      if (!_173)
      {
         float _175 = _157.x * 2.0f;
         float _176 = _157.y * 2.0f;
         float _177 = _157.z * 2.0f;
         float _178 = _157.w * 2.0f;
         _187 = _175;
         _188 = _176;
         _189 = _177;
         _190 = _178;
      }
      else
      {
         float _182 = _157.x / c_tone_control.z;
         float _183 = _157.y / c_tone_control.z;
         float _184 = _157.z / c_tone_control.z;
         float _185 = _157.w / c_tone_control.z;
         _187 = _182;
         _188 = _183;
         _189 = _184;
         _190 = _185;
      }
   }
   int _191 = c_bool & 8;
   bool _192 = (_191 == 0);
   if (!_192)
   {
      float _197 = (c_sdw_param[0].x) * _150;
      float _198 = (c_sdw_param[0].z) * _151;
      float _201 = _197 + (c_sdw_param[0].y);
      float _202 = _198 + (c_sdw_param[0].w);
      _204 = _201;
      _205 = _202;
   }
   else
   {
      _204 = _150;
      _205 = _151;
   }
   float _206 = _205 * _204;
   float _211 = 1.0f - (c_light_col[5].x);
   float _212 = 1.0f - (c_light_col[5].y);
   float _213 = 1.0f - (c_light_col[5].z);
   float _214 = _211 * _206;
   float _215 = _212 * _206;
   float _216 = _213 * _206;
   float _217 = _214 + (c_light_col[5].x);
   float _218 = _215 + (c_light_col[5].y);
   float _219 = _216 + (c_light_col[5].z);
   float _220 = _138 * COLOR.x;
   float _221 = _138 * COLOR.y;
   float _222 = _138 * COLOR.z;
   if (!_192)
   {
      float _226 = (c_sdw_param[2].x) * _217;
      float _227 = (c_sdw_param[2].x) * _218;
      float _228 = (c_sdw_param[2].x) * _219;
      float _230 = _226 + (c_sdw_param[2].y);
      float _231 = _227 + (c_sdw_param[2].y);
      float _232 = _228 + (c_sdw_param[2].y);
      float _235 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _236 = _230 * _235;
      float _237 = _231 * _235;
      float _238 = _232 * _235;
      _240 = _236;
      _241 = _237;
      _242 = _238;
   }
   else
   {
      _240 = _217;
      _241 = _218;
      _242 = _219;
   }
   float _243 = _240 * _138;
   float _244 = _241 * _138;
   float _245 = _242 * _138;
   float _256 = c_mat_emissive.x * _72;
   float _257 = _256 * c_emissive_scale;
   float _258 = c_mat_emissive.y * _73;
   float _259 = _258 * c_emissive_scale;
   float _260 = c_mat_emissive.z * _74;
   float _261 = _260 * c_emissive_scale;
   int _262 = c_bool & 2048;
   bool _263 = (_262 == 0);
   if (_263)
   {
      float _265 = _205 * _187;
      float _266 = _205 * _188;
      float _267 = _205 * _189;
      float _268 = _265 + TEXCOORD_4.x;
      float _269 = _266 + TEXCOORD_4.y;
      float _270 = _267 + TEXCOORD_4.z;
      float _271 = _205 * _190;
      float _272 = _271 + TEXCOORD_5.x;
      float _273 = _271 + TEXCOORD_5.y;
      float _274 = _271 + TEXCOORD_5.z;
      _276 = _272;
      _277 = _273;
      _278 = _274;
      _279 = _268;
      _280 = _269;
      _281 = _270;
   }
   else
   {
      _276 = TEXCOORD_5.x;
      _277 = TEXCOORD_5.y;
      _278 = TEXCOORD_5.z;
      _279 = TEXCOORD_4.x;
      _280 = TEXCOORD_4.y;
      _281 = TEXCOORD_4.z;
   }
   float _286 = -0.0f - (c_light_vec[1].x);
   float _287 = -0.0f - (c_light_vec[1].y);
   float _288 = -0.0f - (c_light_vec[1].z);
   float _289 = dot(float3(_105, _106, _107), float3(_286, _287, _288));
   float _290 = saturate(_289);
   float _295 = saturate(_120);
   float _296 = (c_light_col[0].x) * _295;
   float _297 = (c_light_col[0].y) * _295;
   float _298 = (c_light_col[0].z) * _295;
   float _303 = (c_light_col[1].x) * _290;
   float _304 = (c_light_col[1].y) * _290;
   float _305 = (c_light_col[1].z) * _290;
   float _314 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _315 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _316 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _317 = _296 * _217;
   float _318 = _297 * _218;
   float _319 = _298 * _219;
   float _320 = _303 * _205;
   float _321 = _304 * _205;
   float _322 = _305 * _205;
   float _323 = _320 + _317;
   float _324 = _321 + _318;
   float _325 = _322 + _319;
   float _326 = _314 * _323;
   float _327 = _315 * _324;
   float _328 = _316 * _325;
   float _329 = _257 + c_scene_ambient.x;
   float _330 = _329 + _326;
   float _331 = _259 + c_scene_ambient.y;
   float _332 = _331 + _327;
   float _333 = _261 + c_scene_ambient.z;
   float _334 = _333 + _328;
   float _335 = _330 * _220;
   float _336 = _332 * _221;
   float _337 = _334 * _222;
   float _338 = _279 * _243;
   float _339 = _280 * _244;
   float _340 = _281 * _245;
   float _341 = _335 + _338;
   float _342 = _336 + _339;
   float _343 = _337 + _340;
   float _348 = (c_light_vec[0].x) + _110;
   float _349 = (c_light_vec[0].y) + _111;
   float _350 = (c_light_vec[0].z) + _112;
   float _351 = dot(float3(_348, _349, _350), float3(_348, _349, _350));
   float _352 = rsqrt(_351);
   float _353 = _348 * _352;
   float _354 = -0.0f - _353;
   float _355 = _349 * _352;
   float _356 = -0.0f - _355;
   float _357 = _350 * _352;
   float _358 = -0.0f - _357;
   float _359 = dot(float3(_354, _356, _358), float3(_105, _106, _107));
   float _360 = saturate(_359);
   float _361 = _111 + -0.30000001192092896f;
   float _362 = dot(float3(_110, _361, _112), float3(_110, _361, _112));
   float _363 = rsqrt(_362);
   float _364 = _110 * _363;
   float _365 = -0.0f - _364;
   float _366 = _361 * _363;
   float _367 = -0.0f - _366;
   float _368 = _112 * _363;
   float _369 = -0.0f - _368;
   float _370 = dot(float3(_365, _367, _369), float3(_105, _106, _107));
   float _371 = saturate(_370);
   float _374 = log2(_360);
   float _375 = log2(_371);
   float _376 = _374 * c_mat_power;
   float _377 = _375 * c_mat_power;
   float _378 = exp2(_376);
   float _379 = exp2(_377);
   float _382 = _378 * (c_light_col[0].x);
   float _383 = _378 * (c_light_col[0].y);
   float _384 = _378 * (c_light_col[0].z);
   float _385 = _379 * 2.0f;
   float _386 = _385 * (c_light_vec[5].x);
   float _387 = _386 * (c_light_col[1].x);
   float _388 = _386 * (c_light_col[1].y);
   float _389 = _386 * (c_light_col[1].z);
   float _390 = _382 * _217;
   float _391 = _383 * _218;
   float _392 = _384 * _219;
   float _393 = _387 * _290;
   float _394 = _388 * _290;
   float _395 = _389 * _290;
   float _396 = _393 + _390;
   float _397 = _394 + _391;
   float _398 = _395 + _392;
   float _405 = c_mat_specular.x * c_specular_scale.x;
   float _406 = _405 * _220;
   float _407 = _406 * _396;
   float _408 = c_mat_specular.y * c_specular_scale.y;
   float _409 = _408 * _221;
   float _410 = _409 * _397;
   float _411 = c_mat_specular.z * c_specular_scale.z;
   float _412 = _411 * _222;
   float _413 = _412 * _398;
   float _414 = _276 * _243;
   float _415 = _414 * _379;
   float _416 = _277 * _244;
   float _417 = _416 * _379;
   float _418 = _278 * _245;
   float _419 = _418 * _379;
   float _420 = _407 + _415;
   float _421 = _410 + _417;
   float _422 = _413 + _419;
   if (!_263)
   {
      float _424 = _205 * _187;
      float _425 = _205 * _188;
      float _426 = _205 * _189;
      float _427 = _341 + _424;
      float _428 = _342 + _425;
      float _429 = _343 + _426;
      float _430 = _205 * _190;
      float _431 = _420 + _430;
      float _432 = _421 + _430;
      float _433 = _422 + _430;
      _435 = _427;
      _436 = _428;
      _437 = _429;
      _438 = _431;
      _439 = _432;
      _440 = _433;
   }
   else
   {
      _435 = _341;
      _436 = _342;
      _437 = _343;
      _438 = _420;
      _439 = _421;
      _440 = _422;
   }
   int _441 = c_bool & 128;
   bool _442 = (_441 != 0);
   float _443 = select(_442, 0.0f, _438);
   float _444 = select(_442, 0.0f, _439);
   float _445 = select(_442, 0.0f, _440);
   float _446 = _435 * _72;
   float _447 = _436 * _73;
   float _448 = _437 * _74;
   float _449 = _443 * _75;
   float _450 = _444 * _76;
   float _451 = _445 * _77;
   int _452 = c_bool & 32768;
   bool _453 = (_452 == 0);
   if (!_453)
   {
      float _457 = (c_ps_user[11].x) * _72;
      float _458 = (c_ps_user[11].x) * _73;
      float _459 = (c_ps_user[11].x) * _74;
      float _461 = _105 + 1.0f;
      float _462 = _106 + 1.0f;
      float _463 = _107 + 1.0f;
      float _464 = _461 * 0.5f;
      float _465 = _462 * 0.5f;
      float _466 = _463 * 0.5f;
      float _467 = saturate(_464);
      float _468 = saturate(_465);
      float _469 = saturate(_466);
      float _470 = (c_ps_user[11].y) * _467;
      float _471 = _468 * (c_ps_user[11].y);
      float _472 = _469 * (c_ps_user[11].y);
      float _474 = (c_ps_user[11].z) * _75;
      float _475 = (c_ps_user[11].z) * _76;
      float _476 = (c_ps_user[11].z) * _77;
      float _478 = c_mat_power * 0.009999999776482582f;
      float _479 = _478 * (c_ps_user[11].w);
      float _482 = (c_ps_user[12].x) * _217;
      float _483 = (c_ps_user[12].x) * _218;
      float _484 = (c_ps_user[12].x) * _219;
      float _486 = (c_ps_user[12].y) * _220;
      float _487 = (c_ps_user[12].y) * _221;
      float _488 = (c_ps_user[12].y) * _222;
      float _490 = (c_ps_user[12].z) * _243;
      float _491 = (c_ps_user[12].z) * _244;
      float _492 = (c_ps_user[12].z) * _245;
      float _498 = -0.0f - _110;
      float _499 = -0.0f - _111;
      float _500 = -0.0f - _112;
      float _501 = dot(float3(_498, _499, _500), float3(_105, _106, _107));
      float _502 = 1.0f - _501;
      float _503 = _502 * _502;
      float _504 = _502 * 0.9722222089767456f;
      float _505 = _503 * _503;
      float _506 = _505 * _504;
      float _507 = _506 + 0.02777777425944805f;
      float _508 = _507 * (c_ps_user[13].z);
      float _510 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _511 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _512 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _515 = (c_ps_user[14].x) * _257;
      float _516 = (c_ps_user[14].x) * _259;
      float _517 = (c_ps_user[14].x) * _261;
      float _519 = (c_ps_user[14].y) * _296;
      float _520 = (c_ps_user[14].y) * _297;
      float _521 = (c_ps_user[14].y) * _298;
      float _523 = (c_ps_user[14].z) * _303;
      float _524 = (c_ps_user[14].z) * _304;
      float _525 = (c_ps_user[14].z) * _305;
      float _527 = (c_ps_user[14].w) * _279;
      float _528 = (c_ps_user[14].w) * _280;
      float _529 = (c_ps_user[14].w) * _281;
      float _532 = (c_ps_user[15].x) * _382;
      float _533 = (c_ps_user[15].x) * _383;
      float _534 = (c_ps_user[15].x) * _384;
      float _536 = _405 * _387;
      float _537 = _536 * (c_ps_user[15].y);
      float _538 = _408 * _388;
      float _539 = _538 * (c_ps_user[15].y);
      float _540 = _411 * _389;
      float _541 = _540 * (c_ps_user[15].y);
      float _543 = (c_ps_user[15].z) * _276;
      float _544 = (c_ps_user[15].z) * _277;
      float _545 = (c_ps_user[15].z) * _278;
      float _546 = _537 + _543;
      float _547 = _546 * _379;
      float _548 = _519 + (c_ps_user[13].x);
      float _549 = _548 + _523;
      float _550 = c_diffuse_scale.x * _549;
      float _551 = _550 * c_mat_diffuse.x;
      float _552 = (c_ps_user[13].y) + _474;
      float _553 = _552 + _532;
      float _554 = c_mat_specular.x * _553;
      float _555 = _554 * c_specular_scale.x;
      float _556 = _470 + _457;
      float _557 = _556 + _479;
      float _558 = _557 + (c_ps_user[12].w);
      float _559 = _558 + _482;
      float _560 = _559 + _486;
      float _561 = _560 + _490;
      float _562 = _561 + _510;
      float _563 = _562 + _515;
      float _564 = _563 + _527;
      float _565 = _564 + _508;
      float _566 = _565 + _547;
      float _567 = _566 + _551;
      float _568 = _567 + _555;
      float _569 = _539 + _544;
      float _570 = _569 * _379;
      float _571 = _520 + (c_ps_user[13].x);
      float _572 = _571 + _524;
      float _573 = c_diffuse_scale.y * _572;
      float _574 = _573 * c_mat_diffuse.y;
      float _575 = (c_ps_user[13].y) + _475;
      float _576 = _575 + _533;
      float _577 = c_mat_specular.y * _576;
      float _578 = _577 * c_specular_scale.y;
      float _579 = _471 + _458;
      float _580 = _579 + _479;
      float _581 = _580 + (c_ps_user[12].w);
      float _582 = _581 + _483;
      float _583 = _582 + _487;
      float _584 = _583 + _491;
      float _585 = _584 + _511;
      float _586 = _585 + _516;
      float _587 = _586 + _528;
      float _588 = _587 + _508;
      float _589 = _588 + _570;
      float _590 = _589 + _574;
      float _591 = _590 + _578;
      float _592 = _541 + _545;
      float _593 = _592 * _379;
      float _594 = _521 + (c_ps_user[13].x);
      float _595 = _594 + _525;
      float _596 = c_diffuse_scale.z * _595;
      float _597 = _596 * c_mat_diffuse.z;
      float _598 = (c_ps_user[13].y) + _476;
      float _599 = _598 + _534;
      float _600 = c_mat_specular.z * _599;
      float _601 = _600 * c_specular_scale.z;
      float _602 = _479 + _459;
      float _603 = _602 + _472;
      float _604 = _603 + (c_ps_user[12].w);
      float _605 = _604 + _484;
      float _606 = _605 + _488;
      float _607 = _606 + _492;
      float _608 = _607 + _512;
      float _609 = _608 + _517;
      float _610 = _609 + _529;
      float _611 = _610 + _508;
      float _612 = _611 + _593;
      float _613 = _612 + _597;
      float _614 = _613 + _601;
      _616 = 0.0f;
      _617 = 0.0f;
      _618 = 0.0f;
      _619 = _568;
      _620 = _591;
      _621 = _614;
   }
   else
   {
      _616 = _449;
      _617 = _450;
      _618 = _451;
      _619 = _446;
      _620 = _447;
      _621 = _448;
   }
   float _622 = _619 + _616;
   float _623 = _620 + _617;
   float _624 = _621 + _618;
   float _629 = c_global_opacity * c_mat_opacity;
   float _630 = _629 * c_opacity_scale;
   int _631 = c_bool & 16;
   bool _632 = (_631 == 0);
   if (!_632)
   {
      float _636 = c_tone_control.x * _622;
      float _637 = c_tone_control.x * _623;
      float _638 = c_tone_control.x * _624;
      float _640 = log2(_636);
      float _641 = log2(_637);
      float _642 = log2(_638);
      float _643 = _640 * c_tone_control.y;
      float _644 = _641 * c_tone_control.y;
      float _645 = _642 * c_tone_control.y;
      float _646 = exp2(_643);
      float _647 = exp2(_644);
      float _648 = exp2(_645);
      _696 = _646;
      _697 = _647;
      _698 = _648;
   }
   else
   {
      if (_453)
      {
         float _653 = c_tone_control.x * _622;
         float _654 = c_tone_control.x * _623;
         float _655 = c_tone_control.x * _624;
         float _656 = _653 * 2.0f;
         float _657 = _654 * 2.0f;
         float _658 = _655 * 2.0f;
         float _659 = _653 * 0.30000001192092896f;
         float _660 = _654 * 0.30000001192092896f;
         float _661 = _655 * 0.30000001192092896f;
         float _662 = _659 + 0.05000000074505806f;
         float _663 = _660 + 0.05000000074505806f;
         float _664 = _661 + 0.05000000074505806f;
         float _665 = _662 * _656;
         float _666 = _663 * _657;
         float _667 = _664 * _658;
         float _668 = _665 + 0.004000000189989805f;
         float _669 = _666 + 0.004000000189989805f;
         float _670 = _667 + 0.004000000189989805f;
         float _671 = _659 + 0.5f;
         float _672 = _660 + 0.5f;
         float _673 = _661 + 0.5f;
         float _674 = _671 * _656;
         float _675 = _672 * _657;
         float _676 = _673 * _658;
         float _677 = _674 + 0.06000000238418579f;
         float _678 = _675 + 0.06000000238418579f;
         float _679 = _676 + 0.06000000238418579f;
         float _680 = _668 / _677;
         float _681 = _669 / _678;
         float _682 = _670 / _679;
         float _683 = _680 + -0.06666666269302368f;
         float _684 = _681 + -0.06666666269302368f;
         float _685 = _682 + -0.06666666269302368f;
         float _686 = _683 * 1.3790643215179443f;
         float _687 = _684 * 1.3790643215179443f;
         float _688 = _685 * 1.3790643215179443f;
         float _689 = YKR_MaterialCurve(_686, _653);
         float _690 = YKR_MaterialCurve(_687, _654);
         float _691 = YKR_MaterialCurve(_688, _655);
         float _692 = sqrt(_689);
         float _693 = sqrt(_690);
         float _694 = sqrt(_691);
         _696 = _692;
         _697 = _693;
         _698 = _694;
      }
      else
      {
         _696 = _622;
         _697 = _623;
         _698 = _624;
      }
   }
   int _699 = c_bool & 4;
   bool _700 = (_699 == 0);
   if (_700)
   {
      float _702 = saturate(COLOR_1.x);
      float _707 = (c_fog_param[0].x) - _696;
      float _708 = (c_fog_param[0].y) - _697;
      float _709 = (c_fog_param[0].z) - _698;
      float _710 = _707 * _702;
      float _711 = _708 * _702;
      float _712 = _709 * _702;
      _714 = _710;
      _715 = _711;
      _716 = _712;
   }
   else
   {
      _714 = COLOR_1.x;
      _715 = COLOR_1.y;
      _716 = COLOR_1.z;
   }
   float _717 = _714 + _696;
   float _718 = _715 + _697;
   float _719 = _716 + _698;
   float _725 = (c_ps_col_scl[0].x) * _717;
   float _726 = (c_ps_col_scl[0].y) * _718;
   float _727 = (c_ps_col_scl[0].z) * _719;
   float _728 = (c_ps_col_scl[0].w) * _630;
   SV_Target.x = _725;
   SV_Target.y = _726;
   SV_Target.z = _727;
   SV_Target.w = _728;
   int _731 = c_bool & 8;
   bool _732 = (_731 != 0);
   float _733 = select(_732, _717, _725);
   float _734 = select(_732, _718, _726);
   float _735 = select(_732, _719, _727);
   float _736 = select(_732, _630, _728);
   SV_Target.x = _733;
   SV_Target.y = _734;
   SV_Target.z = _735;
   SV_Target.w = _736;
   return SV_Target;
}
