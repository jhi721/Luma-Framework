// Yakuza Kiwami material r_o1d[sign][ag] (0xF0525ED9, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _72;
   float _73;
   float _74;
   float _86;
   float _87;
   float _119;
   float _120;
   float _121;
   float _135;
   float _136;
   float _171;
   float _172;
   float _173;
   float _210;
   float _211;
   float _212;
   float _283;
   float _284;
   float _285;
   float _405;
   float _406;
   float _407;
   float _480;
   float _481;
   float _482;
   float _498;
   float _499;
   float _500;
   if (!_65)
   {
      float4 _67 = s13_t.Sample(s13_s, float2(_60, _61));
      _72 = _67.x;
      _73 = _67.y;
      _74 = _67.w;
   }
   else
   {
      _72 = 1.0f;
      _73 = 1.0f;
      _74 = 1.0f;
   }
   if (!_65)
   {
      float _79 = (c_sdw_param[0].x) * _72;
      float _80 = (c_sdw_param[0].z) * _73;
      float _83 = _79 + (c_sdw_param[0].y);
      float _84 = _80 + (c_sdw_param[0].w);
      _86 = _83;
      _87 = _84;
   }
   else
   {
      _86 = _72;
      _87 = _73;
   }
   float _91 = c_rt_size.z * SV_Position.x;
   float _92 = c_rt_size.w * SV_Position.y;
   float4 _93 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_91, _92)));
   int _99 = c_bool & 16384;
   bool _100 = (_99 == 0);
   if (!_100)
   {
      float _102 = _93.x * 2.0f;
      float _103 = _93.y * 2.0f;
      float _104 = _93.z * 2.0f;
      _119 = _102;
      _120 = _103;
      _121 = _104;
   }
   else
   {
      int _106 = c_bool & 8192;
      bool _107 = (_106 == 0);
      if (!_107)
      {
         float _109 = _93.x * 2.0f;
         float _110 = _93.y * 2.0f;
         float _111 = _93.z * 2.0f;
         _119 = _109;
         _120 = _110;
         _121 = _111;
      }
      else
      {
         float _115 = _93.x / c_tone_control.z;
         float _116 = _93.y / c_tone_control.z;
         float _117 = _93.z / c_tone_control.z;
         _119 = _115;
         _120 = _116;
         _121 = _117;
      }
   }
   int _122 = c_bool & 8;
   bool _123 = (_122 == 0);
   if (!_123)
   {
      float _128 = (c_sdw_param[0].x) * _86;
      float _129 = (c_sdw_param[0].z) * _87;
      float _132 = _128 + (c_sdw_param[0].y);
      float _133 = _129 + (c_sdw_param[0].w);
      _135 = _132;
      _136 = _133;
   }
   else
   {
      _135 = _86;
      _136 = _87;
   }
   float _137 = _136 * _135;
   float _142 = 1.0f - (c_light_col[5].x);
   float _143 = 1.0f - (c_light_col[5].y);
   float _144 = 1.0f - (c_light_col[5].z);
   float _145 = _142 * _137;
   float _146 = _143 * _137;
   float _147 = _144 * _137;
   float _148 = _145 + (c_light_col[5].x);
   float _149 = _146 + (c_light_col[5].y);
   float _150 = _147 + (c_light_col[5].z);
   float _151 = _74 * COLOR.x;
   float _152 = _74 * COLOR.y;
   float _153 = _74 * COLOR.z;
   if (!_123)
   {
      float _157 = (c_sdw_param[2].x) * _148;
      float _158 = (c_sdw_param[2].x) * _149;
      float _159 = (c_sdw_param[2].x) * _150;
      float _161 = _157 + (c_sdw_param[2].y);
      float _162 = _158 + (c_sdw_param[2].y);
      float _163 = _159 + (c_sdw_param[2].y);
      float _166 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _167 = _161 * _166;
      float _168 = _162 * _166;
      float _169 = _163 * _166;
      _171 = _167;
      _172 = _168;
      _173 = _169;
   }
   else
   {
      _171 = _148;
      _172 = _149;
      _173 = _150;
   }
   float _174 = _171 * _74;
   float _175 = _172 * _74;
   float _176 = _173 * _74;
   float _189 = (c_ps_user[0].x) * c_emissive_scale;
   float _190 = _189 * c_mat_emissive.x;
   float _191 = _189 * c_mat_emissive.y;
   float _192 = _189 * c_mat_emissive.z;
   float _194 = _190 + (c_ps_user[0].y);
   float _195 = _191 + (c_ps_user[0].y);
   float _196 = _192 + (c_ps_user[0].y);
   float _197 = _194 * _39;
   float _198 = _195 * _40;
   float _199 = _196 * _41;
   int _200 = c_bool & 2048;
   bool _201 = (_200 == 0);
   if (_201)
   {
      float _203 = _136 * _119;
      float _204 = _136 * _120;
      float _205 = _136 * _121;
      float _206 = _203 + TEXCOORD_4.x;
      float _207 = _204 + TEXCOORD_4.y;
      float _208 = _205 + TEXCOORD_4.z;
      _210 = _206;
      _211 = _207;
      _212 = _208;
   }
   else
   {
      _210 = TEXCOORD_4.x;
      _211 = TEXCOORD_4.y;
      _212 = TEXCOORD_4.z;
   }
   float _217 = -0.0f - (c_light_vec[1].x);
   float _218 = -0.0f - (c_light_vec[1].y);
   float _219 = -0.0f - (c_light_vec[1].z);
   float _220 = dot(float3(_44, _45, _46), float3(_217, _218, _219));
   float _221 = saturate(_220);
   float _226 = saturate(_56);
   float _227 = (c_light_col[0].x) * _226;
   float _228 = (c_light_col[0].y) * _226;
   float _229 = (c_light_col[0].z) * _226;
   float _234 = (c_light_col[1].x) * _221;
   float _235 = (c_light_col[1].y) * _221;
   float _236 = (c_light_col[1].z) * _221;
   float _245 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _246 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _247 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _248 = _227 * _148;
   float _249 = _228 * _149;
   float _250 = _229 * _150;
   float _251 = _234 * _136;
   float _252 = _235 * _136;
   float _253 = _236 * _136;
   float _254 = _251 + _248;
   float _255 = _252 + _249;
   float _256 = _253 + _250;
   float _257 = _245 * _254;
   float _258 = _246 * _255;
   float _259 = _247 * _256;
   float _260 = _197 + c_scene_ambient.x;
   float _261 = _260 + _257;
   float _262 = _198 + c_scene_ambient.y;
   float _263 = _262 + _258;
   float _264 = _199 + c_scene_ambient.z;
   float _265 = _264 + _259;
   float _266 = _261 * _151;
   float _267 = _263 * _152;
   float _268 = _265 * _153;
   float _269 = _210 * _174;
   float _270 = _211 * _175;
   float _271 = _212 * _176;
   float _272 = _266 + _269;
   float _273 = _267 + _270;
   float _274 = _268 + _271;
   if (!_201)
   {
      float _276 = _136 * _119;
      float _277 = _136 * _120;
      float _278 = _136 * _121;
      float _279 = _272 + _276;
      float _280 = _273 + _277;
      float _281 = _274 + _278;
      _283 = _279;
      _284 = _280;
      _285 = _281;
   }
   else
   {
      _283 = _272;
      _284 = _273;
      _285 = _274;
   }
   float _286 = _283 * _39;
   float _287 = _284 * _40;
   float _288 = _285 * _41;
   int _289 = c_bool & 32768;
   bool _290 = (_289 == 0);
   if (!_290)
   {
      float _294 = (c_ps_user[11].x) * _39;
      float _295 = (c_ps_user[11].x) * _40;
      float _296 = (c_ps_user[11].x) * _41;
      float _298 = _44 + 1.0f;
      float _299 = _45 + 1.0f;
      float _300 = _46 + 1.0f;
      float _301 = _298 * 0.5f;
      float _302 = _299 * 0.5f;
      float _303 = _300 * 0.5f;
      float _304 = saturate(_301);
      float _305 = saturate(_302);
      float _306 = saturate(_303);
      float _307 = (c_ps_user[11].y) * _304;
      float _308 = _305 * (c_ps_user[11].y);
      float _309 = _306 * (c_ps_user[11].y);
      float _312 = (c_ps_user[12].x) * _148;
      float _313 = (c_ps_user[12].x) * _149;
      float _314 = (c_ps_user[12].x) * _150;
      float _316 = (c_ps_user[12].y) * _151;
      float _317 = (c_ps_user[12].y) * _152;
      float _318 = (c_ps_user[12].y) * _153;
      float _320 = (c_ps_user[12].z) * _174;
      float _321 = (c_ps_user[12].z) * _175;
      float _322 = (c_ps_user[12].z) * _176;
      float _327 = TEXCOORD_3.x * _48;
      float _328 = -0.0f - _327;
      float _329 = TEXCOORD_3.y * _48;
      float _330 = -0.0f - _329;
      float _331 = TEXCOORD_3.z * _48;
      float _332 = -0.0f - _331;
      float _333 = dot(float3(_328, _330, _332), float3(_44, _45, _46));
      float _334 = 1.0f - _333;
      float _335 = _334 * _334;
      float _336 = _334 * 0.9722222089767456f;
      float _337 = _335 * _335;
      float _338 = _337 * _336;
      float _339 = _338 + 0.02777777425944805f;
      float _340 = _339 * (c_ps_user[13].z);
      float _342 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _343 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _344 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _347 = (c_ps_user[14].x) * _197;
      float _348 = (c_ps_user[14].x) * _198;
      float _349 = (c_ps_user[14].x) * _199;
      float _351 = (c_ps_user[14].y) * _227;
      float _352 = (c_ps_user[14].y) * _228;
      float _353 = (c_ps_user[14].y) * _229;
      float _355 = (c_ps_user[14].z) * _234;
      float _356 = (c_ps_user[14].z) * _235;
      float _357 = (c_ps_user[14].z) * _236;
      float _359 = (c_ps_user[14].w) * _210;
      float _360 = (c_ps_user[14].w) * _211;
      float _361 = (c_ps_user[14].w) * _212;
      float _362 = _351 + (c_ps_user[13].x);
      float _363 = _362 + _355;
      float _364 = c_diffuse_scale.x * _363;
      float _365 = _364 * c_mat_diffuse.x;
      float _366 = _307 + _294;
      float _367 = _366 + (c_ps_user[12].w);
      float _368 = _367 + _312;
      float _369 = _368 + _316;
      float _370 = _369 + _320;
      float _371 = _370 + _342;
      float _372 = _371 + _347;
      float _373 = _372 + _359;
      float _374 = _373 + _340;
      float _375 = _374 + _365;
      float _376 = _352 + (c_ps_user[13].x);
      float _377 = _376 + _356;
      float _378 = c_diffuse_scale.y * _377;
      float _379 = _378 * c_mat_diffuse.y;
      float _380 = _308 + _295;
      float _381 = _380 + (c_ps_user[12].w);
      float _382 = _381 + _313;
      float _383 = _382 + _317;
      float _384 = _383 + _321;
      float _385 = _384 + _343;
      float _386 = _385 + _348;
      float _387 = _386 + _360;
      float _388 = _387 + _340;
      float _389 = _388 + _379;
      float _390 = _353 + (c_ps_user[13].x);
      float _391 = _390 + _357;
      float _392 = c_diffuse_scale.z * _391;
      float _393 = _392 * c_mat_diffuse.z;
      float _394 = _309 + _296;
      float _395 = _394 + (c_ps_user[12].w);
      float _396 = _395 + _314;
      float _397 = _396 + _318;
      float _398 = _397 + _322;
      float _399 = _398 + _344;
      float _400 = _399 + _349;
      float _401 = _400 + _361;
      float _402 = _401 + _340;
      float _403 = _402 + _393;
      _405 = _375;
      _406 = _389;
      _407 = _403;
   }
   else
   {
      _405 = _286;
      _406 = _287;
      _407 = _288;
   }
   float _412 = c_mat_opacity * _34.w;
   float _413 = _412 * c_global_opacity;
   float _414 = _413 * c_opacity_scale;
   int _415 = c_bool & 16;
   bool _416 = (_415 == 0);
   if (!_416)
   {
      float _420 = c_tone_control.x * _405;
      float _421 = c_tone_control.x * _406;
      float _422 = c_tone_control.x * _407;
      float _424 = log2(_420);
      float _425 = log2(_421);
      float _426 = log2(_422);
      float _427 = _424 * c_tone_control.y;
      float _428 = _425 * c_tone_control.y;
      float _429 = _426 * c_tone_control.y;
      float _430 = exp2(_427);
      float _431 = exp2(_428);
      float _432 = exp2(_429);
      _480 = _430;
      _481 = _431;
      _482 = _432;
   }
   else
   {
      if (_290)
      {
         float _437 = c_tone_control.x * _405;
         float _438 = c_tone_control.x * _406;
         float _439 = c_tone_control.x * _407;
         float _440 = _437 * 2.0f;
         float _441 = _438 * 2.0f;
         float _442 = _439 * 2.0f;
         float _443 = _437 * 0.30000001192092896f;
         float _444 = _438 * 0.30000001192092896f;
         float _445 = _439 * 0.30000001192092896f;
         float _446 = _443 + 0.05000000074505806f;
         float _447 = _444 + 0.05000000074505806f;
         float _448 = _445 + 0.05000000074505806f;
         float _449 = _446 * _440;
         float _450 = _447 * _441;
         float _451 = _448 * _442;
         float _452 = _449 + 0.004000000189989805f;
         float _453 = _450 + 0.004000000189989805f;
         float _454 = _451 + 0.004000000189989805f;
         float _455 = _443 + 0.5f;
         float _456 = _444 + 0.5f;
         float _457 = _445 + 0.5f;
         float _458 = _455 * _440;
         float _459 = _456 * _441;
         float _460 = _457 * _442;
         float _461 = _458 + 0.06000000238418579f;
         float _462 = _459 + 0.06000000238418579f;
         float _463 = _460 + 0.06000000238418579f;
         float _464 = _452 / _461;
         float _465 = _453 / _462;
         float _466 = _454 / _463;
         float _467 = _464 + -0.06666666269302368f;
         float _468 = _465 + -0.06666666269302368f;
         float _469 = _466 + -0.06666666269302368f;
         float _470 = _467 * 1.3790643215179443f;
         float _471 = _468 * 1.3790643215179443f;
         float _472 = _469 * 1.3790643215179443f;
         float _473 = YKR_MaterialCurve(_470, _437);
         float _474 = YKR_MaterialCurve(_471, _438);
         float _475 = YKR_MaterialCurve(_472, _439);
         float _476 = sqrt(_473);
         float _477 = sqrt(_474);
         float _478 = sqrt(_475);
         _480 = _476;
         _481 = _477;
         _482 = _478;
      }
      else
      {
         _480 = _405;
         _481 = _406;
         _482 = _407;
      }
   }
   int _483 = c_bool & 4;
   bool _484 = (_483 == 0);
   if (_484)
   {
      float _486 = saturate(COLOR_1.x);
      float _491 = (c_fog_param[0].x) - _480;
      float _492 = (c_fog_param[0].y) - _481;
      float _493 = (c_fog_param[0].z) - _482;
      float _494 = _491 * _486;
      float _495 = _492 * _486;
      float _496 = _493 * _486;
      _498 = _494;
      _499 = _495;
      _500 = _496;
   }
   else
   {
      _498 = COLOR_1.x;
      _499 = COLOR_1.y;
      _500 = COLOR_1.z;
   }
   float _501 = _498 + _480;
   float _502 = _499 + _481;
   float _503 = _500 + _482;
   float _509 = (c_ps_col_scl[0].x) * _501;
   float _510 = (c_ps_col_scl[0].y) * _502;
   float _511 = (c_ps_col_scl[0].z) * _503;
   float _512 = (c_ps_col_scl[0].w) * _414;
   SV_Target.x = _509;
   SV_Target.y = _510;
   SV_Target.z = _511;
   SV_Target.w = _512;
   int _515 = c_bool & 8;
   bool _516 = (_515 != 0);
   float _517 = select(_516, _501, _509);
   float _518 = select(_516, _502, _510);
   float _519 = select(_516, _503, _511);
   float _520 = select(_516, _414, _512);
   SV_Target.x = _517;
   SV_Target.y = _518;
   SV_Target.z = _519;
   SV_Target.w = _520;
   return SV_Target;
}
