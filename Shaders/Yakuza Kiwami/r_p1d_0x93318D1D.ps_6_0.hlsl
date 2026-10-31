// Yakuza Kiwami material r_p1d (0x93318D1D, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float4 _37 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _42 = _37.x * _37.x;
   float _43 = _37.y * _37.y;
   float _44 = _37.z * _37.z;
   float _45 = dot(float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _46 = rsqrt(_45);
   float _47 = _46 * TEXCOORD_2.x;
   float _48 = _46 * TEXCOORD_2.y;
   float _49 = _46 * TEXCOORD_2.z;
   float _50 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _51 = rsqrt(_50);
   float _52 = _51 * TEXCOORD_3.x;
   float _53 = _51 * TEXCOORD_3.y;
   float _54 = _51 * TEXCOORD_3.z;
   float _59 = -0.0f - (c_light_vec[0].x);
   float _60 = -0.0f - (c_light_vec[0].y);
   float _61 = -0.0f - (c_light_vec[0].z);
   float _62 = dot(float3(_47, _48, _49), float3(_59, _60, _61));
   float _66 = c_rt_size.z * SV_Position.x;
   float _67 = c_rt_size.w * SV_Position.y;
   int _70 = c_bool & 8;
   bool _71 = (_70 == 0);
   float _78;
   float _79;
   float _80;
   float _92;
   float _93;
   float _129;
   float _130;
   float _131;
   float _132;
   float _146;
   float _147;
   float _182;
   float _183;
   float _184;
   float _218;
   float _219;
   float _220;
   float _221;
   float _222;
   float _223;
   float _377;
   float _378;
   float _379;
   float _380;
   float _381;
   float _382;
   float _550;
   float _551;
   float _552;
   float _553;
   float _554;
   float _555;
   float _631;
   float _632;
   float _633;
   float _649;
   float _650;
   float _651;
   if (!_71)
   {
      float4 _73 = s13_t.Sample(s13_s, float2(_66, _67));
      _78 = _73.x;
      _79 = _73.y;
      _80 = _73.w;
   }
   else
   {
      _78 = 1.0f;
      _79 = 1.0f;
      _80 = 1.0f;
   }
   if (!_71)
   {
      float _85 = (c_sdw_param[0].x) * _78;
      float _86 = (c_sdw_param[0].z) * _79;
      float _89 = _85 + (c_sdw_param[0].y);
      float _90 = _86 + (c_sdw_param[0].w);
      _92 = _89;
      _93 = _90;
   }
   else
   {
      _92 = _78;
      _93 = _79;
   }
   float _97 = c_rt_size.z * SV_Position.x;
   float _98 = c_rt_size.w * SV_Position.y;
   float4 _99 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_97, _98)));
   int _106 = c_bool & 16384;
   bool _107 = (_106 == 0);
   if (!_107)
   {
      float _109 = _99.x * 2.0f;
      float _110 = _99.y * 2.0f;
      float _111 = _99.z * 2.0f;
      float _112 = _99.w * 2.0f;
      _129 = _109;
      _130 = _110;
      _131 = _111;
      _132 = _112;
   }
   else
   {
      int _114 = c_bool & 8192;
      bool _115 = (_114 == 0);
      if (!_115)
      {
         float _117 = _99.x * 2.0f;
         float _118 = _99.y * 2.0f;
         float _119 = _99.z * 2.0f;
         float _120 = _99.w * 2.0f;
         _129 = _117;
         _130 = _118;
         _131 = _119;
         _132 = _120;
      }
      else
      {
         float _124 = _99.x / c_tone_control.z;
         float _125 = _99.y / c_tone_control.z;
         float _126 = _99.z / c_tone_control.z;
         float _127 = _99.w / c_tone_control.z;
         _129 = _124;
         _130 = _125;
         _131 = _126;
         _132 = _127;
      }
   }
   int _133 = c_bool & 8;
   bool _134 = (_133 == 0);
   if (!_134)
   {
      float _139 = (c_sdw_param[0].x) * _92;
      float _140 = (c_sdw_param[0].z) * _93;
      float _143 = _139 + (c_sdw_param[0].y);
      float _144 = _140 + (c_sdw_param[0].w);
      _146 = _143;
      _147 = _144;
   }
   else
   {
      _146 = _92;
      _147 = _93;
   }
   float _148 = _147 * _146;
   float _153 = 1.0f - (c_light_col[5].x);
   float _154 = 1.0f - (c_light_col[5].y);
   float _155 = 1.0f - (c_light_col[5].z);
   float _156 = _153 * _148;
   float _157 = _154 * _148;
   float _158 = _155 * _148;
   float _159 = _156 + (c_light_col[5].x);
   float _160 = _157 + (c_light_col[5].y);
   float _161 = _158 + (c_light_col[5].z);
   float _162 = _80 * COLOR.x;
   float _163 = _80 * COLOR.y;
   float _164 = _80 * COLOR.z;
   if (!_134)
   {
      float _168 = (c_sdw_param[2].x) * _159;
      float _169 = (c_sdw_param[2].x) * _160;
      float _170 = (c_sdw_param[2].x) * _161;
      float _172 = _168 + (c_sdw_param[2].y);
      float _173 = _169 + (c_sdw_param[2].y);
      float _174 = _170 + (c_sdw_param[2].y);
      float _177 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _178 = _172 * _177;
      float _179 = _173 * _177;
      float _180 = _174 * _177;
      _182 = _178;
      _183 = _179;
      _184 = _180;
   }
   else
   {
      _182 = _159;
      _183 = _160;
      _184 = _161;
   }
   float _185 = _182 * _80;
   float _186 = _183 * _80;
   float _187 = _184 * _80;
   float _198 = c_mat_emissive.x * _42;
   float _199 = _198 * c_emissive_scale;
   float _200 = c_mat_emissive.y * _43;
   float _201 = _200 * c_emissive_scale;
   float _202 = c_mat_emissive.z * _44;
   float _203 = _202 * c_emissive_scale;
   int _204 = c_bool & 2048;
   bool _205 = (_204 == 0);
   if (_205)
   {
      float _207 = _147 * _129;
      float _208 = _147 * _130;
      float _209 = _147 * _131;
      float _210 = _207 + TEXCOORD_4.x;
      float _211 = _208 + TEXCOORD_4.y;
      float _212 = _209 + TEXCOORD_4.z;
      float _213 = _147 * _132;
      float _214 = _213 + TEXCOORD_5.x;
      float _215 = _213 + TEXCOORD_5.y;
      float _216 = _213 + TEXCOORD_5.z;
      _218 = _214;
      _219 = _215;
      _220 = _216;
      _221 = _210;
      _222 = _211;
      _223 = _212;
   }
   else
   {
      _218 = TEXCOORD_5.x;
      _219 = TEXCOORD_5.y;
      _220 = TEXCOORD_5.z;
      _221 = TEXCOORD_4.x;
      _222 = TEXCOORD_4.y;
      _223 = TEXCOORD_4.z;
   }
   float _228 = -0.0f - (c_light_vec[1].x);
   float _229 = -0.0f - (c_light_vec[1].y);
   float _230 = -0.0f - (c_light_vec[1].z);
   float _231 = dot(float3(_47, _48, _49), float3(_228, _229, _230));
   float _232 = saturate(_231);
   float _237 = saturate(_62);
   float _238 = (c_light_col[0].x) * _237;
   float _239 = (c_light_col[0].y) * _237;
   float _240 = (c_light_col[0].z) * _237;
   float _245 = (c_light_col[1].x) * _232;
   float _246 = (c_light_col[1].y) * _232;
   float _247 = (c_light_col[1].z) * _232;
   float _256 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _257 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _258 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _259 = _238 * _159;
   float _260 = _239 * _160;
   float _261 = _240 * _161;
   float _262 = _245 * _147;
   float _263 = _246 * _147;
   float _264 = _247 * _147;
   float _265 = _262 + _259;
   float _266 = _263 + _260;
   float _267 = _264 + _261;
   float _268 = _256 * _265;
   float _269 = _257 * _266;
   float _270 = _258 * _267;
   float _271 = _199 + c_scene_ambient.x;
   float _272 = _271 + _268;
   float _273 = _201 + c_scene_ambient.y;
   float _274 = _273 + _269;
   float _275 = _203 + c_scene_ambient.z;
   float _276 = _275 + _270;
   float _277 = _272 * _162;
   float _278 = _274 * _163;
   float _279 = _276 * _164;
   float _280 = _221 * _185;
   float _281 = _222 * _186;
   float _282 = _223 * _187;
   float _283 = _277 + _280;
   float _284 = _278 + _281;
   float _285 = _279 + _282;
   float _290 = (c_light_vec[0].x) + _52;
   float _291 = (c_light_vec[0].y) + _53;
   float _292 = (c_light_vec[0].z) + _54;
   float _293 = dot(float3(_290, _291, _292), float3(_290, _291, _292));
   float _294 = rsqrt(_293);
   float _295 = _290 * _294;
   float _296 = -0.0f - _295;
   float _297 = _291 * _294;
   float _298 = -0.0f - _297;
   float _299 = _292 * _294;
   float _300 = -0.0f - _299;
   float _301 = dot(float3(_296, _298, _300), float3(_47, _48, _49));
   float _302 = saturate(_301);
   float _303 = _53 + -0.30000001192092896f;
   float _304 = dot(float3(_52, _303, _54), float3(_52, _303, _54));
   float _305 = rsqrt(_304);
   float _306 = _52 * _305;
   float _307 = -0.0f - _306;
   float _308 = _303 * _305;
   float _309 = -0.0f - _308;
   float _310 = _54 * _305;
   float _311 = -0.0f - _310;
   float _312 = dot(float3(_307, _309, _311), float3(_47, _48, _49));
   float _313 = saturate(_312);
   float _316 = log2(_302);
   float _317 = log2(_313);
   float _318 = _316 * c_mat_power;
   float _319 = _317 * c_mat_power;
   float _320 = exp2(_318);
   float _321 = exp2(_319);
   float _324 = _320 * (c_light_col[0].x);
   float _325 = _320 * (c_light_col[0].y);
   float _326 = _320 * (c_light_col[0].z);
   float _327 = _321 * 2.0f;
   float _328 = _327 * (c_light_vec[5].x);
   float _329 = _328 * (c_light_col[1].x);
   float _330 = _328 * (c_light_col[1].y);
   float _331 = _328 * (c_light_col[1].z);
   float _332 = _324 * _159;
   float _333 = _325 * _160;
   float _334 = _326 * _161;
   float _335 = _329 * _232;
   float _336 = _330 * _232;
   float _337 = _331 * _232;
   float _338 = _335 + _332;
   float _339 = _336 + _333;
   float _340 = _337 + _334;
   float _347 = c_mat_specular.x * c_specular_scale.x;
   float _348 = _347 * _162;
   float _349 = _348 * _338;
   float _350 = c_mat_specular.y * c_specular_scale.y;
   float _351 = _350 * _163;
   float _352 = _351 * _339;
   float _353 = c_mat_specular.z * c_specular_scale.z;
   float _354 = _353 * _164;
   float _355 = _354 * _340;
   float _356 = _218 * _185;
   float _357 = _356 * _321;
   float _358 = _219 * _186;
   float _359 = _358 * _321;
   float _360 = _220 * _187;
   float _361 = _360 * _321;
   float _362 = _349 + _357;
   float _363 = _352 + _359;
   float _364 = _355 + _361;
   if (!_205)
   {
      float _366 = _147 * _129;
      float _367 = _147 * _130;
      float _368 = _147 * _131;
      float _369 = _283 + _366;
      float _370 = _284 + _367;
      float _371 = _285 + _368;
      float _372 = _147 * _132;
      float _373 = _362 + _372;
      float _374 = _363 + _372;
      float _375 = _364 + _372;
      _377 = _369;
      _378 = _370;
      _379 = _371;
      _380 = _373;
      _381 = _374;
      _382 = _375;
   }
   else
   {
      _377 = _283;
      _378 = _284;
      _379 = _285;
      _380 = _362;
      _381 = _363;
      _382 = _364;
   }
   int _383 = c_bool & 128;
   bool _384 = (_383 != 0);
   float _385 = select(_384, 0.0f, _380);
   float _386 = select(_384, 0.0f, _381);
   float _387 = select(_384, 0.0f, _382);
   float _388 = _377 * _42;
   float _389 = _378 * _43;
   float _390 = _379 * _44;
   int _391 = c_bool & 32768;
   bool _392 = (_391 == 0);
   if (!_392)
   {
      float _396 = (c_ps_user[11].x) * _42;
      float _397 = (c_ps_user[11].x) * _43;
      float _398 = (c_ps_user[11].x) * _44;
      float _400 = _47 + 1.0f;
      float _401 = _48 + 1.0f;
      float _402 = _49 + 1.0f;
      float _403 = _400 * 0.5f;
      float _404 = _401 * 0.5f;
      float _405 = _402 * 0.5f;
      float _406 = saturate(_403);
      float _407 = saturate(_404);
      float _408 = saturate(_405);
      float _409 = (c_ps_user[11].y) * _406;
      float _410 = _407 * (c_ps_user[11].y);
      float _411 = _408 * (c_ps_user[11].y);
      float _414 = c_mat_power * 0.009999999776482582f;
      float _415 = _414 * (c_ps_user[11].w);
      float _418 = (c_ps_user[12].x) * _159;
      float _419 = (c_ps_user[12].x) * _160;
      float _420 = (c_ps_user[12].x) * _161;
      float _422 = (c_ps_user[12].y) * _162;
      float _423 = (c_ps_user[12].y) * _163;
      float _424 = (c_ps_user[12].y) * _164;
      float _426 = (c_ps_user[12].z) * _185;
      float _427 = (c_ps_user[12].z) * _186;
      float _428 = (c_ps_user[12].z) * _187;
      float _434 = -0.0f - _52;
      float _435 = -0.0f - _53;
      float _436 = -0.0f - _54;
      float _437 = dot(float3(_434, _435, _436), float3(_47, _48, _49));
      float _438 = 1.0f - _437;
      float _439 = _438 * _438;
      float _440 = _438 * 0.9722222089767456f;
      float _441 = _439 * _439;
      float _442 = _441 * _440;
      float _443 = _442 + 0.02777777425944805f;
      float _444 = _443 * (c_ps_user[13].z);
      float _446 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _447 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _448 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _451 = (c_ps_user[14].x) * _199;
      float _452 = (c_ps_user[14].x) * _201;
      float _453 = (c_ps_user[14].x) * _203;
      float _455 = (c_ps_user[14].y) * _238;
      float _456 = (c_ps_user[14].y) * _239;
      float _457 = (c_ps_user[14].y) * _240;
      float _459 = (c_ps_user[14].z) * _245;
      float _460 = (c_ps_user[14].z) * _246;
      float _461 = (c_ps_user[14].z) * _247;
      float _463 = (c_ps_user[14].w) * _221;
      float _464 = (c_ps_user[14].w) * _222;
      float _465 = (c_ps_user[14].w) * _223;
      float _468 = (c_ps_user[15].x) * _324;
      float _469 = (c_ps_user[15].x) * _325;
      float _470 = (c_ps_user[15].x) * _326;
      float _472 = _347 * _329;
      float _473 = _472 * (c_ps_user[15].y);
      float _474 = _350 * _330;
      float _475 = _474 * (c_ps_user[15].y);
      float _476 = _353 * _331;
      float _477 = _476 * (c_ps_user[15].y);
      float _479 = (c_ps_user[15].z) * _218;
      float _480 = (c_ps_user[15].z) * _219;
      float _481 = (c_ps_user[15].z) * _220;
      float _482 = _473 + _479;
      float _483 = _482 * _321;
      float _484 = _455 + (c_ps_user[13].x);
      float _485 = _484 + _459;
      float _486 = c_diffuse_scale.x * _485;
      float _487 = _486 * c_mat_diffuse.x;
      float _488 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _489 = _488 + _468;
      float _490 = c_mat_specular.x * _489;
      float _491 = _490 * c_specular_scale.x;
      float _492 = _409 + _396;
      float _493 = _492 + _415;
      float _494 = _493 + (c_ps_user[12].w);
      float _495 = _494 + _418;
      float _496 = _495 + _422;
      float _497 = _496 + _426;
      float _498 = _497 + _446;
      float _499 = _498 + _451;
      float _500 = _499 + _463;
      float _501 = _500 + _444;
      float _502 = _501 + _483;
      float _503 = _502 + _487;
      float _504 = _503 + _491;
      float _505 = _475 + _480;
      float _506 = _505 * _321;
      float _507 = _456 + (c_ps_user[13].x);
      float _508 = _507 + _460;
      float _509 = c_diffuse_scale.y * _508;
      float _510 = _509 * c_mat_diffuse.y;
      float _511 = _488 + _469;
      float _512 = c_mat_specular.y * _511;
      float _513 = _512 * c_specular_scale.y;
      float _514 = _410 + _397;
      float _515 = _514 + _415;
      float _516 = _515 + (c_ps_user[12].w);
      float _517 = _516 + _419;
      float _518 = _517 + _423;
      float _519 = _518 + _427;
      float _520 = _519 + _447;
      float _521 = _520 + _452;
      float _522 = _521 + _464;
      float _523 = _522 + _444;
      float _524 = _523 + _506;
      float _525 = _524 + _510;
      float _526 = _525 + _513;
      float _527 = _477 + _481;
      float _528 = _527 * _321;
      float _529 = _457 + (c_ps_user[13].x);
      float _530 = _529 + _461;
      float _531 = c_diffuse_scale.z * _530;
      float _532 = _531 * c_mat_diffuse.z;
      float _533 = _488 + _470;
      float _534 = c_mat_specular.z * _533;
      float _535 = _534 * c_specular_scale.z;
      float _536 = _415 + _398;
      float _537 = _536 + _411;
      float _538 = _537 + (c_ps_user[12].w);
      float _539 = _538 + _420;
      float _540 = _539 + _424;
      float _541 = _540 + _428;
      float _542 = _541 + _448;
      float _543 = _542 + _453;
      float _544 = _543 + _465;
      float _545 = _544 + _444;
      float _546 = _545 + _528;
      float _547 = _546 + _532;
      float _548 = _547 + _535;
      _550 = 0.0f;
      _551 = 0.0f;
      _552 = 0.0f;
      _553 = _504;
      _554 = _526;
      _555 = _548;
   }
   else
   {
      _550 = _385;
      _551 = _386;
      _552 = _387;
      _553 = _388;
      _554 = _389;
      _555 = _390;
   }
   float _556 = _553 + _550;
   float _557 = _554 + _551;
   float _558 = _555 + _552;
   float _563 = c_mat_opacity * _37.w;
   float _564 = _563 * c_global_opacity;
   float _565 = _564 * c_opacity_scale;
   int _566 = c_bool & 16;
   bool _567 = (_566 == 0);
   if (!_567)
   {
      float _571 = c_tone_control.x * _556;
      float _572 = c_tone_control.x * _557;
      float _573 = c_tone_control.x * _558;
      float _575 = log2(_571);
      float _576 = log2(_572);
      float _577 = log2(_573);
      float _578 = _575 * c_tone_control.y;
      float _579 = _576 * c_tone_control.y;
      float _580 = _577 * c_tone_control.y;
      float _581 = exp2(_578);
      float _582 = exp2(_579);
      float _583 = exp2(_580);
      _631 = _581;
      _632 = _582;
      _633 = _583;
   }
   else
   {
      if (_392)
      {
         float _588 = c_tone_control.x * _556;
         float _589 = c_tone_control.x * _557;
         float _590 = c_tone_control.x * _558;
         float _591 = _588 * 2.0f;
         float _592 = _589 * 2.0f;
         float _593 = _590 * 2.0f;
         float _594 = _588 * 0.30000001192092896f;
         float _595 = _589 * 0.30000001192092896f;
         float _596 = _590 * 0.30000001192092896f;
         float _597 = _594 + 0.05000000074505806f;
         float _598 = _595 + 0.05000000074505806f;
         float _599 = _596 + 0.05000000074505806f;
         float _600 = _597 * _591;
         float _601 = _598 * _592;
         float _602 = _599 * _593;
         float _603 = _600 + 0.004000000189989805f;
         float _604 = _601 + 0.004000000189989805f;
         float _605 = _602 + 0.004000000189989805f;
         float _606 = _594 + 0.5f;
         float _607 = _595 + 0.5f;
         float _608 = _596 + 0.5f;
         float _609 = _606 * _591;
         float _610 = _607 * _592;
         float _611 = _608 * _593;
         float _612 = _609 + 0.06000000238418579f;
         float _613 = _610 + 0.06000000238418579f;
         float _614 = _611 + 0.06000000238418579f;
         float _615 = _603 / _612;
         float _616 = _604 / _613;
         float _617 = _605 / _614;
         float _618 = _615 + -0.06666666269302368f;
         float _619 = _616 + -0.06666666269302368f;
         float _620 = _617 + -0.06666666269302368f;
         float _621 = _618 * 1.3790643215179443f;
         float _622 = _619 * 1.3790643215179443f;
         float _623 = _620 * 1.3790643215179443f;
         float _624 = YKR_MaterialCurve(_621, _588);
         float _625 = YKR_MaterialCurve(_622, _589);
         float _626 = YKR_MaterialCurve(_623, _590);
         float _627 = sqrt(_624);
         float _628 = sqrt(_625);
         float _629 = sqrt(_626);
         _631 = _627;
         _632 = _628;
         _633 = _629;
      }
      else
      {
         _631 = _556;
         _632 = _557;
         _633 = _558;
      }
   }
   int _634 = c_bool & 4;
   bool _635 = (_634 == 0);
   if (_635)
   {
      float _637 = saturate(COLOR_1.x);
      float _642 = (c_fog_param[0].x) - _631;
      float _643 = (c_fog_param[0].y) - _632;
      float _644 = (c_fog_param[0].z) - _633;
      float _645 = _642 * _637;
      float _646 = _643 * _637;
      float _647 = _644 * _637;
      _649 = _645;
      _650 = _646;
      _651 = _647;
   }
   else
   {
      _649 = COLOR_1.x;
      _650 = COLOR_1.y;
      _651 = COLOR_1.z;
   }
   float _652 = _649 + _631;
   float _653 = _650 + _632;
   float _654 = _651 + _633;
   float _657 = _37.w - c_mat_punchthrough_ref;
   bool _658 = (_657 < 0.0f);
   if (_658)
      discard;
   float _664 = (c_ps_col_scl[0].x) * _652;
   float _665 = (c_ps_col_scl[0].y) * _653;
   float _666 = (c_ps_col_scl[0].z) * _654;
   float _667 = (c_ps_col_scl[0].w) * _565;
   SV_Target.x = _664;
   SV_Target.y = _665;
   SV_Target.z = _666;
   SV_Target.w = _667;
   int _670 = c_bool & 8;
   bool _671 = (_670 != 0);
   float _672 = select(_671, _652, _664);
   float _673 = select(_671, _653, _665);
   float _674 = select(_671, _654, _666);
   float _675 = select(_671, _565, _667);
   SV_Target.x = _672;
   SV_Target.y = _673;
   SV_Target.z = _674;
   SV_Target.w = _675;
   return SV_Target;
}
