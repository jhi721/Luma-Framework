// Yakuza Kiwami material r_p1d[ag][plant] (0x3D526286, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD_2 : TEXCOORD2,
    linear float3 TEXCOORD_3 : TEXCOORD3,
    linear float3 TEXCOORD_4 : TEXCOORD4,
    linear float3 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _34 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _39 = _34.x * _34.x;
   float _40 = _34.y * _34.y;
   float _41 = _34.z * _34.z;
   float _42 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _43 = rsqrt(_42);
   float _44 = _43 * TEXCOORD_2.x;
   float _45 = _43 * TEXCOORD_2.y;
   float _46 = _43 * TEXCOORD_2.z;
   float _47 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _48 = rsqrt(_47);
   float _53 = -0.0f - (c_light_vec[0].x);
   float _54 = -0.0f - (c_light_vec[0].y);
   float _55 = -0.0f - (c_light_vec[0].z);
   float _56 = dot(float3(_44, _45, _46), float3(_53, _54, _55));
   float _60 = c_rt_size.z * SV_Position.x;
   float _61 = c_rt_size.w * SV_Position.y;
   int _64 = c_bool & 8;
   bool _65 = (_64 == 0);
   float _71;
   float _72;
   float _84;
   float _85;
   float _117;
   float _118;
   float _119;
   float _133;
   float _134;
   float _166;
   float _167;
   float _168;
   float _195;
   float _196;
   float _197;
   float _279;
   float _280;
   float _281;
   float _401;
   float _402;
   float _403;
   float _476;
   float _477;
   float _478;
   float _494;
   float _495;
   float _496;
   if (!_65)
   {
      float4 _67 = s13_t.Sample(s13_s, float2(_60, _61));
      _71 = _67.x;
      _72 = _67.y;
   }
   else
   {
      _71 = 1.0f;
      _72 = 1.0f;
   }
   if (!_65)
   {
      float _77 = (c_sdw_param[0].x) * _71;
      float _78 = (c_sdw_param[0].z) * _72;
      float _81 = _77 + (c_sdw_param[0].y);
      float _82 = _78 + (c_sdw_param[0].w);
      _84 = _81;
      _85 = _82;
   }
   else
   {
      _84 = _71;
      _85 = _72;
   }
   float _89 = c_rt_size.z * SV_Position.x;
   float _90 = c_rt_size.w * SV_Position.y;
   float4 _91 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_89, _90)));
   int _97 = c_bool & 16384;
   bool _98 = (_97 == 0);
   if (!_98)
   {
      float _100 = _91.x * 2.0f;
      float _101 = _91.y * 2.0f;
      float _102 = _91.z * 2.0f;
      _117 = _100;
      _118 = _101;
      _119 = _102;
   }
   else
   {
      int _104 = c_bool & 8192;
      bool _105 = (_104 == 0);
      if (!_105)
      {
         float _107 = _91.x * 2.0f;
         float _108 = _91.y * 2.0f;
         float _109 = _91.z * 2.0f;
         _117 = _107;
         _118 = _108;
         _119 = _109;
      }
      else
      {
         float _113 = _91.x / c_tone_control.z;
         float _114 = _91.y / c_tone_control.z;
         float _115 = _91.z / c_tone_control.z;
         _117 = _113;
         _118 = _114;
         _119 = _115;
      }
   }
   int _120 = c_bool & 8;
   bool _121 = (_120 == 0);
   if (!_121)
   {
      float _126 = (c_sdw_param[0].x) * _84;
      float _127 = (c_sdw_param[0].z) * _85;
      float _130 = _126 + (c_sdw_param[0].y);
      float _131 = _127 + (c_sdw_param[0].w);
      _133 = _130;
      _134 = _131;
   }
   else
   {
      _133 = _84;
      _134 = _85;
   }
   float _135 = _134 * _133;
   float _140 = 1.0f - (c_light_col[5].x);
   float _141 = 1.0f - (c_light_col[5].y);
   float _142 = 1.0f - (c_light_col[5].z);
   float _143 = _140 * _135;
   float _144 = _141 * _135;
   float _145 = _142 * _135;
   float _146 = _143 + (c_light_col[5].x);
   float _147 = _144 + (c_light_col[5].y);
   float _148 = _145 + (c_light_col[5].z);
   if (!_121)
   {
      float _152 = (c_sdw_param[2].x) * _146;
      float _153 = (c_sdw_param[2].x) * _147;
      float _154 = (c_sdw_param[2].x) * _148;
      float _156 = _152 + (c_sdw_param[2].y);
      float _157 = _153 + (c_sdw_param[2].y);
      float _158 = _154 + (c_sdw_param[2].y);
      float _161 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _162 = _156 * _161;
      float _163 = _157 * _161;
      float _164 = _158 * _161;
      _166 = _162;
      _167 = _163;
      _168 = _164;
   }
   else
   {
      _166 = _146;
      _167 = _147;
      _168 = _148;
   }
   float _179 = c_mat_emissive.x * _39;
   float _180 = _179 * c_emissive_scale;
   float _181 = c_mat_emissive.y * _40;
   float _182 = _181 * c_emissive_scale;
   float _183 = c_mat_emissive.z * _41;
   float _184 = _183 * c_emissive_scale;
   int _185 = c_bool & 2048;
   bool _186 = (_185 == 0);
   if (_186)
   {
      float _188 = _134 * _117;
      float _189 = _134 * _118;
      float _190 = _134 * _119;
      float _191 = _188 + TEXCOORD_4.x;
      float _192 = _189 + TEXCOORD_4.y;
      float _193 = _190 + TEXCOORD_4.z;
      _195 = _191;
      _196 = _192;
      _197 = _193;
   }
   else
   {
      _195 = TEXCOORD_4.x;
      _196 = TEXCOORD_4.y;
      _197 = TEXCOORD_4.z;
   }
   float _202 = -0.0f - (c_light_vec[1].x);
   float _203 = -0.0f - (c_light_vec[1].y);
   float _204 = -0.0f - (c_light_vec[1].z);
   float _205 = dot(float3(_44, _45, _46), float3(_202, _203, _204));
   float _206 = saturate(_205);
   float _211 = saturate(_56);
   float _212 = (c_light_col[0].x) * _211;
   float _213 = (c_light_col[0].y) * _211;
   float _214 = (c_light_col[0].z) * _211;
   float _219 = (c_light_col[1].x) * _206;
   float _220 = (c_light_col[1].y) * _206;
   float _221 = (c_light_col[1].z) * _206;
   float _230 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _231 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _232 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _233 = _212 * _146;
   float _234 = _213 * _147;
   float _235 = _214 * _148;
   float _236 = _219 * _134;
   float _237 = _220 * _134;
   float _238 = _221 * _134;
   float _239 = _236 + _233;
   float _240 = _237 + _234;
   float _241 = _238 + _235;
   float _242 = _230 * _239;
   float _243 = _231 * _240;
   float _244 = _232 * _241;
   float _247 = -0.0f - _56;
   float _248 = saturate(_247);
   float _249 = _248 * (c_attribute_ps[2].x);
   float _250 = _249 * (c_light_col[0].x);
   float _251 = _249 * (c_light_col[0].y);
   float _252 = _249 * (c_light_col[0].z);
   float _253 = _180 + c_scene_ambient.x;
   float _254 = _253 + _242;
   float _255 = _254 + _250;
   float _256 = _182 + c_scene_ambient.y;
   float _257 = _256 + _243;
   float _258 = _257 + _251;
   float _259 = _184 + c_scene_ambient.z;
   float _260 = _259 + _244;
   float _261 = _260 + _252;
   float _262 = _255 * COLOR.x;
   float _263 = _258 * COLOR.y;
   float _264 = _261 * COLOR.z;
   float _265 = _195 * _166;
   float _266 = _196 * _167;
   float _267 = _197 * _168;
   float _268 = _262 + _265;
   float _269 = _263 + _266;
   float _270 = _264 + _267;
   if (!_186)
   {
      float _272 = _134 * _117;
      float _273 = _134 * _118;
      float _274 = _134 * _119;
      float _275 = _268 + _272;
      float _276 = _269 + _273;
      float _277 = _270 + _274;
      _279 = _275;
      _280 = _276;
      _281 = _277;
   }
   else
   {
      _279 = _268;
      _280 = _269;
      _281 = _270;
   }
   float _282 = _279 * _39;
   float _283 = _280 * _40;
   float _284 = _281 * _41;
   int _285 = c_bool & 32768;
   bool _286 = (_285 == 0);
   if (!_286)
   {
      float _290 = (c_ps_user[11].x) * _39;
      float _291 = (c_ps_user[11].x) * _40;
      float _292 = (c_ps_user[11].x) * _41;
      float _294 = _44 + 1.0f;
      float _295 = _45 + 1.0f;
      float _296 = _46 + 1.0f;
      float _297 = _294 * 0.5f;
      float _298 = _295 * 0.5f;
      float _299 = _296 * 0.5f;
      float _300 = saturate(_297);
      float _301 = saturate(_298);
      float _302 = saturate(_299);
      float _303 = (c_ps_user[11].y) * _300;
      float _304 = _301 * (c_ps_user[11].y);
      float _305 = _302 * (c_ps_user[11].y);
      float _308 = (c_ps_user[12].x) * _146;
      float _309 = (c_ps_user[12].x) * _147;
      float _310 = (c_ps_user[12].x) * _148;
      float _312 = (c_ps_user[12].y) * COLOR.x;
      float _313 = (c_ps_user[12].y) * COLOR.y;
      float _314 = (c_ps_user[12].y) * COLOR.z;
      float _316 = (c_ps_user[12].z) * _166;
      float _317 = (c_ps_user[12].z) * _167;
      float _318 = (c_ps_user[12].z) * _168;
      float _323 = TEXCOORD_3.x * _48;
      float _324 = -0.0f - _323;
      float _325 = TEXCOORD_3.y * _48;
      float _326 = -0.0f - _325;
      float _327 = TEXCOORD_3.z * _48;
      float _328 = -0.0f - _327;
      float _329 = dot(float3(_324, _326, _328), float3(_44, _45, _46));
      float _330 = 1.0f - _329;
      float _331 = _330 * _330;
      float _332 = _330 * 0.9722222089767456f;
      float _333 = _331 * _331;
      float _334 = _333 * _332;
      float _335 = _334 + 0.02777777425944805f;
      float _336 = _335 * (c_ps_user[13].z);
      float _338 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _339 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _340 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _343 = (c_ps_user[14].x) * _180;
      float _344 = (c_ps_user[14].x) * _182;
      float _345 = (c_ps_user[14].x) * _184;
      float _347 = (c_ps_user[14].y) * _212;
      float _348 = (c_ps_user[14].y) * _213;
      float _349 = (c_ps_user[14].y) * _214;
      float _351 = (c_ps_user[14].z) * _219;
      float _352 = (c_ps_user[14].z) * _220;
      float _353 = (c_ps_user[14].z) * _221;
      float _355 = (c_ps_user[14].w) * _195;
      float _356 = (c_ps_user[14].w) * _196;
      float _357 = (c_ps_user[14].w) * _197;
      float _358 = _347 + (c_ps_user[13].x);
      float _359 = _358 + _351;
      float _360 = c_diffuse_scale.x * _359;
      float _361 = _360 * c_mat_diffuse.x;
      float _362 = _303 + _290;
      float _363 = _362 + (c_ps_user[12].w);
      float _364 = _363 + _308;
      float _365 = _364 + _312;
      float _366 = _365 + _316;
      float _367 = _366 + _338;
      float _368 = _367 + _343;
      float _369 = _368 + _355;
      float _370 = _369 + _336;
      float _371 = _370 + _361;
      float _372 = _348 + (c_ps_user[13].x);
      float _373 = _372 + _352;
      float _374 = c_diffuse_scale.y * _373;
      float _375 = _374 * c_mat_diffuse.y;
      float _376 = _304 + _291;
      float _377 = _376 + (c_ps_user[12].w);
      float _378 = _377 + _309;
      float _379 = _378 + _313;
      float _380 = _379 + _317;
      float _381 = _380 + _339;
      float _382 = _381 + _344;
      float _383 = _382 + _356;
      float _384 = _383 + _336;
      float _385 = _384 + _375;
      float _386 = _349 + (c_ps_user[13].x);
      float _387 = _386 + _353;
      float _388 = c_diffuse_scale.z * _387;
      float _389 = _388 * c_mat_diffuse.z;
      float _390 = _305 + _292;
      float _391 = _390 + (c_ps_user[12].w);
      float _392 = _391 + _310;
      float _393 = _392 + _314;
      float _394 = _393 + _318;
      float _395 = _394 + _340;
      float _396 = _395 + _345;
      float _397 = _396 + _357;
      float _398 = _397 + _336;
      float _399 = _398 + _389;
      _401 = _371;
      _402 = _385;
      _403 = _399;
   }
   else
   {
      _401 = _282;
      _402 = _283;
      _403 = _284;
   }
   float _408 = c_mat_opacity * _34.w;
   float _409 = _408 * c_global_opacity;
   float _410 = _409 * c_opacity_scale;
   int _411 = c_bool & 16;
   bool _412 = (_411 == 0);
   if (!_412)
   {
      float _416 = c_tone_control.x * _401;
      float _417 = c_tone_control.x * _402;
      float _418 = c_tone_control.x * _403;
      float _420 = log2(_416);
      float _421 = log2(_417);
      float _422 = log2(_418);
      float _423 = _420 * c_tone_control.y;
      float _424 = _421 * c_tone_control.y;
      float _425 = _422 * c_tone_control.y;
      float _426 = exp2(_423);
      float _427 = exp2(_424);
      float _428 = exp2(_425);
      _476 = _426;
      _477 = _427;
      _478 = _428;
   }
   else
   {
      if (_286)
      {
         float _433 = c_tone_control.x * _401;
         float _434 = c_tone_control.x * _402;
         float _435 = c_tone_control.x * _403;
         float _436 = _433 * 2.0f;
         float _437 = _434 * 2.0f;
         float _438 = _435 * 2.0f;
         float _439 = _433 * 0.30000001192092896f;
         float _440 = _434 * 0.30000001192092896f;
         float _441 = _435 * 0.30000001192092896f;
         float _442 = _439 + 0.05000000074505806f;
         float _443 = _440 + 0.05000000074505806f;
         float _444 = _441 + 0.05000000074505806f;
         float _445 = _442 * _436;
         float _446 = _443 * _437;
         float _447 = _444 * _438;
         float _448 = _445 + 0.004000000189989805f;
         float _449 = _446 + 0.004000000189989805f;
         float _450 = _447 + 0.004000000189989805f;
         float _451 = _439 + 0.5f;
         float _452 = _440 + 0.5f;
         float _453 = _441 + 0.5f;
         float _454 = _451 * _436;
         float _455 = _452 * _437;
         float _456 = _453 * _438;
         float _457 = _454 + 0.06000000238418579f;
         float _458 = _455 + 0.06000000238418579f;
         float _459 = _456 + 0.06000000238418579f;
         float _460 = _448 / _457;
         float _461 = _449 / _458;
         float _462 = _450 / _459;
         float _463 = _460 + -0.06666666269302368f;
         float _464 = _461 + -0.06666666269302368f;
         float _465 = _462 + -0.06666666269302368f;
         float _466 = _463 * 1.3790643215179443f;
         float _467 = _464 * 1.3790643215179443f;
         float _468 = _465 * 1.3790643215179443f;
         float _469 = YKR_MaterialCurve(_466, _433);
         float _470 = YKR_MaterialCurve(_467, _434);
         float _471 = YKR_MaterialCurve(_468, _435);
         float _472 = sqrt(_469);
         float _473 = sqrt(_470);
         float _474 = sqrt(_471);
         _476 = _472;
         _477 = _473;
         _478 = _474;
      }
      else
      {
         _476 = _401;
         _477 = _402;
         _478 = _403;
      }
   }
   int _479 = c_bool & 4;
   bool _480 = (_479 == 0);
   if (_480)
   {
      float _482 = saturate(COLOR_1.x);
      float _487 = (c_fog_param[0].x) - _476;
      float _488 = (c_fog_param[0].y) - _477;
      float _489 = (c_fog_param[0].z) - _478;
      float _490 = _487 * _482;
      float _491 = _488 * _482;
      float _492 = _489 * _482;
      _494 = _490;
      _495 = _491;
      _496 = _492;
   }
   else
   {
      _494 = COLOR_1.x;
      _495 = COLOR_1.y;
      _496 = COLOR_1.z;
   }
   float _497 = _494 + _476;
   float _498 = _495 + _477;
   float _499 = _496 + _478;
   float _502 = _34.w - c_mat_punchthrough_ref;
   bool _503 = (_502 < 0.0f);
   if (_503)
      discard;
   float _509 = (c_ps_col_scl[0].x) * _497;
   float _510 = (c_ps_col_scl[0].y) * _498;
   float _511 = (c_ps_col_scl[0].z) * _499;
   float _512 = (c_ps_col_scl[0].w) * _410;
   SV_Target.x = _509;
   SV_Target.y = _510;
   SV_Target.z = _511;
   SV_Target.w = _512;
   int _515 = c_bool & 8;
   bool _516 = (_515 != 0);
   float _517 = select(_516, _497, _509);
   float _518 = select(_516, _498, _510);
   float _519 = select(_516, _499, _511);
   float _520 = select(_516, _410, _512);
   SV_Target.x = _517;
   SV_Target.y = _518;
   SV_Target.z = _519;
   SV_Target.w = _520;
   return SV_Target;
}
