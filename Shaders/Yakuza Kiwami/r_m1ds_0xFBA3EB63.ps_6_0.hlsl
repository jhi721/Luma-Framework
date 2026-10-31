// Yakuza Kiwami material r_m1ds (0xFBA3EB63, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s3_s : register(s3);

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
   float4 _40 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _44 = _40.x * _40.x;
   float _45 = _40.y * _40.y;
   float _46 = _40.z * _40.z;
   float _50 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _51 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _52 = s3_t.Sample(s3_s, float2(_50, _51));
   float _56 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _57 = rsqrt(_56);
   float _58 = _57 * TEXCOORD_2.x;
   float _59 = _57 * TEXCOORD_2.y;
   float _60 = _57 * TEXCOORD_2.z;
   float _61 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _62 = rsqrt(_61);
   float _63 = _62 * TEXCOORD_3.x;
   float _64 = _62 * TEXCOORD_3.y;
   float _65 = _62 * TEXCOORD_3.z;
   float _70 = -0.0f - (c_light_vec[0].x);
   float _71 = -0.0f - (c_light_vec[0].y);
   float _72 = -0.0f - (c_light_vec[0].z);
   float _73 = dot(float3(_58, _59, _60), float3(_70, _71, _72));
   float _77 = c_rt_size.z * SV_Position.x;
   float _78 = c_rt_size.w * SV_Position.y;
   int _81 = c_bool & 8;
   bool _82 = (_81 == 0);
   float _89;
   float _90;
   float _91;
   float _103;
   float _104;
   float _140;
   float _141;
   float _142;
   float _143;
   float _157;
   float _158;
   float _193;
   float _194;
   float _195;
   float _229;
   float _230;
   float _231;
   float _232;
   float _233;
   float _234;
   float _388;
   float _389;
   float _390;
   float _391;
   float _392;
   float _393;
   float _569;
   float _570;
   float _571;
   float _572;
   float _573;
   float _574;
   float _650;
   float _651;
   float _652;
   float _668;
   float _669;
   float _670;
   if (!_82)
   {
      float4 _84 = s13_t.Sample(s13_s, float2(_77, _78));
      _89 = _84.x;
      _90 = _84.y;
      _91 = _84.w;
   }
   else
   {
      _89 = 1.0f;
      _90 = 1.0f;
      _91 = 1.0f;
   }
   if (!_82)
   {
      float _96 = (c_sdw_param[0].x) * _89;
      float _97 = (c_sdw_param[0].z) * _90;
      float _100 = _96 + (c_sdw_param[0].y);
      float _101 = _97 + (c_sdw_param[0].w);
      _103 = _100;
      _104 = _101;
   }
   else
   {
      _103 = _89;
      _104 = _90;
   }
   float _108 = c_rt_size.z * SV_Position.x;
   float _109 = c_rt_size.w * SV_Position.y;
   float4 _110 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_108, _109)));
   int _117 = c_bool & 16384;
   bool _118 = (_117 == 0);
   if (!_118)
   {
      float _120 = _110.x * 2.0f;
      float _121 = _110.y * 2.0f;
      float _122 = _110.z * 2.0f;
      float _123 = _110.w * 2.0f;
      _140 = _120;
      _141 = _121;
      _142 = _122;
      _143 = _123;
   }
   else
   {
      int _125 = c_bool & 8192;
      bool _126 = (_125 == 0);
      if (!_126)
      {
         float _128 = _110.x * 2.0f;
         float _129 = _110.y * 2.0f;
         float _130 = _110.z * 2.0f;
         float _131 = _110.w * 2.0f;
         _140 = _128;
         _141 = _129;
         _142 = _130;
         _143 = _131;
      }
      else
      {
         float _135 = _110.x / c_tone_control.z;
         float _136 = _110.y / c_tone_control.z;
         float _137 = _110.z / c_tone_control.z;
         float _138 = _110.w / c_tone_control.z;
         _140 = _135;
         _141 = _136;
         _142 = _137;
         _143 = _138;
      }
   }
   int _144 = c_bool & 8;
   bool _145 = (_144 == 0);
   if (!_145)
   {
      float _150 = (c_sdw_param[0].x) * _103;
      float _151 = (c_sdw_param[0].z) * _104;
      float _154 = _150 + (c_sdw_param[0].y);
      float _155 = _151 + (c_sdw_param[0].w);
      _157 = _154;
      _158 = _155;
   }
   else
   {
      _157 = _103;
      _158 = _104;
   }
   float _159 = _158 * _157;
   float _164 = 1.0f - (c_light_col[5].x);
   float _165 = 1.0f - (c_light_col[5].y);
   float _166 = 1.0f - (c_light_col[5].z);
   float _167 = _164 * _159;
   float _168 = _165 * _159;
   float _169 = _166 * _159;
   float _170 = _167 + (c_light_col[5].x);
   float _171 = _168 + (c_light_col[5].y);
   float _172 = _169 + (c_light_col[5].z);
   float _173 = _91 * COLOR.x;
   float _174 = _91 * COLOR.y;
   float _175 = _91 * COLOR.z;
   if (!_145)
   {
      float _179 = (c_sdw_param[2].x) * _170;
      float _180 = (c_sdw_param[2].x) * _171;
      float _181 = (c_sdw_param[2].x) * _172;
      float _183 = _179 + (c_sdw_param[2].y);
      float _184 = _180 + (c_sdw_param[2].y);
      float _185 = _181 + (c_sdw_param[2].y);
      float _188 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _189 = _183 * _188;
      float _190 = _184 * _188;
      float _191 = _185 * _188;
      _193 = _189;
      _194 = _190;
      _195 = _191;
   }
   else
   {
      _193 = _170;
      _194 = _171;
      _195 = _172;
   }
   float _196 = _193 * _91;
   float _197 = _194 * _91;
   float _198 = _195 * _91;
   float _209 = c_mat_emissive.x * _44;
   float _210 = _209 * c_emissive_scale;
   float _211 = c_mat_emissive.y * _45;
   float _212 = _211 * c_emissive_scale;
   float _213 = c_mat_emissive.z * _46;
   float _214 = _213 * c_emissive_scale;
   int _215 = c_bool & 2048;
   bool _216 = (_215 == 0);
   if (_216)
   {
      float _218 = _158 * _140;
      float _219 = _158 * _141;
      float _220 = _158 * _142;
      float _221 = _218 + TEXCOORD_4.x;
      float _222 = _219 + TEXCOORD_4.y;
      float _223 = _220 + TEXCOORD_4.z;
      float _224 = _158 * _143;
      float _225 = _224 + TEXCOORD_5.x;
      float _226 = _224 + TEXCOORD_5.y;
      float _227 = _224 + TEXCOORD_5.z;
      _229 = _225;
      _230 = _226;
      _231 = _227;
      _232 = _221;
      _233 = _222;
      _234 = _223;
   }
   else
   {
      _229 = TEXCOORD_5.x;
      _230 = TEXCOORD_5.y;
      _231 = TEXCOORD_5.z;
      _232 = TEXCOORD_4.x;
      _233 = TEXCOORD_4.y;
      _234 = TEXCOORD_4.z;
   }
   float _239 = -0.0f - (c_light_vec[1].x);
   float _240 = -0.0f - (c_light_vec[1].y);
   float _241 = -0.0f - (c_light_vec[1].z);
   float _242 = dot(float3(_58, _59, _60), float3(_239, _240, _241));
   float _243 = saturate(_242);
   float _248 = saturate(_73);
   float _249 = (c_light_col[0].x) * _248;
   float _250 = (c_light_col[0].y) * _248;
   float _251 = (c_light_col[0].z) * _248;
   float _256 = (c_light_col[1].x) * _243;
   float _257 = (c_light_col[1].y) * _243;
   float _258 = (c_light_col[1].z) * _243;
   float _267 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _268 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _269 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _270 = _249 * _170;
   float _271 = _250 * _171;
   float _272 = _251 * _172;
   float _273 = _256 * _158;
   float _274 = _257 * _158;
   float _275 = _258 * _158;
   float _276 = _273 + _270;
   float _277 = _274 + _271;
   float _278 = _275 + _272;
   float _279 = _267 * _276;
   float _280 = _268 * _277;
   float _281 = _269 * _278;
   float _282 = _210 + c_scene_ambient.x;
   float _283 = _282 + _279;
   float _284 = _212 + c_scene_ambient.y;
   float _285 = _284 + _280;
   float _286 = _214 + c_scene_ambient.z;
   float _287 = _286 + _281;
   float _288 = _283 * _173;
   float _289 = _285 * _174;
   float _290 = _287 * _175;
   float _291 = _232 * _196;
   float _292 = _233 * _197;
   float _293 = _234 * _198;
   float _294 = _288 + _291;
   float _295 = _289 + _292;
   float _296 = _290 + _293;
   float _301 = (c_light_vec[0].x) + _63;
   float _302 = (c_light_vec[0].y) + _64;
   float _303 = (c_light_vec[0].z) + _65;
   float _304 = dot(float3(_301, _302, _303), float3(_301, _302, _303));
   float _305 = rsqrt(_304);
   float _306 = _301 * _305;
   float _307 = -0.0f - _306;
   float _308 = _302 * _305;
   float _309 = -0.0f - _308;
   float _310 = _303 * _305;
   float _311 = -0.0f - _310;
   float _312 = dot(float3(_307, _309, _311), float3(_58, _59, _60));
   float _313 = saturate(_312);
   float _314 = _64 + -0.30000001192092896f;
   float _315 = dot(float3(_63, _314, _65), float3(_63, _314, _65));
   float _316 = rsqrt(_315);
   float _317 = _63 * _316;
   float _318 = -0.0f - _317;
   float _319 = _314 * _316;
   float _320 = -0.0f - _319;
   float _321 = _65 * _316;
   float _322 = -0.0f - _321;
   float _323 = dot(float3(_318, _320, _322), float3(_58, _59, _60));
   float _324 = saturate(_323);
   float _327 = log2(_313);
   float _328 = log2(_324);
   float _329 = _327 * c_mat_power;
   float _330 = _328 * c_mat_power;
   float _331 = exp2(_329);
   float _332 = exp2(_330);
   float _335 = _331 * (c_light_col[0].x);
   float _336 = _331 * (c_light_col[0].y);
   float _337 = _331 * (c_light_col[0].z);
   float _338 = _332 * 2.0f;
   float _339 = _338 * (c_light_vec[5].x);
   float _340 = _339 * (c_light_col[1].x);
   float _341 = _339 * (c_light_col[1].y);
   float _342 = _339 * (c_light_col[1].z);
   float _343 = _335 * _170;
   float _344 = _336 * _171;
   float _345 = _337 * _172;
   float _346 = _340 * _243;
   float _347 = _341 * _243;
   float _348 = _342 * _243;
   float _349 = _346 + _343;
   float _350 = _347 + _344;
   float _351 = _348 + _345;
   float _358 = c_mat_specular.x * c_specular_scale.x;
   float _359 = _358 * _173;
   float _360 = _359 * _349;
   float _361 = c_mat_specular.y * c_specular_scale.y;
   float _362 = _361 * _174;
   float _363 = _362 * _350;
   float _364 = c_mat_specular.z * c_specular_scale.z;
   float _365 = _364 * _175;
   float _366 = _365 * _351;
   float _367 = _229 * _196;
   float _368 = _367 * _332;
   float _369 = _230 * _197;
   float _370 = _369 * _332;
   float _371 = _231 * _198;
   float _372 = _371 * _332;
   float _373 = _360 + _368;
   float _374 = _363 + _370;
   float _375 = _366 + _372;
   if (!_216)
   {
      float _377 = _158 * _140;
      float _378 = _158 * _141;
      float _379 = _158 * _142;
      float _380 = _294 + _377;
      float _381 = _295 + _378;
      float _382 = _296 + _379;
      float _383 = _158 * _143;
      float _384 = _373 + _383;
      float _385 = _374 + _383;
      float _386 = _375 + _383;
      _388 = _380;
      _389 = _381;
      _390 = _382;
      _391 = _384;
      _392 = _385;
      _393 = _386;
   }
   else
   {
      _388 = _294;
      _389 = _295;
      _390 = _296;
      _391 = _373;
      _392 = _374;
      _393 = _375;
   }
   int _394 = c_bool & 128;
   bool _395 = (_394 != 0);
   float _396 = select(_395, 0.0f, _391);
   float _397 = select(_395, 0.0f, _392);
   float _398 = select(_395, 0.0f, _393);
   float _399 = _388 * _44;
   float _400 = _389 * _45;
   float _401 = _390 * _46;
   float _402 = _396 * _52.x;
   float _403 = _397 * _52.y;
   float _404 = _398 * _52.z;
   int _405 = c_bool & 32768;
   bool _406 = (_405 == 0);
   if (!_406)
   {
      float _410 = (c_ps_user[11].x) * _44;
      float _411 = (c_ps_user[11].x) * _45;
      float _412 = (c_ps_user[11].x) * _46;
      float _414 = _58 + 1.0f;
      float _415 = _59 + 1.0f;
      float _416 = _60 + 1.0f;
      float _417 = _414 * 0.5f;
      float _418 = _415 * 0.5f;
      float _419 = _416 * 0.5f;
      float _420 = saturate(_417);
      float _421 = saturate(_418);
      float _422 = saturate(_419);
      float _423 = (c_ps_user[11].y) * _420;
      float _424 = _421 * (c_ps_user[11].y);
      float _425 = _422 * (c_ps_user[11].y);
      float _427 = (c_ps_user[11].z) * _52.x;
      float _428 = (c_ps_user[11].z) * _52.y;
      float _429 = (c_ps_user[11].z) * _52.z;
      float _431 = c_mat_power * 0.009999999776482582f;
      float _432 = _431 * (c_ps_user[11].w);
      float _435 = (c_ps_user[12].x) * _170;
      float _436 = (c_ps_user[12].x) * _171;
      float _437 = (c_ps_user[12].x) * _172;
      float _439 = (c_ps_user[12].y) * _173;
      float _440 = (c_ps_user[12].y) * _174;
      float _441 = (c_ps_user[12].y) * _175;
      float _443 = (c_ps_user[12].z) * _196;
      float _444 = (c_ps_user[12].z) * _197;
      float _445 = (c_ps_user[12].z) * _198;
      float _451 = -0.0f - _63;
      float _452 = -0.0f - _64;
      float _453 = -0.0f - _65;
      float _454 = dot(float3(_451, _452, _453), float3(_58, _59, _60));
      float _455 = 1.0f - _454;
      float _456 = _455 * _455;
      float _457 = _455 * 0.9722222089767456f;
      float _458 = _456 * _456;
      float _459 = _458 * _457;
      float _460 = _459 + 0.02777777425944805f;
      float _461 = _460 * (c_ps_user[13].z);
      float _463 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _464 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _465 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _468 = (c_ps_user[14].x) * _210;
      float _469 = (c_ps_user[14].x) * _212;
      float _470 = (c_ps_user[14].x) * _214;
      float _472 = (c_ps_user[14].y) * _249;
      float _473 = (c_ps_user[14].y) * _250;
      float _474 = (c_ps_user[14].y) * _251;
      float _476 = (c_ps_user[14].z) * _256;
      float _477 = (c_ps_user[14].z) * _257;
      float _478 = (c_ps_user[14].z) * _258;
      float _480 = (c_ps_user[14].w) * _232;
      float _481 = (c_ps_user[14].w) * _233;
      float _482 = (c_ps_user[14].w) * _234;
      float _485 = (c_ps_user[15].x) * _335;
      float _486 = (c_ps_user[15].x) * _336;
      float _487 = (c_ps_user[15].x) * _337;
      float _489 = _358 * _340;
      float _490 = _489 * (c_ps_user[15].y);
      float _491 = _361 * _341;
      float _492 = _491 * (c_ps_user[15].y);
      float _493 = _364 * _342;
      float _494 = _493 * (c_ps_user[15].y);
      float _496 = (c_ps_user[15].z) * _229;
      float _497 = (c_ps_user[15].z) * _230;
      float _498 = (c_ps_user[15].z) * _231;
      float _499 = _490 + _496;
      float _500 = _499 * _332;
      float _501 = _472 + (c_ps_user[13].x);
      float _502 = _501 + _476;
      float _503 = c_diffuse_scale.x * _502;
      float _504 = _503 * c_mat_diffuse.x;
      float _505 = (c_ps_user[13].y) + _427;
      float _506 = _505 + _485;
      float _507 = c_mat_specular.x * _506;
      float _508 = _507 * c_specular_scale.x;
      float _509 = _423 + _410;
      float _510 = _509 + _432;
      float _511 = _510 + (c_ps_user[12].w);
      float _512 = _511 + _435;
      float _513 = _512 + _439;
      float _514 = _513 + _443;
      float _515 = _514 + _463;
      float _516 = _515 + _468;
      float _517 = _516 + _480;
      float _518 = _517 + _461;
      float _519 = _518 + _500;
      float _520 = _519 + _504;
      float _521 = _520 + _508;
      float _522 = _492 + _497;
      float _523 = _522 * _332;
      float _524 = _473 + (c_ps_user[13].x);
      float _525 = _524 + _477;
      float _526 = c_diffuse_scale.y * _525;
      float _527 = _526 * c_mat_diffuse.y;
      float _528 = (c_ps_user[13].y) + _428;
      float _529 = _528 + _486;
      float _530 = c_mat_specular.y * _529;
      float _531 = _530 * c_specular_scale.y;
      float _532 = _424 + _411;
      float _533 = _532 + _432;
      float _534 = _533 + (c_ps_user[12].w);
      float _535 = _534 + _436;
      float _536 = _535 + _440;
      float _537 = _536 + _444;
      float _538 = _537 + _464;
      float _539 = _538 + _469;
      float _540 = _539 + _481;
      float _541 = _540 + _461;
      float _542 = _541 + _523;
      float _543 = _542 + _527;
      float _544 = _543 + _531;
      float _545 = _494 + _498;
      float _546 = _545 * _332;
      float _547 = _474 + (c_ps_user[13].x);
      float _548 = _547 + _478;
      float _549 = c_diffuse_scale.z * _548;
      float _550 = _549 * c_mat_diffuse.z;
      float _551 = (c_ps_user[13].y) + _429;
      float _552 = _551 + _487;
      float _553 = c_mat_specular.z * _552;
      float _554 = _553 * c_specular_scale.z;
      float _555 = _432 + _412;
      float _556 = _555 + _425;
      float _557 = _556 + (c_ps_user[12].w);
      float _558 = _557 + _437;
      float _559 = _558 + _441;
      float _560 = _559 + _445;
      float _561 = _560 + _465;
      float _562 = _561 + _470;
      float _563 = _562 + _482;
      float _564 = _563 + _461;
      float _565 = _564 + _546;
      float _566 = _565 + _550;
      float _567 = _566 + _554;
      _569 = _521;
      _570 = _544;
      _571 = _567;
      _572 = 0.0f;
      _573 = 0.0f;
      _574 = 0.0f;
   }
   else
   {
      _569 = _399;
      _570 = _400;
      _571 = _401;
      _572 = _402;
      _573 = _403;
      _574 = _404;
   }
   float _575 = _572 + _569;
   float _576 = _573 + _570;
   float _577 = _574 + _571;
   float _582 = c_mat_opacity * COLOR.w;
   float _583 = _582 * c_global_opacity;
   float _584 = _583 * c_opacity_scale;
   int _585 = c_bool & 16;
   bool _586 = (_585 == 0);
   if (!_586)
   {
      float _590 = c_tone_control.x * _575;
      float _591 = c_tone_control.x * _576;
      float _592 = c_tone_control.x * _577;
      float _594 = log2(_590);
      float _595 = log2(_591);
      float _596 = log2(_592);
      float _597 = _594 * c_tone_control.y;
      float _598 = _595 * c_tone_control.y;
      float _599 = _596 * c_tone_control.y;
      float _600 = exp2(_597);
      float _601 = exp2(_598);
      float _602 = exp2(_599);
      _650 = _600;
      _651 = _601;
      _652 = _602;
   }
   else
   {
      if (_406)
      {
         float _607 = c_tone_control.x * _575;
         float _608 = c_tone_control.x * _576;
         float _609 = c_tone_control.x * _577;
         float _610 = _607 * 2.0f;
         float _611 = _608 * 2.0f;
         float _612 = _609 * 2.0f;
         float _613 = _607 * 0.30000001192092896f;
         float _614 = _608 * 0.30000001192092896f;
         float _615 = _609 * 0.30000001192092896f;
         float _616 = _613 + 0.05000000074505806f;
         float _617 = _614 + 0.05000000074505806f;
         float _618 = _615 + 0.05000000074505806f;
         float _619 = _616 * _610;
         float _620 = _617 * _611;
         float _621 = _618 * _612;
         float _622 = _619 + 0.004000000189989805f;
         float _623 = _620 + 0.004000000189989805f;
         float _624 = _621 + 0.004000000189989805f;
         float _625 = _613 + 0.5f;
         float _626 = _614 + 0.5f;
         float _627 = _615 + 0.5f;
         float _628 = _625 * _610;
         float _629 = _626 * _611;
         float _630 = _627 * _612;
         float _631 = _628 + 0.06000000238418579f;
         float _632 = _629 + 0.06000000238418579f;
         float _633 = _630 + 0.06000000238418579f;
         float _634 = _622 / _631;
         float _635 = _623 / _632;
         float _636 = _624 / _633;
         float _637 = _634 + -0.06666666269302368f;
         float _638 = _635 + -0.06666666269302368f;
         float _639 = _636 + -0.06666666269302368f;
         float _640 = _637 * 1.3790643215179443f;
         float _641 = _638 * 1.3790643215179443f;
         float _642 = _639 * 1.3790643215179443f;
         float _643 = YKR_MaterialCurve(_640, _607);
         float _644 = YKR_MaterialCurve(_641, _608);
         float _645 = YKR_MaterialCurve(_642, _609);
         float _646 = sqrt(_643);
         float _647 = sqrt(_644);
         float _648 = sqrt(_645);
         _650 = _646;
         _651 = _647;
         _652 = _648;
      }
      else
      {
         _650 = _575;
         _651 = _576;
         _652 = _577;
      }
   }
   int _653 = c_bool & 4;
   bool _654 = (_653 == 0);
   if (_654)
   {
      float _656 = saturate(COLOR_1.x);
      float _661 = (c_fog_param[0].x) - _650;
      float _662 = (c_fog_param[0].y) - _651;
      float _663 = (c_fog_param[0].z) - _652;
      float _664 = _661 * _656;
      float _665 = _662 * _656;
      float _666 = _663 * _656;
      _668 = _664;
      _669 = _665;
      _670 = _666;
   }
   else
   {
      _668 = COLOR_1.x;
      _669 = COLOR_1.y;
      _670 = COLOR_1.z;
   }
   float _671 = _668 + _650;
   float _672 = _669 + _651;
   float _673 = _670 + _652;
   float _679 = (c_ps_col_scl[0].x) * _671;
   float _680 = (c_ps_col_scl[0].y) * _672;
   float _681 = (c_ps_col_scl[0].z) * _673;
   float _682 = (c_ps_col_scl[0].w) * _584;
   SV_Target.x = _679;
   SV_Target.y = _680;
   SV_Target.z = _681;
   SV_Target.w = _682;
   int _685 = c_bool & 8;
   bool _686 = (_685 != 0);
   float _687 = select(_686, _671, _679);
   float _688 = select(_686, _672, _680);
   float _689 = select(_686, _673, _681);
   float _690 = select(_686, _584, _682);
   SV_Target.x = _687;
   SV_Target.y = _688;
   SV_Target.z = _689;
   SV_Target.w = _690;
   return SV_Target;
}
