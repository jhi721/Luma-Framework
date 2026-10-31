// Yakuza Kiwami material r_o1m2t (0x7C162B06, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
   float4 _45 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _50 = _45.x * _45.x;
   float _51 = _45.y * _45.y;
   float _52 = _45.z * _45.z;
   float4 _53 = s5_t.Sample(s5_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _56 = _53.x * 2.0f;
   float _57 = _53.y * 2.0f;
   float _58 = _56 + -1.0f;
   float _59 = _57 + -1.0f;
   float _60 = dot(float2(_58, _59), float2(_58, _59));
   float _61 = 1.0f - _60;
   float _62 = saturate(_61);
   float _63 = sqrt(_62);
   float _64 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _65 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _66 = _64 - _65;
   float _67 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _68 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _69 = _67 - _68;
   float _70 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _71 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _72 = _70 - _71;
   float _73 = _58 * TEXCOORD_6.x;
   float _74 = _58 * TEXCOORD_6.y;
   float _75 = _58 * TEXCOORD_6.z;
   float _76 = _59 * TEXCOORD_6.w;
   float _77 = _76 * _66;
   float _78 = _76 * _69;
   float _79 = _76 * _72;
   float _80 = _63 * TEXCOORD_2.x;
   float _81 = _63 * TEXCOORD_2.y;
   float _82 = _63 * TEXCOORD_2.z;
   float _83 = _80 + _73;
   float _84 = _83 + _77;
   float _85 = _81 + _74;
   float _86 = _85 + _78;
   float _87 = _82 + _75;
   float _88 = _87 + _79;
   float _89 = dot(float3(_84, _86, _88), float3(_84, _86, _88));
   float _90 = rsqrt(_89);
   float _91 = _84 * _90;
   float _92 = _86 * _90;
   float _93 = _88 * _90;
   float _94 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _95 = rsqrt(_94);
   float _96 = _95 * TEXCOORD_3.x;
   float _97 = _95 * TEXCOORD_3.y;
   float _98 = _95 * TEXCOORD_3.z;
   float _103 = -0.0f - (c_light_vec[0].x);
   float _104 = -0.0f - (c_light_vec[0].y);
   float _105 = -0.0f - (c_light_vec[0].z);
   float _106 = dot(float3(_91, _92, _93), float3(_103, _104, _105));
   float _110 = c_rt_size.z * SV_Position.x;
   float _111 = c_rt_size.w * SV_Position.y;
   int _114 = c_bool & 8;
   bool _115 = (_114 == 0);
   float _122;
   float _123;
   float _124;
   float _136;
   float _137;
   float _173;
   float _174;
   float _175;
   float _176;
   float _190;
   float _191;
   float _226;
   float _227;
   float _228;
   float _262;
   float _263;
   float _264;
   float _265;
   float _266;
   float _267;
   float _421;
   float _422;
   float _423;
   float _424;
   float _425;
   float _426;
   float _598;
   float _599;
   float _600;
   float _601;
   float _602;
   float _603;
   float _678;
   float _679;
   float _680;
   float _696;
   float _697;
   float _698;
   if (!_115)
   {
      float4 _117 = s13_t.Sample(s13_s, float2(_110, _111));
      _122 = _117.x;
      _123 = _117.y;
      _124 = _117.w;
   }
   else
   {
      _122 = 1.0f;
      _123 = 1.0f;
      _124 = 1.0f;
   }
   if (!_115)
   {
      float _129 = (c_sdw_param[0].x) * _122;
      float _130 = (c_sdw_param[0].z) * _123;
      float _133 = _129 + (c_sdw_param[0].y);
      float _134 = _130 + (c_sdw_param[0].w);
      _136 = _133;
      _137 = _134;
   }
   else
   {
      _136 = _122;
      _137 = _123;
   }
   float _141 = c_rt_size.z * SV_Position.x;
   float _142 = c_rt_size.w * SV_Position.y;
   float4 _143 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_141, _142)));
   int _150 = c_bool & 16384;
   bool _151 = (_150 == 0);
   if (!_151)
   {
      float _153 = _143.x * 2.0f;
      float _154 = _143.y * 2.0f;
      float _155 = _143.z * 2.0f;
      float _156 = _143.w * 2.0f;
      _173 = _153;
      _174 = _154;
      _175 = _155;
      _176 = _156;
   }
   else
   {
      int _158 = c_bool & 8192;
      bool _159 = (_158 == 0);
      if (!_159)
      {
         float _161 = _143.x * 2.0f;
         float _162 = _143.y * 2.0f;
         float _163 = _143.z * 2.0f;
         float _164 = _143.w * 2.0f;
         _173 = _161;
         _174 = _162;
         _175 = _163;
         _176 = _164;
      }
      else
      {
         float _168 = _143.x / c_tone_control.z;
         float _169 = _143.y / c_tone_control.z;
         float _170 = _143.z / c_tone_control.z;
         float _171 = _143.w / c_tone_control.z;
         _173 = _168;
         _174 = _169;
         _175 = _170;
         _176 = _171;
      }
   }
   int _177 = c_bool & 8;
   bool _178 = (_177 == 0);
   if (!_178)
   {
      float _183 = (c_sdw_param[0].x) * _136;
      float _184 = (c_sdw_param[0].z) * _137;
      float _187 = _183 + (c_sdw_param[0].y);
      float _188 = _184 + (c_sdw_param[0].w);
      _190 = _187;
      _191 = _188;
   }
   else
   {
      _190 = _136;
      _191 = _137;
   }
   float _192 = _191 * _190;
   float _197 = 1.0f - (c_light_col[5].x);
   float _198 = 1.0f - (c_light_col[5].y);
   float _199 = 1.0f - (c_light_col[5].z);
   float _200 = _197 * _192;
   float _201 = _198 * _192;
   float _202 = _199 * _192;
   float _203 = _200 + (c_light_col[5].x);
   float _204 = _201 + (c_light_col[5].y);
   float _205 = _202 + (c_light_col[5].z);
   float _206 = _124 * COLOR.x;
   float _207 = _124 * COLOR.y;
   float _208 = _124 * COLOR.z;
   if (!_178)
   {
      float _212 = (c_sdw_param[2].x) * _203;
      float _213 = (c_sdw_param[2].x) * _204;
      float _214 = (c_sdw_param[2].x) * _205;
      float _216 = _212 + (c_sdw_param[2].y);
      float _217 = _213 + (c_sdw_param[2].y);
      float _218 = _214 + (c_sdw_param[2].y);
      float _221 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _222 = _216 * _221;
      float _223 = _217 * _221;
      float _224 = _218 * _221;
      _226 = _222;
      _227 = _223;
      _228 = _224;
   }
   else
   {
      _226 = _203;
      _227 = _204;
      _228 = _205;
   }
   float _229 = _226 * _124;
   float _230 = _227 * _124;
   float _231 = _228 * _124;
   float _242 = c_mat_emissive.x * _50;
   float _243 = _242 * c_emissive_scale;
   float _244 = c_mat_emissive.y * _51;
   float _245 = _244 * c_emissive_scale;
   float _246 = c_mat_emissive.z * _52;
   float _247 = _246 * c_emissive_scale;
   int _248 = c_bool & 2048;
   bool _249 = (_248 == 0);
   if (_249)
   {
      float _251 = _191 * _173;
      float _252 = _191 * _174;
      float _253 = _191 * _175;
      float _254 = _251 + TEXCOORD_4.x;
      float _255 = _252 + TEXCOORD_4.y;
      float _256 = _253 + TEXCOORD_4.z;
      float _257 = _191 * _176;
      float _258 = _257 + TEXCOORD_5.x;
      float _259 = _257 + TEXCOORD_5.y;
      float _260 = _257 + TEXCOORD_5.z;
      _262 = _258;
      _263 = _259;
      _264 = _260;
      _265 = _254;
      _266 = _255;
      _267 = _256;
   }
   else
   {
      _262 = TEXCOORD_5.x;
      _263 = TEXCOORD_5.y;
      _264 = TEXCOORD_5.z;
      _265 = TEXCOORD_4.x;
      _266 = TEXCOORD_4.y;
      _267 = TEXCOORD_4.z;
   }
   float _272 = -0.0f - (c_light_vec[1].x);
   float _273 = -0.0f - (c_light_vec[1].y);
   float _274 = -0.0f - (c_light_vec[1].z);
   float _275 = dot(float3(_91, _92, _93), float3(_272, _273, _274));
   float _276 = saturate(_275);
   float _281 = saturate(_106);
   float _282 = (c_light_col[0].x) * _281;
   float _283 = (c_light_col[0].y) * _281;
   float _284 = (c_light_col[0].z) * _281;
   float _289 = (c_light_col[1].x) * _276;
   float _290 = (c_light_col[1].y) * _276;
   float _291 = (c_light_col[1].z) * _276;
   float _300 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _301 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _302 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _303 = _282 * _203;
   float _304 = _283 * _204;
   float _305 = _284 * _205;
   float _306 = _289 * _191;
   float _307 = _290 * _191;
   float _308 = _291 * _191;
   float _309 = _306 + _303;
   float _310 = _307 + _304;
   float _311 = _308 + _305;
   float _312 = _300 * _309;
   float _313 = _301 * _310;
   float _314 = _302 * _311;
   float _315 = _243 + c_scene_ambient.x;
   float _316 = _315 + _312;
   float _317 = _245 + c_scene_ambient.y;
   float _318 = _317 + _313;
   float _319 = _247 + c_scene_ambient.z;
   float _320 = _319 + _314;
   float _321 = _316 * _206;
   float _322 = _318 * _207;
   float _323 = _320 * _208;
   float _324 = _265 * _229;
   float _325 = _266 * _230;
   float _326 = _267 * _231;
   float _327 = _321 + _324;
   float _328 = _322 + _325;
   float _329 = _323 + _326;
   float _334 = (c_light_vec[0].x) + _96;
   float _335 = (c_light_vec[0].y) + _97;
   float _336 = (c_light_vec[0].z) + _98;
   float _337 = dot(float3(_334, _335, _336), float3(_334, _335, _336));
   float _338 = rsqrt(_337);
   float _339 = _334 * _338;
   float _340 = -0.0f - _339;
   float _341 = _335 * _338;
   float _342 = -0.0f - _341;
   float _343 = _336 * _338;
   float _344 = -0.0f - _343;
   float _345 = dot(float3(_340, _342, _344), float3(_91, _92, _93));
   float _346 = saturate(_345);
   float _347 = _97 + -0.30000001192092896f;
   float _348 = dot(float3(_96, _347, _98), float3(_96, _347, _98));
   float _349 = rsqrt(_348);
   float _350 = _96 * _349;
   float _351 = -0.0f - _350;
   float _352 = _347 * _349;
   float _353 = -0.0f - _352;
   float _354 = _98 * _349;
   float _355 = -0.0f - _354;
   float _356 = dot(float3(_351, _353, _355), float3(_91, _92, _93));
   float _357 = saturate(_356);
   float _360 = log2(_346);
   float _361 = log2(_357);
   float _362 = _360 * c_mat_power;
   float _363 = _361 * c_mat_power;
   float _364 = exp2(_362);
   float _365 = exp2(_363);
   float _368 = _364 * (c_light_col[0].x);
   float _369 = _364 * (c_light_col[0].y);
   float _370 = _364 * (c_light_col[0].z);
   float _371 = _365 * 2.0f;
   float _372 = _371 * (c_light_vec[5].x);
   float _373 = _372 * (c_light_col[1].x);
   float _374 = _372 * (c_light_col[1].y);
   float _375 = _372 * (c_light_col[1].z);
   float _376 = _368 * _203;
   float _377 = _369 * _204;
   float _378 = _370 * _205;
   float _379 = _373 * _276;
   float _380 = _374 * _276;
   float _381 = _375 * _276;
   float _382 = _379 + _376;
   float _383 = _380 + _377;
   float _384 = _381 + _378;
   float _391 = c_mat_specular.x * c_specular_scale.x;
   float _392 = _391 * _206;
   float _393 = _392 * _382;
   float _394 = c_mat_specular.y * c_specular_scale.y;
   float _395 = _394 * _207;
   float _396 = _395 * _383;
   float _397 = c_mat_specular.z * c_specular_scale.z;
   float _398 = _397 * _208;
   float _399 = _398 * _384;
   float _400 = _262 * _229;
   float _401 = _400 * _365;
   float _402 = _263 * _230;
   float _403 = _402 * _365;
   float _404 = _264 * _231;
   float _405 = _404 * _365;
   float _406 = _393 + _401;
   float _407 = _396 + _403;
   float _408 = _399 + _405;
   if (!_249)
   {
      float _410 = _191 * _173;
      float _411 = _191 * _174;
      float _412 = _191 * _175;
      float _413 = _327 + _410;
      float _414 = _328 + _411;
      float _415 = _329 + _412;
      float _416 = _191 * _176;
      float _417 = _406 + _416;
      float _418 = _407 + _416;
      float _419 = _408 + _416;
      _421 = _413;
      _422 = _414;
      _423 = _415;
      _424 = _417;
      _425 = _418;
      _426 = _419;
   }
   else
   {
      _421 = _327;
      _422 = _328;
      _423 = _329;
      _424 = _406;
      _425 = _407;
      _426 = _408;
   }
   int _427 = c_bool & 128;
   bool _428 = (_427 != 0);
   float _429 = select(_428, 0.0f, _424);
   float _430 = select(_428, 0.0f, _425);
   float _431 = select(_428, 0.0f, _426);
   float _432 = _421 * _50;
   float _433 = _422 * _51;
   float _434 = _423 * _52;
   float _435 = _429 * _45.w;
   float _436 = _430 * _45.w;
   float _437 = _431 * _45.w;
   int _438 = c_bool & 32768;
   bool _439 = (_438 == 0);
   if (!_439)
   {
      float _443 = (c_ps_user[11].x) * _50;
      float _444 = (c_ps_user[11].x) * _51;
      float _445 = (c_ps_user[11].x) * _52;
      float _447 = _91 + 1.0f;
      float _448 = _92 + 1.0f;
      float _449 = _93 + 1.0f;
      float _450 = _447 * 0.5f;
      float _451 = _448 * 0.5f;
      float _452 = _449 * 0.5f;
      float _453 = saturate(_450);
      float _454 = saturate(_451);
      float _455 = saturate(_452);
      float _456 = (c_ps_user[11].y) * _453;
      float _457 = _454 * (c_ps_user[11].y);
      float _458 = _455 * (c_ps_user[11].y);
      float _460 = (c_ps_user[11].z) * _45.w;
      float _462 = c_mat_power * 0.009999999776482582f;
      float _463 = _462 * (c_ps_user[11].w);
      float _466 = (c_ps_user[12].x) * _203;
      float _467 = (c_ps_user[12].x) * _204;
      float _468 = (c_ps_user[12].x) * _205;
      float _470 = (c_ps_user[12].y) * _206;
      float _471 = (c_ps_user[12].y) * _207;
      float _472 = (c_ps_user[12].y) * _208;
      float _474 = (c_ps_user[12].z) * _229;
      float _475 = (c_ps_user[12].z) * _230;
      float _476 = (c_ps_user[12].z) * _231;
      float _482 = -0.0f - _96;
      float _483 = -0.0f - _97;
      float _484 = -0.0f - _98;
      float _485 = dot(float3(_482, _483, _484), float3(_91, _92, _93));
      float _486 = 1.0f - _485;
      float _487 = _486 * _486;
      float _488 = _486 * 0.9722222089767456f;
      float _489 = _487 * _487;
      float _490 = _489 * _488;
      float _491 = _490 + 0.02777777425944805f;
      float _492 = _491 * (c_ps_user[13].z);
      float _494 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _495 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _496 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _499 = (c_ps_user[14].x) * _243;
      float _500 = (c_ps_user[14].x) * _245;
      float _501 = (c_ps_user[14].x) * _247;
      float _503 = (c_ps_user[14].y) * _282;
      float _504 = (c_ps_user[14].y) * _283;
      float _505 = (c_ps_user[14].y) * _284;
      float _507 = (c_ps_user[14].z) * _289;
      float _508 = (c_ps_user[14].z) * _290;
      float _509 = (c_ps_user[14].z) * _291;
      float _511 = (c_ps_user[14].w) * _265;
      float _512 = (c_ps_user[14].w) * _266;
      float _513 = (c_ps_user[14].w) * _267;
      float _516 = (c_ps_user[15].x) * _368;
      float _517 = (c_ps_user[15].x) * _369;
      float _518 = (c_ps_user[15].x) * _370;
      float _520 = _391 * _373;
      float _521 = _520 * (c_ps_user[15].y);
      float _522 = _394 * _374;
      float _523 = _522 * (c_ps_user[15].y);
      float _524 = _397 * _375;
      float _525 = _524 * (c_ps_user[15].y);
      float _527 = (c_ps_user[15].z) * _262;
      float _528 = (c_ps_user[15].z) * _263;
      float _529 = (c_ps_user[15].z) * _264;
      float _530 = _521 + _527;
      float _531 = _530 * _365;
      float _532 = _503 + (c_ps_user[13].x);
      float _533 = _532 + _507;
      float _534 = c_diffuse_scale.x * _533;
      float _535 = _534 * c_mat_diffuse.x;
      float _536 = (c_ps_user[13].y) + _460;
      float _537 = _536 + _516;
      float _538 = c_mat_specular.x * _537;
      float _539 = _538 * c_specular_scale.x;
      float _540 = _456 + _443;
      float _541 = _540 + _463;
      float _542 = _541 + (c_ps_user[12].w);
      float _543 = _542 + _466;
      float _544 = _543 + _470;
      float _545 = _544 + _474;
      float _546 = _545 + _494;
      float _547 = _546 + _499;
      float _548 = _547 + _511;
      float _549 = _548 + _492;
      float _550 = _549 + _531;
      float _551 = _550 + _535;
      float _552 = _551 + _539;
      float _553 = _523 + _528;
      float _554 = _553 * _365;
      float _555 = _504 + (c_ps_user[13].x);
      float _556 = _555 + _508;
      float _557 = c_diffuse_scale.y * _556;
      float _558 = _557 * c_mat_diffuse.y;
      float _559 = _536 + _517;
      float _560 = c_mat_specular.y * _559;
      float _561 = _560 * c_specular_scale.y;
      float _562 = _457 + _444;
      float _563 = _562 + _463;
      float _564 = _563 + (c_ps_user[12].w);
      float _565 = _564 + _467;
      float _566 = _565 + _471;
      float _567 = _566 + _475;
      float _568 = _567 + _495;
      float _569 = _568 + _500;
      float _570 = _569 + _512;
      float _571 = _570 + _492;
      float _572 = _571 + _554;
      float _573 = _572 + _558;
      float _574 = _573 + _561;
      float _575 = _525 + _529;
      float _576 = _575 * _365;
      float _577 = _505 + (c_ps_user[13].x);
      float _578 = _577 + _509;
      float _579 = c_diffuse_scale.z * _578;
      float _580 = _579 * c_mat_diffuse.z;
      float _581 = _536 + _518;
      float _582 = c_mat_specular.z * _581;
      float _583 = _582 * c_specular_scale.z;
      float _584 = _463 + _445;
      float _585 = _584 + _458;
      float _586 = _585 + (c_ps_user[12].w);
      float _587 = _586 + _468;
      float _588 = _587 + _472;
      float _589 = _588 + _476;
      float _590 = _589 + _496;
      float _591 = _590 + _501;
      float _592 = _591 + _513;
      float _593 = _592 + _492;
      float _594 = _593 + _576;
      float _595 = _594 + _580;
      float _596 = _595 + _583;
      _598 = 0.0f;
      _599 = 0.0f;
      _600 = 0.0f;
      _601 = _552;
      _602 = _574;
      _603 = _596;
   }
   else
   {
      _598 = _435;
      _599 = _436;
      _600 = _437;
      _601 = _432;
      _602 = _433;
      _603 = _434;
   }
   float _604 = _601 + _598;
   float _605 = _602 + _599;
   float _606 = _603 + _600;
   float _611 = c_global_opacity * c_mat_opacity;
   float _612 = _611 * c_opacity_scale;
   int _613 = c_bool & 16;
   bool _614 = (_613 == 0);
   if (!_614)
   {
      float _618 = c_tone_control.x * _604;
      float _619 = c_tone_control.x * _605;
      float _620 = c_tone_control.x * _606;
      float _622 = log2(_618);
      float _623 = log2(_619);
      float _624 = log2(_620);
      float _625 = _622 * c_tone_control.y;
      float _626 = _623 * c_tone_control.y;
      float _627 = _624 * c_tone_control.y;
      float _628 = exp2(_625);
      float _629 = exp2(_626);
      float _630 = exp2(_627);
      _678 = _628;
      _679 = _629;
      _680 = _630;
   }
   else
   {
      if (_439)
      {
         float _635 = c_tone_control.x * _604;
         float _636 = c_tone_control.x * _605;
         float _637 = c_tone_control.x * _606;
         float _638 = _635 * 2.0f;
         float _639 = _636 * 2.0f;
         float _640 = _637 * 2.0f;
         float _641 = _635 * 0.30000001192092896f;
         float _642 = _636 * 0.30000001192092896f;
         float _643 = _637 * 0.30000001192092896f;
         float _644 = _641 + 0.05000000074505806f;
         float _645 = _642 + 0.05000000074505806f;
         float _646 = _643 + 0.05000000074505806f;
         float _647 = _644 * _638;
         float _648 = _645 * _639;
         float _649 = _646 * _640;
         float _650 = _647 + 0.004000000189989805f;
         float _651 = _648 + 0.004000000189989805f;
         float _652 = _649 + 0.004000000189989805f;
         float _653 = _641 + 0.5f;
         float _654 = _642 + 0.5f;
         float _655 = _643 + 0.5f;
         float _656 = _653 * _638;
         float _657 = _654 * _639;
         float _658 = _655 * _640;
         float _659 = _656 + 0.06000000238418579f;
         float _660 = _657 + 0.06000000238418579f;
         float _661 = _658 + 0.06000000238418579f;
         float _662 = _650 / _659;
         float _663 = _651 / _660;
         float _664 = _652 / _661;
         float _665 = _662 + -0.06666666269302368f;
         float _666 = _663 + -0.06666666269302368f;
         float _667 = _664 + -0.06666666269302368f;
         float _668 = _665 * 1.3790643215179443f;
         float _669 = _666 * 1.3790643215179443f;
         float _670 = _667 * 1.3790643215179443f;
         float _671 = YKR_MaterialCurve(_668, _635);
         float _672 = YKR_MaterialCurve(_669, _636);
         float _673 = YKR_MaterialCurve(_670, _637);
         float _674 = sqrt(_671);
         float _675 = sqrt(_672);
         float _676 = sqrt(_673);
         _678 = _674;
         _679 = _675;
         _680 = _676;
      }
      else
      {
         _678 = _604;
         _679 = _605;
         _680 = _606;
      }
   }
   int _681 = c_bool & 4;
   bool _682 = (_681 == 0);
   if (_682)
   {
      float _684 = saturate(COLOR_1.x);
      float _689 = (c_fog_param[0].x) - _678;
      float _690 = (c_fog_param[0].y) - _679;
      float _691 = (c_fog_param[0].z) - _680;
      float _692 = _689 * _684;
      float _693 = _690 * _684;
      float _694 = _691 * _684;
      _696 = _692;
      _697 = _693;
      _698 = _694;
   }
   else
   {
      _696 = COLOR_1.x;
      _697 = COLOR_1.y;
      _698 = COLOR_1.z;
   }
   float _699 = _696 + _678;
   float _700 = _697 + _679;
   float _701 = _698 + _680;
   float _707 = (c_ps_col_scl[0].x) * _699;
   float _708 = (c_ps_col_scl[0].y) * _700;
   float _709 = (c_ps_col_scl[0].z) * _701;
   float _710 = (c_ps_col_scl[0].w) * _612;
   SV_Target.x = _707;
   SV_Target.y = _708;
   SV_Target.z = _709;
   SV_Target.w = _710;
   int _713 = c_bool & 8;
   bool _714 = (_713 != 0);
   float _715 = select(_714, _699, _707);
   float _716 = select(_714, _700, _708);
   float _717 = select(_714, _701, _709);
   float _718 = select(_714, _612, _710);
   SV_Target.x = _715;
   SV_Target.y = _716;
   SV_Target.z = _717;
   SV_Target.w = _718;
   return SV_Target;
}
