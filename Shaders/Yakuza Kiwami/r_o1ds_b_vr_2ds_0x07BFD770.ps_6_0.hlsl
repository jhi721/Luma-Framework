// Yakuza Kiwami material r_o1ds_b(vr)2ds (0x07BFD770, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s4_s : register(s4);

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
   float4 _46 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _51 = _46.x * _46.x;
   float _52 = _46.y * _46.y;
   float _53 = _46.z * _46.z;
   float _57 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _58 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _59 = s3_t.Sample(s3_s, float2(_57, _58));
   float4 _63 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _68 = _63.x * _63.x;
   float _69 = _63.y * _63.y;
   float _70 = _63.z * _63.z;
   float _74 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _75 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _76 = s4_t.Sample(s4_s, float2(_74, _75));
   float _80 = _63.w * COLOR_1.w;
   float _81 = _68 - _51;
   float _82 = _69 - _52;
   float _83 = _70 - _53;
   float _84 = _81 * _80;
   float _85 = _82 * _80;
   float _86 = _83 * _80;
   float _87 = _84 + _51;
   float _88 = _85 + _52;
   float _89 = _86 + _53;
   float _90 = _76.x - _59.x;
   float _91 = _76.y - _59.y;
   float _92 = _76.z - _59.z;
   float _93 = _90 * _80;
   float _94 = _91 * _80;
   float _95 = _92 * _80;
   float _96 = _93 + _59.x;
   float _97 = _94 + _59.y;
   float _98 = _95 + _59.z;
   float _99 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _100 = rsqrt(_99);
   float _101 = _100 * TEXCOORD_2.x;
   float _102 = _100 * TEXCOORD_2.y;
   float _103 = _100 * TEXCOORD_2.z;
   float _104 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _105 = rsqrt(_104);
   float _106 = _105 * TEXCOORD_3.x;
   float _107 = _105 * TEXCOORD_3.y;
   float _108 = _105 * TEXCOORD_3.z;
   float _113 = -0.0f - (c_light_vec[0].x);
   float _114 = -0.0f - (c_light_vec[0].y);
   float _115 = -0.0f - (c_light_vec[0].z);
   float _116 = dot(float3(_101, _102, _103), float3(_113, _114, _115));
   float _120 = c_rt_size.z * SV_Position.x;
   float _121 = c_rt_size.w * SV_Position.y;
   int _124 = c_bool & 8;
   bool _125 = (_124 == 0);
   float _132;
   float _133;
   float _134;
   float _146;
   float _147;
   float _183;
   float _184;
   float _185;
   float _186;
   float _200;
   float _201;
   float _236;
   float _237;
   float _238;
   float _272;
   float _273;
   float _274;
   float _275;
   float _276;
   float _277;
   float _431;
   float _432;
   float _433;
   float _434;
   float _435;
   float _436;
   float _612;
   float _613;
   float _614;
   float _615;
   float _616;
   float _617;
   float _693;
   float _694;
   float _695;
   float _711;
   float _712;
   float _713;
   if (!_125)
   {
      float4 _127 = s13_t.Sample(s13_s, float2(_120, _121));
      _132 = _127.x;
      _133 = _127.y;
      _134 = _127.w;
   }
   else
   {
      _132 = 1.0f;
      _133 = 1.0f;
      _134 = 1.0f;
   }
   if (!_125)
   {
      float _139 = (c_sdw_param[0].x) * _132;
      float _140 = (c_sdw_param[0].z) * _133;
      float _143 = _139 + (c_sdw_param[0].y);
      float _144 = _140 + (c_sdw_param[0].w);
      _146 = _143;
      _147 = _144;
   }
   else
   {
      _146 = _132;
      _147 = _133;
   }
   float _151 = c_rt_size.z * SV_Position.x;
   float _152 = c_rt_size.w * SV_Position.y;
   float4 _153 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_151, _152)));
   int _160 = c_bool & 16384;
   bool _161 = (_160 == 0);
   if (!_161)
   {
      float _163 = _153.x * 2.0f;
      float _164 = _153.y * 2.0f;
      float _165 = _153.z * 2.0f;
      float _166 = _153.w * 2.0f;
      _183 = _163;
      _184 = _164;
      _185 = _165;
      _186 = _166;
   }
   else
   {
      int _168 = c_bool & 8192;
      bool _169 = (_168 == 0);
      if (!_169)
      {
         float _171 = _153.x * 2.0f;
         float _172 = _153.y * 2.0f;
         float _173 = _153.z * 2.0f;
         float _174 = _153.w * 2.0f;
         _183 = _171;
         _184 = _172;
         _185 = _173;
         _186 = _174;
      }
      else
      {
         float _178 = _153.x / c_tone_control.z;
         float _179 = _153.y / c_tone_control.z;
         float _180 = _153.z / c_tone_control.z;
         float _181 = _153.w / c_tone_control.z;
         _183 = _178;
         _184 = _179;
         _185 = _180;
         _186 = _181;
      }
   }
   int _187 = c_bool & 8;
   bool _188 = (_187 == 0);
   if (!_188)
   {
      float _193 = (c_sdw_param[0].x) * _146;
      float _194 = (c_sdw_param[0].z) * _147;
      float _197 = _193 + (c_sdw_param[0].y);
      float _198 = _194 + (c_sdw_param[0].w);
      _200 = _197;
      _201 = _198;
   }
   else
   {
      _200 = _146;
      _201 = _147;
   }
   float _202 = _201 * _200;
   float _207 = 1.0f - (c_light_col[5].x);
   float _208 = 1.0f - (c_light_col[5].y);
   float _209 = 1.0f - (c_light_col[5].z);
   float _210 = _207 * _202;
   float _211 = _208 * _202;
   float _212 = _209 * _202;
   float _213 = _210 + (c_light_col[5].x);
   float _214 = _211 + (c_light_col[5].y);
   float _215 = _212 + (c_light_col[5].z);
   float _216 = _134 * COLOR.x;
   float _217 = _134 * COLOR.y;
   float _218 = _134 * COLOR.z;
   if (!_188)
   {
      float _222 = (c_sdw_param[2].x) * _213;
      float _223 = (c_sdw_param[2].x) * _214;
      float _224 = (c_sdw_param[2].x) * _215;
      float _226 = _222 + (c_sdw_param[2].y);
      float _227 = _223 + (c_sdw_param[2].y);
      float _228 = _224 + (c_sdw_param[2].y);
      float _231 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _232 = _226 * _231;
      float _233 = _227 * _231;
      float _234 = _228 * _231;
      _236 = _232;
      _237 = _233;
      _238 = _234;
   }
   else
   {
      _236 = _213;
      _237 = _214;
      _238 = _215;
   }
   float _239 = _236 * _134;
   float _240 = _237 * _134;
   float _241 = _238 * _134;
   float _252 = c_mat_emissive.x * _87;
   float _253 = _252 * c_emissive_scale;
   float _254 = c_mat_emissive.y * _88;
   float _255 = _254 * c_emissive_scale;
   float _256 = c_mat_emissive.z * _89;
   float _257 = _256 * c_emissive_scale;
   int _258 = c_bool & 2048;
   bool _259 = (_258 == 0);
   if (_259)
   {
      float _261 = _201 * _183;
      float _262 = _201 * _184;
      float _263 = _201 * _185;
      float _264 = _261 + TEXCOORD_4.x;
      float _265 = _262 + TEXCOORD_4.y;
      float _266 = _263 + TEXCOORD_4.z;
      float _267 = _201 * _186;
      float _268 = _267 + TEXCOORD_5.x;
      float _269 = _267 + TEXCOORD_5.y;
      float _270 = _267 + TEXCOORD_5.z;
      _272 = _264;
      _273 = _265;
      _274 = _266;
      _275 = _268;
      _276 = _269;
      _277 = _270;
   }
   else
   {
      _272 = TEXCOORD_4.x;
      _273 = TEXCOORD_4.y;
      _274 = TEXCOORD_4.z;
      _275 = TEXCOORD_5.x;
      _276 = TEXCOORD_5.y;
      _277 = TEXCOORD_5.z;
   }
   float _282 = -0.0f - (c_light_vec[1].x);
   float _283 = -0.0f - (c_light_vec[1].y);
   float _284 = -0.0f - (c_light_vec[1].z);
   float _285 = dot(float3(_101, _102, _103), float3(_282, _283, _284));
   float _286 = saturate(_285);
   float _291 = saturate(_116);
   float _292 = (c_light_col[0].x) * _291;
   float _293 = (c_light_col[0].y) * _291;
   float _294 = (c_light_col[0].z) * _291;
   float _299 = (c_light_col[1].x) * _286;
   float _300 = (c_light_col[1].y) * _286;
   float _301 = (c_light_col[1].z) * _286;
   float _310 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _311 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _312 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _313 = _292 * _213;
   float _314 = _293 * _214;
   float _315 = _294 * _215;
   float _316 = _299 * _201;
   float _317 = _300 * _201;
   float _318 = _301 * _201;
   float _319 = _316 + _313;
   float _320 = _317 + _314;
   float _321 = _318 + _315;
   float _322 = _310 * _319;
   float _323 = _311 * _320;
   float _324 = _312 * _321;
   float _325 = _253 + c_scene_ambient.x;
   float _326 = _325 + _322;
   float _327 = _255 + c_scene_ambient.y;
   float _328 = _327 + _323;
   float _329 = _257 + c_scene_ambient.z;
   float _330 = _329 + _324;
   float _331 = _326 * _216;
   float _332 = _328 * _217;
   float _333 = _330 * _218;
   float _334 = _272 * _239;
   float _335 = _273 * _240;
   float _336 = _274 * _241;
   float _337 = _331 + _334;
   float _338 = _332 + _335;
   float _339 = _333 + _336;
   float _344 = (c_light_vec[0].x) + _106;
   float _345 = (c_light_vec[0].y) + _107;
   float _346 = (c_light_vec[0].z) + _108;
   float _347 = dot(float3(_344, _345, _346), float3(_344, _345, _346));
   float _348 = rsqrt(_347);
   float _349 = _344 * _348;
   float _350 = -0.0f - _349;
   float _351 = _345 * _348;
   float _352 = -0.0f - _351;
   float _353 = _346 * _348;
   float _354 = -0.0f - _353;
   float _355 = dot(float3(_350, _352, _354), float3(_101, _102, _103));
   float _356 = saturate(_355);
   float _357 = _107 + -0.30000001192092896f;
   float _358 = dot(float3(_106, _357, _108), float3(_106, _357, _108));
   float _359 = rsqrt(_358);
   float _360 = _106 * _359;
   float _361 = -0.0f - _360;
   float _362 = _357 * _359;
   float _363 = -0.0f - _362;
   float _364 = _108 * _359;
   float _365 = -0.0f - _364;
   float _366 = dot(float3(_361, _363, _365), float3(_101, _102, _103));
   float _367 = saturate(_366);
   float _370 = log2(_356);
   float _371 = log2(_367);
   float _372 = _370 * c_mat_power;
   float _373 = _371 * c_mat_power;
   float _374 = exp2(_372);
   float _375 = exp2(_373);
   float _378 = _374 * (c_light_col[0].x);
   float _379 = _374 * (c_light_col[0].y);
   float _380 = _374 * (c_light_col[0].z);
   float _381 = _375 * 2.0f;
   float _382 = _381 * (c_light_vec[5].x);
   float _383 = _382 * (c_light_col[1].x);
   float _384 = _382 * (c_light_col[1].y);
   float _385 = _382 * (c_light_col[1].z);
   float _386 = _378 * _213;
   float _387 = _379 * _214;
   float _388 = _380 * _215;
   float _389 = _383 * _286;
   float _390 = _384 * _286;
   float _391 = _385 * _286;
   float _392 = _389 + _386;
   float _393 = _390 + _387;
   float _394 = _391 + _388;
   float _401 = c_mat_specular.x * c_specular_scale.x;
   float _402 = _401 * _216;
   float _403 = _402 * _392;
   float _404 = c_mat_specular.y * c_specular_scale.y;
   float _405 = _404 * _217;
   float _406 = _405 * _393;
   float _407 = c_mat_specular.z * c_specular_scale.z;
   float _408 = _407 * _218;
   float _409 = _408 * _394;
   float _410 = _275 * _239;
   float _411 = _410 * _375;
   float _412 = _276 * _240;
   float _413 = _412 * _375;
   float _414 = _277 * _241;
   float _415 = _414 * _375;
   float _416 = _403 + _411;
   float _417 = _406 + _413;
   float _418 = _409 + _415;
   if (!_259)
   {
      float _420 = _201 * _183;
      float _421 = _201 * _184;
      float _422 = _201 * _185;
      float _423 = _337 + _420;
      float _424 = _338 + _421;
      float _425 = _339 + _422;
      float _426 = _201 * _186;
      float _427 = _416 + _426;
      float _428 = _417 + _426;
      float _429 = _418 + _426;
      _431 = _423;
      _432 = _424;
      _433 = _425;
      _434 = _427;
      _435 = _428;
      _436 = _429;
   }
   else
   {
      _431 = _337;
      _432 = _338;
      _433 = _339;
      _434 = _416;
      _435 = _417;
      _436 = _418;
   }
   int _437 = c_bool & 128;
   bool _438 = (_437 != 0);
   float _439 = select(_438, 0.0f, _434);
   float _440 = select(_438, 0.0f, _435);
   float _441 = select(_438, 0.0f, _436);
   float _442 = _431 * _87;
   float _443 = _432 * _88;
   float _444 = _433 * _89;
   float _445 = _439 * _96;
   float _446 = _440 * _97;
   float _447 = _441 * _98;
   int _448 = c_bool & 32768;
   bool _449 = (_448 == 0);
   if (!_449)
   {
      float _453 = (c_ps_user[11].x) * _87;
      float _454 = (c_ps_user[11].x) * _88;
      float _455 = (c_ps_user[11].x) * _89;
      float _457 = _101 + 1.0f;
      float _458 = _102 + 1.0f;
      float _459 = _103 + 1.0f;
      float _460 = _457 * 0.5f;
      float _461 = _458 * 0.5f;
      float _462 = _459 * 0.5f;
      float _463 = saturate(_460);
      float _464 = saturate(_461);
      float _465 = saturate(_462);
      float _466 = (c_ps_user[11].y) * _463;
      float _467 = _464 * (c_ps_user[11].y);
      float _468 = _465 * (c_ps_user[11].y);
      float _470 = (c_ps_user[11].z) * _96;
      float _471 = (c_ps_user[11].z) * _97;
      float _472 = (c_ps_user[11].z) * _98;
      float _474 = c_mat_power * 0.009999999776482582f;
      float _475 = _474 * (c_ps_user[11].w);
      float _478 = (c_ps_user[12].x) * _213;
      float _479 = (c_ps_user[12].x) * _214;
      float _480 = (c_ps_user[12].x) * _215;
      float _482 = (c_ps_user[12].y) * _216;
      float _483 = (c_ps_user[12].y) * _217;
      float _484 = (c_ps_user[12].y) * _218;
      float _486 = (c_ps_user[12].z) * _239;
      float _487 = (c_ps_user[12].z) * _240;
      float _488 = (c_ps_user[12].z) * _241;
      float _494 = -0.0f - _106;
      float _495 = -0.0f - _107;
      float _496 = -0.0f - _108;
      float _497 = dot(float3(_494, _495, _496), float3(_101, _102, _103));
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
      float _511 = (c_ps_user[14].x) * _253;
      float _512 = (c_ps_user[14].x) * _255;
      float _513 = (c_ps_user[14].x) * _257;
      float _515 = (c_ps_user[14].y) * _292;
      float _516 = (c_ps_user[14].y) * _293;
      float _517 = (c_ps_user[14].y) * _294;
      float _519 = (c_ps_user[14].z) * _299;
      float _520 = (c_ps_user[14].z) * _300;
      float _521 = (c_ps_user[14].z) * _301;
      float _523 = (c_ps_user[14].w) * _272;
      float _524 = (c_ps_user[14].w) * _273;
      float _525 = (c_ps_user[14].w) * _274;
      float _528 = (c_ps_user[15].x) * _378;
      float _529 = (c_ps_user[15].x) * _379;
      float _530 = (c_ps_user[15].x) * _380;
      float _532 = _401 * _383;
      float _533 = _532 * (c_ps_user[15].y);
      float _534 = _404 * _384;
      float _535 = _534 * (c_ps_user[15].y);
      float _536 = _407 * _385;
      float _537 = _536 * (c_ps_user[15].y);
      float _539 = (c_ps_user[15].z) * _275;
      float _540 = (c_ps_user[15].z) * _276;
      float _541 = (c_ps_user[15].z) * _277;
      float _542 = _533 + _539;
      float _543 = _542 * _375;
      float _544 = _515 + (c_ps_user[13].x);
      float _545 = _544 + _519;
      float _546 = c_diffuse_scale.x * _545;
      float _547 = _546 * c_mat_diffuse.x;
      float _548 = (c_ps_user[13].y) + _470;
      float _549 = _548 + _528;
      float _550 = c_mat_specular.x * _549;
      float _551 = _550 * c_specular_scale.x;
      float _552 = _466 + _453;
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
      float _566 = _565 * _375;
      float _567 = _516 + (c_ps_user[13].x);
      float _568 = _567 + _520;
      float _569 = c_diffuse_scale.y * _568;
      float _570 = _569 * c_mat_diffuse.y;
      float _571 = (c_ps_user[13].y) + _471;
      float _572 = _571 + _529;
      float _573 = c_mat_specular.y * _572;
      float _574 = _573 * c_specular_scale.y;
      float _575 = _467 + _454;
      float _576 = _575 + _475;
      float _577 = _576 + (c_ps_user[12].w);
      float _578 = _577 + _479;
      float _579 = _578 + _483;
      float _580 = _579 + _487;
      float _581 = _580 + _507;
      float _582 = _581 + _512;
      float _583 = _582 + _524;
      float _584 = _583 + _504;
      float _585 = _584 + _566;
      float _586 = _585 + _570;
      float _587 = _586 + _574;
      float _588 = _537 + _541;
      float _589 = _588 * _375;
      float _590 = _517 + (c_ps_user[13].x);
      float _591 = _590 + _521;
      float _592 = c_diffuse_scale.z * _591;
      float _593 = _592 * c_mat_diffuse.z;
      float _594 = (c_ps_user[13].y) + _472;
      float _595 = _594 + _530;
      float _596 = c_mat_specular.z * _595;
      float _597 = _596 * c_specular_scale.z;
      float _598 = _475 + _455;
      float _599 = _598 + _468;
      float _600 = _599 + (c_ps_user[12].w);
      float _601 = _600 + _480;
      float _602 = _601 + _484;
      float _603 = _602 + _488;
      float _604 = _603 + _508;
      float _605 = _604 + _513;
      float _606 = _605 + _525;
      float _607 = _606 + _504;
      float _608 = _607 + _589;
      float _609 = _608 + _593;
      float _610 = _609 + _597;
      _612 = _564;
      _613 = _587;
      _614 = _610;
      _615 = 0.0f;
      _616 = 0.0f;
      _617 = 0.0f;
   }
   else
   {
      _612 = _442;
      _613 = _443;
      _614 = _444;
      _615 = _445;
      _616 = _446;
      _617 = _447;
   }
   float _618 = _615 + _612;
   float _619 = _616 + _613;
   float _620 = _617 + _614;
   float _625 = c_mat_opacity * _46.w;
   float _626 = _625 * c_global_opacity;
   float _627 = _626 * c_opacity_scale;
   int _628 = c_bool & 16;
   bool _629 = (_628 == 0);
   if (!_629)
   {
      float _633 = c_tone_control.x * _618;
      float _634 = c_tone_control.x * _619;
      float _635 = c_tone_control.x * _620;
      float _637 = log2(_633);
      float _638 = log2(_634);
      float _639 = log2(_635);
      float _640 = _637 * c_tone_control.y;
      float _641 = _638 * c_tone_control.y;
      float _642 = _639 * c_tone_control.y;
      float _643 = exp2(_640);
      float _644 = exp2(_641);
      float _645 = exp2(_642);
      _693 = _643;
      _694 = _644;
      _695 = _645;
   }
   else
   {
      if (_449)
      {
         float _650 = c_tone_control.x * _618;
         float _651 = c_tone_control.x * _619;
         float _652 = c_tone_control.x * _620;
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
         _693 = _618;
         _694 = _619;
         _695 = _620;
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
   float _725 = (c_ps_col_scl[0].w) * _627;
   SV_Target.x = _722;
   SV_Target.y = _723;
   SV_Target.z = _724;
   SV_Target.w = _725;
   int _728 = c_bool & 8;
   bool _729 = (_728 != 0);
   float _730 = select(_729, _714, _722);
   float _731 = select(_729, _715, _723);
   float _732 = select(_729, _716, _724);
   float _733 = select(_729, _627, _725);
   SV_Target.x = _730;
   SV_Target.y = _731;
   SV_Target.z = _732;
   SV_Target.w = _733;
   return SV_Target;
}
