// Yakuza Kiwami material r_b1dst (0x776652E3, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

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
    linear float4 TEXCOORD_6 : TEXCOORD6,
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
   float _66 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _67 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _68 = s5_t.Sample(s5_s, float2(_66, _67));
   float _71 = _68.x * 2.0f;
   float _72 = _68.y * 2.0f;
   float _73 = _71 + -1.0f;
   float _74 = _72 + -1.0f;
   float _75 = dot(float2(_73, _74), float2(_73, _74));
   float _76 = 1.0f - _75;
   float _77 = saturate(_76);
   float _78 = sqrt(_77);
   float _79 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _80 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _81 = _79 - _80;
   float _82 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _83 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _86 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _87 = _85 - _86;
   float _88 = _73 * TEXCOORD_6.x;
   float _89 = _73 * TEXCOORD_6.y;
   float _90 = _73 * TEXCOORD_6.z;
   float _91 = _74 * TEXCOORD_6.w;
   float _92 = _91 * _81;
   float _93 = _91 * _84;
   float _94 = _91 * _87;
   float _95 = _78 * TEXCOORD_2.x;
   float _96 = _78 * TEXCOORD_2.y;
   float _97 = _78 * TEXCOORD_2.z;
   float _98 = _95 + _88;
   float _99 = _98 + _92;
   float _100 = _96 + _89;
   float _101 = _100 + _93;
   float _102 = _97 + _90;
   float _103 = _102 + _94;
   float _104 = dot(float3(_99, _101, _103), float3(_99, _101, _103));
   float _105 = rsqrt(_104);
   float _106 = _99 * _105;
   float _107 = _101 * _105;
   float _108 = _103 * _105;
   float _109 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _110 = rsqrt(_109);
   float _111 = _110 * TEXCOORD_3.x;
   float _112 = _110 * TEXCOORD_3.y;
   float _113 = _110 * TEXCOORD_3.z;
   float _118 = -0.0f - (c_light_vec[0].x);
   float _119 = -0.0f - (c_light_vec[0].y);
   float _120 = -0.0f - (c_light_vec[0].z);
   float _121 = dot(float3(_106, _107, _108), float3(_118, _119, _120));
   float _125 = c_rt_size.z * SV_Position.x;
   float _126 = c_rt_size.w * SV_Position.y;
   int _129 = c_bool & 8;
   bool _130 = (_129 == 0);
   float _137;
   float _138;
   float _139;
   float _151;
   float _152;
   float _188;
   float _189;
   float _190;
   float _191;
   float _205;
   float _206;
   float _241;
   float _242;
   float _243;
   float _277;
   float _278;
   float _279;
   float _280;
   float _281;
   float _282;
   float _436;
   float _437;
   float _438;
   float _439;
   float _440;
   float _441;
   float _617;
   float _618;
   float _619;
   float _620;
   float _621;
   float _622;
   float _699;
   float _700;
   float _701;
   float _717;
   float _718;
   float _719;
   if (!_130)
   {
      float4 _132 = s13_t.Sample(s13_s, float2(_125, _126));
      _137 = _132.x;
      _138 = _132.y;
      _139 = _132.w;
   }
   else
   {
      _137 = 1.0f;
      _138 = 1.0f;
      _139 = 1.0f;
   }
   if (!_130)
   {
      float _144 = (c_sdw_param[0].x) * _137;
      float _145 = (c_sdw_param[0].z) * _138;
      float _148 = _144 + (c_sdw_param[0].y);
      float _149 = _145 + (c_sdw_param[0].w);
      _151 = _148;
      _152 = _149;
   }
   else
   {
      _151 = _137;
      _152 = _138;
   }
   float _156 = c_rt_size.z * SV_Position.x;
   float _157 = c_rt_size.w * SV_Position.y;
   float4 _158 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_156, _157)));
   int _165 = c_bool & 16384;
   bool _166 = (_165 == 0);
   if (!_166)
   {
      float _168 = _158.x * 2.0f;
      float _169 = _158.y * 2.0f;
      float _170 = _158.z * 2.0f;
      float _171 = _158.w * 2.0f;
      _188 = _168;
      _189 = _169;
      _190 = _170;
      _191 = _171;
   }
   else
   {
      int _173 = c_bool & 8192;
      bool _174 = (_173 == 0);
      if (!_174)
      {
         float _176 = _158.x * 2.0f;
         float _177 = _158.y * 2.0f;
         float _178 = _158.z * 2.0f;
         float _179 = _158.w * 2.0f;
         _188 = _176;
         _189 = _177;
         _190 = _178;
         _191 = _179;
      }
      else
      {
         float _183 = _158.x / c_tone_control.z;
         float _184 = _158.y / c_tone_control.z;
         float _185 = _158.z / c_tone_control.z;
         float _186 = _158.w / c_tone_control.z;
         _188 = _183;
         _189 = _184;
         _190 = _185;
         _191 = _186;
      }
   }
   int _192 = c_bool & 8;
   bool _193 = (_192 == 0);
   if (!_193)
   {
      float _198 = (c_sdw_param[0].x) * _151;
      float _199 = (c_sdw_param[0].z) * _152;
      float _202 = _198 + (c_sdw_param[0].y);
      float _203 = _199 + (c_sdw_param[0].w);
      _205 = _202;
      _206 = _203;
   }
   else
   {
      _205 = _151;
      _206 = _152;
   }
   float _207 = _206 * _205;
   float _212 = 1.0f - (c_light_col[5].x);
   float _213 = 1.0f - (c_light_col[5].y);
   float _214 = 1.0f - (c_light_col[5].z);
   float _215 = _212 * _207;
   float _216 = _213 * _207;
   float _217 = _214 * _207;
   float _218 = _215 + (c_light_col[5].x);
   float _219 = _216 + (c_light_col[5].y);
   float _220 = _217 + (c_light_col[5].z);
   float _221 = _139 * COLOR.x;
   float _222 = _139 * COLOR.y;
   float _223 = _139 * COLOR.z;
   if (!_193)
   {
      float _227 = (c_sdw_param[2].x) * _218;
      float _228 = (c_sdw_param[2].x) * _219;
      float _229 = (c_sdw_param[2].x) * _220;
      float _231 = _227 + (c_sdw_param[2].y);
      float _232 = _228 + (c_sdw_param[2].y);
      float _233 = _229 + (c_sdw_param[2].y);
      float _236 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _237 = _231 * _236;
      float _238 = _232 * _236;
      float _239 = _233 * _236;
      _241 = _237;
      _242 = _238;
      _243 = _239;
   }
   else
   {
      _241 = _218;
      _242 = _219;
      _243 = _220;
   }
   float _244 = _241 * _139;
   float _245 = _242 * _139;
   float _246 = _243 * _139;
   float _257 = c_mat_emissive.x * _51;
   float _258 = _257 * c_emissive_scale;
   float _259 = c_mat_emissive.y * _52;
   float _260 = _259 * c_emissive_scale;
   float _261 = c_mat_emissive.z * _53;
   float _262 = _261 * c_emissive_scale;
   int _263 = c_bool & 2048;
   bool _264 = (_263 == 0);
   if (_264)
   {
      float _266 = _206 * _188;
      float _267 = _206 * _189;
      float _268 = _206 * _190;
      float _269 = _266 + TEXCOORD_4.x;
      float _270 = _267 + TEXCOORD_4.y;
      float _271 = _268 + TEXCOORD_4.z;
      float _272 = _206 * _191;
      float _273 = _272 + TEXCOORD_5.x;
      float _274 = _272 + TEXCOORD_5.y;
      float _275 = _272 + TEXCOORD_5.z;
      _277 = _273;
      _278 = _274;
      _279 = _275;
      _280 = _269;
      _281 = _270;
      _282 = _271;
   }
   else
   {
      _277 = TEXCOORD_5.x;
      _278 = TEXCOORD_5.y;
      _279 = TEXCOORD_5.z;
      _280 = TEXCOORD_4.x;
      _281 = TEXCOORD_4.y;
      _282 = TEXCOORD_4.z;
   }
   float _287 = -0.0f - (c_light_vec[1].x);
   float _288 = -0.0f - (c_light_vec[1].y);
   float _289 = -0.0f - (c_light_vec[1].z);
   float _290 = dot(float3(_106, _107, _108), float3(_287, _288, _289));
   float _291 = saturate(_290);
   float _296 = saturate(_121);
   float _297 = (c_light_col[0].x) * _296;
   float _298 = (c_light_col[0].y) * _296;
   float _299 = (c_light_col[0].z) * _296;
   float _304 = (c_light_col[1].x) * _291;
   float _305 = (c_light_col[1].y) * _291;
   float _306 = (c_light_col[1].z) * _291;
   float _315 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _316 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _317 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _318 = _297 * _218;
   float _319 = _298 * _219;
   float _320 = _299 * _220;
   float _321 = _304 * _206;
   float _322 = _305 * _206;
   float _323 = _306 * _206;
   float _324 = _321 + _318;
   float _325 = _322 + _319;
   float _326 = _323 + _320;
   float _327 = _315 * _324;
   float _328 = _316 * _325;
   float _329 = _317 * _326;
   float _330 = _258 + c_scene_ambient.x;
   float _331 = _330 + _327;
   float _332 = _260 + c_scene_ambient.y;
   float _333 = _332 + _328;
   float _334 = _262 + c_scene_ambient.z;
   float _335 = _334 + _329;
   float _336 = _331 * _221;
   float _337 = _333 * _222;
   float _338 = _335 * _223;
   float _339 = _280 * _244;
   float _340 = _281 * _245;
   float _341 = _282 * _246;
   float _342 = _336 + _339;
   float _343 = _337 + _340;
   float _344 = _338 + _341;
   float _349 = (c_light_vec[0].x) + _111;
   float _350 = (c_light_vec[0].y) + _112;
   float _351 = (c_light_vec[0].z) + _113;
   float _352 = dot(float3(_349, _350, _351), float3(_349, _350, _351));
   float _353 = rsqrt(_352);
   float _354 = _349 * _353;
   float _355 = -0.0f - _354;
   float _356 = _350 * _353;
   float _357 = -0.0f - _356;
   float _358 = _351 * _353;
   float _359 = -0.0f - _358;
   float _360 = dot(float3(_355, _357, _359), float3(_106, _107, _108));
   float _361 = saturate(_360);
   float _362 = _112 + -0.30000001192092896f;
   float _363 = dot(float3(_111, _362, _113), float3(_111, _362, _113));
   float _364 = rsqrt(_363);
   float _365 = _111 * _364;
   float _366 = -0.0f - _365;
   float _367 = _362 * _364;
   float _368 = -0.0f - _367;
   float _369 = _113 * _364;
   float _370 = -0.0f - _369;
   float _371 = dot(float3(_366, _368, _370), float3(_106, _107, _108));
   float _372 = saturate(_371);
   float _375 = log2(_361);
   float _376 = log2(_372);
   float _377 = _375 * c_mat_power;
   float _378 = _376 * c_mat_power;
   float _379 = exp2(_377);
   float _380 = exp2(_378);
   float _383 = _379 * (c_light_col[0].x);
   float _384 = _379 * (c_light_col[0].y);
   float _385 = _379 * (c_light_col[0].z);
   float _386 = _380 * 2.0f;
   float _387 = _386 * (c_light_vec[5].x);
   float _388 = _387 * (c_light_col[1].x);
   float _389 = _387 * (c_light_col[1].y);
   float _390 = _387 * (c_light_col[1].z);
   float _391 = _383 * _218;
   float _392 = _384 * _219;
   float _393 = _385 * _220;
   float _394 = _388 * _291;
   float _395 = _389 * _291;
   float _396 = _390 * _291;
   float _397 = _394 + _391;
   float _398 = _395 + _392;
   float _399 = _396 + _393;
   float _406 = c_mat_specular.x * c_specular_scale.x;
   float _407 = _406 * _221;
   float _408 = _407 * _397;
   float _409 = c_mat_specular.y * c_specular_scale.y;
   float _410 = _409 * _222;
   float _411 = _410 * _398;
   float _412 = c_mat_specular.z * c_specular_scale.z;
   float _413 = _412 * _223;
   float _414 = _413 * _399;
   float _415 = _277 * _244;
   float _416 = _415 * _380;
   float _417 = _278 * _245;
   float _418 = _417 * _380;
   float _419 = _279 * _246;
   float _420 = _419 * _380;
   float _421 = _408 + _416;
   float _422 = _411 + _418;
   float _423 = _414 + _420;
   if (!_264)
   {
      float _425 = _206 * _188;
      float _426 = _206 * _189;
      float _427 = _206 * _190;
      float _428 = _342 + _425;
      float _429 = _343 + _426;
      float _430 = _344 + _427;
      float _431 = _206 * _191;
      float _432 = _421 + _431;
      float _433 = _422 + _431;
      float _434 = _423 + _431;
      _436 = _428;
      _437 = _429;
      _438 = _430;
      _439 = _432;
      _440 = _433;
      _441 = _434;
   }
   else
   {
      _436 = _342;
      _437 = _343;
      _438 = _344;
      _439 = _421;
      _440 = _422;
      _441 = _423;
   }
   int _442 = c_bool & 128;
   bool _443 = (_442 != 0);
   float _444 = select(_443, 0.0f, _439);
   float _445 = select(_443, 0.0f, _440);
   float _446 = select(_443, 0.0f, _441);
   float _447 = _436 * _51;
   float _448 = _437 * _52;
   float _449 = _438 * _53;
   float _450 = _444 * _59.x;
   float _451 = _445 * _59.y;
   float _452 = _446 * _59.z;
   int _453 = c_bool & 32768;
   bool _454 = (_453 == 0);
   if (!_454)
   {
      float _458 = (c_ps_user[11].x) * _51;
      float _459 = (c_ps_user[11].x) * _52;
      float _460 = (c_ps_user[11].x) * _53;
      float _462 = _106 + 1.0f;
      float _463 = _107 + 1.0f;
      float _464 = _108 + 1.0f;
      float _465 = _462 * 0.5f;
      float _466 = _463 * 0.5f;
      float _467 = _464 * 0.5f;
      float _468 = saturate(_465);
      float _469 = saturate(_466);
      float _470 = saturate(_467);
      float _471 = (c_ps_user[11].y) * _468;
      float _472 = _469 * (c_ps_user[11].y);
      float _473 = _470 * (c_ps_user[11].y);
      float _475 = (c_ps_user[11].z) * _59.x;
      float _476 = (c_ps_user[11].z) * _59.y;
      float _477 = (c_ps_user[11].z) * _59.z;
      float _479 = c_mat_power * 0.009999999776482582f;
      float _480 = _479 * (c_ps_user[11].w);
      float _483 = (c_ps_user[12].x) * _218;
      float _484 = (c_ps_user[12].x) * _219;
      float _485 = (c_ps_user[12].x) * _220;
      float _487 = (c_ps_user[12].y) * _221;
      float _488 = (c_ps_user[12].y) * _222;
      float _489 = (c_ps_user[12].y) * _223;
      float _491 = (c_ps_user[12].z) * _244;
      float _492 = (c_ps_user[12].z) * _245;
      float _493 = (c_ps_user[12].z) * _246;
      float _499 = -0.0f - _111;
      float _500 = -0.0f - _112;
      float _501 = -0.0f - _113;
      float _502 = dot(float3(_499, _500, _501), float3(_106, _107, _108));
      float _503 = 1.0f - _502;
      float _504 = _503 * _503;
      float _505 = _503 * 0.9722222089767456f;
      float _506 = _504 * _504;
      float _507 = _506 * _505;
      float _508 = _507 + 0.02777777425944805f;
      float _509 = _508 * (c_ps_user[13].z);
      float _511 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _512 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _513 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _516 = (c_ps_user[14].x) * _258;
      float _517 = (c_ps_user[14].x) * _260;
      float _518 = (c_ps_user[14].x) * _262;
      float _520 = (c_ps_user[14].y) * _297;
      float _521 = (c_ps_user[14].y) * _298;
      float _522 = (c_ps_user[14].y) * _299;
      float _524 = (c_ps_user[14].z) * _304;
      float _525 = (c_ps_user[14].z) * _305;
      float _526 = (c_ps_user[14].z) * _306;
      float _528 = (c_ps_user[14].w) * _280;
      float _529 = (c_ps_user[14].w) * _281;
      float _530 = (c_ps_user[14].w) * _282;
      float _533 = (c_ps_user[15].x) * _383;
      float _534 = (c_ps_user[15].x) * _384;
      float _535 = (c_ps_user[15].x) * _385;
      float _537 = _406 * _388;
      float _538 = _537 * (c_ps_user[15].y);
      float _539 = _409 * _389;
      float _540 = _539 * (c_ps_user[15].y);
      float _541 = _412 * _390;
      float _542 = _541 * (c_ps_user[15].y);
      float _544 = (c_ps_user[15].z) * _277;
      float _545 = (c_ps_user[15].z) * _278;
      float _546 = (c_ps_user[15].z) * _279;
      float _547 = _538 + _544;
      float _548 = _547 * _380;
      float _549 = _520 + (c_ps_user[13].x);
      float _550 = _549 + _524;
      float _551 = c_diffuse_scale.x * _550;
      float _552 = _551 * c_mat_diffuse.x;
      float _553 = (c_ps_user[13].y) + _475;
      float _554 = _553 + _533;
      float _555 = c_mat_specular.x * _554;
      float _556 = _555 * c_specular_scale.x;
      float _557 = _471 + _458;
      float _558 = _557 + _480;
      float _559 = _558 + (c_ps_user[12].w);
      float _560 = _559 + _483;
      float _561 = _560 + _487;
      float _562 = _561 + _491;
      float _563 = _562 + _511;
      float _564 = _563 + _516;
      float _565 = _564 + _528;
      float _566 = _565 + _509;
      float _567 = _566 + _548;
      float _568 = _567 + _552;
      float _569 = _568 + _556;
      float _570 = _540 + _545;
      float _571 = _570 * _380;
      float _572 = _521 + (c_ps_user[13].x);
      float _573 = _572 + _525;
      float _574 = c_diffuse_scale.y * _573;
      float _575 = _574 * c_mat_diffuse.y;
      float _576 = (c_ps_user[13].y) + _476;
      float _577 = _576 + _534;
      float _578 = c_mat_specular.y * _577;
      float _579 = _578 * c_specular_scale.y;
      float _580 = _472 + _459;
      float _581 = _580 + _480;
      float _582 = _581 + (c_ps_user[12].w);
      float _583 = _582 + _484;
      float _584 = _583 + _488;
      float _585 = _584 + _492;
      float _586 = _585 + _512;
      float _587 = _586 + _517;
      float _588 = _587 + _529;
      float _589 = _588 + _509;
      float _590 = _589 + _571;
      float _591 = _590 + _575;
      float _592 = _591 + _579;
      float _593 = _542 + _546;
      float _594 = _593 * _380;
      float _595 = _522 + (c_ps_user[13].x);
      float _596 = _595 + _526;
      float _597 = c_diffuse_scale.z * _596;
      float _598 = _597 * c_mat_diffuse.z;
      float _599 = (c_ps_user[13].y) + _477;
      float _600 = _599 + _535;
      float _601 = c_mat_specular.z * _600;
      float _602 = _601 * c_specular_scale.z;
      float _603 = _480 + _460;
      float _604 = _603 + _473;
      float _605 = _604 + (c_ps_user[12].w);
      float _606 = _605 + _485;
      float _607 = _606 + _489;
      float _608 = _607 + _493;
      float _609 = _608 + _513;
      float _610 = _609 + _518;
      float _611 = _610 + _530;
      float _612 = _611 + _509;
      float _613 = _612 + _594;
      float _614 = _613 + _598;
      float _615 = _614 + _602;
      _617 = _569;
      _618 = _592;
      _619 = _615;
      _620 = 0.0f;
      _621 = 0.0f;
      _622 = 0.0f;
   }
   else
   {
      _617 = _447;
      _618 = _448;
      _619 = _449;
      _620 = _450;
      _621 = _451;
      _622 = _452;
   }
   float _623 = _620 + _617;
   float _624 = _621 + _618;
   float _625 = _622 + _619;
   float _630 = _46.w * COLOR.w;
   float _631 = _630 * c_mat_opacity;
   float _632 = _631 * c_global_opacity;
   float _633 = _632 * c_opacity_scale;
   int _634 = c_bool & 16;
   bool _635 = (_634 == 0);
   if (!_635)
   {
      float _639 = c_tone_control.x * _623;
      float _640 = c_tone_control.x * _624;
      float _641 = c_tone_control.x * _625;
      float _643 = log2(_639);
      float _644 = log2(_640);
      float _645 = log2(_641);
      float _646 = _643 * c_tone_control.y;
      float _647 = _644 * c_tone_control.y;
      float _648 = _645 * c_tone_control.y;
      float _649 = exp2(_646);
      float _650 = exp2(_647);
      float _651 = exp2(_648);
      _699 = _649;
      _700 = _650;
      _701 = _651;
   }
   else
   {
      if (_454)
      {
         float _656 = c_tone_control.x * _623;
         float _657 = c_tone_control.x * _624;
         float _658 = c_tone_control.x * _625;
         float _659 = _656 * 2.0f;
         float _660 = _657 * 2.0f;
         float _661 = _658 * 2.0f;
         float _662 = _656 * 0.30000001192092896f;
         float _663 = _657 * 0.30000001192092896f;
         float _664 = _658 * 0.30000001192092896f;
         float _665 = _662 + 0.05000000074505806f;
         float _666 = _663 + 0.05000000074505806f;
         float _667 = _664 + 0.05000000074505806f;
         float _668 = _665 * _659;
         float _669 = _666 * _660;
         float _670 = _667 * _661;
         float _671 = _668 + 0.004000000189989805f;
         float _672 = _669 + 0.004000000189989805f;
         float _673 = _670 + 0.004000000189989805f;
         float _674 = _662 + 0.5f;
         float _675 = _663 + 0.5f;
         float _676 = _664 + 0.5f;
         float _677 = _674 * _659;
         float _678 = _675 * _660;
         float _679 = _676 * _661;
         float _680 = _677 + 0.06000000238418579f;
         float _681 = _678 + 0.06000000238418579f;
         float _682 = _679 + 0.06000000238418579f;
         float _683 = _671 / _680;
         float _684 = _672 / _681;
         float _685 = _673 / _682;
         float _686 = _683 + -0.06666666269302368f;
         float _687 = _684 + -0.06666666269302368f;
         float _688 = _685 + -0.06666666269302368f;
         float _689 = _686 * 1.3790643215179443f;
         float _690 = _687 * 1.3790643215179443f;
         float _691 = _688 * 1.3790643215179443f;
         float _692 = YKR_MaterialCurve(_689, _656);
         float _693 = YKR_MaterialCurve(_690, _657);
         float _694 = YKR_MaterialCurve(_691, _658);
         float _695 = sqrt(_692);
         float _696 = sqrt(_693);
         float _697 = sqrt(_694);
         _699 = _695;
         _700 = _696;
         _701 = _697;
      }
      else
      {
         _699 = _623;
         _700 = _624;
         _701 = _625;
      }
   }
   int _702 = c_bool & 4;
   bool _703 = (_702 == 0);
   if (_703)
   {
      float _705 = saturate(COLOR_1.x);
      float _710 = (c_fog_param[0].x) - _699;
      float _711 = (c_fog_param[0].y) - _700;
      float _712 = (c_fog_param[0].z) - _701;
      float _713 = _710 * _705;
      float _714 = _711 * _705;
      float _715 = _712 * _705;
      _717 = _713;
      _718 = _714;
      _719 = _715;
   }
   else
   {
      _717 = COLOR_1.x;
      _718 = COLOR_1.y;
      _719 = COLOR_1.z;
   }
   float _720 = _717 + _699;
   float _721 = _718 + _700;
   float _722 = _719 + _701;
   float _725 = _633 - c_mat_punchthrough_ref;
   bool _726 = (_725 < 0.0f);
   if (_726)
      discard;
   float _732 = (c_ps_col_scl[0].x) * _720;
   float _733 = (c_ps_col_scl[0].y) * _721;
   float _734 = (c_ps_col_scl[0].z) * _722;
   float _735 = (c_ps_col_scl[0].w) * _633;
   SV_Target.x = _732;
   SV_Target.y = _733;
   SV_Target.z = _734;
   SV_Target.w = _735;
   int _738 = c_bool & 8;
   bool _739 = (_738 != 0);
   float _740 = select(_739, _720, _732);
   float _741 = select(_739, _721, _733);
   float _742 = select(_739, _722, _734);
   float _743 = select(_739, _633, _735);
   SV_Target.x = _740;
   SV_Target.y = _741;
   SV_Target.z = _742;
   SV_Target.w = _743;
   return SV_Target;
}
