// Yakuza Kiwami material r_o1du_a(vr)2d_m4d (0xC9C991AB, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s1_s : register(s1);

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
   float4 _48 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _53 = _48.x * _48.x;
   float _54 = _48.y * _48.y;
   float _55 = _48.z * _48.z;
   float _59 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _60 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _61 = s3_t.Sample(s3_s, float2(_59, _60));
   float4 _63 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _67 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _70 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _71 = rsqrt(_70);
   float _72 = _71 * TEXCOORD_2.x;
   float _73 = _71 * TEXCOORD_2.y;
   float _74 = _71 * TEXCOORD_2.z;
   float _75 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _76 = rsqrt(_75);
   float _77 = _76 * TEXCOORD_3.x;
   float _78 = _76 * TEXCOORD_3.y;
   float _79 = _76 * TEXCOORD_3.z;
   float _84 = -0.0f - (c_light_vec[0].x);
   float _85 = -0.0f - (c_light_vec[0].y);
   float _86 = -0.0f - (c_light_vec[0].z);
   float _87 = dot(float3(_72, _73, _74), float3(_84, _85, _86));
   float _91 = c_rt_size.z * SV_Position.x;
   float _92 = c_rt_size.w * SV_Position.y;
   int _95 = c_bool & 8;
   bool _96 = (_95 == 0);
   float _103;
   float _104;
   float _105;
   float _117;
   float _118;
   float _173;
   float _174;
   float _175;
   float _176;
   float _190;
   float _191;
   float _237;
   float _238;
   float _239;
   float _274;
   float _275;
   float _276;
   float _277;
   float _278;
   float _279;
   float _433;
   float _434;
   float _435;
   float _436;
   float _437;
   float _438;
   float _610;
   float _611;
   float _612;
   float _613;
   float _614;
   float _615;
   float _700;
   float _701;
   float _702;
   float _718;
   float _719;
   float _720;
   if (!_96)
   {
      float4 _98 = s13_t.Sample(s13_s, float2(_91, _92));
      _103 = _98.x;
      _104 = _98.y;
      _105 = _98.w;
   }
   else
   {
      _103 = 1.0f;
      _104 = 1.0f;
      _105 = 1.0f;
   }
   if (!_96)
   {
      float _110 = (c_sdw_param[0].x) * _103;
      float _111 = (c_sdw_param[0].z) * _104;
      float _114 = _110 + (c_sdw_param[0].y);
      float _115 = _111 + (c_sdw_param[0].w);
      _117 = _114;
      _118 = _115;
   }
   else
   {
      _117 = _103;
      _118 = _104;
   }
   float _125 = (c_sdw_param[1].x) * _67.y;
   float _127 = _125 + (c_sdw_param[1].y);
   float _128 = _127 * _67.x;
   float _129 = 1.0f - (c_light_col[5].x);
   float _130 = 1.0f - (c_light_col[5].y);
   float _131 = 1.0f - (c_light_col[5].z);
   float _132 = _128 * _129;
   float _133 = _128 * _130;
   float _134 = _128 * _131;
   float _135 = _132 + (c_light_col[5].x);
   float _136 = _133 + (c_light_col[5].y);
   float _137 = _134 + (c_light_col[5].z);
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
      float _183 = (c_sdw_param[0].x) * _117;
      float _184 = (c_sdw_param[0].z) * _118;
      float _187 = _183 + (c_sdw_param[0].y);
      float _188 = _184 + (c_sdw_param[0].w);
      _190 = _187;
      _191 = _188;
   }
   else
   {
      _190 = _117;
      _191 = _118;
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
   float _206 = _105 * COLOR.x;
   float _207 = _206 * _135;
   float _208 = _105 * COLOR.y;
   float _209 = _208 * _136;
   float _210 = _105 * COLOR.z;
   float _211 = _210 * _137;
   float _212 = _203 * _135;
   float _213 = _204 * _136;
   float _214 = _205 * _137;
   if (!_178)
   {
      float _218 = (c_sdw_param[2].x) * _203;
      float _219 = (c_sdw_param[2].x) * _204;
      float _220 = (c_sdw_param[2].x) * _205;
      float _222 = _218 + (c_sdw_param[2].y);
      float _223 = _219 + (c_sdw_param[2].y);
      float _224 = _220 + (c_sdw_param[2].y);
      float _226 = (c_sdw_param[2].z) * _135;
      float _227 = (c_sdw_param[2].z) * _136;
      float _228 = (c_sdw_param[2].z) * _137;
      float _230 = _226 + (c_sdw_param[2].w);
      float _231 = _227 + (c_sdw_param[2].w);
      float _232 = _228 + (c_sdw_param[2].w);
      float _233 = _230 * _222;
      float _234 = _231 * _223;
      float _235 = _232 * _224;
      _237 = _233;
      _238 = _234;
      _239 = _235;
   }
   else
   {
      _237 = _212;
      _238 = _213;
      _239 = _214;
   }
   float _240 = _105 * _67.x;
   float _241 = _237 * _240;
   float _242 = _238 * _240;
   float _243 = _239 * _240;
   float _254 = c_mat_emissive.x * _53;
   float _255 = _254 * c_emissive_scale;
   float _256 = c_mat_emissive.y * _54;
   float _257 = _256 * c_emissive_scale;
   float _258 = c_mat_emissive.z * _55;
   float _259 = _258 * c_emissive_scale;
   int _260 = c_bool & 2048;
   bool _261 = (_260 == 0);
   if (_261)
   {
      float _263 = _191 * _173;
      float _264 = _191 * _174;
      float _265 = _191 * _175;
      float _266 = _263 + TEXCOORD_4.x;
      float _267 = _264 + TEXCOORD_4.y;
      float _268 = _265 + TEXCOORD_4.z;
      float _269 = _191 * _176;
      float _270 = _269 + TEXCOORD_5.x;
      float _271 = _269 + TEXCOORD_5.y;
      float _272 = _269 + TEXCOORD_5.z;
      _274 = _266;
      _275 = _267;
      _276 = _268;
      _277 = _270;
      _278 = _271;
      _279 = _272;
   }
   else
   {
      _274 = TEXCOORD_4.x;
      _275 = TEXCOORD_4.y;
      _276 = TEXCOORD_4.z;
      _277 = TEXCOORD_5.x;
      _278 = TEXCOORD_5.y;
      _279 = TEXCOORD_5.z;
   }
   float _284 = -0.0f - (c_light_vec[1].x);
   float _285 = -0.0f - (c_light_vec[1].y);
   float _286 = -0.0f - (c_light_vec[1].z);
   float _287 = dot(float3(_72, _73, _74), float3(_284, _285, _286));
   float _288 = saturate(_287);
   float _293 = saturate(_87);
   float _294 = (c_light_col[0].x) * _293;
   float _295 = (c_light_col[0].y) * _293;
   float _296 = (c_light_col[0].z) * _293;
   float _301 = (c_light_col[1].x) * _288;
   float _302 = (c_light_col[1].y) * _288;
   float _303 = (c_light_col[1].z) * _288;
   float _312 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _313 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _314 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _315 = _294 * _203;
   float _316 = _295 * _204;
   float _317 = _296 * _205;
   float _318 = _301 * _191;
   float _319 = _302 * _191;
   float _320 = _303 * _191;
   float _321 = _318 + _315;
   float _322 = _319 + _316;
   float _323 = _320 + _317;
   float _324 = _312 * _321;
   float _325 = _313 * _322;
   float _326 = _314 * _323;
   float _327 = _255 + c_scene_ambient.x;
   float _328 = _327 + _324;
   float _329 = _257 + c_scene_ambient.y;
   float _330 = _329 + _325;
   float _331 = _259 + c_scene_ambient.z;
   float _332 = _331 + _326;
   float _333 = _328 * _207;
   float _334 = _330 * _209;
   float _335 = _332 * _211;
   float _336 = _274 * _241;
   float _337 = _275 * _242;
   float _338 = _276 * _243;
   float _339 = _333 + _336;
   float _340 = _334 + _337;
   float _341 = _335 + _338;
   float _346 = (c_light_vec[0].x) + _77;
   float _347 = (c_light_vec[0].y) + _78;
   float _348 = (c_light_vec[0].z) + _79;
   float _349 = dot(float3(_346, _347, _348), float3(_346, _347, _348));
   float _350 = rsqrt(_349);
   float _351 = _346 * _350;
   float _352 = -0.0f - _351;
   float _353 = _347 * _350;
   float _354 = -0.0f - _353;
   float _355 = _348 * _350;
   float _356 = -0.0f - _355;
   float _357 = dot(float3(_352, _354, _356), float3(_72, _73, _74));
   float _358 = saturate(_357);
   float _359 = _78 + -0.30000001192092896f;
   float _360 = dot(float3(_77, _359, _79), float3(_77, _359, _79));
   float _361 = rsqrt(_360);
   float _362 = _77 * _361;
   float _363 = -0.0f - _362;
   float _364 = _359 * _361;
   float _365 = -0.0f - _364;
   float _366 = _79 * _361;
   float _367 = -0.0f - _366;
   float _368 = dot(float3(_363, _365, _367), float3(_72, _73, _74));
   float _369 = saturate(_368);
   float _372 = log2(_358);
   float _373 = log2(_369);
   float _374 = _372 * c_mat_power;
   float _375 = _373 * c_mat_power;
   float _376 = exp2(_374);
   float _377 = exp2(_375);
   float _380 = _376 * (c_light_col[0].x);
   float _381 = _376 * (c_light_col[0].y);
   float _382 = _376 * (c_light_col[0].z);
   float _383 = _377 * 2.0f;
   float _384 = _383 * (c_light_vec[5].x);
   float _385 = _384 * (c_light_col[1].x);
   float _386 = _384 * (c_light_col[1].y);
   float _387 = _384 * (c_light_col[1].z);
   float _388 = _380 * _203;
   float _389 = _381 * _204;
   float _390 = _382 * _205;
   float _391 = _385 * _288;
   float _392 = _386 * _288;
   float _393 = _387 * _288;
   float _394 = _391 + _388;
   float _395 = _392 + _389;
   float _396 = _393 + _390;
   float _403 = c_mat_specular.x * c_specular_scale.x;
   float _404 = _403 * _207;
   float _405 = _404 * _394;
   float _406 = c_mat_specular.y * c_specular_scale.y;
   float _407 = _406 * _209;
   float _408 = _407 * _395;
   float _409 = c_mat_specular.z * c_specular_scale.z;
   float _410 = _409 * _211;
   float _411 = _410 * _396;
   float _412 = _277 * _241;
   float _413 = _412 * _377;
   float _414 = _278 * _242;
   float _415 = _414 * _377;
   float _416 = _279 * _243;
   float _417 = _416 * _377;
   float _418 = _405 + _413;
   float _419 = _408 + _415;
   float _420 = _411 + _417;
   if (!_261)
   {
      float _422 = _191 * _173;
      float _423 = _191 * _174;
      float _424 = _191 * _175;
      float _425 = _339 + _422;
      float _426 = _340 + _423;
      float _427 = _341 + _424;
      float _428 = _191 * _176;
      float _429 = _418 + _428;
      float _430 = _419 + _428;
      float _431 = _420 + _428;
      _433 = _425;
      _434 = _426;
      _435 = _427;
      _436 = _429;
      _437 = _430;
      _438 = _431;
   }
   else
   {
      _433 = _339;
      _434 = _340;
      _435 = _341;
      _436 = _418;
      _437 = _419;
      _438 = _420;
   }
   int _439 = c_bool & 128;
   bool _440 = (_439 != 0);
   float _441 = select(_440, 0.0f, _436);
   float _442 = select(_440, 0.0f, _437);
   float _443 = select(_440, 0.0f, _438);
   float _444 = _433 * _53;
   float _445 = _434 * _54;
   float _446 = _435 * _55;
   float _447 = _441 * _61.y;
   float _448 = _442 * _61.y;
   float _449 = _443 * _61.y;
   int _450 = c_bool & 32768;
   bool _451 = (_450 == 0);
   if (!_451)
   {
      float _455 = (c_ps_user[11].x) * _53;
      float _456 = (c_ps_user[11].x) * _54;
      float _457 = (c_ps_user[11].x) * _55;
      float _459 = _72 + 1.0f;
      float _460 = _73 + 1.0f;
      float _461 = _74 + 1.0f;
      float _462 = _459 * 0.5f;
      float _463 = _460 * 0.5f;
      float _464 = _461 * 0.5f;
      float _465 = saturate(_462);
      float _466 = saturate(_463);
      float _467 = saturate(_464);
      float _468 = (c_ps_user[11].y) * _465;
      float _469 = _466 * (c_ps_user[11].y);
      float _470 = _467 * (c_ps_user[11].y);
      float _472 = (c_ps_user[11].z) * _61.y;
      float _474 = c_mat_power * 0.009999999776482582f;
      float _475 = _474 * (c_ps_user[11].w);
      float _478 = (c_ps_user[12].x) * _203;
      float _479 = (c_ps_user[12].x) * _204;
      float _480 = (c_ps_user[12].x) * _205;
      float _482 = (c_ps_user[12].y) * _207;
      float _483 = (c_ps_user[12].y) * _209;
      float _484 = (c_ps_user[12].y) * _211;
      float _486 = (c_ps_user[12].z) * _241;
      float _487 = (c_ps_user[12].z) * _242;
      float _488 = (c_ps_user[12].z) * _243;
      float _494 = -0.0f - _77;
      float _495 = -0.0f - _78;
      float _496 = -0.0f - _79;
      float _497 = dot(float3(_494, _495, _496), float3(_72, _73, _74));
      float _498 = 1.0f - _497;
      float _499 = _498 * _498;
      float _500 = _498 * 0.9722222089767456f;
      float _501 = _499 * _499;
      float _502 = _501 * _500;
      float _503 = _502 + 0.02777777425944805f;
      float _504 = _503 * (c_ps_user[13].z);
      float _506 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _507 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _508 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _511 = (c_ps_user[14].x) * _255;
      float _512 = (c_ps_user[14].x) * _257;
      float _513 = (c_ps_user[14].x) * _259;
      float _515 = (c_ps_user[14].y) * _294;
      float _516 = (c_ps_user[14].y) * _295;
      float _517 = (c_ps_user[14].y) * _296;
      float _519 = (c_ps_user[14].z) * _301;
      float _520 = (c_ps_user[14].z) * _302;
      float _521 = (c_ps_user[14].z) * _303;
      float _523 = (c_ps_user[14].w) * _274;
      float _524 = (c_ps_user[14].w) * _275;
      float _525 = (c_ps_user[14].w) * _276;
      float _528 = (c_ps_user[15].x) * _380;
      float _529 = (c_ps_user[15].x) * _381;
      float _530 = (c_ps_user[15].x) * _382;
      float _532 = _403 * _385;
      float _533 = _532 * (c_ps_user[15].y);
      float _534 = _406 * _386;
      float _535 = _534 * (c_ps_user[15].y);
      float _536 = _409 * _387;
      float _537 = _536 * (c_ps_user[15].y);
      float _539 = (c_ps_user[15].z) * _277;
      float _540 = (c_ps_user[15].z) * _278;
      float _541 = (c_ps_user[15].z) * _279;
      float _542 = _533 + _539;
      float _543 = _542 * _377;
      float _544 = _515 + (c_ps_user[13].x);
      float _545 = _544 + _519;
      float _546 = c_diffuse_scale.x * _545;
      float _547 = _546 * c_mat_diffuse.x;
      float _548 = (c_ps_user[13].y) + _472;
      float _549 = _548 + _528;
      float _550 = c_mat_specular.x * _549;
      float _551 = _550 * c_specular_scale.x;
      float _552 = _468 + _455;
      float _553 = _552 + _475;
      float _554 = _553 + (c_ps_user[12].w);
      float _555 = _554 + _478;
      float _556 = _555 + _482;
      float _557 = _556 + _486;
      float _558 = _557 + _506;
      float _559 = _558 + _511;
      float _560 = _559 + _523;
      float _561 = _560 + _504;
      float _562 = _561 + _543;
      float _563 = _562 + _547;
      float _564 = _563 + _551;
      float _565 = _535 + _540;
      float _566 = _565 * _377;
      float _567 = _516 + (c_ps_user[13].x);
      float _568 = _567 + _520;
      float _569 = c_diffuse_scale.y * _568;
      float _570 = _569 * c_mat_diffuse.y;
      float _571 = _548 + _529;
      float _572 = c_mat_specular.y * _571;
      float _573 = _572 * c_specular_scale.y;
      float _574 = _469 + _456;
      float _575 = _574 + _475;
      float _576 = _575 + (c_ps_user[12].w);
      float _577 = _576 + _479;
      float _578 = _577 + _483;
      float _579 = _578 + _487;
      float _580 = _579 + _507;
      float _581 = _580 + _512;
      float _582 = _581 + _524;
      float _583 = _582 + _504;
      float _584 = _583 + _566;
      float _585 = _584 + _570;
      float _586 = _585 + _573;
      float _587 = _537 + _541;
      float _588 = _587 * _377;
      float _589 = _517 + (c_ps_user[13].x);
      float _590 = _589 + _521;
      float _591 = c_diffuse_scale.z * _590;
      float _592 = _591 * c_mat_diffuse.z;
      float _593 = _548 + _530;
      float _594 = c_mat_specular.z * _593;
      float _595 = _594 * c_specular_scale.z;
      float _596 = _475 + _457;
      float _597 = _596 + _470;
      float _598 = _597 + (c_ps_user[12].w);
      float _599 = _598 + _480;
      float _600 = _599 + _484;
      float _601 = _600 + _488;
      float _602 = _601 + _508;
      float _603 = _602 + _513;
      float _604 = _603 + _525;
      float _605 = _604 + _504;
      float _606 = _605 + _588;
      float _607 = _606 + _592;
      float _608 = _607 + _595;
      _610 = _564;
      _611 = _586;
      _612 = _608;
      _613 = 0.0f;
      _614 = 0.0f;
      _615 = 0.0f;
   }
   else
   {
      _610 = _444;
      _611 = _445;
      _612 = _446;
      _613 = _447;
      _614 = _448;
      _615 = _449;
   }
   float _616 = _63.x * _63.x;
   float _617 = _616 * COLOR_1.w;
   float _618 = _63.y * _63.y;
   float _619 = _618 * COLOR_1.w;
   float _620 = _63.z * _63.z;
   float _621 = _620 * COLOR_1.w;
   float _622 = _610 + _617;
   float _623 = _622 + _613;
   float _624 = _611 + _619;
   float _625 = _624 + _614;
   float _626 = _612 + _621;
   float _627 = _626 + _615;
   float _632 = c_mat_opacity * _48.w;
   float _633 = _632 * c_global_opacity;
   float _634 = _633 * c_opacity_scale;
   int _635 = c_bool & 16;
   bool _636 = (_635 == 0);
   if (!_636)
   {
      float _640 = c_tone_control.x * _623;
      float _641 = c_tone_control.x * _625;
      float _642 = c_tone_control.x * _627;
      float _644 = log2(_640);
      float _645 = log2(_641);
      float _646 = log2(_642);
      float _647 = _644 * c_tone_control.y;
      float _648 = _645 * c_tone_control.y;
      float _649 = _646 * c_tone_control.y;
      float _650 = exp2(_647);
      float _651 = exp2(_648);
      float _652 = exp2(_649);
      _700 = _650;
      _701 = _651;
      _702 = _652;
   }
   else
   {
      if (_451)
      {
         float _657 = c_tone_control.x * _623;
         float _658 = c_tone_control.x * _625;
         float _659 = c_tone_control.x * _627;
         float _660 = _657 * 2.0f;
         float _661 = _658 * 2.0f;
         float _662 = _659 * 2.0f;
         float _663 = _657 * 0.30000001192092896f;
         float _664 = _658 * 0.30000001192092896f;
         float _665 = _659 * 0.30000001192092896f;
         float _666 = _663 + 0.05000000074505806f;
         float _667 = _664 + 0.05000000074505806f;
         float _668 = _665 + 0.05000000074505806f;
         float _669 = _666 * _660;
         float _670 = _667 * _661;
         float _671 = _668 * _662;
         float _672 = _669 + 0.004000000189989805f;
         float _673 = _670 + 0.004000000189989805f;
         float _674 = _671 + 0.004000000189989805f;
         float _675 = _663 + 0.5f;
         float _676 = _664 + 0.5f;
         float _677 = _665 + 0.5f;
         float _678 = _675 * _660;
         float _679 = _676 * _661;
         float _680 = _677 * _662;
         float _681 = _678 + 0.06000000238418579f;
         float _682 = _679 + 0.06000000238418579f;
         float _683 = _680 + 0.06000000238418579f;
         float _684 = _672 / _681;
         float _685 = _673 / _682;
         float _686 = _674 / _683;
         float _687 = _684 + -0.06666666269302368f;
         float _688 = _685 + -0.06666666269302368f;
         float _689 = _686 + -0.06666666269302368f;
         float _690 = _687 * 1.3790643215179443f;
         float _691 = _688 * 1.3790643215179443f;
         float _692 = _689 * 1.3790643215179443f;
         float _693 = YKR_MaterialCurve(_690, _657);
         float _694 = YKR_MaterialCurve(_691, _658);
         float _695 = YKR_MaterialCurve(_692, _659);
         float _696 = sqrt(_693);
         float _697 = sqrt(_694);
         float _698 = sqrt(_695);
         _700 = _696;
         _701 = _697;
         _702 = _698;
      }
      else
      {
         _700 = _623;
         _701 = _625;
         _702 = _627;
      }
   }
   int _703 = c_bool & 4;
   bool _704 = (_703 == 0);
   if (_704)
   {
      float _706 = saturate(COLOR_1.x);
      float _711 = (c_fog_param[0].x) - _700;
      float _712 = (c_fog_param[0].y) - _701;
      float _713 = (c_fog_param[0].z) - _702;
      float _714 = _711 * _706;
      float _715 = _712 * _706;
      float _716 = _713 * _706;
      _718 = _714;
      _719 = _715;
      _720 = _716;
   }
   else
   {
      _718 = COLOR_1.x;
      _719 = COLOR_1.y;
      _720 = COLOR_1.z;
   }
   float _721 = _718 + _700;
   float _722 = _719 + _701;
   float _723 = _720 + _702;
   float _729 = (c_ps_col_scl[0].x) * _721;
   float _730 = (c_ps_col_scl[0].y) * _722;
   float _731 = (c_ps_col_scl[0].z) * _723;
   float _732 = (c_ps_col_scl[0].w) * _634;
   SV_Target.x = _729;
   SV_Target.y = _730;
   SV_Target.z = _731;
   SV_Target.w = _732;
   int _735 = c_bool & 8;
   bool _736 = (_735 != 0);
   float _737 = select(_736, _721, _729);
   float _738 = select(_736, _722, _730);
   float _739 = select(_736, _723, _731);
   float _740 = select(_736, _634, _732);
   SV_Target.x = _737;
   SV_Target.y = _738;
   SV_Target.z = _739;
   SV_Target.w = _740;
   return SV_Target;
}
