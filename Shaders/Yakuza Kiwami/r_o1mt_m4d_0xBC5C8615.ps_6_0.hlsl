// Yakuza Kiwami material r_o1mt_m4d (0xBC5C8615, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s5_s : register(s5);

SamplerState s7_s : register(s7);

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
   float4 _47 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _52 = _47.x * _47.x;
   float _53 = _47.y * _47.y;
   float _54 = _47.z * _47.z;
   float _58 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _59 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _60 = s5_t.Sample(s5_s, float2(_58, _59));
   float4 _63 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _66 = _60.x * 2.0f;
   float _67 = _60.y * 2.0f;
   float _68 = _66 + -1.0f;
   float _69 = _67 + -1.0f;
   float _70 = dot(float2(_68, _69), float2(_68, _69));
   float _71 = 1.0f - _70;
   float _72 = saturate(_71);
   float _73 = sqrt(_72);
   float _74 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _75 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _76 = _74 - _75;
   float _77 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _78 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _79 = _77 - _78;
   float _80 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _81 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _82 = _80 - _81;
   float _83 = _68 * TEXCOORD_6.x;
   float _84 = _68 * TEXCOORD_6.y;
   float _85 = _68 * TEXCOORD_6.z;
   float _86 = _69 * TEXCOORD_6.w;
   float _87 = _86 * _76;
   float _88 = _86 * _79;
   float _89 = _86 * _82;
   float _90 = _87 + _83;
   float _91 = _88 + _84;
   float _92 = _89 + _85;
   float _93 = _73 * TEXCOORD_2.x;
   float _94 = _73 * TEXCOORD_2.y;
   float _95 = _73 * TEXCOORD_2.z;
   float _96 = _90 + _93;
   float _97 = _91 + _94;
   float _98 = _92 + _95;
   float _99 = dot(float3(_96, _97, _98), float3(_96, _97, _98));
   float _100 = rsqrt(_99);
   float _101 = _96 * _100;
   float _102 = _97 * _100;
   float _103 = _98 * _100;
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
   float _202;
   float _203;
   float _204;
   float _205;
   float _219;
   float _220;
   float _266;
   float _267;
   float _268;
   float _303;
   float _304;
   float _305;
   float _306;
   float _307;
   float _308;
   float _462;
   float _463;
   float _464;
   float _465;
   float _466;
   float _467;
   float _639;
   float _640;
   float _641;
   float _642;
   float _643;
   float _644;
   float _719;
   float _720;
   float _721;
   float _737;
   float _738;
   float _739;
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
   float _154 = (c_sdw_param[1].x) * _63.y;
   float _156 = _154 + (c_sdw_param[1].y);
   float _157 = _156 * _63.x;
   float _158 = 1.0f - (c_light_col[5].x);
   float _159 = 1.0f - (c_light_col[5].y);
   float _160 = 1.0f - (c_light_col[5].z);
   float _161 = _157 * _158;
   float _162 = _157 * _159;
   float _163 = _157 * _160;
   float _164 = _161 + (c_light_col[5].x);
   float _165 = _162 + (c_light_col[5].y);
   float _166 = _163 + (c_light_col[5].z);
   float _170 = c_rt_size.z * SV_Position.x;
   float _171 = c_rt_size.w * SV_Position.y;
   float4 _172 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_170, _171)));
   int _179 = c_bool & 16384;
   bool _180 = (_179 == 0);
   if (!_180)
   {
      float _182 = _172.x * 2.0f;
      float _183 = _172.y * 2.0f;
      float _184 = _172.z * 2.0f;
      float _185 = _172.w * 2.0f;
      _202 = _182;
      _203 = _183;
      _204 = _184;
      _205 = _185;
   }
   else
   {
      int _187 = c_bool & 8192;
      bool _188 = (_187 == 0);
      if (!_188)
      {
         float _190 = _172.x * 2.0f;
         float _191 = _172.y * 2.0f;
         float _192 = _172.z * 2.0f;
         float _193 = _172.w * 2.0f;
         _202 = _190;
         _203 = _191;
         _204 = _192;
         _205 = _193;
      }
      else
      {
         float _197 = _172.x / c_tone_control.z;
         float _198 = _172.y / c_tone_control.z;
         float _199 = _172.z / c_tone_control.z;
         float _200 = _172.w / c_tone_control.z;
         _202 = _197;
         _203 = _198;
         _204 = _199;
         _205 = _200;
      }
   }
   int _206 = c_bool & 8;
   bool _207 = (_206 == 0);
   if (!_207)
   {
      float _212 = (c_sdw_param[0].x) * _146;
      float _213 = (c_sdw_param[0].z) * _147;
      float _216 = _212 + (c_sdw_param[0].y);
      float _217 = _213 + (c_sdw_param[0].w);
      _219 = _216;
      _220 = _217;
   }
   else
   {
      _219 = _146;
      _220 = _147;
   }
   float _221 = _220 * _219;
   float _226 = 1.0f - (c_light_col[5].x);
   float _227 = 1.0f - (c_light_col[5].y);
   float _228 = 1.0f - (c_light_col[5].z);
   float _229 = _226 * _221;
   float _230 = _227 * _221;
   float _231 = _228 * _221;
   float _232 = _229 + (c_light_col[5].x);
   float _233 = _230 + (c_light_col[5].y);
   float _234 = _231 + (c_light_col[5].z);
   float _235 = _134 * COLOR.x;
   float _236 = _235 * _164;
   float _237 = _134 * COLOR.y;
   float _238 = _237 * _165;
   float _239 = _134 * COLOR.z;
   float _240 = _239 * _166;
   float _241 = _232 * _164;
   float _242 = _233 * _165;
   float _243 = _234 * _166;
   if (!_207)
   {
      float _247 = (c_sdw_param[2].x) * _232;
      float _248 = (c_sdw_param[2].x) * _233;
      float _249 = (c_sdw_param[2].x) * _234;
      float _251 = _247 + (c_sdw_param[2].y);
      float _252 = _248 + (c_sdw_param[2].y);
      float _253 = _249 + (c_sdw_param[2].y);
      float _255 = (c_sdw_param[2].z) * _164;
      float _256 = (c_sdw_param[2].z) * _165;
      float _257 = (c_sdw_param[2].z) * _166;
      float _259 = _255 + (c_sdw_param[2].w);
      float _260 = _256 + (c_sdw_param[2].w);
      float _261 = _257 + (c_sdw_param[2].w);
      float _262 = _259 * _251;
      float _263 = _260 * _252;
      float _264 = _261 * _253;
      _266 = _262;
      _267 = _263;
      _268 = _264;
   }
   else
   {
      _266 = _241;
      _267 = _242;
      _268 = _243;
   }
   float _269 = _134 * _63.x;
   float _270 = _266 * _269;
   float _271 = _267 * _269;
   float _272 = _268 * _269;
   float _283 = c_mat_emissive.x * _52;
   float _284 = _283 * c_emissive_scale;
   float _285 = c_mat_emissive.y * _53;
   float _286 = _285 * c_emissive_scale;
   float _287 = c_mat_emissive.z * _54;
   float _288 = _287 * c_emissive_scale;
   int _289 = c_bool & 2048;
   bool _290 = (_289 == 0);
   if (_290)
   {
      float _292 = _220 * _202;
      float _293 = _220 * _203;
      float _294 = _220 * _204;
      float _295 = _292 + TEXCOORD_4.x;
      float _296 = _293 + TEXCOORD_4.y;
      float _297 = _294 + TEXCOORD_4.z;
      float _298 = _220 * _205;
      float _299 = _298 + TEXCOORD_5.x;
      float _300 = _298 + TEXCOORD_5.y;
      float _301 = _298 + TEXCOORD_5.z;
      _303 = _299;
      _304 = _300;
      _305 = _301;
      _306 = _295;
      _307 = _296;
      _308 = _297;
   }
   else
   {
      _303 = TEXCOORD_5.x;
      _304 = TEXCOORD_5.y;
      _305 = TEXCOORD_5.z;
      _306 = TEXCOORD_4.x;
      _307 = TEXCOORD_4.y;
      _308 = TEXCOORD_4.z;
   }
   float _313 = -0.0f - (c_light_vec[1].x);
   float _314 = -0.0f - (c_light_vec[1].y);
   float _315 = -0.0f - (c_light_vec[1].z);
   float _316 = dot(float3(_101, _102, _103), float3(_313, _314, _315));
   float _317 = saturate(_316);
   float _322 = saturate(_116);
   float _323 = (c_light_col[0].x) * _322;
   float _324 = (c_light_col[0].y) * _322;
   float _325 = (c_light_col[0].z) * _322;
   float _330 = (c_light_col[1].x) * _317;
   float _331 = (c_light_col[1].y) * _317;
   float _332 = (c_light_col[1].z) * _317;
   float _341 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _342 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _343 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _344 = _323 * _232;
   float _345 = _324 * _233;
   float _346 = _325 * _234;
   float _347 = _330 * _220;
   float _348 = _331 * _220;
   float _349 = _332 * _220;
   float _350 = _347 + _344;
   float _351 = _348 + _345;
   float _352 = _349 + _346;
   float _353 = _341 * _350;
   float _354 = _342 * _351;
   float _355 = _343 * _352;
   float _356 = _284 + c_scene_ambient.x;
   float _357 = _356 + _353;
   float _358 = _286 + c_scene_ambient.y;
   float _359 = _358 + _354;
   float _360 = _288 + c_scene_ambient.z;
   float _361 = _360 + _355;
   float _362 = _357 * _236;
   float _363 = _359 * _238;
   float _364 = _361 * _240;
   float _365 = _306 * _270;
   float _366 = _307 * _271;
   float _367 = _308 * _272;
   float _368 = _362 + _365;
   float _369 = _363 + _366;
   float _370 = _364 + _367;
   float _375 = (c_light_vec[0].x) + _106;
   float _376 = (c_light_vec[0].y) + _107;
   float _377 = (c_light_vec[0].z) + _108;
   float _378 = dot(float3(_375, _376, _377), float3(_375, _376, _377));
   float _379 = rsqrt(_378);
   float _380 = _375 * _379;
   float _381 = -0.0f - _380;
   float _382 = _376 * _379;
   float _383 = -0.0f - _382;
   float _384 = _377 * _379;
   float _385 = -0.0f - _384;
   float _386 = dot(float3(_381, _383, _385), float3(_101, _102, _103));
   float _387 = saturate(_386);
   float _388 = _107 + -0.30000001192092896f;
   float _389 = dot(float3(_106, _388, _108), float3(_106, _388, _108));
   float _390 = rsqrt(_389);
   float _391 = _106 * _390;
   float _392 = -0.0f - _391;
   float _393 = _388 * _390;
   float _394 = -0.0f - _393;
   float _395 = _108 * _390;
   float _396 = -0.0f - _395;
   float _397 = dot(float3(_392, _394, _396), float3(_101, _102, _103));
   float _398 = saturate(_397);
   float _401 = log2(_387);
   float _402 = log2(_398);
   float _403 = _401 * c_mat_power;
   float _404 = _402 * c_mat_power;
   float _405 = exp2(_403);
   float _406 = exp2(_404);
   float _409 = _405 * (c_light_col[0].x);
   float _410 = _405 * (c_light_col[0].y);
   float _411 = _405 * (c_light_col[0].z);
   float _412 = _406 * 2.0f;
   float _413 = _412 * (c_light_vec[5].x);
   float _414 = _413 * (c_light_col[1].x);
   float _415 = _413 * (c_light_col[1].y);
   float _416 = _413 * (c_light_col[1].z);
   float _417 = _409 * _232;
   float _418 = _410 * _233;
   float _419 = _411 * _234;
   float _420 = _414 * _317;
   float _421 = _415 * _317;
   float _422 = _416 * _317;
   float _423 = _420 + _417;
   float _424 = _421 + _418;
   float _425 = _422 + _419;
   float _432 = c_mat_specular.x * c_specular_scale.x;
   float _433 = _432 * _236;
   float _434 = _433 * _423;
   float _435 = c_mat_specular.y * c_specular_scale.y;
   float _436 = _435 * _238;
   float _437 = _436 * _424;
   float _438 = c_mat_specular.z * c_specular_scale.z;
   float _439 = _438 * _240;
   float _440 = _439 * _425;
   float _441 = _303 * _270;
   float _442 = _441 * _406;
   float _443 = _304 * _271;
   float _444 = _443 * _406;
   float _445 = _305 * _272;
   float _446 = _445 * _406;
   float _447 = _434 + _442;
   float _448 = _437 + _444;
   float _449 = _440 + _446;
   if (!_290)
   {
      float _451 = _220 * _202;
      float _452 = _220 * _203;
      float _453 = _220 * _204;
      float _454 = _368 + _451;
      float _455 = _369 + _452;
      float _456 = _370 + _453;
      float _457 = _220 * _205;
      float _458 = _447 + _457;
      float _459 = _448 + _457;
      float _460 = _449 + _457;
      _462 = _454;
      _463 = _455;
      _464 = _456;
      _465 = _458;
      _466 = _459;
      _467 = _460;
   }
   else
   {
      _462 = _368;
      _463 = _369;
      _464 = _370;
      _465 = _447;
      _466 = _448;
      _467 = _449;
   }
   int _468 = c_bool & 128;
   bool _469 = (_468 != 0);
   float _470 = select(_469, 0.0f, _465);
   float _471 = select(_469, 0.0f, _466);
   float _472 = select(_469, 0.0f, _467);
   float _473 = _462 * _52;
   float _474 = _463 * _53;
   float _475 = _464 * _54;
   float _476 = _470 * _47.w;
   float _477 = _471 * _47.w;
   float _478 = _472 * _47.w;
   int _479 = c_bool & 32768;
   bool _480 = (_479 == 0);
   if (!_480)
   {
      float _484 = (c_ps_user[11].x) * _52;
      float _485 = (c_ps_user[11].x) * _53;
      float _486 = (c_ps_user[11].x) * _54;
      float _488 = _101 + 1.0f;
      float _489 = _102 + 1.0f;
      float _490 = _103 + 1.0f;
      float _491 = _488 * 0.5f;
      float _492 = _489 * 0.5f;
      float _493 = _490 * 0.5f;
      float _494 = saturate(_491);
      float _495 = saturate(_492);
      float _496 = saturate(_493);
      float _497 = (c_ps_user[11].y) * _494;
      float _498 = _495 * (c_ps_user[11].y);
      float _499 = _496 * (c_ps_user[11].y);
      float _501 = (c_ps_user[11].z) * _47.w;
      float _503 = c_mat_power * 0.009999999776482582f;
      float _504 = _503 * (c_ps_user[11].w);
      float _507 = (c_ps_user[12].x) * _232;
      float _508 = (c_ps_user[12].x) * _233;
      float _509 = (c_ps_user[12].x) * _234;
      float _511 = (c_ps_user[12].y) * _236;
      float _512 = (c_ps_user[12].y) * _238;
      float _513 = (c_ps_user[12].y) * _240;
      float _515 = (c_ps_user[12].z) * _270;
      float _516 = (c_ps_user[12].z) * _271;
      float _517 = (c_ps_user[12].z) * _272;
      float _523 = -0.0f - _106;
      float _524 = -0.0f - _107;
      float _525 = -0.0f - _108;
      float _526 = dot(float3(_523, _524, _525), float3(_101, _102, _103));
      float _527 = 1.0f - _526;
      float _528 = _527 * _527;
      float _529 = _527 * 0.9722222089767456f;
      float _530 = _528 * _528;
      float _531 = _530 * _529;
      float _532 = _531 + 0.02777777425944805f;
      float _533 = _532 * (c_ps_user[13].z);
      float _535 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _536 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _537 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _540 = (c_ps_user[14].x) * _284;
      float _541 = (c_ps_user[14].x) * _286;
      float _542 = (c_ps_user[14].x) * _288;
      float _544 = (c_ps_user[14].y) * _323;
      float _545 = (c_ps_user[14].y) * _324;
      float _546 = (c_ps_user[14].y) * _325;
      float _548 = (c_ps_user[14].z) * _330;
      float _549 = (c_ps_user[14].z) * _331;
      float _550 = (c_ps_user[14].z) * _332;
      float _552 = (c_ps_user[14].w) * _306;
      float _553 = (c_ps_user[14].w) * _307;
      float _554 = (c_ps_user[14].w) * _308;
      float _557 = (c_ps_user[15].x) * _409;
      float _558 = (c_ps_user[15].x) * _410;
      float _559 = (c_ps_user[15].x) * _411;
      float _561 = _432 * _414;
      float _562 = _561 * (c_ps_user[15].y);
      float _563 = _435 * _415;
      float _564 = _563 * (c_ps_user[15].y);
      float _565 = _438 * _416;
      float _566 = _565 * (c_ps_user[15].y);
      float _568 = (c_ps_user[15].z) * _303;
      float _569 = (c_ps_user[15].z) * _304;
      float _570 = (c_ps_user[15].z) * _305;
      float _571 = _562 + _568;
      float _572 = _571 * _406;
      float _573 = _544 + (c_ps_user[13].x);
      float _574 = _573 + _548;
      float _575 = c_diffuse_scale.x * _574;
      float _576 = _575 * c_mat_diffuse.x;
      float _577 = (c_ps_user[13].y) + _501;
      float _578 = _577 + _557;
      float _579 = c_mat_specular.x * _578;
      float _580 = _579 * c_specular_scale.x;
      float _581 = _497 + _484;
      float _582 = _581 + _504;
      float _583 = _582 + (c_ps_user[12].w);
      float _584 = _583 + _507;
      float _585 = _584 + _511;
      float _586 = _585 + _515;
      float _587 = _586 + _535;
      float _588 = _587 + _540;
      float _589 = _588 + _552;
      float _590 = _589 + _533;
      float _591 = _590 + _572;
      float _592 = _591 + _576;
      float _593 = _592 + _580;
      float _594 = _564 + _569;
      float _595 = _594 * _406;
      float _596 = _545 + (c_ps_user[13].x);
      float _597 = _596 + _549;
      float _598 = c_diffuse_scale.y * _597;
      float _599 = _598 * c_mat_diffuse.y;
      float _600 = _577 + _558;
      float _601 = c_mat_specular.y * _600;
      float _602 = _601 * c_specular_scale.y;
      float _603 = _498 + _485;
      float _604 = _603 + _504;
      float _605 = _604 + (c_ps_user[12].w);
      float _606 = _605 + _508;
      float _607 = _606 + _512;
      float _608 = _607 + _516;
      float _609 = _608 + _536;
      float _610 = _609 + _541;
      float _611 = _610 + _553;
      float _612 = _611 + _533;
      float _613 = _612 + _595;
      float _614 = _613 + _599;
      float _615 = _614 + _602;
      float _616 = _566 + _570;
      float _617 = _616 * _406;
      float _618 = _546 + (c_ps_user[13].x);
      float _619 = _618 + _550;
      float _620 = c_diffuse_scale.z * _619;
      float _621 = _620 * c_mat_diffuse.z;
      float _622 = _577 + _559;
      float _623 = c_mat_specular.z * _622;
      float _624 = _623 * c_specular_scale.z;
      float _625 = _504 + _486;
      float _626 = _625 + _499;
      float _627 = _626 + (c_ps_user[12].w);
      float _628 = _627 + _509;
      float _629 = _628 + _513;
      float _630 = _629 + _517;
      float _631 = _630 + _537;
      float _632 = _631 + _542;
      float _633 = _632 + _554;
      float _634 = _633 + _533;
      float _635 = _634 + _617;
      float _636 = _635 + _621;
      float _637 = _636 + _624;
      _639 = _593;
      _640 = _615;
      _641 = _637;
      _642 = 0.0f;
      _643 = 0.0f;
      _644 = 0.0f;
   }
   else
   {
      _639 = _473;
      _640 = _474;
      _641 = _475;
      _642 = _476;
      _643 = _477;
      _644 = _478;
   }
   float _645 = _642 + _639;
   float _646 = _643 + _640;
   float _647 = _644 + _641;
   float _652 = c_global_opacity * c_mat_opacity;
   float _653 = _652 * c_opacity_scale;
   int _654 = c_bool & 16;
   bool _655 = (_654 == 0);
   if (!_655)
   {
      float _659 = c_tone_control.x * _645;
      float _660 = c_tone_control.x * _646;
      float _661 = c_tone_control.x * _647;
      float _663 = log2(_659);
      float _664 = log2(_660);
      float _665 = log2(_661);
      float _666 = _663 * c_tone_control.y;
      float _667 = _664 * c_tone_control.y;
      float _668 = _665 * c_tone_control.y;
      float _669 = exp2(_666);
      float _670 = exp2(_667);
      float _671 = exp2(_668);
      _719 = _669;
      _720 = _670;
      _721 = _671;
   }
   else
   {
      if (_480)
      {
         float _676 = c_tone_control.x * _645;
         float _677 = c_tone_control.x * _646;
         float _678 = c_tone_control.x * _647;
         float _679 = _676 * 2.0f;
         float _680 = _677 * 2.0f;
         float _681 = _678 * 2.0f;
         float _682 = _676 * 0.30000001192092896f;
         float _683 = _677 * 0.30000001192092896f;
         float _684 = _678 * 0.30000001192092896f;
         float _685 = _682 + 0.05000000074505806f;
         float _686 = _683 + 0.05000000074505806f;
         float _687 = _684 + 0.05000000074505806f;
         float _688 = _685 * _679;
         float _689 = _686 * _680;
         float _690 = _687 * _681;
         float _691 = _688 + 0.004000000189989805f;
         float _692 = _689 + 0.004000000189989805f;
         float _693 = _690 + 0.004000000189989805f;
         float _694 = _682 + 0.5f;
         float _695 = _683 + 0.5f;
         float _696 = _684 + 0.5f;
         float _697 = _694 * _679;
         float _698 = _695 * _680;
         float _699 = _696 * _681;
         float _700 = _697 + 0.06000000238418579f;
         float _701 = _698 + 0.06000000238418579f;
         float _702 = _699 + 0.06000000238418579f;
         float _703 = _691 / _700;
         float _704 = _692 / _701;
         float _705 = _693 / _702;
         float _706 = _703 + -0.06666666269302368f;
         float _707 = _704 + -0.06666666269302368f;
         float _708 = _705 + -0.06666666269302368f;
         float _709 = _706 * 1.3790643215179443f;
         float _710 = _707 * 1.3790643215179443f;
         float _711 = _708 * 1.3790643215179443f;
         float _712 = YKR_MaterialCurve(_709, _676);
         float _713 = YKR_MaterialCurve(_710, _677);
         float _714 = YKR_MaterialCurve(_711, _678);
         float _715 = sqrt(_712);
         float _716 = sqrt(_713);
         float _717 = sqrt(_714);
         _719 = _715;
         _720 = _716;
         _721 = _717;
      }
      else
      {
         _719 = _645;
         _720 = _646;
         _721 = _647;
      }
   }
   int _722 = c_bool & 4;
   bool _723 = (_722 == 0);
   if (_723)
   {
      float _725 = saturate(COLOR_1.x);
      float _730 = (c_fog_param[0].x) - _719;
      float _731 = (c_fog_param[0].y) - _720;
      float _732 = (c_fog_param[0].z) - _721;
      float _733 = _730 * _725;
      float _734 = _731 * _725;
      float _735 = _732 * _725;
      _737 = _733;
      _738 = _734;
      _739 = _735;
   }
   else
   {
      _737 = COLOR_1.x;
      _738 = COLOR_1.y;
      _739 = COLOR_1.z;
   }
   float _740 = _737 + _719;
   float _741 = _738 + _720;
   float _742 = _739 + _721;
   float _748 = (c_ps_col_scl[0].x) * _740;
   float _749 = (c_ps_col_scl[0].y) * _741;
   float _750 = (c_ps_col_scl[0].z) * _742;
   float _751 = (c_ps_col_scl[0].w) * _653;
   SV_Target.x = _748;
   SV_Target.y = _749;
   SV_Target.z = _750;
   SV_Target.w = _751;
   int _754 = c_bool & 8;
   bool _755 = (_754 != 0);
   float _756 = select(_755, _740, _748);
   float _757 = select(_755, _741, _749);
   float _758 = select(_755, _742, _750);
   float _759 = select(_755, _653, _751);
   SV_Target.x = _756;
   SV_Target.y = _757;
   SV_Target.z = _758;
   SV_Target.w = _759;
   return SV_Target;
}
