// Yakuza Kiwami material r_o1du_a(3i)2d[glmask] (0x6C113E86, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s3_t : register(t3);

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
   float4 _47 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _52 = _47.x * _47.x;
   float _53 = _47.y * _47.y;
   float _54 = _47.z * _47.z;
   float _58 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _59 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _60 = s3_t.Sample(s3_s, float2(_58, _59));
   float4 _62 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _66 = s2_t.Sample(s2_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
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
   float _577;
   float _578;
   float _579;
   float _580;
   float _581;
   float _582;
   float _655;
   float _656;
   float _657;
   float _673;
   float _674;
   float _675;
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
   float _221 = c_mat_emissive.x * _52;
   float _222 = _221 * c_emissive_scale;
   float _223 = c_mat_emissive.y * _53;
   float _224 = _223 * c_emissive_scale;
   float _225 = c_mat_emissive.z * _54;
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
      _241 = _233;
      _242 = _234;
      _243 = _235;
      _244 = _237;
      _245 = _238;
      _246 = _239;
   }
   else
   {
      _241 = TEXCOORD_4.x;
      _242 = TEXCOORD_4.y;
      _243 = TEXCOORD_4.z;
      _244 = TEXCOORD_5.x;
      _245 = TEXCOORD_5.y;
      _246 = TEXCOORD_5.z;
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
   float _303 = _241 * _208;
   float _304 = _242 * _209;
   float _305 = _243 * _210;
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
   float _379 = _244 * _208;
   float _380 = _379 * _344;
   float _381 = _245 * _209;
   float _382 = _381 * _344;
   float _383 = _246 * _210;
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
   float _411 = _400 * _52;
   float _412 = _401 * _53;
   float _413 = _402 * _54;
   float _414 = _408 * _60.y;
   float _415 = _409 * _60.y;
   float _416 = _410 * _60.y;
   int _417 = c_bool & 32768;
   bool _418 = (_417 == 0);
   if (!_418)
   {
      float _422 = (c_ps_user[11].x) * _52;
      float _423 = (c_ps_user[11].x) * _53;
      float _424 = (c_ps_user[11].x) * _54;
      float _426 = _70 + 1.0f;
      float _427 = _71 + 1.0f;
      float _428 = _72 + 1.0f;
      float _429 = _426 * 0.5f;
      float _430 = _427 * 0.5f;
      float _431 = _428 * 0.5f;
      float _432 = saturate(_429);
      float _433 = saturate(_430);
      float _434 = saturate(_431);
      float _435 = (c_ps_user[11].y) * _432;
      float _436 = _433 * (c_ps_user[11].y);
      float _437 = _434 * (c_ps_user[11].y);
      float _439 = (c_ps_user[11].z) * _60.y;
      float _441 = c_mat_power * 0.009999999776482582f;
      float _442 = _441 * (c_ps_user[11].w);
      float _445 = (c_ps_user[12].x) * _182;
      float _446 = (c_ps_user[12].x) * _183;
      float _447 = (c_ps_user[12].x) * _184;
      float _449 = (c_ps_user[12].y) * _185;
      float _450 = (c_ps_user[12].y) * _186;
      float _451 = (c_ps_user[12].y) * _187;
      float _453 = (c_ps_user[12].z) * _208;
      float _454 = (c_ps_user[12].z) * _209;
      float _455 = (c_ps_user[12].z) * _210;
      float _461 = -0.0f - _75;
      float _462 = -0.0f - _76;
      float _463 = -0.0f - _77;
      float _464 = dot(float3(_461, _462, _463), float3(_70, _71, _72));
      float _465 = 1.0f - _464;
      float _466 = _465 * _465;
      float _467 = _465 * 0.9722222089767456f;
      float _468 = _466 * _466;
      float _469 = _468 * _467;
      float _470 = _469 + 0.02777777425944805f;
      float _471 = _470 * (c_ps_user[13].z);
      float _473 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _474 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _475 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _478 = (c_ps_user[14].x) * _222;
      float _479 = (c_ps_user[14].x) * _224;
      float _480 = (c_ps_user[14].x) * _226;
      float _482 = (c_ps_user[14].y) * _261;
      float _483 = (c_ps_user[14].y) * _262;
      float _484 = (c_ps_user[14].y) * _263;
      float _486 = (c_ps_user[14].z) * _268;
      float _487 = (c_ps_user[14].z) * _269;
      float _488 = (c_ps_user[14].z) * _270;
      float _490 = (c_ps_user[14].w) * _241;
      float _491 = (c_ps_user[14].w) * _242;
      float _492 = (c_ps_user[14].w) * _243;
      float _495 = (c_ps_user[15].x) * _347;
      float _496 = (c_ps_user[15].x) * _348;
      float _497 = (c_ps_user[15].x) * _349;
      float _499 = _370 * _352;
      float _500 = _499 * (c_ps_user[15].y);
      float _501 = _373 * _353;
      float _502 = _501 * (c_ps_user[15].y);
      float _503 = _376 * _354;
      float _504 = _503 * (c_ps_user[15].y);
      float _506 = (c_ps_user[15].z) * _244;
      float _507 = (c_ps_user[15].z) * _245;
      float _508 = (c_ps_user[15].z) * _246;
      float _509 = _500 + _506;
      float _510 = _509 * _344;
      float _511 = _482 + (c_ps_user[13].x);
      float _512 = _511 + _486;
      float _513 = c_diffuse_scale.x * _512;
      float _514 = _513 * c_mat_diffuse.x;
      float _515 = (c_ps_user[13].y) + _439;
      float _516 = _515 + _495;
      float _517 = c_mat_specular.x * _516;
      float _518 = _517 * c_specular_scale.x;
      float _519 = _435 + _422;
      float _520 = _519 + _442;
      float _521 = _520 + (c_ps_user[12].w);
      float _522 = _521 + _445;
      float _523 = _522 + _449;
      float _524 = _523 + _453;
      float _525 = _524 + _473;
      float _526 = _525 + _478;
      float _527 = _526 + _490;
      float _528 = _527 + _471;
      float _529 = _528 + _510;
      float _530 = _529 + _514;
      float _531 = _530 + _518;
      float _532 = _502 + _507;
      float _533 = _532 * _344;
      float _534 = _483 + (c_ps_user[13].x);
      float _535 = _534 + _487;
      float _536 = c_diffuse_scale.y * _535;
      float _537 = _536 * c_mat_diffuse.y;
      float _538 = _515 + _496;
      float _539 = c_mat_specular.y * _538;
      float _540 = _539 * c_specular_scale.y;
      float _541 = _436 + _423;
      float _542 = _541 + _442;
      float _543 = _542 + (c_ps_user[12].w);
      float _544 = _543 + _446;
      float _545 = _544 + _450;
      float _546 = _545 + _454;
      float _547 = _546 + _474;
      float _548 = _547 + _479;
      float _549 = _548 + _491;
      float _550 = _549 + _471;
      float _551 = _550 + _533;
      float _552 = _551 + _537;
      float _553 = _552 + _540;
      float _554 = _504 + _508;
      float _555 = _554 * _344;
      float _556 = _484 + (c_ps_user[13].x);
      float _557 = _556 + _488;
      float _558 = c_diffuse_scale.z * _557;
      float _559 = _558 * c_mat_diffuse.z;
      float _560 = _515 + _497;
      float _561 = c_mat_specular.z * _560;
      float _562 = _561 * c_specular_scale.z;
      float _563 = _442 + _424;
      float _564 = _563 + _437;
      float _565 = _564 + (c_ps_user[12].w);
      float _566 = _565 + _447;
      float _567 = _566 + _451;
      float _568 = _567 + _455;
      float _569 = _568 + _475;
      float _570 = _569 + _480;
      float _571 = _570 + _492;
      float _572 = _571 + _471;
      float _573 = _572 + _555;
      float _574 = _573 + _559;
      float _575 = _574 + _562;
      _577 = _531;
      _578 = _553;
      _579 = _575;
      _580 = 0.0f;
      _581 = 0.0f;
      _582 = 0.0f;
   }
   else
   {
      _577 = _411;
      _578 = _412;
      _579 = _413;
      _580 = _414;
      _581 = _415;
      _582 = _416;
   }
   float _583 = _580 + _577;
   float _584 = _581 + _578;
   float _585 = _582 + _579;
   int _590 = c_bool & 16;
   bool _591 = (_590 == 0);
   if (!_591)
   {
      float _595 = c_tone_control.x * _583;
      float _596 = c_tone_control.x * _584;
      float _597 = c_tone_control.x * _585;
      float _599 = log2(_595);
      float _600 = log2(_596);
      float _601 = log2(_597);
      float _602 = _599 * c_tone_control.y;
      float _603 = _600 * c_tone_control.y;
      float _604 = _601 * c_tone_control.y;
      float _605 = exp2(_602);
      float _606 = exp2(_603);
      float _607 = exp2(_604);
      _655 = _605;
      _656 = _606;
      _657 = _607;
   }
   else
   {
      if (_418)
      {
         float _612 = c_tone_control.x * _583;
         float _613 = c_tone_control.x * _584;
         float _614 = c_tone_control.x * _585;
         float _615 = _612 * 2.0f;
         float _616 = _613 * 2.0f;
         float _617 = _614 * 2.0f;
         float _618 = _612 * 0.30000001192092896f;
         float _619 = _613 * 0.30000001192092896f;
         float _620 = _614 * 0.30000001192092896f;
         float _621 = _618 + 0.05000000074505806f;
         float _622 = _619 + 0.05000000074505806f;
         float _623 = _620 + 0.05000000074505806f;
         float _624 = _621 * _615;
         float _625 = _622 * _616;
         float _626 = _623 * _617;
         float _627 = _624 + 0.004000000189989805f;
         float _628 = _625 + 0.004000000189989805f;
         float _629 = _626 + 0.004000000189989805f;
         float _630 = _618 + 0.5f;
         float _631 = _619 + 0.5f;
         float _632 = _620 + 0.5f;
         float _633 = _630 * _615;
         float _634 = _631 * _616;
         float _635 = _632 * _617;
         float _636 = _633 + 0.06000000238418579f;
         float _637 = _634 + 0.06000000238418579f;
         float _638 = _635 + 0.06000000238418579f;
         float _639 = _627 / _636;
         float _640 = _628 / _637;
         float _641 = _629 / _638;
         float _642 = _639 + -0.06666666269302368f;
         float _643 = _640 + -0.06666666269302368f;
         float _644 = _641 + -0.06666666269302368f;
         float _645 = _642 * 1.3790643215179443f;
         float _646 = _643 * 1.3790643215179443f;
         float _647 = _644 * 1.3790643215179443f;
         float _648 = YKR_MaterialCurve(_645, _612);
         float _649 = YKR_MaterialCurve(_646, _613);
         float _650 = YKR_MaterialCurve(_647, _614);
         float _651 = sqrt(_648);
         float _652 = sqrt(_649);
         float _653 = sqrt(_650);
         _655 = _651;
         _656 = _652;
         _657 = _653;
      }
      else
      {
         _655 = _583;
         _656 = _584;
         _657 = _585;
      }
   }
   int _658 = c_bool & 4;
   bool _659 = (_658 == 0);
   if (_659)
   {
      float _661 = saturate(COLOR_1.x);
      float _666 = (c_fog_param[0].x) - _655;
      float _667 = (c_fog_param[0].y) - _656;
      float _668 = (c_fog_param[0].z) - _657;
      float _669 = _666 * _661;
      float _670 = _667 * _661;
      float _671 = _668 * _661;
      _673 = _669;
      _674 = _670;
      _675 = _671;
   }
   else
   {
      _673 = COLOR_1.x;
      _674 = COLOR_1.y;
      _675 = COLOR_1.z;
   }
   float _676 = _66.y * _62.x;
   float _677 = _66.y * _62.y;
   float _678 = _66.y * _62.z;
   float _679 = _655 + _676;
   float _680 = _679 + _673;
   float _681 = _656 + _677;
   float _682 = _681 + _674;
   float _683 = _657 + _678;
   float _684 = _683 + _675;
   float _687 = (c_ps_user[0].w) + _66.y;
   float _688 = saturate(_687);
   float _689 = _688 * _680;
   float _690 = _688 * _682;
   float _691 = _688 * _684;
   float _692 = c_mat_opacity * _47.w;
   float _693 = _692 * c_global_opacity;
   float _694 = _693 * c_opacity_scale;
   float _695 = _694 * _688;
   float _701 = (c_ps_col_scl[0].x) * _689;
   float _702 = (c_ps_col_scl[0].y) * _690;
   float _703 = (c_ps_col_scl[0].z) * _691;
   float _704 = (c_ps_col_scl[0].w) * _695;
   SV_Target.x = _701;
   SV_Target.y = _702;
   SV_Target.z = _703;
   SV_Target.w = _704;
   int _707 = c_bool & 8;
   bool _708 = (_707 != 0);
   float _709 = select(_708, _689, _701);
   float _710 = select(_708, _690, _702);
   float _711 = select(_708, _691, _703);
   float _712 = select(_708, _695, _704);
   SV_Target.x = _709;
   SV_Target.y = _710;
   SV_Target.z = _711;
   SV_Target.w = _712;
   return SV_Target;
}
