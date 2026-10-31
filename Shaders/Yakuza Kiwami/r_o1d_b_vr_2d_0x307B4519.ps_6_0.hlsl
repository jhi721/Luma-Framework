// Yakuza Kiwami material r_o1d_b(vr)2d (0x307B4519, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _42 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _47 = _42.x * _42.x;
   float _48 = _42.y * _42.y;
   float _49 = _42.z * _42.z;
   float4 _50 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float _58 = _50.w * COLOR_1.w;
   float _59 = _55 - _47;
   float _60 = _56 - _48;
   float _61 = _57 - _49;
   float _62 = _59 * _58;
   float _63 = _60 * _58;
   float _64 = _61 * _58;
   float _65 = _62 + _47;
   float _66 = _63 + _48;
   float _67 = _64 + _49;
   float _68 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _69 = rsqrt(_68);
   float _70 = _69 * TEXCOORD_2.x;
   float _71 = _69 * TEXCOORD_2.y;
   float _72 = _69 * TEXCOORD_2.z;
   float _73 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _74 = rsqrt(_73);
   float _75 = _74 * TEXCOORD_3.x;
   float _76 = _74 * TEXCOORD_3.y;
   float _77 = _74 * TEXCOORD_3.z;
   float _82 = -0.0f - (c_light_vec[0].x);
   float _83 = -0.0f - (c_light_vec[0].y);
   float _84 = -0.0f - (c_light_vec[0].z);
   float _85 = dot(float3(_70, _71, _72), float3(_82, _83, _84));
   float _89 = c_rt_size.z * SV_Position.x;
   float _90 = c_rt_size.w * SV_Position.y;
   int _93 = c_bool & 8;
   bool _94 = (_93 == 0);
   float _101;
   float _102;
   float _103;
   float _115;
   float _116;
   float _152;
   float _153;
   float _154;
   float _155;
   float _169;
   float _170;
   float _205;
   float _206;
   float _207;
   float _241;
   float _242;
   float _243;
   float _244;
   float _245;
   float _246;
   float _400;
   float _401;
   float _402;
   float _403;
   float _404;
   float _405;
   float _573;
   float _574;
   float _575;
   float _576;
   float _577;
   float _578;
   float _654;
   float _655;
   float _656;
   float _672;
   float _673;
   float _674;
   if (!_94)
   {
      float4 _96 = s13_t.Sample(s13_s, float2(_89, _90));
      _101 = _96.x;
      _102 = _96.y;
      _103 = _96.w;
   }
   else
   {
      _101 = 1.0f;
      _102 = 1.0f;
      _103 = 1.0f;
   }
   if (!_94)
   {
      float _108 = (c_sdw_param[0].x) * _101;
      float _109 = (c_sdw_param[0].z) * _102;
      float _112 = _108 + (c_sdw_param[0].y);
      float _113 = _109 + (c_sdw_param[0].w);
      _115 = _112;
      _116 = _113;
   }
   else
   {
      _115 = _101;
      _116 = _102;
   }
   float _120 = c_rt_size.z * SV_Position.x;
   float _121 = c_rt_size.w * SV_Position.y;
   float4 _122 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_120, _121)));
   int _129 = c_bool & 16384;
   bool _130 = (_129 == 0);
   if (!_130)
   {
      float _132 = _122.x * 2.0f;
      float _133 = _122.y * 2.0f;
      float _134 = _122.z * 2.0f;
      float _135 = _122.w * 2.0f;
      _152 = _132;
      _153 = _133;
      _154 = _134;
      _155 = _135;
   }
   else
   {
      int _137 = c_bool & 8192;
      bool _138 = (_137 == 0);
      if (!_138)
      {
         float _140 = _122.x * 2.0f;
         float _141 = _122.y * 2.0f;
         float _142 = _122.z * 2.0f;
         float _143 = _122.w * 2.0f;
         _152 = _140;
         _153 = _141;
         _154 = _142;
         _155 = _143;
      }
      else
      {
         float _147 = _122.x / c_tone_control.z;
         float _148 = _122.y / c_tone_control.z;
         float _149 = _122.z / c_tone_control.z;
         float _150 = _122.w / c_tone_control.z;
         _152 = _147;
         _153 = _148;
         _154 = _149;
         _155 = _150;
      }
   }
   int _156 = c_bool & 8;
   bool _157 = (_156 == 0);
   if (!_157)
   {
      float _162 = (c_sdw_param[0].x) * _115;
      float _163 = (c_sdw_param[0].z) * _116;
      float _166 = _162 + (c_sdw_param[0].y);
      float _167 = _163 + (c_sdw_param[0].w);
      _169 = _166;
      _170 = _167;
   }
   else
   {
      _169 = _115;
      _170 = _116;
   }
   float _171 = _170 * _169;
   float _176 = 1.0f - (c_light_col[5].x);
   float _177 = 1.0f - (c_light_col[5].y);
   float _178 = 1.0f - (c_light_col[5].z);
   float _179 = _176 * _171;
   float _180 = _177 * _171;
   float _181 = _178 * _171;
   float _182 = _179 + (c_light_col[5].x);
   float _183 = _180 + (c_light_col[5].y);
   float _184 = _181 + (c_light_col[5].z);
   float _185 = _103 * COLOR.x;
   float _186 = _103 * COLOR.y;
   float _187 = _103 * COLOR.z;
   if (!_157)
   {
      float _191 = (c_sdw_param[2].x) * _182;
      float _192 = (c_sdw_param[2].x) * _183;
      float _193 = (c_sdw_param[2].x) * _184;
      float _195 = _191 + (c_sdw_param[2].y);
      float _196 = _192 + (c_sdw_param[2].y);
      float _197 = _193 + (c_sdw_param[2].y);
      float _200 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _201 = _195 * _200;
      float _202 = _196 * _200;
      float _203 = _197 * _200;
      _205 = _201;
      _206 = _202;
      _207 = _203;
   }
   else
   {
      _205 = _182;
      _206 = _183;
      _207 = _184;
   }
   float _208 = _205 * _103;
   float _209 = _206 * _103;
   float _210 = _207 * _103;
   float _221 = c_mat_emissive.x * _65;
   float _222 = _221 * c_emissive_scale;
   float _223 = c_mat_emissive.y * _66;
   float _224 = _223 * c_emissive_scale;
   float _225 = c_mat_emissive.z * _67;
   float _226 = _225 * c_emissive_scale;
   int _227 = c_bool & 2048;
   bool _228 = (_227 == 0);
   if (_228)
   {
      float _230 = _170 * _152;
      float _231 = _170 * _153;
      float _232 = _170 * _154;
      float _233 = _230 + TEXCOORD_4.x;
      float _234 = _231 + TEXCOORD_4.y;
      float _235 = _232 + TEXCOORD_4.z;
      float _236 = _170 * _155;
      float _237 = _236 + TEXCOORD_5.x;
      float _238 = _236 + TEXCOORD_5.y;
      float _239 = _236 + TEXCOORD_5.z;
      _241 = _237;
      _242 = _238;
      _243 = _239;
      _244 = _233;
      _245 = _234;
      _246 = _235;
   }
   else
   {
      _241 = TEXCOORD_5.x;
      _242 = TEXCOORD_5.y;
      _243 = TEXCOORD_5.z;
      _244 = TEXCOORD_4.x;
      _245 = TEXCOORD_4.y;
      _246 = TEXCOORD_4.z;
   }
   float _251 = -0.0f - (c_light_vec[1].x);
   float _252 = -0.0f - (c_light_vec[1].y);
   float _253 = -0.0f - (c_light_vec[1].z);
   float _254 = dot(float3(_70, _71, _72), float3(_251, _252, _253));
   float _255 = saturate(_254);
   float _260 = saturate(_85);
   float _261 = (c_light_col[0].x) * _260;
   float _262 = (c_light_col[0].y) * _260;
   float _263 = (c_light_col[0].z) * _260;
   float _268 = (c_light_col[1].x) * _255;
   float _269 = (c_light_col[1].y) * _255;
   float _270 = (c_light_col[1].z) * _255;
   float _279 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _280 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _281 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _282 = _261 * _182;
   float _283 = _262 * _183;
   float _284 = _263 * _184;
   float _285 = _268 * _170;
   float _286 = _269 * _170;
   float _287 = _270 * _170;
   float _288 = _285 + _282;
   float _289 = _286 + _283;
   float _290 = _287 + _284;
   float _291 = _279 * _288;
   float _292 = _280 * _289;
   float _293 = _281 * _290;
   float _294 = _222 + c_scene_ambient.x;
   float _295 = _294 + _291;
   float _296 = _224 + c_scene_ambient.y;
   float _297 = _296 + _292;
   float _298 = _226 + c_scene_ambient.z;
   float _299 = _298 + _293;
   float _300 = _295 * _185;
   float _301 = _297 * _186;
   float _302 = _299 * _187;
   float _303 = _244 * _208;
   float _304 = _245 * _209;
   float _305 = _246 * _210;
   float _306 = _300 + _303;
   float _307 = _301 + _304;
   float _308 = _302 + _305;
   float _313 = (c_light_vec[0].x) + _75;
   float _314 = (c_light_vec[0].y) + _76;
   float _315 = (c_light_vec[0].z) + _77;
   float _316 = dot(float3(_313, _314, _315), float3(_313, _314, _315));
   float _317 = rsqrt(_316);
   float _318 = _313 * _317;
   float _319 = -0.0f - _318;
   float _320 = _314 * _317;
   float _321 = -0.0f - _320;
   float _322 = _315 * _317;
   float _323 = -0.0f - _322;
   float _324 = dot(float3(_319, _321, _323), float3(_70, _71, _72));
   float _325 = saturate(_324);
   float _326 = _76 + -0.30000001192092896f;
   float _327 = dot(float3(_75, _326, _77), float3(_75, _326, _77));
   float _328 = rsqrt(_327);
   float _329 = _75 * _328;
   float _330 = -0.0f - _329;
   float _331 = _326 * _328;
   float _332 = -0.0f - _331;
   float _333 = _77 * _328;
   float _334 = -0.0f - _333;
   float _335 = dot(float3(_330, _332, _334), float3(_70, _71, _72));
   float _336 = saturate(_335);
   float _339 = log2(_325);
   float _340 = log2(_336);
   float _341 = _339 * c_mat_power;
   float _342 = _340 * c_mat_power;
   float _343 = exp2(_341);
   float _344 = exp2(_342);
   float _347 = _343 * (c_light_col[0].x);
   float _348 = _343 * (c_light_col[0].y);
   float _349 = _343 * (c_light_col[0].z);
   float _350 = _344 * 2.0f;
   float _351 = _350 * (c_light_vec[5].x);
   float _352 = _351 * (c_light_col[1].x);
   float _353 = _351 * (c_light_col[1].y);
   float _354 = _351 * (c_light_col[1].z);
   float _355 = _347 * _182;
   float _356 = _348 * _183;
   float _357 = _349 * _184;
   float _358 = _352 * _255;
   float _359 = _353 * _255;
   float _360 = _354 * _255;
   float _361 = _358 + _355;
   float _362 = _359 + _356;
   float _363 = _360 + _357;
   float _370 = c_mat_specular.x * c_specular_scale.x;
   float _371 = _370 * _185;
   float _372 = _371 * _361;
   float _373 = c_mat_specular.y * c_specular_scale.y;
   float _374 = _373 * _186;
   float _375 = _374 * _362;
   float _376 = c_mat_specular.z * c_specular_scale.z;
   float _377 = _376 * _187;
   float _378 = _377 * _363;
   float _379 = _241 * _208;
   float _380 = _379 * _344;
   float _381 = _242 * _209;
   float _382 = _381 * _344;
   float _383 = _243 * _210;
   float _384 = _383 * _344;
   float _385 = _372 + _380;
   float _386 = _375 + _382;
   float _387 = _378 + _384;
   if (!_228)
   {
      float _389 = _170 * _152;
      float _390 = _170 * _153;
      float _391 = _170 * _154;
      float _392 = _306 + _389;
      float _393 = _307 + _390;
      float _394 = _308 + _391;
      float _395 = _170 * _155;
      float _396 = _385 + _395;
      float _397 = _386 + _395;
      float _398 = _387 + _395;
      _400 = _392;
      _401 = _393;
      _402 = _394;
      _403 = _396;
      _404 = _397;
      _405 = _398;
   }
   else
   {
      _400 = _306;
      _401 = _307;
      _402 = _308;
      _403 = _385;
      _404 = _386;
      _405 = _387;
   }
   int _406 = c_bool & 128;
   bool _407 = (_406 != 0);
   float _408 = select(_407, 0.0f, _403);
   float _409 = select(_407, 0.0f, _404);
   float _410 = select(_407, 0.0f, _405);
   float _411 = _400 * _65;
   float _412 = _401 * _66;
   float _413 = _402 * _67;
   int _414 = c_bool & 32768;
   bool _415 = (_414 == 0);
   if (!_415)
   {
      float _419 = (c_ps_user[11].x) * _65;
      float _420 = (c_ps_user[11].x) * _66;
      float _421 = (c_ps_user[11].x) * _67;
      float _423 = _70 + 1.0f;
      float _424 = _71 + 1.0f;
      float _425 = _72 + 1.0f;
      float _426 = _423 * 0.5f;
      float _427 = _424 * 0.5f;
      float _428 = _425 * 0.5f;
      float _429 = saturate(_426);
      float _430 = saturate(_427);
      float _431 = saturate(_428);
      float _432 = (c_ps_user[11].y) * _429;
      float _433 = _430 * (c_ps_user[11].y);
      float _434 = _431 * (c_ps_user[11].y);
      float _437 = c_mat_power * 0.009999999776482582f;
      float _438 = _437 * (c_ps_user[11].w);
      float _441 = (c_ps_user[12].x) * _182;
      float _442 = (c_ps_user[12].x) * _183;
      float _443 = (c_ps_user[12].x) * _184;
      float _445 = (c_ps_user[12].y) * _185;
      float _446 = (c_ps_user[12].y) * _186;
      float _447 = (c_ps_user[12].y) * _187;
      float _449 = (c_ps_user[12].z) * _208;
      float _450 = (c_ps_user[12].z) * _209;
      float _451 = (c_ps_user[12].z) * _210;
      float _457 = -0.0f - _75;
      float _458 = -0.0f - _76;
      float _459 = -0.0f - _77;
      float _460 = dot(float3(_457, _458, _459), float3(_70, _71, _72));
      float _461 = 1.0f - _460;
      float _462 = _461 * _461;
      float _463 = _461 * 0.9722222089767456f;
      float _464 = _462 * _462;
      float _465 = _464 * _463;
      float _466 = _465 + 0.02777777425944805f;
      float _467 = _466 * (c_ps_user[13].z);
      float _469 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _470 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _471 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _474 = (c_ps_user[14].x) * _222;
      float _475 = (c_ps_user[14].x) * _224;
      float _476 = (c_ps_user[14].x) * _226;
      float _478 = (c_ps_user[14].y) * _261;
      float _479 = (c_ps_user[14].y) * _262;
      float _480 = (c_ps_user[14].y) * _263;
      float _482 = (c_ps_user[14].z) * _268;
      float _483 = (c_ps_user[14].z) * _269;
      float _484 = (c_ps_user[14].z) * _270;
      float _486 = (c_ps_user[14].w) * _244;
      float _487 = (c_ps_user[14].w) * _245;
      float _488 = (c_ps_user[14].w) * _246;
      float _491 = (c_ps_user[15].x) * _347;
      float _492 = (c_ps_user[15].x) * _348;
      float _493 = (c_ps_user[15].x) * _349;
      float _495 = _370 * _352;
      float _496 = _495 * (c_ps_user[15].y);
      float _497 = _373 * _353;
      float _498 = _497 * (c_ps_user[15].y);
      float _499 = _376 * _354;
      float _500 = _499 * (c_ps_user[15].y);
      float _502 = (c_ps_user[15].z) * _241;
      float _503 = (c_ps_user[15].z) * _242;
      float _504 = (c_ps_user[15].z) * _243;
      float _505 = _496 + _502;
      float _506 = _505 * _344;
      float _507 = _478 + (c_ps_user[13].x);
      float _508 = _507 + _482;
      float _509 = c_diffuse_scale.x * _508;
      float _510 = _509 * c_mat_diffuse.x;
      float _511 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _512 = _511 + _491;
      float _513 = c_mat_specular.x * _512;
      float _514 = _513 * c_specular_scale.x;
      float _515 = _432 + _419;
      float _516 = _515 + _438;
      float _517 = _516 + (c_ps_user[12].w);
      float _518 = _517 + _441;
      float _519 = _518 + _445;
      float _520 = _519 + _449;
      float _521 = _520 + _469;
      float _522 = _521 + _474;
      float _523 = _522 + _486;
      float _524 = _523 + _467;
      float _525 = _524 + _506;
      float _526 = _525 + _510;
      float _527 = _526 + _514;
      float _528 = _498 + _503;
      float _529 = _528 * _344;
      float _530 = _479 + (c_ps_user[13].x);
      float _531 = _530 + _483;
      float _532 = c_diffuse_scale.y * _531;
      float _533 = _532 * c_mat_diffuse.y;
      float _534 = _511 + _492;
      float _535 = c_mat_specular.y * _534;
      float _536 = _535 * c_specular_scale.y;
      float _537 = _433 + _420;
      float _538 = _537 + _438;
      float _539 = _538 + (c_ps_user[12].w);
      float _540 = _539 + _442;
      float _541 = _540 + _446;
      float _542 = _541 + _450;
      float _543 = _542 + _470;
      float _544 = _543 + _475;
      float _545 = _544 + _487;
      float _546 = _545 + _467;
      float _547 = _546 + _529;
      float _548 = _547 + _533;
      float _549 = _548 + _536;
      float _550 = _500 + _504;
      float _551 = _550 * _344;
      float _552 = _480 + (c_ps_user[13].x);
      float _553 = _552 + _484;
      float _554 = c_diffuse_scale.z * _553;
      float _555 = _554 * c_mat_diffuse.z;
      float _556 = _511 + _493;
      float _557 = c_mat_specular.z * _556;
      float _558 = _557 * c_specular_scale.z;
      float _559 = _438 + _421;
      float _560 = _559 + _434;
      float _561 = _560 + (c_ps_user[12].w);
      float _562 = _561 + _443;
      float _563 = _562 + _447;
      float _564 = _563 + _451;
      float _565 = _564 + _471;
      float _566 = _565 + _476;
      float _567 = _566 + _488;
      float _568 = _567 + _467;
      float _569 = _568 + _551;
      float _570 = _569 + _555;
      float _571 = _570 + _558;
      _573 = _527;
      _574 = _549;
      _575 = _571;
      _576 = 0.0f;
      _577 = 0.0f;
      _578 = 0.0f;
   }
   else
   {
      _573 = _411;
      _574 = _412;
      _575 = _413;
      _576 = _408;
      _577 = _409;
      _578 = _410;
   }
   float _579 = _576 + _573;
   float _580 = _577 + _574;
   float _581 = _578 + _575;
   float _586 = c_mat_opacity * _42.w;
   float _587 = _586 * c_global_opacity;
   float _588 = _587 * c_opacity_scale;
   int _589 = c_bool & 16;
   bool _590 = (_589 == 0);
   if (!_590)
   {
      float _594 = c_tone_control.x * _579;
      float _595 = c_tone_control.x * _580;
      float _596 = c_tone_control.x * _581;
      float _598 = log2(_594);
      float _599 = log2(_595);
      float _600 = log2(_596);
      float _601 = _598 * c_tone_control.y;
      float _602 = _599 * c_tone_control.y;
      float _603 = _600 * c_tone_control.y;
      float _604 = exp2(_601);
      float _605 = exp2(_602);
      float _606 = exp2(_603);
      _654 = _604;
      _655 = _605;
      _656 = _606;
   }
   else
   {
      if (_415)
      {
         float _611 = c_tone_control.x * _579;
         float _612 = c_tone_control.x * _580;
         float _613 = c_tone_control.x * _581;
         float _614 = _611 * 2.0f;
         float _615 = _612 * 2.0f;
         float _616 = _613 * 2.0f;
         float _617 = _611 * 0.30000001192092896f;
         float _618 = _612 * 0.30000001192092896f;
         float _619 = _613 * 0.30000001192092896f;
         float _620 = _617 + 0.05000000074505806f;
         float _621 = _618 + 0.05000000074505806f;
         float _622 = _619 + 0.05000000074505806f;
         float _623 = _620 * _614;
         float _624 = _621 * _615;
         float _625 = _622 * _616;
         float _626 = _623 + 0.004000000189989805f;
         float _627 = _624 + 0.004000000189989805f;
         float _628 = _625 + 0.004000000189989805f;
         float _629 = _617 + 0.5f;
         float _630 = _618 + 0.5f;
         float _631 = _619 + 0.5f;
         float _632 = _629 * _614;
         float _633 = _630 * _615;
         float _634 = _631 * _616;
         float _635 = _632 + 0.06000000238418579f;
         float _636 = _633 + 0.06000000238418579f;
         float _637 = _634 + 0.06000000238418579f;
         float _638 = _626 / _635;
         float _639 = _627 / _636;
         float _640 = _628 / _637;
         float _641 = _638 + -0.06666666269302368f;
         float _642 = _639 + -0.06666666269302368f;
         float _643 = _640 + -0.06666666269302368f;
         float _644 = _641 * 1.3790643215179443f;
         float _645 = _642 * 1.3790643215179443f;
         float _646 = _643 * 1.3790643215179443f;
         float _647 = YKR_MaterialCurve(_644, _611);
         float _648 = YKR_MaterialCurve(_645, _612);
         float _649 = YKR_MaterialCurve(_646, _613);
         float _650 = sqrt(_647);
         float _651 = sqrt(_648);
         float _652 = sqrt(_649);
         _654 = _650;
         _655 = _651;
         _656 = _652;
      }
      else
      {
         _654 = _579;
         _655 = _580;
         _656 = _581;
      }
   }
   int _657 = c_bool & 4;
   bool _658 = (_657 == 0);
   if (_658)
   {
      float _660 = saturate(COLOR_1.x);
      float _665 = (c_fog_param[0].x) - _654;
      float _666 = (c_fog_param[0].y) - _655;
      float _667 = (c_fog_param[0].z) - _656;
      float _668 = _665 * _660;
      float _669 = _666 * _660;
      float _670 = _667 * _660;
      _672 = _668;
      _673 = _669;
      _674 = _670;
   }
   else
   {
      _672 = COLOR_1.x;
      _673 = COLOR_1.y;
      _674 = COLOR_1.z;
   }
   float _675 = _672 + _654;
   float _676 = _673 + _655;
   float _677 = _674 + _656;
   float _683 = (c_ps_col_scl[0].x) * _675;
   float _684 = (c_ps_col_scl[0].y) * _676;
   float _685 = (c_ps_col_scl[0].z) * _677;
   float _686 = (c_ps_col_scl[0].w) * _588;
   SV_Target.x = _683;
   SV_Target.y = _684;
   SV_Target.z = _685;
   SV_Target.w = _686;
   int _689 = c_bool & 8;
   bool _690 = (_689 != 0);
   float _691 = select(_690, _675, _683);
   float _692 = select(_690, _676, _684);
   float _693 = select(_690, _677, _685);
   float _694 = select(_690, _588, _686);
   SV_Target.x = _691;
   SV_Target.y = _692;
   SV_Target.z = _693;
   SV_Target.w = _694;
   return SV_Target;
}
