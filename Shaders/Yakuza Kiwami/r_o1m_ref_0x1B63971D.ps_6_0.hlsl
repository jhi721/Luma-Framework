// Yakuza Kiwami material r_o1m[ref] (0x1B63971D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _40 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _45 = _40.x * _40.x;
   float _46 = _40.y * _40.y;
   float _47 = _40.z * _40.z;
   float _48 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _49 = rsqrt(_48);
   float _50 = _49 * TEXCOORD_2.x;
   float _51 = _49 * TEXCOORD_2.y;
   float _52 = _49 * TEXCOORD_2.z;
   float _53 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _54 = rsqrt(_53);
   float _55 = _54 * TEXCOORD_3.x;
   float _56 = _54 * TEXCOORD_3.y;
   float _57 = _54 * TEXCOORD_3.z;
   float _62 = -0.0f - (c_light_vec[0].x);
   float _63 = -0.0f - (c_light_vec[0].y);
   float _64 = -0.0f - (c_light_vec[0].z);
   float _65 = dot(float3(_50, _51, _52), float3(_62, _63, _64));
   float _69 = c_rt_size.z * SV_Position.x;
   float _70 = c_rt_size.w * SV_Position.y;
   int _73 = c_bool & 8;
   bool _74 = (_73 == 0);
   float _81;
   float _82;
   float _83;
   float _95;
   float _96;
   float _132;
   float _133;
   float _134;
   float _135;
   float _149;
   float _150;
   float _185;
   float _186;
   float _187;
   float _221;
   float _222;
   float _223;
   float _224;
   float _225;
   float _226;
   float _380;
   float _381;
   float _382;
   float _383;
   float _384;
   float _385;
   float _557;
   float _558;
   float _559;
   float _560;
   float _561;
   float _562;
   float _680;
   float _681;
   float _682;
   float _698;
   float _699;
   float _700;
   if (!_74)
   {
      float4 _76 = s13_t.Sample(s13_s, float2(_69, _70));
      _81 = _76.x;
      _82 = _76.y;
      _83 = _76.w;
   }
   else
   {
      _81 = 1.0f;
      _82 = 1.0f;
      _83 = 1.0f;
   }
   if (!_74)
   {
      float _88 = (c_sdw_param[0].x) * _81;
      float _89 = (c_sdw_param[0].z) * _82;
      float _92 = _88 + (c_sdw_param[0].y);
      float _93 = _89 + (c_sdw_param[0].w);
      _95 = _92;
      _96 = _93;
   }
   else
   {
      _95 = _81;
      _96 = _82;
   }
   float _100 = c_rt_size.z * SV_Position.x;
   float _101 = c_rt_size.w * SV_Position.y;
   float4 _102 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_100, _101)));
   int _109 = c_bool & 16384;
   bool _110 = (_109 == 0);
   if (!_110)
   {
      float _112 = _102.x * 2.0f;
      float _113 = _102.y * 2.0f;
      float _114 = _102.z * 2.0f;
      float _115 = _102.w * 2.0f;
      _132 = _112;
      _133 = _113;
      _134 = _114;
      _135 = _115;
   }
   else
   {
      int _117 = c_bool & 8192;
      bool _118 = (_117 == 0);
      if (!_118)
      {
         float _120 = _102.x * 2.0f;
         float _121 = _102.y * 2.0f;
         float _122 = _102.z * 2.0f;
         float _123 = _102.w * 2.0f;
         _132 = _120;
         _133 = _121;
         _134 = _122;
         _135 = _123;
      }
      else
      {
         float _127 = _102.x / c_tone_control.z;
         float _128 = _102.y / c_tone_control.z;
         float _129 = _102.z / c_tone_control.z;
         float _130 = _102.w / c_tone_control.z;
         _132 = _127;
         _133 = _128;
         _134 = _129;
         _135 = _130;
      }
   }
   int _136 = c_bool & 8;
   bool _137 = (_136 == 0);
   if (!_137)
   {
      float _142 = (c_sdw_param[0].x) * _95;
      float _143 = (c_sdw_param[0].z) * _96;
      float _146 = _142 + (c_sdw_param[0].y);
      float _147 = _143 + (c_sdw_param[0].w);
      _149 = _146;
      _150 = _147;
   }
   else
   {
      _149 = _95;
      _150 = _96;
   }
   float _151 = _150 * _149;
   float _156 = 1.0f - (c_light_col[5].x);
   float _157 = 1.0f - (c_light_col[5].y);
   float _158 = 1.0f - (c_light_col[5].z);
   float _159 = _156 * _151;
   float _160 = _157 * _151;
   float _161 = _158 * _151;
   float _162 = _159 + (c_light_col[5].x);
   float _163 = _160 + (c_light_col[5].y);
   float _164 = _161 + (c_light_col[5].z);
   float _165 = _83 * COLOR.x;
   float _166 = _83 * COLOR.y;
   float _167 = _83 * COLOR.z;
   if (!_137)
   {
      float _171 = (c_sdw_param[2].x) * _162;
      float _172 = (c_sdw_param[2].x) * _163;
      float _173 = (c_sdw_param[2].x) * _164;
      float _175 = _171 + (c_sdw_param[2].y);
      float _176 = _172 + (c_sdw_param[2].y);
      float _177 = _173 + (c_sdw_param[2].y);
      float _180 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _181 = _175 * _180;
      float _182 = _176 * _180;
      float _183 = _177 * _180;
      _185 = _181;
      _186 = _182;
      _187 = _183;
   }
   else
   {
      _185 = _162;
      _186 = _163;
      _187 = _164;
   }
   float _188 = _185 * _83;
   float _189 = _186 * _83;
   float _190 = _187 * _83;
   float _201 = c_mat_emissive.x * _45;
   float _202 = _201 * c_emissive_scale;
   float _203 = c_mat_emissive.y * _46;
   float _204 = _203 * c_emissive_scale;
   float _205 = c_mat_emissive.z * _47;
   float _206 = _205 * c_emissive_scale;
   int _207 = c_bool & 2048;
   bool _208 = (_207 == 0);
   if (_208)
   {
      float _210 = _150 * _132;
      float _211 = _150 * _133;
      float _212 = _150 * _134;
      float _213 = _210 + TEXCOORD_4.x;
      float _214 = _211 + TEXCOORD_4.y;
      float _215 = _212 + TEXCOORD_4.z;
      float _216 = _150 * _135;
      float _217 = _216 + TEXCOORD_5.x;
      float _218 = _216 + TEXCOORD_5.y;
      float _219 = _216 + TEXCOORD_5.z;
      _221 = _217;
      _222 = _218;
      _223 = _219;
      _224 = _213;
      _225 = _214;
      _226 = _215;
   }
   else
   {
      _221 = TEXCOORD_5.x;
      _222 = TEXCOORD_5.y;
      _223 = TEXCOORD_5.z;
      _224 = TEXCOORD_4.x;
      _225 = TEXCOORD_4.y;
      _226 = TEXCOORD_4.z;
   }
   float _231 = -0.0f - (c_light_vec[1].x);
   float _232 = -0.0f - (c_light_vec[1].y);
   float _233 = -0.0f - (c_light_vec[1].z);
   float _234 = dot(float3(_50, _51, _52), float3(_231, _232, _233));
   float _235 = saturate(_234);
   float _240 = saturate(_65);
   float _241 = (c_light_col[0].x) * _240;
   float _242 = (c_light_col[0].y) * _240;
   float _243 = (c_light_col[0].z) * _240;
   float _248 = (c_light_col[1].x) * _235;
   float _249 = (c_light_col[1].y) * _235;
   float _250 = (c_light_col[1].z) * _235;
   float _259 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _260 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _261 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _262 = _241 * _162;
   float _263 = _242 * _163;
   float _264 = _243 * _164;
   float _265 = _248 * _150;
   float _266 = _249 * _150;
   float _267 = _250 * _150;
   float _268 = _265 + _262;
   float _269 = _266 + _263;
   float _270 = _267 + _264;
   float _271 = _259 * _268;
   float _272 = _260 * _269;
   float _273 = _261 * _270;
   float _274 = _202 + c_scene_ambient.x;
   float _275 = _274 + _271;
   float _276 = _204 + c_scene_ambient.y;
   float _277 = _276 + _272;
   float _278 = _206 + c_scene_ambient.z;
   float _279 = _278 + _273;
   float _280 = _275 * _165;
   float _281 = _277 * _166;
   float _282 = _279 * _167;
   float _283 = _224 * _188;
   float _284 = _225 * _189;
   float _285 = _226 * _190;
   float _286 = _280 + _283;
   float _287 = _281 + _284;
   float _288 = _282 + _285;
   float _293 = (c_light_vec[0].x) + _55;
   float _294 = (c_light_vec[0].y) + _56;
   float _295 = (c_light_vec[0].z) + _57;
   float _296 = dot(float3(_293, _294, _295), float3(_293, _294, _295));
   float _297 = rsqrt(_296);
   float _298 = _293 * _297;
   float _299 = -0.0f - _298;
   float _300 = _294 * _297;
   float _301 = -0.0f - _300;
   float _302 = _295 * _297;
   float _303 = -0.0f - _302;
   float _304 = dot(float3(_299, _301, _303), float3(_50, _51, _52));
   float _305 = saturate(_304);
   float _306 = _56 + -0.30000001192092896f;
   float _307 = dot(float3(_55, _306, _57), float3(_55, _306, _57));
   float _308 = rsqrt(_307);
   float _309 = _55 * _308;
   float _310 = -0.0f - _309;
   float _311 = _306 * _308;
   float _312 = -0.0f - _311;
   float _313 = _57 * _308;
   float _314 = -0.0f - _313;
   float _315 = dot(float3(_310, _312, _314), float3(_50, _51, _52));
   float _316 = saturate(_315);
   float _319 = log2(_305);
   float _320 = log2(_316);
   float _321 = _319 * c_mat_power;
   float _322 = _320 * c_mat_power;
   float _323 = exp2(_321);
   float _324 = exp2(_322);
   float _327 = _323 * (c_light_col[0].x);
   float _328 = _323 * (c_light_col[0].y);
   float _329 = _323 * (c_light_col[0].z);
   float _330 = _324 * 2.0f;
   float _331 = _330 * (c_light_vec[5].x);
   float _332 = _331 * (c_light_col[1].x);
   float _333 = _331 * (c_light_col[1].y);
   float _334 = _331 * (c_light_col[1].z);
   float _335 = _327 * _162;
   float _336 = _328 * _163;
   float _337 = _329 * _164;
   float _338 = _332 * _235;
   float _339 = _333 * _235;
   float _340 = _334 * _235;
   float _341 = _338 + _335;
   float _342 = _339 + _336;
   float _343 = _340 + _337;
   float _350 = c_mat_specular.x * c_specular_scale.x;
   float _351 = _350 * _165;
   float _352 = _351 * _341;
   float _353 = c_mat_specular.y * c_specular_scale.y;
   float _354 = _353 * _166;
   float _355 = _354 * _342;
   float _356 = c_mat_specular.z * c_specular_scale.z;
   float _357 = _356 * _167;
   float _358 = _357 * _343;
   float _359 = _221 * _188;
   float _360 = _359 * _324;
   float _361 = _222 * _189;
   float _362 = _361 * _324;
   float _363 = _223 * _190;
   float _364 = _363 * _324;
   float _365 = _352 + _360;
   float _366 = _355 + _362;
   float _367 = _358 + _364;
   if (!_208)
   {
      float _369 = _150 * _132;
      float _370 = _150 * _133;
      float _371 = _150 * _134;
      float _372 = _286 + _369;
      float _373 = _287 + _370;
      float _374 = _288 + _371;
      float _375 = _150 * _135;
      float _376 = _365 + _375;
      float _377 = _366 + _375;
      float _378 = _367 + _375;
      _380 = _372;
      _381 = _373;
      _382 = _374;
      _383 = _376;
      _384 = _377;
      _385 = _378;
   }
   else
   {
      _380 = _286;
      _381 = _287;
      _382 = _288;
      _383 = _365;
      _384 = _366;
      _385 = _367;
   }
   int _386 = c_bool & 128;
   bool _387 = (_386 != 0);
   float _388 = select(_387, 0.0f, _383);
   float _389 = select(_387, 0.0f, _384);
   float _390 = select(_387, 0.0f, _385);
   float _391 = _380 * _45;
   float _392 = _381 * _46;
   float _393 = _382 * _47;
   float _394 = _388 * _40.w;
   float _395 = _389 * _40.w;
   float _396 = _390 * _40.w;
   int _397 = c_bool & 32768;
   bool _398 = (_397 == 0);
   if (!_398)
   {
      float _402 = (c_ps_user[11].x) * _45;
      float _403 = (c_ps_user[11].x) * _46;
      float _404 = (c_ps_user[11].x) * _47;
      float _406 = _50 + 1.0f;
      float _407 = _51 + 1.0f;
      float _408 = _52 + 1.0f;
      float _409 = _406 * 0.5f;
      float _410 = _407 * 0.5f;
      float _411 = _408 * 0.5f;
      float _412 = saturate(_409);
      float _413 = saturate(_410);
      float _414 = saturate(_411);
      float _415 = (c_ps_user[11].y) * _412;
      float _416 = _413 * (c_ps_user[11].y);
      float _417 = _414 * (c_ps_user[11].y);
      float _419 = (c_ps_user[11].z) * _40.w;
      float _421 = c_mat_power * 0.009999999776482582f;
      float _422 = _421 * (c_ps_user[11].w);
      float _425 = (c_ps_user[12].x) * _162;
      float _426 = (c_ps_user[12].x) * _163;
      float _427 = (c_ps_user[12].x) * _164;
      float _429 = (c_ps_user[12].y) * _165;
      float _430 = (c_ps_user[12].y) * _166;
      float _431 = (c_ps_user[12].y) * _167;
      float _433 = (c_ps_user[12].z) * _188;
      float _434 = (c_ps_user[12].z) * _189;
      float _435 = (c_ps_user[12].z) * _190;
      float _441 = -0.0f - _55;
      float _442 = -0.0f - _56;
      float _443 = -0.0f - _57;
      float _444 = dot(float3(_441, _442, _443), float3(_50, _51, _52));
      float _445 = 1.0f - _444;
      float _446 = _445 * _445;
      float _447 = _445 * 0.9722222089767456f;
      float _448 = _446 * _446;
      float _449 = _448 * _447;
      float _450 = _449 + 0.02777777425944805f;
      float _451 = _450 * (c_ps_user[13].z);
      float _453 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _454 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _455 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _458 = (c_ps_user[14].x) * _202;
      float _459 = (c_ps_user[14].x) * _204;
      float _460 = (c_ps_user[14].x) * _206;
      float _462 = (c_ps_user[14].y) * _241;
      float _463 = (c_ps_user[14].y) * _242;
      float _464 = (c_ps_user[14].y) * _243;
      float _466 = (c_ps_user[14].z) * _248;
      float _467 = (c_ps_user[14].z) * _249;
      float _468 = (c_ps_user[14].z) * _250;
      float _470 = (c_ps_user[14].w) * _224;
      float _471 = (c_ps_user[14].w) * _225;
      float _472 = (c_ps_user[14].w) * _226;
      float _475 = (c_ps_user[15].x) * _327;
      float _476 = (c_ps_user[15].x) * _328;
      float _477 = (c_ps_user[15].x) * _329;
      float _479 = _350 * _332;
      float _480 = _479 * (c_ps_user[15].y);
      float _481 = _353 * _333;
      float _482 = _481 * (c_ps_user[15].y);
      float _483 = _356 * _334;
      float _484 = _483 * (c_ps_user[15].y);
      float _486 = (c_ps_user[15].z) * _221;
      float _487 = (c_ps_user[15].z) * _222;
      float _488 = (c_ps_user[15].z) * _223;
      float _489 = _480 + _486;
      float _490 = _489 * _324;
      float _491 = _462 + (c_ps_user[13].x);
      float _492 = _491 + _466;
      float _493 = c_diffuse_scale.x * _492;
      float _494 = _493 * c_mat_diffuse.x;
      float _495 = (c_ps_user[13].y) + _419;
      float _496 = _495 + _475;
      float _497 = c_mat_specular.x * _496;
      float _498 = _497 * c_specular_scale.x;
      float _499 = _415 + _402;
      float _500 = _499 + _422;
      float _501 = _500 + (c_ps_user[12].w);
      float _502 = _501 + _425;
      float _503 = _502 + _429;
      float _504 = _503 + _433;
      float _505 = _504 + _453;
      float _506 = _505 + _458;
      float _507 = _506 + _470;
      float _508 = _507 + _451;
      float _509 = _508 + _490;
      float _510 = _509 + _494;
      float _511 = _510 + _498;
      float _512 = _482 + _487;
      float _513 = _512 * _324;
      float _514 = _463 + (c_ps_user[13].x);
      float _515 = _514 + _467;
      float _516 = c_diffuse_scale.y * _515;
      float _517 = _516 * c_mat_diffuse.y;
      float _518 = _495 + _476;
      float _519 = c_mat_specular.y * _518;
      float _520 = _519 * c_specular_scale.y;
      float _521 = _416 + _403;
      float _522 = _521 + _422;
      float _523 = _522 + (c_ps_user[12].w);
      float _524 = _523 + _426;
      float _525 = _524 + _430;
      float _526 = _525 + _434;
      float _527 = _526 + _454;
      float _528 = _527 + _459;
      float _529 = _528 + _471;
      float _530 = _529 + _451;
      float _531 = _530 + _513;
      float _532 = _531 + _517;
      float _533 = _532 + _520;
      float _534 = _484 + _488;
      float _535 = _534 * _324;
      float _536 = _464 + (c_ps_user[13].x);
      float _537 = _536 + _468;
      float _538 = c_diffuse_scale.z * _537;
      float _539 = _538 * c_mat_diffuse.z;
      float _540 = _495 + _477;
      float _541 = c_mat_specular.z * _540;
      float _542 = _541 * c_specular_scale.z;
      float _543 = _422 + _404;
      float _544 = _543 + _417;
      float _545 = _544 + (c_ps_user[12].w);
      float _546 = _545 + _427;
      float _547 = _546 + _431;
      float _548 = _547 + _435;
      float _549 = _548 + _455;
      float _550 = _549 + _460;
      float _551 = _550 + _472;
      float _552 = _551 + _451;
      float _553 = _552 + _535;
      float _554 = _553 + _539;
      float _555 = _554 + _542;
      _557 = _511;
      _558 = _533;
      _559 = _555;
      _560 = 0.0f;
      _561 = 0.0f;
      _562 = 0.0f;
   }
   else
   {
      _557 = _391;
      _558 = _392;
      _559 = _393;
      _560 = _394;
      _561 = _395;
      _562 = _396;
   }
   float _563 = _560 + _557;
   float _564 = _561 + _558;
   float _565 = _562 + _559;
   float _566 = dot(float3(_55, _56, _57), float3(_50, _51, _52));
   float _567 = _566 * 2.0f;
   float _568 = _567 * _50;
   float _569 = _567 * _51;
   float _570 = _567 * _52;
   float _571 = _55 - _568;
   float _572 = _56 - _569;
   float _573 = _57 - _570;
   float _574 = -0.0f - _571;
   float4 _575 = s6c_t.Sample(s6c_s, float3(_574, _572, _573));
   float _582 = _96 * _95;
   float _583 = _65 + 0.75f;
   float _584 = saturate(_583);
   float _585 = _582 * _584;
   float _586 = max(_585, (c_light_vec[5].y));
   float _587 = max(_575.w, 0.003921568859368563f);
   float _588 = 0.25f / _587;
   float _589 = _588 * _575.x;
   float _590 = _588 * _575.y;
   float _591 = _588 * _575.z;
   float _592 = _40.w * TEXCOORD_2.w;
   float _593 = _592 * _586;
   float _594 = _589 * _589;
   float _595 = _594 * _593;
   float _596 = _590 * _590;
   float _597 = _596 * _593;
   float _598 = _591 * _591;
   float _599 = _598 * _593;
   float _600 = _563 + _595;
   float _601 = _564 + _597;
   float _602 = _565 + _599;
   float _607 = c_opacity_scale * c_mat_opacity;
   float _610 = _607 * c_global_opacity;
   int _613 = c_bool & 16;
   bool _614 = (_613 == 0);
   if (!_614)
   {
      float _618 = c_tone_control.x * _600;
      float _619 = c_tone_control.x * _601;
      float _620 = c_tone_control.x * _602;
      float _622 = log2(_618);
      float _623 = log2(_619);
      float _624 = log2(_620);
      float _625 = _622 * c_tone_control.y;
      float _626 = _623 * c_tone_control.y;
      float _627 = _624 * c_tone_control.y;
      float _628 = exp2(_625);
      float _629 = exp2(_626);
      float _630 = exp2(_627);
      _680 = _628;
      _681 = _629;
      _682 = _630;
   }
   else
   {
      int _632 = c_bool & 32768;
      bool _633 = (_632 == 0);
      if (_633)
      {
         float _637 = c_tone_control.x * _600;
         float _638 = c_tone_control.x * _601;
         float _639 = c_tone_control.x * _602;
         float _640 = _637 * 2.0f;
         float _641 = _638 * 2.0f;
         float _642 = _639 * 2.0f;
         float _643 = _637 * 0.30000001192092896f;
         float _644 = _638 * 0.30000001192092896f;
         float _645 = _639 * 0.30000001192092896f;
         float _646 = _643 + 0.05000000074505806f;
         float _647 = _644 + 0.05000000074505806f;
         float _648 = _645 + 0.05000000074505806f;
         float _649 = _646 * _640;
         float _650 = _647 * _641;
         float _651 = _648 * _642;
         float _652 = _649 + 0.004000000189989805f;
         float _653 = _650 + 0.004000000189989805f;
         float _654 = _651 + 0.004000000189989805f;
         float _655 = _643 + 0.5f;
         float _656 = _644 + 0.5f;
         float _657 = _645 + 0.5f;
         float _658 = _655 * _640;
         float _659 = _656 * _641;
         float _660 = _657 * _642;
         float _661 = _658 + 0.06000000238418579f;
         float _662 = _659 + 0.06000000238418579f;
         float _663 = _660 + 0.06000000238418579f;
         float _664 = _652 / _661;
         float _665 = _653 / _662;
         float _666 = _654 / _663;
         float _667 = _664 + -0.06666666269302368f;
         float _668 = _665 + -0.06666666269302368f;
         float _669 = _666 + -0.06666666269302368f;
         float _670 = _667 * 1.3790643215179443f;
         float _671 = _668 * 1.3790643215179443f;
         float _672 = _669 * 1.3790643215179443f;
         float _673 = YKR_MaterialCurve(_670, _637);
         float _674 = YKR_MaterialCurve(_671, _638);
         float _675 = YKR_MaterialCurve(_672, _639);
         float _676 = sqrt(_673);
         float _677 = sqrt(_674);
         float _678 = sqrt(_675);
         _680 = _676;
         _681 = _677;
         _682 = _678;
      }
      else
      {
         _680 = _600;
         _681 = _601;
         _682 = _602;
      }
   }
   int _683 = c_bool & 4;
   bool _684 = (_683 == 0);
   if (_684)
   {
      float _686 = saturate(COLOR_1.x);
      float _691 = (c_fog_param[0].x) - _680;
      float _692 = (c_fog_param[0].y) - _681;
      float _693 = (c_fog_param[0].z) - _682;
      float _694 = _691 * _686;
      float _695 = _692 * _686;
      float _696 = _693 * _686;
      _698 = _694;
      _699 = _695;
      _700 = _696;
   }
   else
   {
      _698 = COLOR_1.x;
      _699 = COLOR_1.y;
      _700 = COLOR_1.z;
   }
   float _701 = _698 + _680;
   float _702 = _699 + _681;
   float _703 = _700 + _682;
   float _709 = (c_ps_col_scl[0].x) * _701;
   float _710 = (c_ps_col_scl[0].y) * _702;
   float _711 = (c_ps_col_scl[0].z) * _703;
   float _712 = (c_ps_col_scl[0].w) * _610;
   SV_Target.x = _709;
   SV_Target.y = _710;
   SV_Target.z = _711;
   SV_Target.w = _712;
   int _715 = c_bool & 8;
   bool _716 = (_715 != 0);
   float _717 = select(_716, _701, _709);
   float _718 = select(_716, _702, _710);
   float _719 = select(_716, _703, _711);
   float _720 = select(_716, _610, _712);
   SV_Target.x = _717;
   SV_Target.y = _718;
   SV_Target.z = _719;
   SV_Target.w = _720;
   return SV_Target;
}
