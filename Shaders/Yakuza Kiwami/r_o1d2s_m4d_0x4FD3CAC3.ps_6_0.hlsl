// Yakuza Kiwami material r_o1d2s_m4d (0x4FD3CAC3, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

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
   float4 _45 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _50 = _45.x * _45.x;
   float _51 = _45.y * _45.y;
   float _52 = _45.z * _45.z;
   float4 _53 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _57 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
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
   float _163;
   float _164;
   float _165;
   float _166;
   float _180;
   float _181;
   float _227;
   float _228;
   float _229;
   float _264;
   float _265;
   float _266;
   float _267;
   float _268;
   float _269;
   float _423;
   float _424;
   float _425;
   float _426;
   float _427;
   float _428;
   float _604;
   float _605;
   float _606;
   float _607;
   float _608;
   float _609;
   float _685;
   float _686;
   float _687;
   float _703;
   float _704;
   float _705;
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
   float _115 = (c_sdw_param[1].x) * _57.y;
   float _117 = _115 + (c_sdw_param[1].y);
   float _118 = _117 * _57.x;
   float _119 = 1.0f - (c_light_col[5].x);
   float _120 = 1.0f - (c_light_col[5].y);
   float _121 = 1.0f - (c_light_col[5].z);
   float _122 = _118 * _119;
   float _123 = _118 * _120;
   float _124 = _118 * _121;
   float _125 = _122 + (c_light_col[5].x);
   float _126 = _123 + (c_light_col[5].y);
   float _127 = _124 + (c_light_col[5].z);
   float _131 = c_rt_size.z * SV_Position.x;
   float _132 = c_rt_size.w * SV_Position.y;
   float4 _133 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_131, _132)));
   int _140 = c_bool & 16384;
   bool _141 = (_140 == 0);
   if (!_141)
   {
      float _143 = _133.x * 2.0f;
      float _144 = _133.y * 2.0f;
      float _145 = _133.z * 2.0f;
      float _146 = _133.w * 2.0f;
      _163 = _143;
      _164 = _144;
      _165 = _145;
      _166 = _146;
   }
   else
   {
      int _148 = c_bool & 8192;
      bool _149 = (_148 == 0);
      if (!_149)
      {
         float _151 = _133.x * 2.0f;
         float _152 = _133.y * 2.0f;
         float _153 = _133.z * 2.0f;
         float _154 = _133.w * 2.0f;
         _163 = _151;
         _164 = _152;
         _165 = _153;
         _166 = _154;
      }
      else
      {
         float _158 = _133.x / c_tone_control.z;
         float _159 = _133.y / c_tone_control.z;
         float _160 = _133.z / c_tone_control.z;
         float _161 = _133.w / c_tone_control.z;
         _163 = _158;
         _164 = _159;
         _165 = _160;
         _166 = _161;
      }
   }
   int _167 = c_bool & 8;
   bool _168 = (_167 == 0);
   if (!_168)
   {
      float _173 = (c_sdw_param[0].x) * _107;
      float _174 = (c_sdw_param[0].z) * _108;
      float _177 = _173 + (c_sdw_param[0].y);
      float _178 = _174 + (c_sdw_param[0].w);
      _180 = _177;
      _181 = _178;
   }
   else
   {
      _180 = _107;
      _181 = _108;
   }
   float _182 = _181 * _180;
   float _187 = 1.0f - (c_light_col[5].x);
   float _188 = 1.0f - (c_light_col[5].y);
   float _189 = 1.0f - (c_light_col[5].z);
   float _190 = _187 * _182;
   float _191 = _188 * _182;
   float _192 = _189 * _182;
   float _193 = _190 + (c_light_col[5].x);
   float _194 = _191 + (c_light_col[5].y);
   float _195 = _192 + (c_light_col[5].z);
   float _196 = _95 * COLOR.x;
   float _197 = _196 * _125;
   float _198 = _95 * COLOR.y;
   float _199 = _198 * _126;
   float _200 = _95 * COLOR.z;
   float _201 = _200 * _127;
   float _202 = _193 * _125;
   float _203 = _194 * _126;
   float _204 = _195 * _127;
   if (!_168)
   {
      float _208 = (c_sdw_param[2].x) * _193;
      float _209 = (c_sdw_param[2].x) * _194;
      float _210 = (c_sdw_param[2].x) * _195;
      float _212 = _208 + (c_sdw_param[2].y);
      float _213 = _209 + (c_sdw_param[2].y);
      float _214 = _210 + (c_sdw_param[2].y);
      float _216 = (c_sdw_param[2].z) * _125;
      float _217 = (c_sdw_param[2].z) * _126;
      float _218 = (c_sdw_param[2].z) * _127;
      float _220 = _216 + (c_sdw_param[2].w);
      float _221 = _217 + (c_sdw_param[2].w);
      float _222 = _218 + (c_sdw_param[2].w);
      float _223 = _220 * _212;
      float _224 = _221 * _213;
      float _225 = _222 * _214;
      _227 = _223;
      _228 = _224;
      _229 = _225;
   }
   else
   {
      _227 = _202;
      _228 = _203;
      _229 = _204;
   }
   float _230 = _95 * _57.x;
   float _231 = _227 * _230;
   float _232 = _228 * _230;
   float _233 = _229 * _230;
   float _244 = c_mat_emissive.x * _50;
   float _245 = _244 * c_emissive_scale;
   float _246 = c_mat_emissive.y * _51;
   float _247 = _246 * c_emissive_scale;
   float _248 = c_mat_emissive.z * _52;
   float _249 = _248 * c_emissive_scale;
   int _250 = c_bool & 2048;
   bool _251 = (_250 == 0);
   if (_251)
   {
      float _253 = _181 * _163;
      float _254 = _181 * _164;
      float _255 = _181 * _165;
      float _256 = _253 + TEXCOORD_4.x;
      float _257 = _254 + TEXCOORD_4.y;
      float _258 = _255 + TEXCOORD_4.z;
      float _259 = _181 * _166;
      float _260 = _259 + TEXCOORD_5.x;
      float _261 = _259 + TEXCOORD_5.y;
      float _262 = _259 + TEXCOORD_5.z;
      _264 = _256;
      _265 = _257;
      _266 = _258;
      _267 = _260;
      _268 = _261;
      _269 = _262;
   }
   else
   {
      _264 = TEXCOORD_4.x;
      _265 = TEXCOORD_4.y;
      _266 = TEXCOORD_4.z;
      _267 = TEXCOORD_5.x;
      _268 = TEXCOORD_5.y;
      _269 = TEXCOORD_5.z;
   }
   float _274 = -0.0f - (c_light_vec[1].x);
   float _275 = -0.0f - (c_light_vec[1].y);
   float _276 = -0.0f - (c_light_vec[1].z);
   float _277 = dot(float3(_62, _63, _64), float3(_274, _275, _276));
   float _278 = saturate(_277);
   float _283 = saturate(_77);
   float _284 = (c_light_col[0].x) * _283;
   float _285 = (c_light_col[0].y) * _283;
   float _286 = (c_light_col[0].z) * _283;
   float _291 = (c_light_col[1].x) * _278;
   float _292 = (c_light_col[1].y) * _278;
   float _293 = (c_light_col[1].z) * _278;
   float _302 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _303 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _304 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _305 = _284 * _193;
   float _306 = _285 * _194;
   float _307 = _286 * _195;
   float _308 = _291 * _181;
   float _309 = _292 * _181;
   float _310 = _293 * _181;
   float _311 = _308 + _305;
   float _312 = _309 + _306;
   float _313 = _310 + _307;
   float _314 = _302 * _311;
   float _315 = _303 * _312;
   float _316 = _304 * _313;
   float _317 = _245 + c_scene_ambient.x;
   float _318 = _317 + _314;
   float _319 = _247 + c_scene_ambient.y;
   float _320 = _319 + _315;
   float _321 = _249 + c_scene_ambient.z;
   float _322 = _321 + _316;
   float _323 = _318 * _197;
   float _324 = _320 * _199;
   float _325 = _322 * _201;
   float _326 = _264 * _231;
   float _327 = _265 * _232;
   float _328 = _266 * _233;
   float _329 = _323 + _326;
   float _330 = _324 + _327;
   float _331 = _325 + _328;
   float _336 = (c_light_vec[0].x) + _67;
   float _337 = (c_light_vec[0].y) + _68;
   float _338 = (c_light_vec[0].z) + _69;
   float _339 = dot(float3(_336, _337, _338), float3(_336, _337, _338));
   float _340 = rsqrt(_339);
   float _341 = _336 * _340;
   float _342 = -0.0f - _341;
   float _343 = _337 * _340;
   float _344 = -0.0f - _343;
   float _345 = _338 * _340;
   float _346 = -0.0f - _345;
   float _347 = dot(float3(_342, _344, _346), float3(_62, _63, _64));
   float _348 = saturate(_347);
   float _349 = _68 + -0.30000001192092896f;
   float _350 = dot(float3(_67, _349, _69), float3(_67, _349, _69));
   float _351 = rsqrt(_350);
   float _352 = _67 * _351;
   float _353 = -0.0f - _352;
   float _354 = _349 * _351;
   float _355 = -0.0f - _354;
   float _356 = _69 * _351;
   float _357 = -0.0f - _356;
   float _358 = dot(float3(_353, _355, _357), float3(_62, _63, _64));
   float _359 = saturate(_358);
   float _362 = log2(_348);
   float _363 = log2(_359);
   float _364 = _362 * c_mat_power;
   float _365 = _363 * c_mat_power;
   float _366 = exp2(_364);
   float _367 = exp2(_365);
   float _370 = _366 * (c_light_col[0].x);
   float _371 = _366 * (c_light_col[0].y);
   float _372 = _366 * (c_light_col[0].z);
   float _373 = _367 * 2.0f;
   float _374 = _373 * (c_light_vec[5].x);
   float _375 = _374 * (c_light_col[1].x);
   float _376 = _374 * (c_light_col[1].y);
   float _377 = _374 * (c_light_col[1].z);
   float _378 = _370 * _193;
   float _379 = _371 * _194;
   float _380 = _372 * _195;
   float _381 = _375 * _278;
   float _382 = _376 * _278;
   float _383 = _377 * _278;
   float _384 = _381 + _378;
   float _385 = _382 + _379;
   float _386 = _383 + _380;
   float _393 = c_mat_specular.x * c_specular_scale.x;
   float _394 = _393 * _197;
   float _395 = _394 * _384;
   float _396 = c_mat_specular.y * c_specular_scale.y;
   float _397 = _396 * _199;
   float _398 = _397 * _385;
   float _399 = c_mat_specular.z * c_specular_scale.z;
   float _400 = _399 * _201;
   float _401 = _400 * _386;
   float _402 = _267 * _231;
   float _403 = _402 * _367;
   float _404 = _268 * _232;
   float _405 = _404 * _367;
   float _406 = _269 * _233;
   float _407 = _406 * _367;
   float _408 = _395 + _403;
   float _409 = _398 + _405;
   float _410 = _401 + _407;
   if (!_251)
   {
      float _412 = _181 * _163;
      float _413 = _181 * _164;
      float _414 = _181 * _165;
      float _415 = _329 + _412;
      float _416 = _330 + _413;
      float _417 = _331 + _414;
      float _418 = _181 * _166;
      float _419 = _408 + _418;
      float _420 = _409 + _418;
      float _421 = _410 + _418;
      _423 = _415;
      _424 = _416;
      _425 = _417;
      _426 = _419;
      _427 = _420;
      _428 = _421;
   }
   else
   {
      _423 = _329;
      _424 = _330;
      _425 = _331;
      _426 = _408;
      _427 = _409;
      _428 = _410;
   }
   int _429 = c_bool & 128;
   bool _430 = (_429 != 0);
   float _431 = select(_430, 0.0f, _426);
   float _432 = select(_430, 0.0f, _427);
   float _433 = select(_430, 0.0f, _428);
   float _434 = _423 * _50;
   float _435 = _424 * _51;
   float _436 = _425 * _52;
   float _437 = _431 * _53.x;
   float _438 = _432 * _53.y;
   float _439 = _433 * _53.z;
   int _440 = c_bool & 32768;
   bool _441 = (_440 == 0);
   if (!_441)
   {
      float _445 = (c_ps_user[11].x) * _50;
      float _446 = (c_ps_user[11].x) * _51;
      float _447 = (c_ps_user[11].x) * _52;
      float _449 = _62 + 1.0f;
      float _450 = _63 + 1.0f;
      float _451 = _64 + 1.0f;
      float _452 = _449 * 0.5f;
      float _453 = _450 * 0.5f;
      float _454 = _451 * 0.5f;
      float _455 = saturate(_452);
      float _456 = saturate(_453);
      float _457 = saturate(_454);
      float _458 = (c_ps_user[11].y) * _455;
      float _459 = _456 * (c_ps_user[11].y);
      float _460 = _457 * (c_ps_user[11].y);
      float _462 = (c_ps_user[11].z) * _53.x;
      float _463 = (c_ps_user[11].z) * _53.y;
      float _464 = (c_ps_user[11].z) * _53.z;
      float _466 = c_mat_power * 0.009999999776482582f;
      float _467 = _466 * (c_ps_user[11].w);
      float _470 = (c_ps_user[12].x) * _193;
      float _471 = (c_ps_user[12].x) * _194;
      float _472 = (c_ps_user[12].x) * _195;
      float _474 = (c_ps_user[12].y) * _197;
      float _475 = (c_ps_user[12].y) * _199;
      float _476 = (c_ps_user[12].y) * _201;
      float _478 = (c_ps_user[12].z) * _231;
      float _479 = (c_ps_user[12].z) * _232;
      float _480 = (c_ps_user[12].z) * _233;
      float _486 = -0.0f - _67;
      float _487 = -0.0f - _68;
      float _488 = -0.0f - _69;
      float _489 = dot(float3(_486, _487, _488), float3(_62, _63, _64));
      float _490 = 1.0f - _489;
      float _491 = _490 * _490;
      float _492 = _490 * 0.9722222089767456f;
      float _493 = _491 * _491;
      float _494 = _493 * _492;
      float _495 = _494 + 0.02777777425944805f;
      float _496 = _495 * (c_ps_user[13].z);
      float _498 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _499 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _500 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _503 = (c_ps_user[14].x) * _245;
      float _504 = (c_ps_user[14].x) * _247;
      float _505 = (c_ps_user[14].x) * _249;
      float _507 = (c_ps_user[14].y) * _284;
      float _508 = (c_ps_user[14].y) * _285;
      float _509 = (c_ps_user[14].y) * _286;
      float _511 = (c_ps_user[14].z) * _291;
      float _512 = (c_ps_user[14].z) * _292;
      float _513 = (c_ps_user[14].z) * _293;
      float _515 = (c_ps_user[14].w) * _264;
      float _516 = (c_ps_user[14].w) * _265;
      float _517 = (c_ps_user[14].w) * _266;
      float _520 = (c_ps_user[15].x) * _370;
      float _521 = (c_ps_user[15].x) * _371;
      float _522 = (c_ps_user[15].x) * _372;
      float _524 = _393 * _375;
      float _525 = _524 * (c_ps_user[15].y);
      float _526 = _396 * _376;
      float _527 = _526 * (c_ps_user[15].y);
      float _528 = _399 * _377;
      float _529 = _528 * (c_ps_user[15].y);
      float _531 = (c_ps_user[15].z) * _267;
      float _532 = (c_ps_user[15].z) * _268;
      float _533 = (c_ps_user[15].z) * _269;
      float _534 = _525 + _531;
      float _535 = _534 * _367;
      float _536 = _507 + (c_ps_user[13].x);
      float _537 = _536 + _511;
      float _538 = c_diffuse_scale.x * _537;
      float _539 = _538 * c_mat_diffuse.x;
      float _540 = (c_ps_user[13].y) + _462;
      float _541 = _540 + _520;
      float _542 = c_mat_specular.x * _541;
      float _543 = _542 * c_specular_scale.x;
      float _544 = _458 + _445;
      float _545 = _544 + _467;
      float _546 = _545 + (c_ps_user[12].w);
      float _547 = _546 + _470;
      float _548 = _547 + _474;
      float _549 = _548 + _478;
      float _550 = _549 + _498;
      float _551 = _550 + _503;
      float _552 = _551 + _515;
      float _553 = _552 + _496;
      float _554 = _553 + _535;
      float _555 = _554 + _539;
      float _556 = _555 + _543;
      float _557 = _527 + _532;
      float _558 = _557 * _367;
      float _559 = _508 + (c_ps_user[13].x);
      float _560 = _559 + _512;
      float _561 = c_diffuse_scale.y * _560;
      float _562 = _561 * c_mat_diffuse.y;
      float _563 = (c_ps_user[13].y) + _463;
      float _564 = _563 + _521;
      float _565 = c_mat_specular.y * _564;
      float _566 = _565 * c_specular_scale.y;
      float _567 = _459 + _446;
      float _568 = _567 + _467;
      float _569 = _568 + (c_ps_user[12].w);
      float _570 = _569 + _471;
      float _571 = _570 + _475;
      float _572 = _571 + _479;
      float _573 = _572 + _499;
      float _574 = _573 + _504;
      float _575 = _574 + _516;
      float _576 = _575 + _496;
      float _577 = _576 + _558;
      float _578 = _577 + _562;
      float _579 = _578 + _566;
      float _580 = _529 + _533;
      float _581 = _580 * _367;
      float _582 = _509 + (c_ps_user[13].x);
      float _583 = _582 + _513;
      float _584 = c_diffuse_scale.z * _583;
      float _585 = _584 * c_mat_diffuse.z;
      float _586 = (c_ps_user[13].y) + _464;
      float _587 = _586 + _522;
      float _588 = c_mat_specular.z * _587;
      float _589 = _588 * c_specular_scale.z;
      float _590 = _467 + _447;
      float _591 = _590 + _460;
      float _592 = _591 + (c_ps_user[12].w);
      float _593 = _592 + _472;
      float _594 = _593 + _476;
      float _595 = _594 + _480;
      float _596 = _595 + _500;
      float _597 = _596 + _505;
      float _598 = _597 + _517;
      float _599 = _598 + _496;
      float _600 = _599 + _581;
      float _601 = _600 + _585;
      float _602 = _601 + _589;
      _604 = _556;
      _605 = _579;
      _606 = _602;
      _607 = 0.0f;
      _608 = 0.0f;
      _609 = 0.0f;
   }
   else
   {
      _604 = _434;
      _605 = _435;
      _606 = _436;
      _607 = _437;
      _608 = _438;
      _609 = _439;
   }
   float _610 = _607 + _604;
   float _611 = _608 + _605;
   float _612 = _609 + _606;
   float _617 = c_mat_opacity * _45.w;
   float _618 = _617 * c_global_opacity;
   float _619 = _618 * c_opacity_scale;
   int _620 = c_bool & 16;
   bool _621 = (_620 == 0);
   if (!_621)
   {
      float _625 = c_tone_control.x * _610;
      float _626 = c_tone_control.x * _611;
      float _627 = c_tone_control.x * _612;
      float _629 = log2(_625);
      float _630 = log2(_626);
      float _631 = log2(_627);
      float _632 = _629 * c_tone_control.y;
      float _633 = _630 * c_tone_control.y;
      float _634 = _631 * c_tone_control.y;
      float _635 = exp2(_632);
      float _636 = exp2(_633);
      float _637 = exp2(_634);
      _685 = _635;
      _686 = _636;
      _687 = _637;
   }
   else
   {
      if (_441)
      {
         float _642 = c_tone_control.x * _610;
         float _643 = c_tone_control.x * _611;
         float _644 = c_tone_control.x * _612;
         float _645 = _642 * 2.0f;
         float _646 = _643 * 2.0f;
         float _647 = _644 * 2.0f;
         float _648 = _642 * 0.30000001192092896f;
         float _649 = _643 * 0.30000001192092896f;
         float _650 = _644 * 0.30000001192092896f;
         float _651 = _648 + 0.05000000074505806f;
         float _652 = _649 + 0.05000000074505806f;
         float _653 = _650 + 0.05000000074505806f;
         float _654 = _651 * _645;
         float _655 = _652 * _646;
         float _656 = _653 * _647;
         float _657 = _654 + 0.004000000189989805f;
         float _658 = _655 + 0.004000000189989805f;
         float _659 = _656 + 0.004000000189989805f;
         float _660 = _648 + 0.5f;
         float _661 = _649 + 0.5f;
         float _662 = _650 + 0.5f;
         float _663 = _660 * _645;
         float _664 = _661 * _646;
         float _665 = _662 * _647;
         float _666 = _663 + 0.06000000238418579f;
         float _667 = _664 + 0.06000000238418579f;
         float _668 = _665 + 0.06000000238418579f;
         float _669 = _657 / _666;
         float _670 = _658 / _667;
         float _671 = _659 / _668;
         float _672 = _669 + -0.06666666269302368f;
         float _673 = _670 + -0.06666666269302368f;
         float _674 = _671 + -0.06666666269302368f;
         float _675 = _672 * 1.3790643215179443f;
         float _676 = _673 * 1.3790643215179443f;
         float _677 = _674 * 1.3790643215179443f;
         float _678 = YKR_MaterialCurve(_675, _642);
         float _679 = YKR_MaterialCurve(_676, _643);
         float _680 = YKR_MaterialCurve(_677, _644);
         float _681 = sqrt(_678);
         float _682 = sqrt(_679);
         float _683 = sqrt(_680);
         _685 = _681;
         _686 = _682;
         _687 = _683;
      }
      else
      {
         _685 = _610;
         _686 = _611;
         _687 = _612;
      }
   }
   int _688 = c_bool & 4;
   bool _689 = (_688 == 0);
   if (_689)
   {
      float _691 = saturate(COLOR_1.x);
      float _696 = (c_fog_param[0].x) - _685;
      float _697 = (c_fog_param[0].y) - _686;
      float _698 = (c_fog_param[0].z) - _687;
      float _699 = _696 * _691;
      float _700 = _697 * _691;
      float _701 = _698 * _691;
      _703 = _699;
      _704 = _700;
      _705 = _701;
   }
   else
   {
      _703 = COLOR_1.x;
      _704 = COLOR_1.y;
      _705 = COLOR_1.z;
   }
   float _706 = _703 + _685;
   float _707 = _704 + _686;
   float _708 = _705 + _687;
   float _714 = (c_ps_col_scl[0].x) * _706;
   float _715 = (c_ps_col_scl[0].y) * _707;
   float _716 = (c_ps_col_scl[0].z) * _708;
   float _717 = (c_ps_col_scl[0].w) * _619;
   SV_Target.x = _714;
   SV_Target.y = _715;
   SV_Target.z = _716;
   SV_Target.w = _717;
   int _720 = c_bool & 8;
   bool _721 = (_720 != 0);
   float _722 = select(_721, _706, _714);
   float _723 = select(_721, _707, _715);
   float _724 = select(_721, _708, _716);
   float _725 = select(_721, _619, _717);
   SV_Target.x = _722;
   SV_Target.y = _723;
   SV_Target.z = _724;
   SV_Target.w = _725;
   return SV_Target;
}
