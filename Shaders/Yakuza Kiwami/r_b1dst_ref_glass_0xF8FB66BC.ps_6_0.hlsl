// Yakuza Kiwami material r_b1dst[ref][glass] (0xF8FB66BC, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _49 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _54 = _49.x * _49.x;
   float _55 = _49.y * _49.y;
   float _56 = _49.z * _49.z;
   float _60 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _61 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _62 = s3_t.Sample(s3_s, float2(_60, _61));
   float _70 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _71 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _72 = s5_t.Sample(s5_s, float2(_70, _71));
   float _75 = _72.x * 2.0f;
   float _76 = _72.y * 2.0f;
   float _77 = _75 + -1.0f;
   float _78 = _76 + -1.0f;
   float _79 = dot(float2(_77, _78), float2(_77, _78));
   float _80 = 1.0f - _79;
   float _81 = saturate(_80);
   float _82 = sqrt(_81);
   float _83 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _84 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _87 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _90 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _91 = _89 - _90;
   float _92 = _77 * TEXCOORD_6.x;
   float _93 = _77 * TEXCOORD_6.y;
   float _94 = _77 * TEXCOORD_6.z;
   float _95 = _78 * TEXCOORD_6.w;
   float _96 = _95 * _85;
   float _97 = _95 * _88;
   float _98 = _95 * _91;
   float _99 = _82 * TEXCOORD_2.x;
   float _100 = _82 * TEXCOORD_2.y;
   float _101 = _82 * TEXCOORD_2.z;
   float _102 = _99 + _92;
   float _103 = _102 + _96;
   float _104 = _100 + _93;
   float _105 = _104 + _97;
   float _106 = _101 + _94;
   float _107 = _106 + _98;
   float _108 = dot(float3(_103, _105, _107), float3(_103, _105, _107));
   float _109 = rsqrt(_108);
   float _110 = _103 * _109;
   float _111 = _105 * _109;
   float _112 = _107 * _109;
   float _113 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _114 = rsqrt(_113);
   float _115 = _114 * TEXCOORD_3.x;
   float _116 = _114 * TEXCOORD_3.y;
   float _117 = _114 * TEXCOORD_3.z;
   float _122 = -0.0f - (c_light_vec[0].x);
   float _123 = -0.0f - (c_light_vec[0].y);
   float _124 = -0.0f - (c_light_vec[0].z);
   float _125 = dot(float3(_110, _111, _112), float3(_122, _123, _124));
   float _129 = c_rt_size.z * SV_Position.x;
   float _130 = c_rt_size.w * SV_Position.y;
   int _133 = c_bool & 8;
   bool _134 = (_133 == 0);
   float _141;
   float _142;
   float _143;
   float _155;
   float _156;
   float _192;
   float _193;
   float _194;
   float _195;
   float _209;
   float _210;
   float _245;
   float _246;
   float _247;
   float _281;
   float _282;
   float _283;
   float _284;
   float _285;
   float _286;
   float _440;
   float _441;
   float _442;
   float _443;
   float _444;
   float _445;
   float _621;
   float _622;
   float _623;
   float _624;
   float _625;
   float _626;
   float _757;
   float _758;
   float _759;
   float _775;
   float _776;
   float _777;
   if (!_134)
   {
      float4 _136 = s13_t.Sample(s13_s, float2(_129, _130));
      _141 = _136.x;
      _142 = _136.y;
      _143 = _136.w;
   }
   else
   {
      _141 = 1.0f;
      _142 = 1.0f;
      _143 = 1.0f;
   }
   if (!_134)
   {
      float _148 = (c_sdw_param[0].x) * _141;
      float _149 = (c_sdw_param[0].z) * _142;
      float _152 = _148 + (c_sdw_param[0].y);
      float _153 = _149 + (c_sdw_param[0].w);
      _155 = _152;
      _156 = _153;
   }
   else
   {
      _155 = _141;
      _156 = _142;
   }
   float _160 = c_rt_size.z * SV_Position.x;
   float _161 = c_rt_size.w * SV_Position.y;
   float4 _162 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_160, _161)));
   int _169 = c_bool & 16384;
   bool _170 = (_169 == 0);
   if (!_170)
   {
      float _172 = _162.x * 2.0f;
      float _173 = _162.y * 2.0f;
      float _174 = _162.z * 2.0f;
      float _175 = _162.w * 2.0f;
      _192 = _172;
      _193 = _173;
      _194 = _174;
      _195 = _175;
   }
   else
   {
      int _177 = c_bool & 8192;
      bool _178 = (_177 == 0);
      if (!_178)
      {
         float _180 = _162.x * 2.0f;
         float _181 = _162.y * 2.0f;
         float _182 = _162.z * 2.0f;
         float _183 = _162.w * 2.0f;
         _192 = _180;
         _193 = _181;
         _194 = _182;
         _195 = _183;
      }
      else
      {
         float _187 = _162.x / c_tone_control.z;
         float _188 = _162.y / c_tone_control.z;
         float _189 = _162.z / c_tone_control.z;
         float _190 = _162.w / c_tone_control.z;
         _192 = _187;
         _193 = _188;
         _194 = _189;
         _195 = _190;
      }
   }
   int _196 = c_bool & 8;
   bool _197 = (_196 == 0);
   if (!_197)
   {
      float _202 = (c_sdw_param[0].x) * _155;
      float _203 = (c_sdw_param[0].z) * _156;
      float _206 = _202 + (c_sdw_param[0].y);
      float _207 = _203 + (c_sdw_param[0].w);
      _209 = _206;
      _210 = _207;
   }
   else
   {
      _209 = _155;
      _210 = _156;
   }
   float _211 = _210 * _209;
   float _216 = 1.0f - (c_light_col[5].x);
   float _217 = 1.0f - (c_light_col[5].y);
   float _218 = 1.0f - (c_light_col[5].z);
   float _219 = _216 * _211;
   float _220 = _217 * _211;
   float _221 = _218 * _211;
   float _222 = _219 + (c_light_col[5].x);
   float _223 = _220 + (c_light_col[5].y);
   float _224 = _221 + (c_light_col[5].z);
   float _225 = _143 * COLOR.x;
   float _226 = _143 * COLOR.y;
   float _227 = _143 * COLOR.z;
   if (!_197)
   {
      float _231 = (c_sdw_param[2].x) * _222;
      float _232 = (c_sdw_param[2].x) * _223;
      float _233 = (c_sdw_param[2].x) * _224;
      float _235 = _231 + (c_sdw_param[2].y);
      float _236 = _232 + (c_sdw_param[2].y);
      float _237 = _233 + (c_sdw_param[2].y);
      float _240 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _241 = _235 * _240;
      float _242 = _236 * _240;
      float _243 = _237 * _240;
      _245 = _241;
      _246 = _242;
      _247 = _243;
   }
   else
   {
      _245 = _222;
      _246 = _223;
      _247 = _224;
   }
   float _248 = _245 * _143;
   float _249 = _246 * _143;
   float _250 = _247 * _143;
   float _261 = c_mat_emissive.x * _54;
   float _262 = _261 * c_emissive_scale;
   float _263 = c_mat_emissive.y * _55;
   float _264 = _263 * c_emissive_scale;
   float _265 = c_mat_emissive.z * _56;
   float _266 = _265 * c_emissive_scale;
   int _267 = c_bool & 2048;
   bool _268 = (_267 == 0);
   if (_268)
   {
      float _270 = _210 * _192;
      float _271 = _210 * _193;
      float _272 = _210 * _194;
      float _273 = _270 + TEXCOORD_4.x;
      float _274 = _271 + TEXCOORD_4.y;
      float _275 = _272 + TEXCOORD_4.z;
      float _276 = _210 * _195;
      float _277 = _276 + TEXCOORD_5.x;
      float _278 = _276 + TEXCOORD_5.y;
      float _279 = _276 + TEXCOORD_5.z;
      _281 = _273;
      _282 = _274;
      _283 = _275;
      _284 = _277;
      _285 = _278;
      _286 = _279;
   }
   else
   {
      _281 = TEXCOORD_4.x;
      _282 = TEXCOORD_4.y;
      _283 = TEXCOORD_4.z;
      _284 = TEXCOORD_5.x;
      _285 = TEXCOORD_5.y;
      _286 = TEXCOORD_5.z;
   }
   float _291 = -0.0f - (c_light_vec[1].x);
   float _292 = -0.0f - (c_light_vec[1].y);
   float _293 = -0.0f - (c_light_vec[1].z);
   float _294 = dot(float3(_110, _111, _112), float3(_291, _292, _293));
   float _295 = saturate(_294);
   float _300 = saturate(_125);
   float _301 = (c_light_col[0].x) * _300;
   float _302 = (c_light_col[0].y) * _300;
   float _303 = (c_light_col[0].z) * _300;
   float _308 = (c_light_col[1].x) * _295;
   float _309 = (c_light_col[1].y) * _295;
   float _310 = (c_light_col[1].z) * _295;
   float _319 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _320 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _321 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _322 = _301 * _222;
   float _323 = _302 * _223;
   float _324 = _303 * _224;
   float _325 = _308 * _210;
   float _326 = _309 * _210;
   float _327 = _310 * _210;
   float _328 = _325 + _322;
   float _329 = _326 + _323;
   float _330 = _327 + _324;
   float _331 = _319 * _328;
   float _332 = _320 * _329;
   float _333 = _321 * _330;
   float _334 = _262 + c_scene_ambient.x;
   float _335 = _334 + _331;
   float _336 = _264 + c_scene_ambient.y;
   float _337 = _336 + _332;
   float _338 = _266 + c_scene_ambient.z;
   float _339 = _338 + _333;
   float _340 = _335 * _225;
   float _341 = _337 * _226;
   float _342 = _339 * _227;
   float _343 = _281 * _248;
   float _344 = _282 * _249;
   float _345 = _283 * _250;
   float _346 = _340 + _343;
   float _347 = _341 + _344;
   float _348 = _342 + _345;
   float _353 = (c_light_vec[0].x) + _115;
   float _354 = (c_light_vec[0].y) + _116;
   float _355 = (c_light_vec[0].z) + _117;
   float _356 = dot(float3(_353, _354, _355), float3(_353, _354, _355));
   float _357 = rsqrt(_356);
   float _358 = _353 * _357;
   float _359 = -0.0f - _358;
   float _360 = _354 * _357;
   float _361 = -0.0f - _360;
   float _362 = _355 * _357;
   float _363 = -0.0f - _362;
   float _364 = dot(float3(_359, _361, _363), float3(_110, _111, _112));
   float _365 = saturate(_364);
   float _366 = _116 + -0.30000001192092896f;
   float _367 = dot(float3(_115, _366, _117), float3(_115, _366, _117));
   float _368 = rsqrt(_367);
   float _369 = _115 * _368;
   float _370 = -0.0f - _369;
   float _371 = _366 * _368;
   float _372 = -0.0f - _371;
   float _373 = _117 * _368;
   float _374 = -0.0f - _373;
   float _375 = dot(float3(_370, _372, _374), float3(_110, _111, _112));
   float _376 = saturate(_375);
   float _379 = log2(_365);
   float _380 = log2(_376);
   float _381 = _379 * c_mat_power;
   float _382 = _380 * c_mat_power;
   float _383 = exp2(_381);
   float _384 = exp2(_382);
   float _387 = _383 * (c_light_col[0].x);
   float _388 = _383 * (c_light_col[0].y);
   float _389 = _383 * (c_light_col[0].z);
   float _390 = _384 * 2.0f;
   float _391 = _390 * (c_light_vec[5].x);
   float _392 = _391 * (c_light_col[1].x);
   float _393 = _391 * (c_light_col[1].y);
   float _394 = _391 * (c_light_col[1].z);
   float _395 = _387 * _222;
   float _396 = _388 * _223;
   float _397 = _389 * _224;
   float _398 = _392 * _295;
   float _399 = _393 * _295;
   float _400 = _394 * _295;
   float _401 = _398 + _395;
   float _402 = _399 + _396;
   float _403 = _400 + _397;
   float _410 = c_mat_specular.x * c_specular_scale.x;
   float _411 = _410 * _225;
   float _412 = _411 * _401;
   float _413 = c_mat_specular.y * c_specular_scale.y;
   float _414 = _413 * _226;
   float _415 = _414 * _402;
   float _416 = c_mat_specular.z * c_specular_scale.z;
   float _417 = _416 * _227;
   float _418 = _417 * _403;
   float _419 = _284 * _248;
   float _420 = _419 * _384;
   float _421 = _285 * _249;
   float _422 = _421 * _384;
   float _423 = _286 * _250;
   float _424 = _423 * _384;
   float _425 = _412 + _420;
   float _426 = _415 + _422;
   float _427 = _418 + _424;
   if (!_268)
   {
      float _429 = _210 * _192;
      float _430 = _210 * _193;
      float _431 = _210 * _194;
      float _432 = _346 + _429;
      float _433 = _347 + _430;
      float _434 = _348 + _431;
      float _435 = _210 * _195;
      float _436 = _425 + _435;
      float _437 = _426 + _435;
      float _438 = _427 + _435;
      _440 = _432;
      _441 = _433;
      _442 = _434;
      _443 = _436;
      _444 = _437;
      _445 = _438;
   }
   else
   {
      _440 = _346;
      _441 = _347;
      _442 = _348;
      _443 = _425;
      _444 = _426;
      _445 = _427;
   }
   int _446 = c_bool & 128;
   bool _447 = (_446 != 0);
   float _448 = select(_447, 0.0f, _443);
   float _449 = select(_447, 0.0f, _444);
   float _450 = select(_447, 0.0f, _445);
   float _451 = _440 * _54;
   float _452 = _441 * _55;
   float _453 = _442 * _56;
   float _454 = _448 * _62.x;
   float _455 = _449 * _62.y;
   float _456 = _450 * _62.z;
   int _457 = c_bool & 32768;
   bool _458 = (_457 == 0);
   if (!_458)
   {
      float _462 = (c_ps_user[11].x) * _54;
      float _463 = (c_ps_user[11].x) * _55;
      float _464 = (c_ps_user[11].x) * _56;
      float _466 = _110 + 1.0f;
      float _467 = _111 + 1.0f;
      float _468 = _112 + 1.0f;
      float _469 = _466 * 0.5f;
      float _470 = _467 * 0.5f;
      float _471 = _468 * 0.5f;
      float _472 = saturate(_469);
      float _473 = saturate(_470);
      float _474 = saturate(_471);
      float _475 = (c_ps_user[11].y) * _472;
      float _476 = _473 * (c_ps_user[11].y);
      float _477 = _474 * (c_ps_user[11].y);
      float _479 = (c_ps_user[11].z) * _62.x;
      float _480 = (c_ps_user[11].z) * _62.y;
      float _481 = (c_ps_user[11].z) * _62.z;
      float _483 = c_mat_power * 0.009999999776482582f;
      float _484 = _483 * (c_ps_user[11].w);
      float _487 = (c_ps_user[12].x) * _222;
      float _488 = (c_ps_user[12].x) * _223;
      float _489 = (c_ps_user[12].x) * _224;
      float _491 = (c_ps_user[12].y) * _225;
      float _492 = (c_ps_user[12].y) * _226;
      float _493 = (c_ps_user[12].y) * _227;
      float _495 = (c_ps_user[12].z) * _248;
      float _496 = (c_ps_user[12].z) * _249;
      float _497 = (c_ps_user[12].z) * _250;
      float _503 = -0.0f - _115;
      float _504 = -0.0f - _116;
      float _505 = -0.0f - _117;
      float _506 = dot(float3(_503, _504, _505), float3(_110, _111, _112));
      float _507 = 1.0f - _506;
      float _508 = _507 * _507;
      float _509 = _507 * 0.9722222089767456f;
      float _510 = _508 * _508;
      float _511 = _510 * _509;
      float _512 = _511 + 0.02777777425944805f;
      float _513 = _512 * (c_ps_user[13].z);
      float _515 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _516 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _517 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _520 = (c_ps_user[14].x) * _262;
      float _521 = (c_ps_user[14].x) * _264;
      float _522 = (c_ps_user[14].x) * _266;
      float _524 = (c_ps_user[14].y) * _301;
      float _525 = (c_ps_user[14].y) * _302;
      float _526 = (c_ps_user[14].y) * _303;
      float _528 = (c_ps_user[14].z) * _308;
      float _529 = (c_ps_user[14].z) * _309;
      float _530 = (c_ps_user[14].z) * _310;
      float _532 = (c_ps_user[14].w) * _281;
      float _533 = (c_ps_user[14].w) * _282;
      float _534 = (c_ps_user[14].w) * _283;
      float _537 = (c_ps_user[15].x) * _387;
      float _538 = (c_ps_user[15].x) * _388;
      float _539 = (c_ps_user[15].x) * _389;
      float _541 = _410 * _392;
      float _542 = _541 * (c_ps_user[15].y);
      float _543 = _413 * _393;
      float _544 = _543 * (c_ps_user[15].y);
      float _545 = _416 * _394;
      float _546 = _545 * (c_ps_user[15].y);
      float _548 = (c_ps_user[15].z) * _284;
      float _549 = (c_ps_user[15].z) * _285;
      float _550 = (c_ps_user[15].z) * _286;
      float _551 = _542 + _548;
      float _552 = _551 * _384;
      float _553 = _524 + (c_ps_user[13].x);
      float _554 = _553 + _528;
      float _555 = c_diffuse_scale.x * _554;
      float _556 = _555 * c_mat_diffuse.x;
      float _557 = (c_ps_user[13].y) + _479;
      float _558 = _557 + _537;
      float _559 = c_mat_specular.x * _558;
      float _560 = _559 * c_specular_scale.x;
      float _561 = _475 + _462;
      float _562 = _561 + _484;
      float _563 = _562 + (c_ps_user[12].w);
      float _564 = _563 + _487;
      float _565 = _564 + _491;
      float _566 = _565 + _495;
      float _567 = _566 + _515;
      float _568 = _567 + _520;
      float _569 = _568 + _532;
      float _570 = _569 + _513;
      float _571 = _570 + _552;
      float _572 = _571 + _556;
      float _573 = _572 + _560;
      float _574 = _544 + _549;
      float _575 = _574 * _384;
      float _576 = _525 + (c_ps_user[13].x);
      float _577 = _576 + _529;
      float _578 = c_diffuse_scale.y * _577;
      float _579 = _578 * c_mat_diffuse.y;
      float _580 = (c_ps_user[13].y) + _480;
      float _581 = _580 + _538;
      float _582 = c_mat_specular.y * _581;
      float _583 = _582 * c_specular_scale.y;
      float _584 = _476 + _463;
      float _585 = _584 + _484;
      float _586 = _585 + (c_ps_user[12].w);
      float _587 = _586 + _488;
      float _588 = _587 + _492;
      float _589 = _588 + _496;
      float _590 = _589 + _516;
      float _591 = _590 + _521;
      float _592 = _591 + _533;
      float _593 = _592 + _513;
      float _594 = _593 + _575;
      float _595 = _594 + _579;
      float _596 = _595 + _583;
      float _597 = _546 + _550;
      float _598 = _597 * _384;
      float _599 = _526 + (c_ps_user[13].x);
      float _600 = _599 + _530;
      float _601 = c_diffuse_scale.z * _600;
      float _602 = _601 * c_mat_diffuse.z;
      float _603 = (c_ps_user[13].y) + _481;
      float _604 = _603 + _539;
      float _605 = c_mat_specular.z * _604;
      float _606 = _605 * c_specular_scale.z;
      float _607 = _484 + _464;
      float _608 = _607 + _477;
      float _609 = _608 + (c_ps_user[12].w);
      float _610 = _609 + _489;
      float _611 = _610 + _493;
      float _612 = _611 + _497;
      float _613 = _612 + _517;
      float _614 = _613 + _522;
      float _615 = _614 + _534;
      float _616 = _615 + _513;
      float _617 = _616 + _598;
      float _618 = _617 + _602;
      float _619 = _618 + _606;
      _621 = _573;
      _622 = _596;
      _623 = _619;
      _624 = 0.0f;
      _625 = 0.0f;
      _626 = 0.0f;
   }
   else
   {
      _621 = _451;
      _622 = _452;
      _623 = _453;
      _624 = _454;
      _625 = _455;
      _626 = _456;
   }
   float _627 = _624 + _621;
   float _628 = _625 + _622;
   float _629 = _626 + _623;
   float _630 = dot(float3(_115, _116, _117), float3(_110, _111, _112));
   float _631 = _630 * 2.0f;
   float _632 = _631 * _110;
   float _633 = _631 * _111;
   float _634 = _631 * _112;
   float _635 = _115 - _632;
   float _636 = _116 - _633;
   float _637 = _117 - _634;
   float _638 = -0.0f - _635;
   float4 _639 = s6c_t.Sample(s6c_s, float3(_638, _636, _637));
   float _646 = _156 * _155;
   float _647 = _125 + 0.75f;
   float _648 = saturate(_647);
   float _649 = _646 * _648;
   float _650 = max(_649, (c_light_vec[5].y));
   float _651 = max(_639.w, 0.003921568859368563f);
   float _652 = 0.25f / _651;
   float _653 = _652 * _639.x;
   float _654 = _652 * _639.y;
   float _655 = _652 * _639.z;
   float _656 = _62.w * TEXCOORD_2.w;
   float _657 = _656 * _650;
   float _658 = _653 * _653;
   float _659 = _658 * _657;
   float _660 = _654 * _654;
   float _661 = _660 * _657;
   float _662 = _655 * _655;
   float _663 = _662 * _657;
   float _664 = _659 * _62.x;
   float _665 = _661 * _62.y;
   float _666 = _663 * _62.z;
   float _667 = _627 + _664;
   float _668 = _628 + _665;
   float _669 = _629 + _666;
   float _676 = _49.w * COLOR.w;
   float _677 = _676 * c_mat_opacity;
   float _678 = _677 * c_opacity_scale;
   float _679 = _678 * c_global_opacity;
   float _680 = _659 * 4.0f;
   float _681 = _661 * 4.0f;
   float _682 = _663 * 4.0f;
   float _683 = _680 + _624;
   float _684 = _681 + _625;
   float _685 = _682 + _626;
   float _686 = dot(float3(_683, _684, _685), float3(0.33000001311302185f, 0.33000001311302185f, 0.33000001311302185f));
   float _687 = _679 + _686;
   int _690 = c_bool & 16;
   bool _691 = (_690 == 0);
   if (!_691)
   {
      float _695 = c_tone_control.x * _667;
      float _696 = c_tone_control.x * _668;
      float _697 = c_tone_control.x * _669;
      float _699 = log2(_695);
      float _700 = log2(_696);
      float _701 = log2(_697);
      float _702 = _699 * c_tone_control.y;
      float _703 = _700 * c_tone_control.y;
      float _704 = _701 * c_tone_control.y;
      float _705 = exp2(_702);
      float _706 = exp2(_703);
      float _707 = exp2(_704);
      _757 = _705;
      _758 = _706;
      _759 = _707;
   }
   else
   {
      int _709 = c_bool & 32768;
      bool _710 = (_709 == 0);
      if (_710)
      {
         float _714 = c_tone_control.x * _667;
         float _715 = c_tone_control.x * _668;
         float _716 = c_tone_control.x * _669;
         float _717 = _714 * 2.0f;
         float _718 = _715 * 2.0f;
         float _719 = _716 * 2.0f;
         float _720 = _714 * 0.30000001192092896f;
         float _721 = _715 * 0.30000001192092896f;
         float _722 = _716 * 0.30000001192092896f;
         float _723 = _720 + 0.05000000074505806f;
         float _724 = _721 + 0.05000000074505806f;
         float _725 = _722 + 0.05000000074505806f;
         float _726 = _723 * _717;
         float _727 = _724 * _718;
         float _728 = _725 * _719;
         float _729 = _726 + 0.004000000189989805f;
         float _730 = _727 + 0.004000000189989805f;
         float _731 = _728 + 0.004000000189989805f;
         float _732 = _720 + 0.5f;
         float _733 = _721 + 0.5f;
         float _734 = _722 + 0.5f;
         float _735 = _732 * _717;
         float _736 = _733 * _718;
         float _737 = _734 * _719;
         float _738 = _735 + 0.06000000238418579f;
         float _739 = _736 + 0.06000000238418579f;
         float _740 = _737 + 0.06000000238418579f;
         float _741 = _729 / _738;
         float _742 = _730 / _739;
         float _743 = _731 / _740;
         float _744 = _741 + -0.06666666269302368f;
         float _745 = _742 + -0.06666666269302368f;
         float _746 = _743 + -0.06666666269302368f;
         float _747 = _744 * 1.3790643215179443f;
         float _748 = _745 * 1.3790643215179443f;
         float _749 = _746 * 1.3790643215179443f;
         float _750 = YKR_MaterialCurve(_747, _714);
         float _751 = YKR_MaterialCurve(_748, _715);
         float _752 = YKR_MaterialCurve(_749, _716);
         float _753 = sqrt(_750);
         float _754 = sqrt(_751);
         float _755 = sqrt(_752);
         _757 = _753;
         _758 = _754;
         _759 = _755;
      }
      else
      {
         _757 = _667;
         _758 = _668;
         _759 = _669;
      }
   }
   int _760 = c_bool & 4;
   bool _761 = (_760 == 0);
   if (_761)
   {
      float _763 = saturate(COLOR_1.x);
      float _768 = (c_fog_param[0].x) - _757;
      float _769 = (c_fog_param[0].y) - _758;
      float _770 = (c_fog_param[0].z) - _759;
      float _771 = _768 * _763;
      float _772 = _769 * _763;
      float _773 = _770 * _763;
      _775 = _771;
      _776 = _772;
      _777 = _773;
   }
   else
   {
      _775 = COLOR_1.x;
      _776 = COLOR_1.y;
      _777 = COLOR_1.z;
   }
   float _778 = _775 + _757;
   float _779 = _776 + _758;
   float _780 = _777 + _759;
   float _786 = (c_ps_col_scl[0].x) * _778;
   float _787 = (c_ps_col_scl[0].y) * _779;
   float _788 = (c_ps_col_scl[0].z) * _780;
   float _789 = (c_ps_col_scl[0].w) * _687;
   SV_Target.x = _786;
   SV_Target.y = _787;
   SV_Target.z = _788;
   SV_Target.w = _789;
   int _792 = c_bool & 8;
   bool _793 = (_792 != 0);
   float _794 = select(_793, _778, _786);
   float _795 = select(_793, _779, _787);
   float _796 = select(_793, _780, _788);
   float _797 = select(_793, _687, _789);
   SV_Target.x = _794;
   SV_Target.y = _795;
   SV_Target.z = _796;
   SV_Target.w = _797;
   return SV_Target;
}
