// Yakuza Kiwami material r_o1dut_m(vr)2du[ref] (0x0C1C7C05, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

TextureCube<float4> s2c_t : register(t2);

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

SamplerState s2c_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
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
   float4 _55 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _60 = _55.x * _55.x;
   float _61 = _55.y * _55.y;
   float _62 = _55.z * _55.z;
   float _66 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _67 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _68 = s3_t.Sample(s3_s, float2(_66, _67));
   float _74 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _75 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _76 = s5_t.Sample(s5_s, float2(_74, _75));
   float4 _79 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _84 = _79.x * _79.x;
   float _85 = _79.y * _79.y;
   float _86 = _79.z * _79.z;
   float _90 = (c_attribute_ps[1].x) * TEXCOORD.z;
   float _91 = (c_attribute_ps[1].y) * TEXCOORD.w;
   float4 _92 = s4_t.Sample(s4_s, float2(_90, _91));
   float _95 = _79.w * COLOR_1.w;
   float _96 = 1.0f - _92.x;
   float _97 = 1.0f - _92.y;
   float _98 = _96 * _95;
   float _99 = _97 * _95;
   float _100 = 1.0f - _98;
   float _101 = 1.0f - _99;
   float _102 = _101 * _68.y;
   float _103 = 1.0f - _84;
   float _104 = 1.0f - _85;
   float _105 = 1.0f - _86;
   float _106 = _103 * _95;
   float _107 = _104 * _95;
   float _108 = _105 * _95;
   float _109 = 1.0f - _106;
   float _110 = 1.0f - _107;
   float _111 = 1.0f - _108;
   float _112 = _60 * _109;
   float _113 = _61 * _110;
   float _114 = _62 * _111;
   float _115 = _76.x * 2.0f;
   float _116 = _76.y * 2.0f;
   float _117 = _115 + -1.0f;
   float _118 = _116 + -1.0f;
   float _119 = dot(float2(_117, _118), float2(_117, _118));
   float _120 = 1.0f - _119;
   float _121 = saturate(_120);
   float _122 = sqrt(_121);
   float _123 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _124 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _125 = _123 - _124;
   float _126 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _127 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _128 = _126 - _127;
   float _129 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _130 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _131 = _129 - _130;
   float _132 = _117 * TEXCOORD_6.x;
   float _133 = _117 * TEXCOORD_6.y;
   float _134 = _117 * TEXCOORD_6.z;
   float _135 = _118 * TEXCOORD_6.w;
   float _136 = _135 * _125;
   float _137 = _135 * _128;
   float _138 = _135 * _131;
   float _139 = _136 + _132;
   float _140 = _137 + _133;
   float _141 = _138 + _134;
   float _142 = _122 * TEXCOORD_2.x;
   float _143 = _122 * TEXCOORD_2.y;
   float _144 = _122 * TEXCOORD_2.z;
   float _145 = _139 + _142;
   float _146 = _140 + _143;
   float _147 = _141 + _144;
   float _148 = dot(float3(_145, _146, _147), float3(_145, _146, _147));
   float _149 = rsqrt(_148);
   float _150 = _145 * _149;
   float _151 = _146 * _149;
   float _152 = _147 * _149;
   float _153 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _154 = rsqrt(_153);
   float _155 = _154 * TEXCOORD_3.x;
   float _156 = _154 * TEXCOORD_3.y;
   float _157 = _154 * TEXCOORD_3.z;
   float _162 = -0.0f - (c_light_vec[0].x);
   float _163 = -0.0f - (c_light_vec[0].y);
   float _164 = -0.0f - (c_light_vec[0].z);
   float _165 = dot(float3(_150, _151, _152), float3(_162, _163, _164));
   float _169 = c_rt_size.z * SV_Position.x;
   float _170 = c_rt_size.w * SV_Position.y;
   int _173 = c_bool & 8;
   bool _174 = (_173 == 0);
   float _181;
   float _182;
   float _183;
   float _195;
   float _196;
   float _232;
   float _233;
   float _234;
   float _235;
   float _249;
   float _250;
   float _285;
   float _286;
   float _287;
   float _321;
   float _322;
   float _323;
   float _324;
   float _325;
   float _326;
   float _480;
   float _481;
   float _482;
   float _483;
   float _484;
   float _485;
   float _657;
   float _658;
   float _659;
   float _660;
   float _661;
   float _662;
   float _782;
   float _783;
   float _784;
   float _800;
   float _801;
   float _802;
   if (!_174)
   {
      float4 _176 = s13_t.Sample(s13_s, float2(_169, _170));
      _181 = _176.x;
      _182 = _176.y;
      _183 = _176.w;
   }
   else
   {
      _181 = 1.0f;
      _182 = 1.0f;
      _183 = 1.0f;
   }
   if (!_174)
   {
      float _188 = (c_sdw_param[0].x) * _181;
      float _189 = (c_sdw_param[0].z) * _182;
      float _192 = _188 + (c_sdw_param[0].y);
      float _193 = _189 + (c_sdw_param[0].w);
      _195 = _192;
      _196 = _193;
   }
   else
   {
      _195 = _181;
      _196 = _182;
   }
   float _200 = c_rt_size.z * SV_Position.x;
   float _201 = c_rt_size.w * SV_Position.y;
   float4 _202 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_200, _201)));
   int _209 = c_bool & 16384;
   bool _210 = (_209 == 0);
   if (!_210)
   {
      float _212 = _202.x * 2.0f;
      float _213 = _202.y * 2.0f;
      float _214 = _202.z * 2.0f;
      float _215 = _202.w * 2.0f;
      _232 = _212;
      _233 = _213;
      _234 = _214;
      _235 = _215;
   }
   else
   {
      int _217 = c_bool & 8192;
      bool _218 = (_217 == 0);
      if (!_218)
      {
         float _220 = _202.x * 2.0f;
         float _221 = _202.y * 2.0f;
         float _222 = _202.z * 2.0f;
         float _223 = _202.w * 2.0f;
         _232 = _220;
         _233 = _221;
         _234 = _222;
         _235 = _223;
      }
      else
      {
         float _227 = _202.x / c_tone_control.z;
         float _228 = _202.y / c_tone_control.z;
         float _229 = _202.z / c_tone_control.z;
         float _230 = _202.w / c_tone_control.z;
         _232 = _227;
         _233 = _228;
         _234 = _229;
         _235 = _230;
      }
   }
   int _236 = c_bool & 8;
   bool _237 = (_236 == 0);
   if (!_237)
   {
      float _242 = (c_sdw_param[0].x) * _195;
      float _243 = (c_sdw_param[0].z) * _196;
      float _246 = _242 + (c_sdw_param[0].y);
      float _247 = _243 + (c_sdw_param[0].w);
      _249 = _246;
      _250 = _247;
   }
   else
   {
      _249 = _195;
      _250 = _196;
   }
   float _251 = _250 * _249;
   float _256 = 1.0f - (c_light_col[5].x);
   float _257 = 1.0f - (c_light_col[5].y);
   float _258 = 1.0f - (c_light_col[5].z);
   float _259 = _256 * _251;
   float _260 = _257 * _251;
   float _261 = _258 * _251;
   float _262 = _259 + (c_light_col[5].x);
   float _263 = _260 + (c_light_col[5].y);
   float _264 = _261 + (c_light_col[5].z);
   float _265 = _183 * COLOR.x;
   float _266 = _183 * COLOR.y;
   float _267 = _183 * COLOR.z;
   if (!_237)
   {
      float _271 = (c_sdw_param[2].x) * _262;
      float _272 = (c_sdw_param[2].x) * _263;
      float _273 = (c_sdw_param[2].x) * _264;
      float _275 = _271 + (c_sdw_param[2].y);
      float _276 = _272 + (c_sdw_param[2].y);
      float _277 = _273 + (c_sdw_param[2].y);
      float _280 = (c_sdw_param[2].w) + (c_sdw_param[2].z);
      float _281 = _275 * _280;
      float _282 = _276 * _280;
      float _283 = _277 * _280;
      _285 = _281;
      _286 = _282;
      _287 = _283;
   }
   else
   {
      _285 = _262;
      _286 = _263;
      _287 = _264;
   }
   float _288 = _285 * _183;
   float _289 = _286 * _183;
   float _290 = _287 * _183;
   float _301 = c_mat_emissive.x * _112;
   float _302 = _301 * c_emissive_scale;
   float _303 = c_mat_emissive.y * _113;
   float _304 = _303 * c_emissive_scale;
   float _305 = c_mat_emissive.z * _114;
   float _306 = _305 * c_emissive_scale;
   int _307 = c_bool & 2048;
   bool _308 = (_307 == 0);
   if (_308)
   {
      float _310 = _250 * _232;
      float _311 = _250 * _233;
      float _312 = _250 * _234;
      float _313 = _310 + TEXCOORD_4.x;
      float _314 = _311 + TEXCOORD_4.y;
      float _315 = _312 + TEXCOORD_4.z;
      float _316 = _250 * _235;
      float _317 = _316 + TEXCOORD_5.x;
      float _318 = _316 + TEXCOORD_5.y;
      float _319 = _316 + TEXCOORD_5.z;
      _321 = _313;
      _322 = _314;
      _323 = _315;
      _324 = _317;
      _325 = _318;
      _326 = _319;
   }
   else
   {
      _321 = TEXCOORD_4.x;
      _322 = TEXCOORD_4.y;
      _323 = TEXCOORD_4.z;
      _324 = TEXCOORD_5.x;
      _325 = TEXCOORD_5.y;
      _326 = TEXCOORD_5.z;
   }
   float _331 = -0.0f - (c_light_vec[1].x);
   float _332 = -0.0f - (c_light_vec[1].y);
   float _333 = -0.0f - (c_light_vec[1].z);
   float _334 = dot(float3(_150, _151, _152), float3(_331, _332, _333));
   float _335 = saturate(_334);
   float _340 = saturate(_165);
   float _341 = (c_light_col[0].x) * _340;
   float _342 = (c_light_col[0].y) * _340;
   float _343 = (c_light_col[0].z) * _340;
   float _348 = (c_light_col[1].x) * _335;
   float _349 = (c_light_col[1].y) * _335;
   float _350 = (c_light_col[1].z) * _335;
   float _359 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _360 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _361 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _362 = _341 * _262;
   float _363 = _342 * _263;
   float _364 = _343 * _264;
   float _365 = _348 * _250;
   float _366 = _349 * _250;
   float _367 = _350 * _250;
   float _368 = _365 + _362;
   float _369 = _366 + _363;
   float _370 = _367 + _364;
   float _371 = _359 * _368;
   float _372 = _360 * _369;
   float _373 = _361 * _370;
   float _374 = _302 + c_scene_ambient.x;
   float _375 = _374 + _371;
   float _376 = _304 + c_scene_ambient.y;
   float _377 = _376 + _372;
   float _378 = _306 + c_scene_ambient.z;
   float _379 = _378 + _373;
   float _380 = _375 * _265;
   float _381 = _377 * _266;
   float _382 = _379 * _267;
   float _383 = _321 * _288;
   float _384 = _322 * _289;
   float _385 = _323 * _290;
   float _386 = _380 + _383;
   float _387 = _381 + _384;
   float _388 = _382 + _385;
   float _393 = (c_light_vec[0].x) + _155;
   float _394 = (c_light_vec[0].y) + _156;
   float _395 = (c_light_vec[0].z) + _157;
   float _396 = dot(float3(_393, _394, _395), float3(_393, _394, _395));
   float _397 = rsqrt(_396);
   float _398 = _393 * _397;
   float _399 = -0.0f - _398;
   float _400 = _394 * _397;
   float _401 = -0.0f - _400;
   float _402 = _395 * _397;
   float _403 = -0.0f - _402;
   float _404 = dot(float3(_399, _401, _403), float3(_150, _151, _152));
   float _405 = saturate(_404);
   float _406 = _156 + -0.30000001192092896f;
   float _407 = dot(float3(_155, _406, _157), float3(_155, _406, _157));
   float _408 = rsqrt(_407);
   float _409 = _155 * _408;
   float _410 = -0.0f - _409;
   float _411 = _406 * _408;
   float _412 = -0.0f - _411;
   float _413 = _157 * _408;
   float _414 = -0.0f - _413;
   float _415 = dot(float3(_410, _412, _414), float3(_150, _151, _152));
   float _416 = saturate(_415);
   float _419 = log2(_405);
   float _420 = log2(_416);
   float _421 = _419 * c_mat_power;
   float _422 = _420 * c_mat_power;
   float _423 = exp2(_421);
   float _424 = exp2(_422);
   float _427 = _423 * (c_light_col[0].x);
   float _428 = _423 * (c_light_col[0].y);
   float _429 = _423 * (c_light_col[0].z);
   float _430 = _424 * 2.0f;
   float _431 = _430 * (c_light_vec[5].x);
   float _432 = _431 * (c_light_col[1].x);
   float _433 = _431 * (c_light_col[1].y);
   float _434 = _431 * (c_light_col[1].z);
   float _435 = _427 * _262;
   float _436 = _428 * _263;
   float _437 = _429 * _264;
   float _438 = _432 * _335;
   float _439 = _433 * _335;
   float _440 = _434 * _335;
   float _441 = _438 + _435;
   float _442 = _439 + _436;
   float _443 = _440 + _437;
   float _450 = c_mat_specular.x * c_specular_scale.x;
   float _451 = _450 * _265;
   float _452 = _451 * _441;
   float _453 = c_mat_specular.y * c_specular_scale.y;
   float _454 = _453 * _266;
   float _455 = _454 * _442;
   float _456 = c_mat_specular.z * c_specular_scale.z;
   float _457 = _456 * _267;
   float _458 = _457 * _443;
   float _459 = _324 * _288;
   float _460 = _459 * _424;
   float _461 = _325 * _289;
   float _462 = _461 * _424;
   float _463 = _326 * _290;
   float _464 = _463 * _424;
   float _465 = _452 + _460;
   float _466 = _455 + _462;
   float _467 = _458 + _464;
   if (!_308)
   {
      float _469 = _250 * _232;
      float _470 = _250 * _233;
      float _471 = _250 * _234;
      float _472 = _386 + _469;
      float _473 = _387 + _470;
      float _474 = _388 + _471;
      float _475 = _250 * _235;
      float _476 = _465 + _475;
      float _477 = _466 + _475;
      float _478 = _467 + _475;
      _480 = _472;
      _481 = _473;
      _482 = _474;
      _483 = _476;
      _484 = _477;
      _485 = _478;
   }
   else
   {
      _480 = _386;
      _481 = _387;
      _482 = _388;
      _483 = _465;
      _484 = _466;
      _485 = _467;
   }
   int _486 = c_bool & 128;
   bool _487 = (_486 != 0);
   float _488 = select(_487, 0.0f, _483);
   float _489 = select(_487, 0.0f, _484);
   float _490 = select(_487, 0.0f, _485);
   float _491 = _480 * _112;
   float _492 = _481 * _113;
   float _493 = _482 * _114;
   float _494 = _488 * _102;
   float _495 = _489 * _102;
   float _496 = _490 * _102;
   int _497 = c_bool & 32768;
   bool _498 = (_497 == 0);
   if (!_498)
   {
      float _502 = (c_ps_user[11].x) * _112;
      float _503 = (c_ps_user[11].x) * _113;
      float _504 = (c_ps_user[11].x) * _114;
      float _506 = _150 + 1.0f;
      float _507 = _151 + 1.0f;
      float _508 = _152 + 1.0f;
      float _509 = _506 * 0.5f;
      float _510 = _507 * 0.5f;
      float _511 = _508 * 0.5f;
      float _512 = saturate(_509);
      float _513 = saturate(_510);
      float _514 = saturate(_511);
      float _515 = (c_ps_user[11].y) * _512;
      float _516 = _513 * (c_ps_user[11].y);
      float _517 = _514 * (c_ps_user[11].y);
      float _519 = (c_ps_user[11].z) * _102;
      float _521 = c_mat_power * 0.009999999776482582f;
      float _522 = _521 * (c_ps_user[11].w);
      float _525 = (c_ps_user[12].x) * _262;
      float _526 = (c_ps_user[12].x) * _263;
      float _527 = (c_ps_user[12].x) * _264;
      float _529 = (c_ps_user[12].y) * _265;
      float _530 = (c_ps_user[12].y) * _266;
      float _531 = (c_ps_user[12].y) * _267;
      float _533 = (c_ps_user[12].z) * _288;
      float _534 = (c_ps_user[12].z) * _289;
      float _535 = (c_ps_user[12].z) * _290;
      float _541 = -0.0f - _155;
      float _542 = -0.0f - _156;
      float _543 = -0.0f - _157;
      float _544 = dot(float3(_541, _542, _543), float3(_150, _151, _152));
      float _545 = 1.0f - _544;
      float _546 = _545 * _545;
      float _547 = _545 * 0.9722222089767456f;
      float _548 = _546 * _546;
      float _549 = _548 * _547;
      float _550 = _549 + 0.02777777425944805f;
      float _551 = _550 * (c_ps_user[13].z);
      float _553 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _554 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _555 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _558 = (c_ps_user[14].x) * _302;
      float _559 = (c_ps_user[14].x) * _304;
      float _560 = (c_ps_user[14].x) * _306;
      float _562 = (c_ps_user[14].y) * _341;
      float _563 = (c_ps_user[14].y) * _342;
      float _564 = (c_ps_user[14].y) * _343;
      float _566 = (c_ps_user[14].z) * _348;
      float _567 = (c_ps_user[14].z) * _349;
      float _568 = (c_ps_user[14].z) * _350;
      float _570 = (c_ps_user[14].w) * _321;
      float _571 = (c_ps_user[14].w) * _322;
      float _572 = (c_ps_user[14].w) * _323;
      float _575 = (c_ps_user[15].x) * _427;
      float _576 = (c_ps_user[15].x) * _428;
      float _577 = (c_ps_user[15].x) * _429;
      float _579 = _450 * _432;
      float _580 = _579 * (c_ps_user[15].y);
      float _581 = _453 * _433;
      float _582 = _581 * (c_ps_user[15].y);
      float _583 = _456 * _434;
      float _584 = _583 * (c_ps_user[15].y);
      float _586 = (c_ps_user[15].z) * _324;
      float _587 = (c_ps_user[15].z) * _325;
      float _588 = (c_ps_user[15].z) * _326;
      float _589 = _580 + _586;
      float _590 = _589 * _424;
      float _591 = _562 + (c_ps_user[13].x);
      float _592 = _591 + _566;
      float _593 = c_diffuse_scale.x * _592;
      float _594 = _593 * c_mat_diffuse.x;
      float _595 = (c_ps_user[13].y) + _519;
      float _596 = _595 + _575;
      float _597 = c_mat_specular.x * _596;
      float _598 = _597 * c_specular_scale.x;
      float _599 = _515 + _502;
      float _600 = _599 + _522;
      float _601 = _600 + (c_ps_user[12].w);
      float _602 = _601 + _525;
      float _603 = _602 + _529;
      float _604 = _603 + _533;
      float _605 = _604 + _553;
      float _606 = _605 + _558;
      float _607 = _606 + _570;
      float _608 = _607 + _551;
      float _609 = _608 + _590;
      float _610 = _609 + _594;
      float _611 = _610 + _598;
      float _612 = _582 + _587;
      float _613 = _612 * _424;
      float _614 = _563 + (c_ps_user[13].x);
      float _615 = _614 + _567;
      float _616 = c_diffuse_scale.y * _615;
      float _617 = _616 * c_mat_diffuse.y;
      float _618 = _595 + _576;
      float _619 = c_mat_specular.y * _618;
      float _620 = _619 * c_specular_scale.y;
      float _621 = _516 + _503;
      float _622 = _621 + _522;
      float _623 = _622 + (c_ps_user[12].w);
      float _624 = _623 + _526;
      float _625 = _624 + _530;
      float _626 = _625 + _534;
      float _627 = _626 + _554;
      float _628 = _627 + _559;
      float _629 = _628 + _571;
      float _630 = _629 + _551;
      float _631 = _630 + _613;
      float _632 = _631 + _617;
      float _633 = _632 + _620;
      float _634 = _584 + _588;
      float _635 = _634 * _424;
      float _636 = _564 + (c_ps_user[13].x);
      float _637 = _636 + _568;
      float _638 = c_diffuse_scale.z * _637;
      float _639 = _638 * c_mat_diffuse.z;
      float _640 = _595 + _577;
      float _641 = c_mat_specular.z * _640;
      float _642 = _641 * c_specular_scale.z;
      float _643 = _522 + _504;
      float _644 = _643 + _517;
      float _645 = _644 + (c_ps_user[12].w);
      float _646 = _645 + _527;
      float _647 = _646 + _531;
      float _648 = _647 + _535;
      float _649 = _648 + _555;
      float _650 = _649 + _560;
      float _651 = _650 + _572;
      float _652 = _651 + _551;
      float _653 = _652 + _635;
      float _654 = _653 + _639;
      float _655 = _654 + _642;
      _657 = _611;
      _658 = _633;
      _659 = _655;
      _660 = 0.0f;
      _661 = 0.0f;
      _662 = 0.0f;
   }
   else
   {
      _657 = _491;
      _658 = _492;
      _659 = _493;
      _660 = _494;
      _661 = _495;
      _662 = _496;
   }
   float _663 = _660 + _657;
   float _664 = _661 + _658;
   float _665 = _662 + _659;
   float _666 = dot(float3(_155, _156, _157), float3(_150, _151, _152));
   float _667 = _666 * 2.0f;
   float _668 = _667 * _150;
   float _669 = _667 * _151;
   float _670 = _667 * _152;
   float _671 = _155 - _668;
   float _672 = _156 - _669;
   float _673 = _157 - _670;
   float _674 = -0.0f - _671;
   float4 _675 = s2c_t.Sample(s2c_s, float3(_674, _672, _673));
   float _682 = _196 * _195;
   float _683 = _165 + 0.75f;
   float _684 = saturate(_683);
   float _685 = _682 * _684;
   float _686 = max(_685, (c_light_vec[5].y));
   float _687 = max(_675.w, 0.003921568859368563f);
   float _688 = 0.25f / _687;
   float _689 = _688 * _675.x;
   float _690 = _688 * _675.y;
   float _691 = _688 * _675.z;
   float _692 = _68.x * TEXCOORD_2.w;
   float _693 = _692 * _100;
   float _694 = _693 * _686;
   float _695 = _689 * _689;
   float _696 = _695 * _694;
   float _697 = _690 * _690;
   float _698 = _697 * _694;
   float _699 = _691 * _691;
   float _700 = _699 * _694;
   float _701 = _663 + _696;
   float _702 = _664 + _698;
   float _703 = _665 + _700;
   float _710 = c_mat_opacity * _55.w;
   float _711 = _710 * c_opacity_scale;
   float _712 = _711 * c_global_opacity;
   int _715 = c_bool & 16;
   bool _716 = (_715 == 0);
   if (!_716)
   {
      float _720 = c_tone_control.x * _701;
      float _721 = c_tone_control.x * _702;
      float _722 = c_tone_control.x * _703;
      float _724 = log2(_720);
      float _725 = log2(_721);
      float _726 = log2(_722);
      float _727 = _724 * c_tone_control.y;
      float _728 = _725 * c_tone_control.y;
      float _729 = _726 * c_tone_control.y;
      float _730 = exp2(_727);
      float _731 = exp2(_728);
      float _732 = exp2(_729);
      _782 = _730;
      _783 = _731;
      _784 = _732;
   }
   else
   {
      int _734 = c_bool & 32768;
      bool _735 = (_734 == 0);
      if (_735)
      {
         float _739 = c_tone_control.x * _701;
         float _740 = c_tone_control.x * _702;
         float _741 = c_tone_control.x * _703;
         float _742 = _739 * 2.0f;
         float _743 = _740 * 2.0f;
         float _744 = _741 * 2.0f;
         float _745 = _739 * 0.30000001192092896f;
         float _746 = _740 * 0.30000001192092896f;
         float _747 = _741 * 0.30000001192092896f;
         float _748 = _745 + 0.05000000074505806f;
         float _749 = _746 + 0.05000000074505806f;
         float _750 = _747 + 0.05000000074505806f;
         float _751 = _748 * _742;
         float _752 = _749 * _743;
         float _753 = _750 * _744;
         float _754 = _751 + 0.004000000189989805f;
         float _755 = _752 + 0.004000000189989805f;
         float _756 = _753 + 0.004000000189989805f;
         float _757 = _745 + 0.5f;
         float _758 = _746 + 0.5f;
         float _759 = _747 + 0.5f;
         float _760 = _757 * _742;
         float _761 = _758 * _743;
         float _762 = _759 * _744;
         float _763 = _760 + 0.06000000238418579f;
         float _764 = _761 + 0.06000000238418579f;
         float _765 = _762 + 0.06000000238418579f;
         float _766 = _754 / _763;
         float _767 = _755 / _764;
         float _768 = _756 / _765;
         float _769 = _766 + -0.06666666269302368f;
         float _770 = _767 + -0.06666666269302368f;
         float _771 = _768 + -0.06666666269302368f;
         float _772 = _769 * 1.3790643215179443f;
         float _773 = _770 * 1.3790643215179443f;
         float _774 = _771 * 1.3790643215179443f;
         float _775 = YKR_MaterialCurve(_772, _739);
         float _776 = YKR_MaterialCurve(_773, _740);
         float _777 = YKR_MaterialCurve(_774, _741);
         float _778 = sqrt(_775);
         float _779 = sqrt(_776);
         float _780 = sqrt(_777);
         _782 = _778;
         _783 = _779;
         _784 = _780;
      }
      else
      {
         _782 = _701;
         _783 = _702;
         _784 = _703;
      }
   }
   int _785 = c_bool & 4;
   bool _786 = (_785 == 0);
   if (_786)
   {
      float _788 = saturate(COLOR_1.x);
      float _793 = (c_fog_param[0].x) - _782;
      float _794 = (c_fog_param[0].y) - _783;
      float _795 = (c_fog_param[0].z) - _784;
      float _796 = _793 * _788;
      float _797 = _794 * _788;
      float _798 = _795 * _788;
      _800 = _796;
      _801 = _797;
      _802 = _798;
   }
   else
   {
      _800 = COLOR_1.x;
      _801 = COLOR_1.y;
      _802 = COLOR_1.z;
   }
   float _803 = _800 + _782;
   float _804 = _801 + _783;
   float _805 = _802 + _784;
   float _811 = (c_ps_col_scl[0].x) * _803;
   float _812 = (c_ps_col_scl[0].y) * _804;
   float _813 = (c_ps_col_scl[0].z) * _805;
   float _814 = (c_ps_col_scl[0].w) * _712;
   SV_Target.x = _811;
   SV_Target.y = _812;
   SV_Target.z = _813;
   SV_Target.w = _814;
   int _817 = c_bool & 8;
   bool _818 = (_817 != 0);
   float _819 = select(_818, _803, _811);
   float _820 = select(_818, _804, _812);
   float _821 = select(_818, _805, _813);
   float _822 = select(_818, _712, _814);
   SV_Target.x = _819;
   SV_Target.y = _820;
   SV_Target.z = _821;
   SV_Target.w = _822;
   return SV_Target;
}
