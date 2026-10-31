// Yakuza Kiwami material r_p1dut[ref] (0xA762FB04, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
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
   float _67 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _68 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _69 = s5_t.Sample(s5_s, float2(_67, _68));
   float _72 = _69.x * 2.0f;
   float _73 = _69.y * 2.0f;
   float _74 = _72 + -1.0f;
   float _75 = _73 + -1.0f;
   float _76 = dot(float2(_74, _75), float2(_74, _75));
   float _77 = 1.0f - _76;
   float _78 = saturate(_77);
   float _79 = sqrt(_78);
   float _80 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _81 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _82 = _80 - _81;
   float _83 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _84 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _87 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _88 = _86 - _87;
   float _89 = _74 * TEXCOORD_6.x;
   float _90 = _74 * TEXCOORD_6.y;
   float _91 = _74 * TEXCOORD_6.z;
   float _92 = _75 * TEXCOORD_6.w;
   float _93 = _92 * _82;
   float _94 = _92 * _85;
   float _95 = _92 * _88;
   float _96 = _79 * TEXCOORD_2.x;
   float _97 = _79 * TEXCOORD_2.y;
   float _98 = _79 * TEXCOORD_2.z;
   float _99 = _96 + _89;
   float _100 = _99 + _93;
   float _101 = _97 + _90;
   float _102 = _101 + _94;
   float _103 = _98 + _91;
   float _104 = _103 + _95;
   float _105 = dot(float3(_100, _102, _104), float3(_100, _102, _104));
   float _106 = rsqrt(_105);
   float _107 = _100 * _106;
   float _108 = _102 * _106;
   float _109 = _104 * _106;
   float _110 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _111 = rsqrt(_110);
   float _112 = _111 * TEXCOORD_3.x;
   float _113 = _111 * TEXCOORD_3.y;
   float _114 = _111 * TEXCOORD_3.z;
   float _119 = -0.0f - (c_light_vec[0].x);
   float _120 = -0.0f - (c_light_vec[0].y);
   float _121 = -0.0f - (c_light_vec[0].z);
   float _122 = dot(float3(_107, _108, _109), float3(_119, _120, _121));
   float _126 = c_rt_size.z * SV_Position.x;
   float _127 = c_rt_size.w * SV_Position.y;
   int _130 = c_bool & 8;
   bool _131 = (_130 == 0);
   float _138;
   float _139;
   float _140;
   float _152;
   float _153;
   float _189;
   float _190;
   float _191;
   float _192;
   float _206;
   float _207;
   float _242;
   float _243;
   float _244;
   float _278;
   float _279;
   float _280;
   float _281;
   float _282;
   float _283;
   float _437;
   float _438;
   float _439;
   float _440;
   float _441;
   float _442;
   float _614;
   float _615;
   float _616;
   float _617;
   float _618;
   float _619;
   float _738;
   float _739;
   float _740;
   float _756;
   float _757;
   float _758;
   if (!_131)
   {
      float4 _133 = s13_t.Sample(s13_s, float2(_126, _127));
      _138 = _133.x;
      _139 = _133.y;
      _140 = _133.w;
   }
   else
   {
      _138 = 1.0f;
      _139 = 1.0f;
      _140 = 1.0f;
   }
   if (!_131)
   {
      float _145 = (c_sdw_param[0].x) * _138;
      float _146 = (c_sdw_param[0].z) * _139;
      float _149 = _145 + (c_sdw_param[0].y);
      float _150 = _146 + (c_sdw_param[0].w);
      _152 = _149;
      _153 = _150;
   }
   else
   {
      _152 = _138;
      _153 = _139;
   }
   float _157 = c_rt_size.z * SV_Position.x;
   float _158 = c_rt_size.w * SV_Position.y;
   float4 _159 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_157, _158)));
   int _166 = c_bool & 16384;
   bool _167 = (_166 == 0);
   if (!_167)
   {
      float _169 = _159.x * 2.0f;
      float _170 = _159.y * 2.0f;
      float _171 = _159.z * 2.0f;
      float _172 = _159.w * 2.0f;
      _189 = _169;
      _190 = _170;
      _191 = _171;
      _192 = _172;
   }
   else
   {
      int _174 = c_bool & 8192;
      bool _175 = (_174 == 0);
      if (!_175)
      {
         float _177 = _159.x * 2.0f;
         float _178 = _159.y * 2.0f;
         float _179 = _159.z * 2.0f;
         float _180 = _159.w * 2.0f;
         _189 = _177;
         _190 = _178;
         _191 = _179;
         _192 = _180;
      }
      else
      {
         float _184 = _159.x / c_tone_control.z;
         float _185 = _159.y / c_tone_control.z;
         float _186 = _159.z / c_tone_control.z;
         float _187 = _159.w / c_tone_control.z;
         _189 = _184;
         _190 = _185;
         _191 = _186;
         _192 = _187;
      }
   }
   int _193 = c_bool & 8;
   bool _194 = (_193 == 0);
   if (!_194)
   {
      float _199 = (c_sdw_param[0].x) * _152;
      float _200 = (c_sdw_param[0].z) * _153;
      float _203 = _199 + (c_sdw_param[0].y);
      float _204 = _200 + (c_sdw_param[0].w);
      _206 = _203;
      _207 = _204;
   }
   else
   {
      _206 = _152;
      _207 = _153;
   }
   float _208 = _207 * _206;
   float _213 = 1.0f - (c_light_col[5].x);
   float _214 = 1.0f - (c_light_col[5].y);
   float _215 = 1.0f - (c_light_col[5].z);
   float _216 = _213 * _208;
   float _217 = _214 * _208;
   float _218 = _215 * _208;
   float _219 = _216 + (c_light_col[5].x);
   float _220 = _217 + (c_light_col[5].y);
   float _221 = _218 + (c_light_col[5].z);
   float _222 = _140 * COLOR.x;
   float _223 = _140 * COLOR.y;
   float _224 = _140 * COLOR.z;
   if (!_194)
   {
      float _228 = (c_sdw_param[2].x) * _219;
      float _229 = (c_sdw_param[2].x) * _220;
      float _230 = (c_sdw_param[2].x) * _221;
      float _232 = _228 + (c_sdw_param[2].y);
      float _233 = _229 + (c_sdw_param[2].y);
      float _234 = _230 + (c_sdw_param[2].y);
      float _237 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _238 = _232 * _237;
      float _239 = _233 * _237;
      float _240 = _234 * _237;
      _242 = _238;
      _243 = _239;
      _244 = _240;
   }
   else
   {
      _242 = _219;
      _243 = _220;
      _244 = _221;
   }
   float _245 = _242 * _140;
   float _246 = _243 * _140;
   float _247 = _244 * _140;
   float _258 = c_mat_emissive.x * _53;
   float _259 = _258 * c_emissive_scale;
   float _260 = c_mat_emissive.y * _54;
   float _261 = _260 * c_emissive_scale;
   float _262 = c_mat_emissive.z * _55;
   float _263 = _262 * c_emissive_scale;
   int _264 = c_bool & 2048;
   bool _265 = (_264 == 0);
   if (_265)
   {
      float _267 = _207 * _189;
      float _268 = _207 * _190;
      float _269 = _207 * _191;
      float _270 = _267 + TEXCOORD_4.x;
      float _271 = _268 + TEXCOORD_4.y;
      float _272 = _269 + TEXCOORD_4.z;
      float _273 = _207 * _192;
      float _274 = _273 + TEXCOORD_5.x;
      float _275 = _273 + TEXCOORD_5.y;
      float _276 = _273 + TEXCOORD_5.z;
      _278 = _270;
      _279 = _271;
      _280 = _272;
      _281 = _274;
      _282 = _275;
      _283 = _276;
   }
   else
   {
      _278 = TEXCOORD_4.x;
      _279 = TEXCOORD_4.y;
      _280 = TEXCOORD_4.z;
      _281 = TEXCOORD_5.x;
      _282 = TEXCOORD_5.y;
      _283 = TEXCOORD_5.z;
   }
   float _288 = -0.0f - (c_light_vec[1].x);
   float _289 = -0.0f - (c_light_vec[1].y);
   float _290 = -0.0f - (c_light_vec[1].z);
   float _291 = dot(float3(_107, _108, _109), float3(_288, _289, _290));
   float _292 = saturate(_291);
   float _297 = saturate(_122);
   float _298 = (c_light_col[0].x) * _297;
   float _299 = (c_light_col[0].y) * _297;
   float _300 = (c_light_col[0].z) * _297;
   float _305 = (c_light_col[1].x) * _292;
   float _306 = (c_light_col[1].y) * _292;
   float _307 = (c_light_col[1].z) * _292;
   float _316 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _317 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _318 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _319 = _298 * _219;
   float _320 = _299 * _220;
   float _321 = _300 * _221;
   float _322 = _305 * _207;
   float _323 = _306 * _207;
   float _324 = _307 * _207;
   float _325 = _322 + _319;
   float _326 = _323 + _320;
   float _327 = _324 + _321;
   float _328 = _316 * _325;
   float _329 = _317 * _326;
   float _330 = _318 * _327;
   float _331 = _259 + c_scene_ambient.x;
   float _332 = _331 + _328;
   float _333 = _261 + c_scene_ambient.y;
   float _334 = _333 + _329;
   float _335 = _263 + c_scene_ambient.z;
   float _336 = _335 + _330;
   float _337 = _332 * _222;
   float _338 = _334 * _223;
   float _339 = _336 * _224;
   float _340 = _278 * _245;
   float _341 = _279 * _246;
   float _342 = _280 * _247;
   float _343 = _337 + _340;
   float _344 = _338 + _341;
   float _345 = _339 + _342;
   float _350 = (c_light_vec[0].x) + _112;
   float _351 = (c_light_vec[0].y) + _113;
   float _352 = (c_light_vec[0].z) + _114;
   float _353 = dot(float3(_350, _351, _352), float3(_350, _351, _352));
   float _354 = rsqrt(_353);
   float _355 = _350 * _354;
   float _356 = -0.0f - _355;
   float _357 = _351 * _354;
   float _358 = -0.0f - _357;
   float _359 = _352 * _354;
   float _360 = -0.0f - _359;
   float _361 = dot(float3(_356, _358, _360), float3(_107, _108, _109));
   float _362 = saturate(_361);
   float _363 = _113 + -0.30000001192092896f;
   float _364 = dot(float3(_112, _363, _114), float3(_112, _363, _114));
   float _365 = rsqrt(_364);
   float _366 = _112 * _365;
   float _367 = -0.0f - _366;
   float _368 = _363 * _365;
   float _369 = -0.0f - _368;
   float _370 = _114 * _365;
   float _371 = -0.0f - _370;
   float _372 = dot(float3(_367, _369, _371), float3(_107, _108, _109));
   float _373 = saturate(_372);
   float _376 = log2(_362);
   float _377 = log2(_373);
   float _378 = _376 * c_mat_power;
   float _379 = _377 * c_mat_power;
   float _380 = exp2(_378);
   float _381 = exp2(_379);
   float _384 = _380 * (c_light_col[0].x);
   float _385 = _380 * (c_light_col[0].y);
   float _386 = _380 * (c_light_col[0].z);
   float _387 = _381 * 2.0f;
   float _388 = _387 * (c_light_vec[5].x);
   float _389 = _388 * (c_light_col[1].x);
   float _390 = _388 * (c_light_col[1].y);
   float _391 = _388 * (c_light_col[1].z);
   float _392 = _384 * _219;
   float _393 = _385 * _220;
   float _394 = _386 * _221;
   float _395 = _389 * _292;
   float _396 = _390 * _292;
   float _397 = _391 * _292;
   float _398 = _395 + _392;
   float _399 = _396 + _393;
   float _400 = _397 + _394;
   float _407 = c_mat_specular.x * c_specular_scale.x;
   float _408 = _407 * _222;
   float _409 = _408 * _398;
   float _410 = c_mat_specular.y * c_specular_scale.y;
   float _411 = _410 * _223;
   float _412 = _411 * _399;
   float _413 = c_mat_specular.z * c_specular_scale.z;
   float _414 = _413 * _224;
   float _415 = _414 * _400;
   float _416 = _281 * _245;
   float _417 = _416 * _381;
   float _418 = _282 * _246;
   float _419 = _418 * _381;
   float _420 = _283 * _247;
   float _421 = _420 * _381;
   float _422 = _409 + _417;
   float _423 = _412 + _419;
   float _424 = _415 + _421;
   if (!_265)
   {
      float _426 = _207 * _189;
      float _427 = _207 * _190;
      float _428 = _207 * _191;
      float _429 = _343 + _426;
      float _430 = _344 + _427;
      float _431 = _345 + _428;
      float _432 = _207 * _192;
      float _433 = _422 + _432;
      float _434 = _423 + _432;
      float _435 = _424 + _432;
      _437 = _429;
      _438 = _430;
      _439 = _431;
      _440 = _433;
      _441 = _434;
      _442 = _435;
   }
   else
   {
      _437 = _343;
      _438 = _344;
      _439 = _345;
      _440 = _422;
      _441 = _423;
      _442 = _424;
   }
   int _443 = c_bool & 128;
   bool _444 = (_443 != 0);
   float _445 = select(_444, 0.0f, _440);
   float _446 = select(_444, 0.0f, _441);
   float _447 = select(_444, 0.0f, _442);
   float _448 = _437 * _53;
   float _449 = _438 * _54;
   float _450 = _439 * _55;
   float _451 = _445 * _61.y;
   float _452 = _446 * _61.y;
   float _453 = _447 * _61.y;
   int _454 = c_bool & 32768;
   bool _455 = (_454 == 0);
   if (!_455)
   {
      float _459 = (c_ps_user[11].x) * _53;
      float _460 = (c_ps_user[11].x) * _54;
      float _461 = (c_ps_user[11].x) * _55;
      float _463 = _107 + 1.0f;
      float _464 = _108 + 1.0f;
      float _465 = _109 + 1.0f;
      float _466 = _463 * 0.5f;
      float _467 = _464 * 0.5f;
      float _468 = _465 * 0.5f;
      float _469 = saturate(_466);
      float _470 = saturate(_467);
      float _471 = saturate(_468);
      float _472 = (c_ps_user[11].y) * _469;
      float _473 = _470 * (c_ps_user[11].y);
      float _474 = _471 * (c_ps_user[11].y);
      float _476 = (c_ps_user[11].z) * _61.y;
      float _478 = c_mat_power * 0.009999999776482582f;
      float _479 = _478 * (c_ps_user[11].w);
      float _482 = (c_ps_user[12].x) * _219;
      float _483 = (c_ps_user[12].x) * _220;
      float _484 = (c_ps_user[12].x) * _221;
      float _486 = (c_ps_user[12].y) * _222;
      float _487 = (c_ps_user[12].y) * _223;
      float _488 = (c_ps_user[12].y) * _224;
      float _490 = (c_ps_user[12].z) * _245;
      float _491 = (c_ps_user[12].z) * _246;
      float _492 = (c_ps_user[12].z) * _247;
      float _498 = -0.0f - _112;
      float _499 = -0.0f - _113;
      float _500 = -0.0f - _114;
      float _501 = dot(float3(_498, _499, _500), float3(_107, _108, _109));
      float _502 = 1.0f - _501;
      float _503 = _502 * _502;
      float _504 = _502 * 0.9722222089767456f;
      float _505 = _503 * _503;
      float _506 = _505 * _504;
      float _507 = _506 + 0.02777777425944805f;
      float _508 = _507 * (c_ps_user[13].z);
      float _510 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _511 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _512 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _515 = (c_ps_user[14].x) * _259;
      float _516 = (c_ps_user[14].x) * _261;
      float _517 = (c_ps_user[14].x) * _263;
      float _519 = (c_ps_user[14].y) * _298;
      float _520 = (c_ps_user[14].y) * _299;
      float _521 = (c_ps_user[14].y) * _300;
      float _523 = (c_ps_user[14].z) * _305;
      float _524 = (c_ps_user[14].z) * _306;
      float _525 = (c_ps_user[14].z) * _307;
      float _527 = (c_ps_user[14].w) * _278;
      float _528 = (c_ps_user[14].w) * _279;
      float _529 = (c_ps_user[14].w) * _280;
      float _532 = (c_ps_user[15].x) * _384;
      float _533 = (c_ps_user[15].x) * _385;
      float _534 = (c_ps_user[15].x) * _386;
      float _536 = _407 * _389;
      float _537 = _536 * (c_ps_user[15].y);
      float _538 = _410 * _390;
      float _539 = _538 * (c_ps_user[15].y);
      float _540 = _413 * _391;
      float _541 = _540 * (c_ps_user[15].y);
      float _543 = (c_ps_user[15].z) * _281;
      float _544 = (c_ps_user[15].z) * _282;
      float _545 = (c_ps_user[15].z) * _283;
      float _546 = _537 + _543;
      float _547 = _546 * _381;
      float _548 = _519 + (c_ps_user[13].x);
      float _549 = _548 + _523;
      float _550 = c_diffuse_scale.x * _549;
      float _551 = _550 * c_mat_diffuse.x;
      float _552 = (c_ps_user[13].y) + _476;
      float _553 = _552 + _532;
      float _554 = c_mat_specular.x * _553;
      float _555 = _554 * c_specular_scale.x;
      float _556 = _472 + _459;
      float _557 = _556 + _479;
      float _558 = _557 + (c_ps_user[12].w);
      float _559 = _558 + _482;
      float _560 = _559 + _486;
      float _561 = _560 + _490;
      float _562 = _561 + _510;
      float _563 = _562 + _515;
      float _564 = _563 + _527;
      float _565 = _564 + _508;
      float _566 = _565 + _547;
      float _567 = _566 + _551;
      float _568 = _567 + _555;
      float _569 = _539 + _544;
      float _570 = _569 * _381;
      float _571 = _520 + (c_ps_user[13].x);
      float _572 = _571 + _524;
      float _573 = c_diffuse_scale.y * _572;
      float _574 = _573 * c_mat_diffuse.y;
      float _575 = _552 + _533;
      float _576 = c_mat_specular.y * _575;
      float _577 = _576 * c_specular_scale.y;
      float _578 = _473 + _460;
      float _579 = _578 + _479;
      float _580 = _579 + (c_ps_user[12].w);
      float _581 = _580 + _483;
      float _582 = _581 + _487;
      float _583 = _582 + _491;
      float _584 = _583 + _511;
      float _585 = _584 + _516;
      float _586 = _585 + _528;
      float _587 = _586 + _508;
      float _588 = _587 + _570;
      float _589 = _588 + _574;
      float _590 = _589 + _577;
      float _591 = _541 + _545;
      float _592 = _591 * _381;
      float _593 = _521 + (c_ps_user[13].x);
      float _594 = _593 + _525;
      float _595 = c_diffuse_scale.z * _594;
      float _596 = _595 * c_mat_diffuse.z;
      float _597 = _552 + _534;
      float _598 = c_mat_specular.z * _597;
      float _599 = _598 * c_specular_scale.z;
      float _600 = _479 + _461;
      float _601 = _600 + _474;
      float _602 = _601 + (c_ps_user[12].w);
      float _603 = _602 + _484;
      float _604 = _603 + _488;
      float _605 = _604 + _492;
      float _606 = _605 + _512;
      float _607 = _606 + _517;
      float _608 = _607 + _529;
      float _609 = _608 + _508;
      float _610 = _609 + _592;
      float _611 = _610 + _596;
      float _612 = _611 + _599;
      _614 = _568;
      _615 = _590;
      _616 = _612;
      _617 = 0.0f;
      _618 = 0.0f;
      _619 = 0.0f;
   }
   else
   {
      _614 = _448;
      _615 = _449;
      _616 = _450;
      _617 = _451;
      _618 = _452;
      _619 = _453;
   }
   float _620 = _617 + _614;
   float _621 = _618 + _615;
   float _622 = _619 + _616;
   float _623 = dot(float3(_112, _113, _114), float3(_107, _108, _109));
   float _624 = _623 * 2.0f;
   float _625 = _624 * _107;
   float _626 = _624 * _108;
   float _627 = _624 * _109;
   float _628 = _112 - _625;
   float _629 = _113 - _626;
   float _630 = _114 - _627;
   float _631 = -0.0f - _628;
   float4 _632 = s6c_t.Sample(s6c_s, float3(_631, _629, _630));
   float _639 = _153 * _152;
   float _640 = _122 + 0.75f;
   float _641 = saturate(_640);
   float _642 = _639 * _641;
   float _643 = max(_642, (c_light_vec[5].y));
   float _644 = max(_632.w, 0.003921568859368563f);
   float _645 = 0.25f / _644;
   float _646 = _645 * _632.x;
   float _647 = _645 * _632.y;
   float _648 = _645 * _632.z;
   float _649 = _61.x * TEXCOORD_2.w;
   float _650 = _649 * _643;
   float _651 = _646 * _646;
   float _652 = _651 * _650;
   float _653 = _647 * _647;
   float _654 = _653 * _650;
   float _655 = _648 * _648;
   float _656 = _655 * _650;
   float _657 = _620 + _652;
   float _658 = _621 + _654;
   float _659 = _622 + _656;
   float _666 = c_mat_opacity * _48.w;
   float _667 = _666 * c_opacity_scale;
   float _668 = _667 * c_global_opacity;
   int _671 = c_bool & 16;
   bool _672 = (_671 == 0);
   if (!_672)
   {
      float _676 = c_tone_control.x * _657;
      float _677 = c_tone_control.x * _658;
      float _678 = c_tone_control.x * _659;
      float _680 = log2(_676);
      float _681 = log2(_677);
      float _682 = log2(_678);
      float _683 = _680 * c_tone_control.y;
      float _684 = _681 * c_tone_control.y;
      float _685 = _682 * c_tone_control.y;
      float _686 = exp2(_683);
      float _687 = exp2(_684);
      float _688 = exp2(_685);
      _738 = _686;
      _739 = _687;
      _740 = _688;
   }
   else
   {
      int _690 = c_bool & 32768;
      bool _691 = (_690 == 0);
      if (_691)
      {
         float _695 = c_tone_control.x * _657;
         float _696 = c_tone_control.x * _658;
         float _697 = c_tone_control.x * _659;
         float _698 = _695 * 2.0f;
         float _699 = _696 * 2.0f;
         float _700 = _697 * 2.0f;
         float _701 = _695 * 0.30000001192092896f;
         float _702 = _696 * 0.30000001192092896f;
         float _703 = _697 * 0.30000001192092896f;
         float _704 = _701 + 0.05000000074505806f;
         float _705 = _702 + 0.05000000074505806f;
         float _706 = _703 + 0.05000000074505806f;
         float _707 = _704 * _698;
         float _708 = _705 * _699;
         float _709 = _706 * _700;
         float _710 = _707 + 0.004000000189989805f;
         float _711 = _708 + 0.004000000189989805f;
         float _712 = _709 + 0.004000000189989805f;
         float _713 = _701 + 0.5f;
         float _714 = _702 + 0.5f;
         float _715 = _703 + 0.5f;
         float _716 = _713 * _698;
         float _717 = _714 * _699;
         float _718 = _715 * _700;
         float _719 = _716 + 0.06000000238418579f;
         float _720 = _717 + 0.06000000238418579f;
         float _721 = _718 + 0.06000000238418579f;
         float _722 = _710 / _719;
         float _723 = _711 / _720;
         float _724 = _712 / _721;
         float _725 = _722 + -0.06666666269302368f;
         float _726 = _723 + -0.06666666269302368f;
         float _727 = _724 + -0.06666666269302368f;
         float _728 = _725 * 1.3790643215179443f;
         float _729 = _726 * 1.3790643215179443f;
         float _730 = _727 * 1.3790643215179443f;
         float _731 = YKR_MaterialCurve(_728, _695);
         float _732 = YKR_MaterialCurve(_729, _696);
         float _733 = YKR_MaterialCurve(_730, _697);
         float _734 = sqrt(_731);
         float _735 = sqrt(_732);
         float _736 = sqrt(_733);
         _738 = _734;
         _739 = _735;
         _740 = _736;
      }
      else
      {
         _738 = _657;
         _739 = _658;
         _740 = _659;
      }
   }
   int _741 = c_bool & 4;
   bool _742 = (_741 == 0);
   if (_742)
   {
      float _744 = saturate(COLOR_1.x);
      float _749 = (c_fog_param[0].x) - _738;
      float _750 = (c_fog_param[0].y) - _739;
      float _751 = (c_fog_param[0].z) - _740;
      float _752 = _749 * _744;
      float _753 = _750 * _744;
      float _754 = _751 * _744;
      _756 = _752;
      _757 = _753;
      _758 = _754;
   }
   else
   {
      _756 = COLOR_1.x;
      _757 = COLOR_1.y;
      _758 = COLOR_1.z;
   }
   float _759 = _756 + _738;
   float _760 = _757 + _739;
   float _761 = _758 + _740;
   float _764 = _48.w - c_mat_punchthrough_ref;
   bool _765 = (_764 < 0.0f);
   if (_765)
      discard;
   float _771 = (c_ps_col_scl[0].x) * _759;
   float _772 = (c_ps_col_scl[0].y) * _760;
   float _773 = (c_ps_col_scl[0].z) * _761;
   float _774 = (c_ps_col_scl[0].w) * _668;
   SV_Target.x = _771;
   SV_Target.y = _772;
   SV_Target.z = _773;
   SV_Target.w = _774;
   int _777 = c_bool & 8;
   bool _778 = (_777 != 0);
   float _779 = select(_778, _759, _771);
   float _780 = select(_778, _760, _772);
   float _781 = select(_778, _761, _773);
   float _782 = select(_778, _668, _774);
   SV_Target.x = _779;
   SV_Target.y = _780;
   SV_Target.z = _781;
   SV_Target.w = _782;
   return SV_Target;
}
