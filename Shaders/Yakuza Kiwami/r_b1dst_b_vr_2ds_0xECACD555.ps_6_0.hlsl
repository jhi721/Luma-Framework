// Yakuza Kiwami material r_b1dst_b(vr)2ds (0xECACD555, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s1_s : register(s1);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

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
   float4 _53 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _58 = _53.x * _53.x;
   float _59 = _53.y * _53.y;
   float _60 = _53.z * _53.z;
   float _64 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _65 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _66 = s3_t.Sample(s3_s, float2(_64, _65));
   float _73 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _74 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _75 = s5_t.Sample(s5_s, float2(_73, _74));
   float4 _78 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _83 = _78.x * _78.x;
   float _84 = _78.y * _78.y;
   float _85 = _78.z * _78.z;
   float _89 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _90 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _91 = s4_t.Sample(s4_s, float2(_89, _90));
   float _95 = _78.w * COLOR_1.w;
   float _96 = _83 - _58;
   float _97 = _84 - _59;
   float _98 = _85 - _60;
   float _99 = _96 * _95;
   float _100 = _97 * _95;
   float _101 = _98 * _95;
   float _102 = _99 + _58;
   float _103 = _100 + _59;
   float _104 = _101 + _60;
   float _105 = _91.x - _66.x;
   float _106 = _91.y - _66.y;
   float _107 = _91.z - _66.z;
   float _108 = _105 * _95;
   float _109 = _106 * _95;
   float _110 = _107 * _95;
   float _111 = _108 + _66.x;
   float _112 = _109 + _66.y;
   float _113 = _110 + _66.z;
   float _114 = 0.5f - _75.x;
   float _115 = 0.5f - _75.y;
   float _116 = _95 * _114;
   float _117 = _95 * _115;
   float _118 = _116 + _75.x;
   float _119 = _117 + _75.y;
   float _120 = _118 * 2.0f;
   float _121 = _119 * 2.0f;
   float _122 = _120 + -1.0f;
   float _123 = _121 + -1.0f;
   float _124 = dot(float2(_122, _123), float2(_122, _123));
   float _125 = 1.0f - _124;
   float _126 = saturate(_125);
   float _127 = sqrt(_126);
   float _128 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _129 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _130 = _128 - _129;
   float _131 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _132 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _133 = _131 - _132;
   float _134 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _135 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _136 = _134 - _135;
   float _137 = _122 * TEXCOORD_6.x;
   float _138 = _122 * TEXCOORD_6.y;
   float _139 = _122 * TEXCOORD_6.z;
   float _140 = _123 * TEXCOORD_6.w;
   float _141 = _140 * _130;
   float _142 = _140 * _133;
   float _143 = _140 * _136;
   float _144 = _141 + _137;
   float _145 = _142 + _138;
   float _146 = _143 + _139;
   float _147 = _127 * TEXCOORD_2.x;
   float _148 = _127 * TEXCOORD_2.y;
   float _149 = _127 * TEXCOORD_2.z;
   float _150 = _144 + _147;
   float _151 = _145 + _148;
   float _152 = _146 + _149;
   float _153 = dot(float3(_150, _151, _152), float3(_150, _151, _152));
   float _154 = rsqrt(_153);
   float _155 = _150 * _154;
   float _156 = _151 * _154;
   float _157 = _152 * _154;
   float _158 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _159 = rsqrt(_158);
   float _160 = _159 * TEXCOORD_3.x;
   float _161 = _159 * TEXCOORD_3.y;
   float _162 = _159 * TEXCOORD_3.z;
   float _167 = -0.0f - (c_light_vec[0].x);
   float _168 = -0.0f - (c_light_vec[0].y);
   float _169 = -0.0f - (c_light_vec[0].z);
   float _170 = dot(float3(_155, _156, _157), float3(_167, _168, _169));
   float _174 = c_rt_size.z * SV_Position.x;
   float _175 = c_rt_size.w * SV_Position.y;
   int _178 = c_bool & 8;
   bool _179 = (_178 == 0);
   float _186;
   float _187;
   float _188;
   float _200;
   float _201;
   float _237;
   float _238;
   float _239;
   float _240;
   float _254;
   float _255;
   float _290;
   float _291;
   float _292;
   float _326;
   float _327;
   float _328;
   float _329;
   float _330;
   float _331;
   float _485;
   float _486;
   float _487;
   float _488;
   float _489;
   float _490;
   float _666;
   float _667;
   float _668;
   float _669;
   float _670;
   float _671;
   float _748;
   float _749;
   float _750;
   float _766;
   float _767;
   float _768;
   if (!_179)
   {
      float4 _181 = s13_t.Sample(s13_s, float2(_174, _175));
      _186 = _181.x;
      _187 = _181.y;
      _188 = _181.w;
   }
   else
   {
      _186 = 1.0f;
      _187 = 1.0f;
      _188 = 1.0f;
   }
   if (!_179)
   {
      float _193 = (c_sdw_param[0].x) * _186;
      float _194 = (c_sdw_param[0].z) * _187;
      float _197 = _193 + (c_sdw_param[0].y);
      float _198 = _194 + (c_sdw_param[0].w);
      _200 = _197;
      _201 = _198;
   }
   else
   {
      _200 = _186;
      _201 = _187;
   }
   float _205 = c_rt_size.z * SV_Position.x;
   float _206 = c_rt_size.w * SV_Position.y;
   float4 _207 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_205, _206)));
   int _214 = c_bool & 16384;
   bool _215 = (_214 == 0);
   if (!_215)
   {
      float _217 = _207.x * 2.0f;
      float _218 = _207.y * 2.0f;
      float _219 = _207.z * 2.0f;
      float _220 = _207.w * 2.0f;
      _237 = _217;
      _238 = _218;
      _239 = _219;
      _240 = _220;
   }
   else
   {
      int _222 = c_bool & 8192;
      bool _223 = (_222 == 0);
      if (!_223)
      {
         float _225 = _207.x * 2.0f;
         float _226 = _207.y * 2.0f;
         float _227 = _207.z * 2.0f;
         float _228 = _207.w * 2.0f;
         _237 = _225;
         _238 = _226;
         _239 = _227;
         _240 = _228;
      }
      else
      {
         float _232 = _207.x / c_tone_control.z;
         float _233 = _207.y / c_tone_control.z;
         float _234 = _207.z / c_tone_control.z;
         float _235 = _207.w / c_tone_control.z;
         _237 = _232;
         _238 = _233;
         _239 = _234;
         _240 = _235;
      }
   }
   int _241 = c_bool & 8;
   bool _242 = (_241 == 0);
   if (!_242)
   {
      float _247 = (c_sdw_param[0].x) * _200;
      float _248 = (c_sdw_param[0].z) * _201;
      float _251 = _247 + (c_sdw_param[0].y);
      float _252 = _248 + (c_sdw_param[0].w);
      _254 = _251;
      _255 = _252;
   }
   else
   {
      _254 = _200;
      _255 = _201;
   }
   float _256 = _255 * _254;
   float _261 = 1.0f - (c_light_col[5].x);
   float _262 = 1.0f - (c_light_col[5].y);
   float _263 = 1.0f - (c_light_col[5].z);
   float _264 = _261 * _256;
   float _265 = _262 * _256;
   float _266 = _263 * _256;
   float _267 = _264 + (c_light_col[5].x);
   float _268 = _265 + (c_light_col[5].y);
   float _269 = _266 + (c_light_col[5].z);
   float _270 = _188 * COLOR.x;
   float _271 = _188 * COLOR.y;
   float _272 = _188 * COLOR.z;
   if (!_242)
   {
      float _276 = (c_sdw_param[2].x) * _267;
      float _277 = (c_sdw_param[2].x) * _268;
      float _278 = (c_sdw_param[2].x) * _269;
      float _280 = _276 + (c_sdw_param[2].y);
      float _281 = _277 + (c_sdw_param[2].y);
      float _282 = _278 + (c_sdw_param[2].y);
      float _285 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _286 = _280 * _285;
      float _287 = _281 * _285;
      float _288 = _282 * _285;
      _290 = _286;
      _291 = _287;
      _292 = _288;
   }
   else
   {
      _290 = _267;
      _291 = _268;
      _292 = _269;
   }
   float _293 = _290 * _188;
   float _294 = _291 * _188;
   float _295 = _292 * _188;
   float _306 = c_mat_emissive.x * _102;
   float _307 = _306 * c_emissive_scale;
   float _308 = c_mat_emissive.y * _103;
   float _309 = _308 * c_emissive_scale;
   float _310 = c_mat_emissive.z * _104;
   float _311 = _310 * c_emissive_scale;
   int _312 = c_bool & 2048;
   bool _313 = (_312 == 0);
   if (_313)
   {
      float _315 = _255 * _237;
      float _316 = _255 * _238;
      float _317 = _255 * _239;
      float _318 = _315 + TEXCOORD_4.x;
      float _319 = _316 + TEXCOORD_4.y;
      float _320 = _317 + TEXCOORD_4.z;
      float _321 = _255 * _240;
      float _322 = _321 + TEXCOORD_5.x;
      float _323 = _321 + TEXCOORD_5.y;
      float _324 = _321 + TEXCOORD_5.z;
      _326 = _318;
      _327 = _319;
      _328 = _320;
      _329 = _322;
      _330 = _323;
      _331 = _324;
   }
   else
   {
      _326 = TEXCOORD_4.x;
      _327 = TEXCOORD_4.y;
      _328 = TEXCOORD_4.z;
      _329 = TEXCOORD_5.x;
      _330 = TEXCOORD_5.y;
      _331 = TEXCOORD_5.z;
   }
   float _336 = -0.0f - (c_light_vec[1].x);
   float _337 = -0.0f - (c_light_vec[1].y);
   float _338 = -0.0f - (c_light_vec[1].z);
   float _339 = dot(float3(_155, _156, _157), float3(_336, _337, _338));
   float _340 = saturate(_339);
   float _345 = saturate(_170);
   float _346 = (c_light_col[0].x) * _345;
   float _347 = (c_light_col[0].y) * _345;
   float _348 = (c_light_col[0].z) * _345;
   float _353 = (c_light_col[1].x) * _340;
   float _354 = (c_light_col[1].y) * _340;
   float _355 = (c_light_col[1].z) * _340;
   float _364 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _365 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _366 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _367 = _346 * _267;
   float _368 = _347 * _268;
   float _369 = _348 * _269;
   float _370 = _353 * _255;
   float _371 = _354 * _255;
   float _372 = _355 * _255;
   float _373 = _370 + _367;
   float _374 = _371 + _368;
   float _375 = _372 + _369;
   float _376 = _364 * _373;
   float _377 = _365 * _374;
   float _378 = _366 * _375;
   float _379 = _307 + c_scene_ambient.x;
   float _380 = _379 + _376;
   float _381 = _309 + c_scene_ambient.y;
   float _382 = _381 + _377;
   float _383 = _311 + c_scene_ambient.z;
   float _384 = _383 + _378;
   float _385 = _380 * _270;
   float _386 = _382 * _271;
   float _387 = _384 * _272;
   float _388 = _326 * _293;
   float _389 = _327 * _294;
   float _390 = _328 * _295;
   float _391 = _385 + _388;
   float _392 = _386 + _389;
   float _393 = _387 + _390;
   float _398 = (c_light_vec[0].x) + _160;
   float _399 = (c_light_vec[0].y) + _161;
   float _400 = (c_light_vec[0].z) + _162;
   float _401 = dot(float3(_398, _399, _400), float3(_398, _399, _400));
   float _402 = rsqrt(_401);
   float _403 = _398 * _402;
   float _404 = -0.0f - _403;
   float _405 = _399 * _402;
   float _406 = -0.0f - _405;
   float _407 = _400 * _402;
   float _408 = -0.0f - _407;
   float _409 = dot(float3(_404, _406, _408), float3(_155, _156, _157));
   float _410 = saturate(_409);
   float _411 = _161 + -0.30000001192092896f;
   float _412 = dot(float3(_160, _411, _162), float3(_160, _411, _162));
   float _413 = rsqrt(_412);
   float _414 = _160 * _413;
   float _415 = -0.0f - _414;
   float _416 = _411 * _413;
   float _417 = -0.0f - _416;
   float _418 = _162 * _413;
   float _419 = -0.0f - _418;
   float _420 = dot(float3(_415, _417, _419), float3(_155, _156, _157));
   float _421 = saturate(_420);
   float _424 = log2(_410);
   float _425 = log2(_421);
   float _426 = _424 * c_mat_power;
   float _427 = _425 * c_mat_power;
   float _428 = exp2(_426);
   float _429 = exp2(_427);
   float _432 = _428 * (c_light_col[0].x);
   float _433 = _428 * (c_light_col[0].y);
   float _434 = _428 * (c_light_col[0].z);
   float _435 = _429 * 2.0f;
   float _436 = _435 * (c_light_vec[5].x);
   float _437 = _436 * (c_light_col[1].x);
   float _438 = _436 * (c_light_col[1].y);
   float _439 = _436 * (c_light_col[1].z);
   float _440 = _432 * _267;
   float _441 = _433 * _268;
   float _442 = _434 * _269;
   float _443 = _437 * _340;
   float _444 = _438 * _340;
   float _445 = _439 * _340;
   float _446 = _443 + _440;
   float _447 = _444 + _441;
   float _448 = _445 + _442;
   float _455 = c_mat_specular.x * c_specular_scale.x;
   float _456 = _455 * _270;
   float _457 = _456 * _446;
   float _458 = c_mat_specular.y * c_specular_scale.y;
   float _459 = _458 * _271;
   float _460 = _459 * _447;
   float _461 = c_mat_specular.z * c_specular_scale.z;
   float _462 = _461 * _272;
   float _463 = _462 * _448;
   float _464 = _329 * _293;
   float _465 = _464 * _429;
   float _466 = _330 * _294;
   float _467 = _466 * _429;
   float _468 = _331 * _295;
   float _469 = _468 * _429;
   float _470 = _457 + _465;
   float _471 = _460 + _467;
   float _472 = _463 + _469;
   if (!_313)
   {
      float _474 = _255 * _237;
      float _475 = _255 * _238;
      float _476 = _255 * _239;
      float _477 = _391 + _474;
      float _478 = _392 + _475;
      float _479 = _393 + _476;
      float _480 = _255 * _240;
      float _481 = _470 + _480;
      float _482 = _471 + _480;
      float _483 = _472 + _480;
      _485 = _477;
      _486 = _478;
      _487 = _479;
      _488 = _481;
      _489 = _482;
      _490 = _483;
   }
   else
   {
      _485 = _391;
      _486 = _392;
      _487 = _393;
      _488 = _470;
      _489 = _471;
      _490 = _472;
   }
   int _491 = c_bool & 128;
   bool _492 = (_491 != 0);
   float _493 = select(_492, 0.0f, _488);
   float _494 = select(_492, 0.0f, _489);
   float _495 = select(_492, 0.0f, _490);
   float _496 = _485 * _102;
   float _497 = _486 * _103;
   float _498 = _487 * _104;
   float _499 = _493 * _111;
   float _500 = _494 * _112;
   float _501 = _495 * _113;
   int _502 = c_bool & 32768;
   bool _503 = (_502 == 0);
   if (!_503)
   {
      float _507 = (c_ps_user[11].x) * _102;
      float _508 = (c_ps_user[11].x) * _103;
      float _509 = (c_ps_user[11].x) * _104;
      float _511 = _155 + 1.0f;
      float _512 = _156 + 1.0f;
      float _513 = _157 + 1.0f;
      float _514 = _511 * 0.5f;
      float _515 = _512 * 0.5f;
      float _516 = _513 * 0.5f;
      float _517 = saturate(_514);
      float _518 = saturate(_515);
      float _519 = saturate(_516);
      float _520 = (c_ps_user[11].y) * _517;
      float _521 = _518 * (c_ps_user[11].y);
      float _522 = _519 * (c_ps_user[11].y);
      float _524 = (c_ps_user[11].z) * _111;
      float _525 = (c_ps_user[11].z) * _112;
      float _526 = (c_ps_user[11].z) * _113;
      float _528 = c_mat_power * 0.009999999776482582f;
      float _529 = _528 * (c_ps_user[11].w);
      float _532 = (c_ps_user[12].x) * _267;
      float _533 = (c_ps_user[12].x) * _268;
      float _534 = (c_ps_user[12].x) * _269;
      float _536 = (c_ps_user[12].y) * _270;
      float _537 = (c_ps_user[12].y) * _271;
      float _538 = (c_ps_user[12].y) * _272;
      float _540 = (c_ps_user[12].z) * _293;
      float _541 = (c_ps_user[12].z) * _294;
      float _542 = (c_ps_user[12].z) * _295;
      float _548 = -0.0f - _160;
      float _549 = -0.0f - _161;
      float _550 = -0.0f - _162;
      float _551 = dot(float3(_548, _549, _550), float3(_155, _156, _157));
      float _552 = 1.0f - _551;
      float _553 = _552 * _552;
      float _554 = _552 * 0.9722222089767456f;
      float _555 = _553 * _553;
      float _556 = _555 * _554;
      float _557 = _556 + 0.02777777425944805f;
      float _558 = _557 * (c_ps_user[13].z);
      float _560 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _561 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _562 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _565 = (c_ps_user[14].x) * _307;
      float _566 = (c_ps_user[14].x) * _309;
      float _567 = (c_ps_user[14].x) * _311;
      float _569 = (c_ps_user[14].y) * _346;
      float _570 = (c_ps_user[14].y) * _347;
      float _571 = (c_ps_user[14].y) * _348;
      float _573 = (c_ps_user[14].z) * _353;
      float _574 = (c_ps_user[14].z) * _354;
      float _575 = (c_ps_user[14].z) * _355;
      float _577 = (c_ps_user[14].w) * _326;
      float _578 = (c_ps_user[14].w) * _327;
      float _579 = (c_ps_user[14].w) * _328;
      float _582 = (c_ps_user[15].x) * _432;
      float _583 = (c_ps_user[15].x) * _433;
      float _584 = (c_ps_user[15].x) * _434;
      float _586 = _455 * _437;
      float _587 = _586 * (c_ps_user[15].y);
      float _588 = _458 * _438;
      float _589 = _588 * (c_ps_user[15].y);
      float _590 = _461 * _439;
      float _591 = _590 * (c_ps_user[15].y);
      float _593 = (c_ps_user[15].z) * _329;
      float _594 = (c_ps_user[15].z) * _330;
      float _595 = (c_ps_user[15].z) * _331;
      float _596 = _587 + _593;
      float _597 = _596 * _429;
      float _598 = _569 + (c_ps_user[13].x);
      float _599 = _598 + _573;
      float _600 = c_diffuse_scale.x * _599;
      float _601 = _600 * c_mat_diffuse.x;
      float _602 = (c_ps_user[13].y) + _524;
      float _603 = _602 + _582;
      float _604 = c_mat_specular.x * _603;
      float _605 = _604 * c_specular_scale.x;
      float _606 = _520 + _507;
      float _607 = _606 + _529;
      float _608 = _607 + (c_ps_user[12].w);
      float _609 = _608 + _532;
      float _610 = _609 + _536;
      float _611 = _610 + _540;
      float _612 = _611 + _560;
      float _613 = _612 + _565;
      float _614 = _613 + _577;
      float _615 = _614 + _558;
      float _616 = _615 + _597;
      float _617 = _616 + _601;
      float _618 = _617 + _605;
      float _619 = _589 + _594;
      float _620 = _619 * _429;
      float _621 = _570 + (c_ps_user[13].x);
      float _622 = _621 + _574;
      float _623 = c_diffuse_scale.y * _622;
      float _624 = _623 * c_mat_diffuse.y;
      float _625 = (c_ps_user[13].y) + _525;
      float _626 = _625 + _583;
      float _627 = c_mat_specular.y * _626;
      float _628 = _627 * c_specular_scale.y;
      float _629 = _521 + _508;
      float _630 = _629 + _529;
      float _631 = _630 + (c_ps_user[12].w);
      float _632 = _631 + _533;
      float _633 = _632 + _537;
      float _634 = _633 + _541;
      float _635 = _634 + _561;
      float _636 = _635 + _566;
      float _637 = _636 + _578;
      float _638 = _637 + _558;
      float _639 = _638 + _620;
      float _640 = _639 + _624;
      float _641 = _640 + _628;
      float _642 = _591 + _595;
      float _643 = _642 * _429;
      float _644 = _571 + (c_ps_user[13].x);
      float _645 = _644 + _575;
      float _646 = c_diffuse_scale.z * _645;
      float _647 = _646 * c_mat_diffuse.z;
      float _648 = (c_ps_user[13].y) + _526;
      float _649 = _648 + _584;
      float _650 = c_mat_specular.z * _649;
      float _651 = _650 * c_specular_scale.z;
      float _652 = _529 + _509;
      float _653 = _652 + _522;
      float _654 = _653 + (c_ps_user[12].w);
      float _655 = _654 + _534;
      float _656 = _655 + _538;
      float _657 = _656 + _542;
      float _658 = _657 + _562;
      float _659 = _658 + _567;
      float _660 = _659 + _579;
      float _661 = _660 + _558;
      float _662 = _661 + _643;
      float _663 = _662 + _647;
      float _664 = _663 + _651;
      _666 = _618;
      _667 = _641;
      _668 = _664;
      _669 = 0.0f;
      _670 = 0.0f;
      _671 = 0.0f;
   }
   else
   {
      _666 = _496;
      _667 = _497;
      _668 = _498;
      _669 = _499;
      _670 = _500;
      _671 = _501;
   }
   float _672 = _669 + _666;
   float _673 = _670 + _667;
   float _674 = _671 + _668;
   float _679 = _53.w * COLOR.w;
   float _680 = _679 * c_mat_opacity;
   float _681 = _680 * c_global_opacity;
   float _682 = _681 * c_opacity_scale;
   int _683 = c_bool & 16;
   bool _684 = (_683 == 0);
   if (!_684)
   {
      float _688 = c_tone_control.x * _672;
      float _689 = c_tone_control.x * _673;
      float _690 = c_tone_control.x * _674;
      float _692 = log2(_688);
      float _693 = log2(_689);
      float _694 = log2(_690);
      float _695 = _692 * c_tone_control.y;
      float _696 = _693 * c_tone_control.y;
      float _697 = _694 * c_tone_control.y;
      float _698 = exp2(_695);
      float _699 = exp2(_696);
      float _700 = exp2(_697);
      _748 = _698;
      _749 = _699;
      _750 = _700;
   }
   else
   {
      if (_503)
      {
         float _705 = c_tone_control.x * _672;
         float _706 = c_tone_control.x * _673;
         float _707 = c_tone_control.x * _674;
         float _708 = _705 * 2.0f;
         float _709 = _706 * 2.0f;
         float _710 = _707 * 2.0f;
         float _711 = _705 * 0.30000001192092896f;
         float _712 = _706 * 0.30000001192092896f;
         float _713 = _707 * 0.30000001192092896f;
         float _714 = _711 + 0.05000000074505806f;
         float _715 = _712 + 0.05000000074505806f;
         float _716 = _713 + 0.05000000074505806f;
         float _717 = _714 * _708;
         float _718 = _715 * _709;
         float _719 = _716 * _710;
         float _720 = _717 + 0.004000000189989805f;
         float _721 = _718 + 0.004000000189989805f;
         float _722 = _719 + 0.004000000189989805f;
         float _723 = _711 + 0.5f;
         float _724 = _712 + 0.5f;
         float _725 = _713 + 0.5f;
         float _726 = _723 * _708;
         float _727 = _724 * _709;
         float _728 = _725 * _710;
         float _729 = _726 + 0.06000000238418579f;
         float _730 = _727 + 0.06000000238418579f;
         float _731 = _728 + 0.06000000238418579f;
         float _732 = _720 / _729;
         float _733 = _721 / _730;
         float _734 = _722 / _731;
         float _735 = _732 + -0.06666666269302368f;
         float _736 = _733 + -0.06666666269302368f;
         float _737 = _734 + -0.06666666269302368f;
         float _738 = _735 * 1.3790643215179443f;
         float _739 = _736 * 1.3790643215179443f;
         float _740 = _737 * 1.3790643215179443f;
         float _741 = YKR_MaterialCurve(_738, _705);
         float _742 = YKR_MaterialCurve(_739, _706);
         float _743 = YKR_MaterialCurve(_740, _707);
         float _744 = sqrt(_741);
         float _745 = sqrt(_742);
         float _746 = sqrt(_743);
         _748 = _744;
         _749 = _745;
         _750 = _746;
      }
      else
      {
         _748 = _672;
         _749 = _673;
         _750 = _674;
      }
   }
   int _751 = c_bool & 4;
   bool _752 = (_751 == 0);
   if (_752)
   {
      float _754 = saturate(COLOR_1.x);
      float _759 = (c_fog_param[0].x) - _748;
      float _760 = (c_fog_param[0].y) - _749;
      float _761 = (c_fog_param[0].z) - _750;
      float _762 = _759 * _754;
      float _763 = _760 * _754;
      float _764 = _761 * _754;
      _766 = _762;
      _767 = _763;
      _768 = _764;
   }
   else
   {
      _766 = COLOR_1.x;
      _767 = COLOR_1.y;
      _768 = COLOR_1.z;
   }
   float _769 = _766 + _748;
   float _770 = _767 + _749;
   float _771 = _768 + _750;
   float _777 = (c_ps_col_scl[0].x) * _769;
   float _778 = (c_ps_col_scl[0].y) * _770;
   float _779 = (c_ps_col_scl[0].z) * _771;
   float _780 = (c_ps_col_scl[0].w) * _682;
   SV_Target.x = _777;
   SV_Target.y = _778;
   SV_Target.z = _779;
   SV_Target.w = _780;
   int _783 = c_bool & 8;
   bool _784 = (_783 != 0);
   float _785 = select(_784, _769, _777);
   float _786 = select(_784, _770, _778);
   float _787 = select(_784, _771, _779);
   float _788 = select(_784, _682, _780);
   SV_Target.x = _785;
   SV_Target.y = _786;
   SV_Target.z = _787;
   SV_Target.w = _788;
   return SV_Target;
}
