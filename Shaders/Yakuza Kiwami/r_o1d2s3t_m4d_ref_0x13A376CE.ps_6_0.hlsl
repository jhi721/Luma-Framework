// Yakuza Kiwami material r_o1d2s3t_m4d[ref] (0x13A376CE, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s5_t : register(t5);

TextureCube<float4> s6c_t : register(t6);

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

SamplerState s3_s : register(s3);

SamplerState s5_s : register(s5);

SamplerState s6c_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
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
   float4 _56 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _61 = _56.x * _56.x;
   float _62 = _56.y * _56.y;
   float _63 = _56.z * _56.z;
   float4 _64 = s3_t.Sample(s3_s, float2(TEXCOORD.z, TEXCOORD.w));
   float4 _69 = s5_t.Sample(s5_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float4 _72 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _75 = _69.x * 2.0f;
   float _76 = _69.y * 2.0f;
   float _77 = _75 + -1.0f;
   float _78 = _76 + -1.0f;
   float _79 = dot(float2(_77, _78), float2(_77, _78));
   float _80 = 1.0f - _79;
   float _81 = saturate(_80);
   float _82 = sqrt(_81);
   float _83 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _84 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _87 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _90 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _91 = _89 - _90;
   float _92 = _77 * TEXCOORD_6.x;
   float _93 = _77 * TEXCOORD_6.y;
   float _94 = _77 * TEXCOORD_6.z;
   float _95 = _78 * TEXCOORD_6.w;
   float _96 = _95 * _85;
   float _97 = _95 * _88;
   float _98 = _95 * _91;
   float _99 = _96 + _92;
   float _100 = _97 + _93;
   float _101 = _98 + _94;
   float _102 = _82 * TEXCOORD_2.x;
   float _103 = _82 * TEXCOORD_2.y;
   float _104 = _82 * TEXCOORD_2.z;
   float _105 = _99 + _102;
   float _106 = _100 + _103;
   float _107 = _101 + _104;
   float _108 = dot(float3(_105, _106, _107), float3(_105, _106, _107));
   float _109 = rsqrt(_108);
   float _110 = _105 * _109;
   float _111 = _106 * _109;
   float _112 = _107 * _109;
   float _113 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _114 = rsqrt(_113);
   float _115 = _114 * TEXCOORD_3.x;
   float _116 = _114 * TEXCOORD_3.y;
   float _117 = _114 * TEXCOORD_3.z;
   float _122 = -0.0f - (c_light_vec[0].x);
   float _123 = -0.0f - (c_light_vec[0].y);
   float _124 = -0.0f - (c_light_vec[0].z);
   float _125 = dot(float3(_110, _111, _112), float3(_122, _123, _124));
   float _129 = c_rt_size.z * SV_Position.x;
   float _130 = c_rt_size.w * SV_Position.y;
   int _133 = c_bool & 8;
   bool _134 = (_133 == 0);
   float _141;
   float _142;
   float _143;
   float _155;
   float _156;
   float _211;
   float _212;
   float _213;
   float _214;
   float _228;
   float _229;
   float _275;
   float _276;
   float _277;
   float _312;
   float _313;
   float _314;
   float _315;
   float _316;
   float _317;
   float _471;
   float _472;
   float _473;
   float _474;
   float _475;
   float _476;
   float _652;
   float _653;
   float _654;
   float _655;
   float _656;
   float _657;
   float _785;
   float _786;
   float _787;
   float _803;
   float _804;
   float _805;
   if (!_134)
   {
      float4 _136 = s13_t.Sample(s13_s, float2(_129, _130));
      _141 = _136.x;
      _142 = _136.y;
      _143 = _136.w;
   }
   else
   {
      _141 = 1.0f;
      _142 = 1.0f;
      _143 = 1.0f;
   }
   if (!_134)
   {
      float _148 = (c_sdw_param[0].x) * _141;
      float _149 = (c_sdw_param[0].z) * _142;
      float _152 = _148 + (c_sdw_param[0].y);
      float _153 = _149 + (c_sdw_param[0].w);
      _155 = _152;
      _156 = _153;
   }
   else
   {
      _155 = _141;
      _156 = _142;
   }
   float _163 = (c_sdw_param[1].x) * _72.y;
   float _165 = _163 + (c_sdw_param[1].y);
   float _166 = _165 * _72.x;
   float _167 = 1.0f - (c_light_col[5].x);
   float _168 = 1.0f - (c_light_col[5].y);
   float _169 = 1.0f - (c_light_col[5].z);
   float _170 = _166 * _167;
   float _171 = _166 * _168;
   float _172 = _166 * _169;
   float _173 = _170 + (c_light_col[5].x);
   float _174 = _171 + (c_light_col[5].y);
   float _175 = _172 + (c_light_col[5].z);
   float _179 = c_rt_size.z * SV_Position.x;
   float _180 = c_rt_size.w * SV_Position.y;
   float4 _181 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_179, _180)));
   int _188 = c_bool & 16384;
   bool _189 = (_188 == 0);
   if (!_189)
   {
      float _191 = _181.x * 2.0f;
      float _192 = _181.y * 2.0f;
      float _193 = _181.z * 2.0f;
      float _194 = _181.w * 2.0f;
      _211 = _191;
      _212 = _192;
      _213 = _193;
      _214 = _194;
   }
   else
   {
      int _196 = c_bool & 8192;
      bool _197 = (_196 == 0);
      if (!_197)
      {
         float _199 = _181.x * 2.0f;
         float _200 = _181.y * 2.0f;
         float _201 = _181.z * 2.0f;
         float _202 = _181.w * 2.0f;
         _211 = _199;
         _212 = _200;
         _213 = _201;
         _214 = _202;
      }
      else
      {
         float _206 = _181.x / c_tone_control.z;
         float _207 = _181.y / c_tone_control.z;
         float _208 = _181.z / c_tone_control.z;
         float _209 = _181.w / c_tone_control.z;
         _211 = _206;
         _212 = _207;
         _213 = _208;
         _214 = _209;
      }
   }
   int _215 = c_bool & 8;
   bool _216 = (_215 == 0);
   if (!_216)
   {
      float _221 = (c_sdw_param[0].x) * _155;
      float _222 = (c_sdw_param[0].z) * _156;
      float _225 = _221 + (c_sdw_param[0].y);
      float _226 = _222 + (c_sdw_param[0].w);
      _228 = _225;
      _229 = _226;
   }
   else
   {
      _228 = _155;
      _229 = _156;
   }
   float _230 = _229 * _228;
   float _235 = 1.0f - (c_light_col[5].x);
   float _236 = 1.0f - (c_light_col[5].y);
   float _237 = 1.0f - (c_light_col[5].z);
   float _238 = _235 * _230;
   float _239 = _236 * _230;
   float _240 = _237 * _230;
   float _241 = _238 + (c_light_col[5].x);
   float _242 = _239 + (c_light_col[5].y);
   float _243 = _240 + (c_light_col[5].z);
   float _244 = _143 * COLOR.x;
   float _245 = _244 * _173;
   float _246 = _143 * COLOR.y;
   float _247 = _246 * _174;
   float _248 = _143 * COLOR.z;
   float _249 = _248 * _175;
   float _250 = _241 * _173;
   float _251 = _242 * _174;
   float _252 = _243 * _175;
   if (!_216)
   {
      float _256 = (c_sdw_param[2].x) * _241;
      float _257 = (c_sdw_param[2].x) * _242;
      float _258 = (c_sdw_param[2].x) * _243;
      float _260 = _256 + (c_sdw_param[2].y);
      float _261 = _257 + (c_sdw_param[2].y);
      float _262 = _258 + (c_sdw_param[2].y);
      float _264 = (c_sdw_param[2].z) * _173;
      float _265 = (c_sdw_param[2].z) * _174;
      float _266 = (c_sdw_param[2].z) * _175;
      float _268 = _264 + (c_sdw_param[2].w);
      float _269 = _265 + (c_sdw_param[2].w);
      float _270 = _266 + (c_sdw_param[2].w);
      float _271 = _268 * _260;
      float _272 = _269 * _261;
      float _273 = _270 * _262;
      _275 = _271;
      _276 = _272;
      _277 = _273;
   }
   else
   {
      _275 = _250;
      _276 = _251;
      _277 = _252;
   }
   float _278 = _143 * _72.x;
   float _279 = _275 * _278;
   float _280 = _276 * _278;
   float _281 = _277 * _278;
   float _292 = c_mat_emissive.x * _61;
   float _293 = _292 * c_emissive_scale;
   float _294 = c_mat_emissive.y * _62;
   float _295 = _294 * c_emissive_scale;
   float _296 = c_mat_emissive.z * _63;
   float _297 = _296 * c_emissive_scale;
   int _298 = c_bool & 2048;
   bool _299 = (_298 == 0);
   if (_299)
   {
      float _301 = _229 * _211;
      float _302 = _229 * _212;
      float _303 = _229 * _213;
      float _304 = _301 + TEXCOORD_4.x;
      float _305 = _302 + TEXCOORD_4.y;
      float _306 = _303 + TEXCOORD_4.z;
      float _307 = _229 * _214;
      float _308 = _307 + TEXCOORD_5.x;
      float _309 = _307 + TEXCOORD_5.y;
      float _310 = _307 + TEXCOORD_5.z;
      _312 = _304;
      _313 = _305;
      _314 = _306;
      _315 = _308;
      _316 = _309;
      _317 = _310;
   }
   else
   {
      _312 = TEXCOORD_4.x;
      _313 = TEXCOORD_4.y;
      _314 = TEXCOORD_4.z;
      _315 = TEXCOORD_5.x;
      _316 = TEXCOORD_5.y;
      _317 = TEXCOORD_5.z;
   }
   float _322 = -0.0f - (c_light_vec[1].x);
   float _323 = -0.0f - (c_light_vec[1].y);
   float _324 = -0.0f - (c_light_vec[1].z);
   float _325 = dot(float3(_110, _111, _112), float3(_322, _323, _324));
   float _326 = saturate(_325);
   float _331 = saturate(_125);
   float _332 = (c_light_col[0].x) * _331;
   float _333 = (c_light_col[0].y) * _331;
   float _334 = (c_light_col[0].z) * _331;
   float _339 = (c_light_col[1].x) * _326;
   float _340 = (c_light_col[1].y) * _326;
   float _341 = (c_light_col[1].z) * _326;
   float _350 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _351 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _352 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _353 = _332 * _241;
   float _354 = _333 * _242;
   float _355 = _334 * _243;
   float _356 = _339 * _229;
   float _357 = _340 * _229;
   float _358 = _341 * _229;
   float _359 = _356 + _353;
   float _360 = _357 + _354;
   float _361 = _358 + _355;
   float _362 = _350 * _359;
   float _363 = _351 * _360;
   float _364 = _352 * _361;
   float _365 = _293 + c_scene_ambient.x;
   float _366 = _365 + _362;
   float _367 = _295 + c_scene_ambient.y;
   float _368 = _367 + _363;
   float _369 = _297 + c_scene_ambient.z;
   float _370 = _369 + _364;
   float _371 = _366 * _245;
   float _372 = _368 * _247;
   float _373 = _370 * _249;
   float _374 = _312 * _279;
   float _375 = _313 * _280;
   float _376 = _314 * _281;
   float _377 = _371 + _374;
   float _378 = _372 + _375;
   float _379 = _373 + _376;
   float _384 = (c_light_vec[0].x) + _115;
   float _385 = (c_light_vec[0].y) + _116;
   float _386 = (c_light_vec[0].z) + _117;
   float _387 = dot(float3(_384, _385, _386), float3(_384, _385, _386));
   float _388 = rsqrt(_387);
   float _389 = _384 * _388;
   float _390 = -0.0f - _389;
   float _391 = _385 * _388;
   float _392 = -0.0f - _391;
   float _393 = _386 * _388;
   float _394 = -0.0f - _393;
   float _395 = dot(float3(_390, _392, _394), float3(_110, _111, _112));
   float _396 = saturate(_395);
   float _397 = _116 + -0.30000001192092896f;
   float _398 = dot(float3(_115, _397, _117), float3(_115, _397, _117));
   float _399 = rsqrt(_398);
   float _400 = _115 * _399;
   float _401 = -0.0f - _400;
   float _402 = _397 * _399;
   float _403 = -0.0f - _402;
   float _404 = _117 * _399;
   float _405 = -0.0f - _404;
   float _406 = dot(float3(_401, _403, _405), float3(_110, _111, _112));
   float _407 = saturate(_406);
   float _410 = log2(_396);
   float _411 = log2(_407);
   float _412 = _410 * c_mat_power;
   float _413 = _411 * c_mat_power;
   float _414 = exp2(_412);
   float _415 = exp2(_413);
   float _418 = _414 * (c_light_col[0].x);
   float _419 = _414 * (c_light_col[0].y);
   float _420 = _414 * (c_light_col[0].z);
   float _421 = _415 * 2.0f;
   float _422 = _421 * (c_light_vec[5].x);
   float _423 = _422 * (c_light_col[1].x);
   float _424 = _422 * (c_light_col[1].y);
   float _425 = _422 * (c_light_col[1].z);
   float _426 = _418 * _241;
   float _427 = _419 * _242;
   float _428 = _420 * _243;
   float _429 = _423 * _326;
   float _430 = _424 * _326;
   float _431 = _425 * _326;
   float _432 = _429 + _426;
   float _433 = _430 + _427;
   float _434 = _431 + _428;
   float _441 = c_mat_specular.x * c_specular_scale.x;
   float _442 = _441 * _245;
   float _443 = _442 * _432;
   float _444 = c_mat_specular.y * c_specular_scale.y;
   float _445 = _444 * _247;
   float _446 = _445 * _433;
   float _447 = c_mat_specular.z * c_specular_scale.z;
   float _448 = _447 * _249;
   float _449 = _448 * _434;
   float _450 = _315 * _279;
   float _451 = _450 * _415;
   float _452 = _316 * _280;
   float _453 = _452 * _415;
   float _454 = _317 * _281;
   float _455 = _454 * _415;
   float _456 = _443 + _451;
   float _457 = _446 + _453;
   float _458 = _449 + _455;
   if (!_299)
   {
      float _460 = _229 * _211;
      float _461 = _229 * _212;
      float _462 = _229 * _213;
      float _463 = _377 + _460;
      float _464 = _378 + _461;
      float _465 = _379 + _462;
      float _466 = _229 * _214;
      float _467 = _456 + _466;
      float _468 = _457 + _466;
      float _469 = _458 + _466;
      _471 = _463;
      _472 = _464;
      _473 = _465;
      _474 = _467;
      _475 = _468;
      _476 = _469;
   }
   else
   {
      _471 = _377;
      _472 = _378;
      _473 = _379;
      _474 = _456;
      _475 = _457;
      _476 = _458;
   }
   int _477 = c_bool & 128;
   bool _478 = (_477 != 0);
   float _479 = select(_478, 0.0f, _474);
   float _480 = select(_478, 0.0f, _475);
   float _481 = select(_478, 0.0f, _476);
   float _482 = _471 * _61;
   float _483 = _472 * _62;
   float _484 = _473 * _63;
   float _485 = _479 * _64.x;
   float _486 = _480 * _64.y;
   float _487 = _481 * _64.z;
   int _488 = c_bool & 32768;
   bool _489 = (_488 == 0);
   if (!_489)
   {
      float _493 = (c_ps_user[11].x) * _61;
      float _494 = (c_ps_user[11].x) * _62;
      float _495 = (c_ps_user[11].x) * _63;
      float _497 = _110 + 1.0f;
      float _498 = _111 + 1.0f;
      float _499 = _112 + 1.0f;
      float _500 = _497 * 0.5f;
      float _501 = _498 * 0.5f;
      float _502 = _499 * 0.5f;
      float _503 = saturate(_500);
      float _504 = saturate(_501);
      float _505 = saturate(_502);
      float _506 = (c_ps_user[11].y) * _503;
      float _507 = _504 * (c_ps_user[11].y);
      float _508 = _505 * (c_ps_user[11].y);
      float _510 = (c_ps_user[11].z) * _64.x;
      float _511 = (c_ps_user[11].z) * _64.y;
      float _512 = (c_ps_user[11].z) * _64.z;
      float _514 = c_mat_power * 0.009999999776482582f;
      float _515 = _514 * (c_ps_user[11].w);
      float _518 = (c_ps_user[12].x) * _241;
      float _519 = (c_ps_user[12].x) * _242;
      float _520 = (c_ps_user[12].x) * _243;
      float _522 = (c_ps_user[12].y) * _245;
      float _523 = (c_ps_user[12].y) * _247;
      float _524 = (c_ps_user[12].y) * _249;
      float _526 = (c_ps_user[12].z) * _279;
      float _527 = (c_ps_user[12].z) * _280;
      float _528 = (c_ps_user[12].z) * _281;
      float _534 = -0.0f - _115;
      float _535 = -0.0f - _116;
      float _536 = -0.0f - _117;
      float _537 = dot(float3(_534, _535, _536), float3(_110, _111, _112));
      float _538 = 1.0f - _537;
      float _539 = _538 * _538;
      float _540 = _538 * 0.9722222089767456f;
      float _541 = _539 * _539;
      float _542 = _541 * _540;
      float _543 = _542 + 0.02777777425944805f;
      float _544 = _543 * (c_ps_user[13].z);
      float _546 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _547 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _548 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _551 = (c_ps_user[14].x) * _293;
      float _552 = (c_ps_user[14].x) * _295;
      float _553 = (c_ps_user[14].x) * _297;
      float _555 = (c_ps_user[14].y) * _332;
      float _556 = (c_ps_user[14].y) * _333;
      float _557 = (c_ps_user[14].y) * _334;
      float _559 = (c_ps_user[14].z) * _339;
      float _560 = (c_ps_user[14].z) * _340;
      float _561 = (c_ps_user[14].z) * _341;
      float _563 = (c_ps_user[14].w) * _312;
      float _564 = (c_ps_user[14].w) * _313;
      float _565 = (c_ps_user[14].w) * _314;
      float _568 = (c_ps_user[15].x) * _418;
      float _569 = (c_ps_user[15].x) * _419;
      float _570 = (c_ps_user[15].x) * _420;
      float _572 = _441 * _423;
      float _573 = _572 * (c_ps_user[15].y);
      float _574 = _444 * _424;
      float _575 = _574 * (c_ps_user[15].y);
      float _576 = _447 * _425;
      float _577 = _576 * (c_ps_user[15].y);
      float _579 = (c_ps_user[15].z) * _315;
      float _580 = (c_ps_user[15].z) * _316;
      float _581 = (c_ps_user[15].z) * _317;
      float _582 = _573 + _579;
      float _583 = _582 * _415;
      float _584 = _555 + (c_ps_user[13].x);
      float _585 = _584 + _559;
      float _586 = c_diffuse_scale.x * _585;
      float _587 = _586 * c_mat_diffuse.x;
      float _588 = (c_ps_user[13].y) + _510;
      float _589 = _588 + _568;
      float _590 = c_mat_specular.x * _589;
      float _591 = _590 * c_specular_scale.x;
      float _592 = _506 + _493;
      float _593 = _592 + _515;
      float _594 = _593 + (c_ps_user[12].w);
      float _595 = _594 + _518;
      float _596 = _595 + _522;
      float _597 = _596 + _526;
      float _598 = _597 + _546;
      float _599 = _598 + _551;
      float _600 = _599 + _563;
      float _601 = _600 + _544;
      float _602 = _601 + _583;
      float _603 = _602 + _587;
      float _604 = _603 + _591;
      float _605 = _575 + _580;
      float _606 = _605 * _415;
      float _607 = _556 + (c_ps_user[13].x);
      float _608 = _607 + _560;
      float _609 = c_diffuse_scale.y * _608;
      float _610 = _609 * c_mat_diffuse.y;
      float _611 = (c_ps_user[13].y) + _511;
      float _612 = _611 + _569;
      float _613 = c_mat_specular.y * _612;
      float _614 = _613 * c_specular_scale.y;
      float _615 = _507 + _494;
      float _616 = _615 + _515;
      float _617 = _616 + (c_ps_user[12].w);
      float _618 = _617 + _519;
      float _619 = _618 + _523;
      float _620 = _619 + _527;
      float _621 = _620 + _547;
      float _622 = _621 + _552;
      float _623 = _622 + _564;
      float _624 = _623 + _544;
      float _625 = _624 + _606;
      float _626 = _625 + _610;
      float _627 = _626 + _614;
      float _628 = _577 + _581;
      float _629 = _628 * _415;
      float _630 = _557 + (c_ps_user[13].x);
      float _631 = _630 + _561;
      float _632 = c_diffuse_scale.z * _631;
      float _633 = _632 * c_mat_diffuse.z;
      float _634 = (c_ps_user[13].y) + _512;
      float _635 = _634 + _570;
      float _636 = c_mat_specular.z * _635;
      float _637 = _636 * c_specular_scale.z;
      float _638 = _515 + _495;
      float _639 = _638 + _508;
      float _640 = _639 + (c_ps_user[12].w);
      float _641 = _640 + _520;
      float _642 = _641 + _524;
      float _643 = _642 + _528;
      float _644 = _643 + _548;
      float _645 = _644 + _553;
      float _646 = _645 + _565;
      float _647 = _646 + _544;
      float _648 = _647 + _629;
      float _649 = _648 + _633;
      float _650 = _649 + _637;
      _652 = _604;
      _653 = _627;
      _654 = _650;
      _655 = 0.0f;
      _656 = 0.0f;
      _657 = 0.0f;
   }
   else
   {
      _652 = _482;
      _653 = _483;
      _654 = _484;
      _655 = _485;
      _656 = _486;
      _657 = _487;
   }
   float _658 = _655 + _652;
   float _659 = _656 + _653;
   float _660 = _657 + _654;
   float _661 = dot(float3(_115, _116, _117), float3(_110, _111, _112));
   float _662 = _661 * 2.0f;
   float _663 = _662 * _110;
   float _664 = _662 * _111;
   float _665 = _662 * _112;
   float _666 = _115 - _663;
   float _667 = _116 - _664;
   float _668 = _117 - _665;
   float _669 = -0.0f - _666;
   float4 _670 = s6c_t.Sample(s6c_s, float3(_669, _667, _668));
   float _677 = _156 * _155;
   float _678 = _125 + 0.75f;
   float _679 = saturate(_678);
   float _680 = _677 * _679;
   float _681 = max(_680, (c_light_vec[5].y));
   float _682 = max(_670.w, 0.003921568859368563f);
   float _683 = 0.25f / _682;
   float _684 = _683 * _670.x;
   float _685 = _683 * _670.y;
   float _686 = _683 * _670.z;
   float _687 = _64.w * TEXCOORD_2.w;
   float _688 = _72.x * _687;
   float _689 = _688 * _64.x;
   float _690 = _689 * _173;
   float _691 = _690 * _681;
   float _692 = _684 * _684;
   float _693 = _692 * _691;
   float _694 = _688 * _64.y;
   float _695 = _694 * _174;
   float _696 = _695 * _681;
   float _697 = _685 * _685;
   float _698 = _697 * _696;
   float _699 = _688 * _64.z;
   float _700 = _699 * _175;
   float _701 = _700 * _681;
   float _702 = _686 * _686;
   float _703 = _702 * _701;
   float _704 = _658 + _693;
   float _705 = _659 + _698;
   float _706 = _660 + _703;
   float _713 = c_mat_opacity * _56.w;
   float _714 = _713 * c_opacity_scale;
   float _715 = _714 * c_global_opacity;
   int _718 = c_bool & 16;
   bool _719 = (_718 == 0);
   if (!_719)
   {
      float _723 = c_tone_control.x * _704;
      float _724 = c_tone_control.x * _705;
      float _725 = c_tone_control.x * _706;
      float _727 = log2(_723);
      float _728 = log2(_724);
      float _729 = log2(_725);
      float _730 = _727 * c_tone_control.y;
      float _731 = _728 * c_tone_control.y;
      float _732 = _729 * c_tone_control.y;
      float _733 = exp2(_730);
      float _734 = exp2(_731);
      float _735 = exp2(_732);
      _785 = _733;
      _786 = _734;
      _787 = _735;
   }
   else
   {
      int _737 = c_bool & 32768;
      bool _738 = (_737 == 0);
      if (_738)
      {
         float _742 = c_tone_control.x * _704;
         float _743 = c_tone_control.x * _705;
         float _744 = c_tone_control.x * _706;
         float _745 = _742 * 2.0f;
         float _746 = _743 * 2.0f;
         float _747 = _744 * 2.0f;
         float _748 = _742 * 0.30000001192092896f;
         float _749 = _743 * 0.30000001192092896f;
         float _750 = _744 * 0.30000001192092896f;
         float _751 = _748 + 0.05000000074505806f;
         float _752 = _749 + 0.05000000074505806f;
         float _753 = _750 + 0.05000000074505806f;
         float _754 = _751 * _745;
         float _755 = _752 * _746;
         float _756 = _753 * _747;
         float _757 = _754 + 0.004000000189989805f;
         float _758 = _755 + 0.004000000189989805f;
         float _759 = _756 + 0.004000000189989805f;
         float _760 = _748 + 0.5f;
         float _761 = _749 + 0.5f;
         float _762 = _750 + 0.5f;
         float _763 = _760 * _745;
         float _764 = _761 * _746;
         float _765 = _762 * _747;
         float _766 = _763 + 0.06000000238418579f;
         float _767 = _764 + 0.06000000238418579f;
         float _768 = _765 + 0.06000000238418579f;
         float _769 = _757 / _766;
         float _770 = _758 / _767;
         float _771 = _759 / _768;
         float _772 = _769 + -0.06666666269302368f;
         float _773 = _770 + -0.06666666269302368f;
         float _774 = _771 + -0.06666666269302368f;
         float _775 = _772 * 1.3790643215179443f;
         float _776 = _773 * 1.3790643215179443f;
         float _777 = _774 * 1.3790643215179443f;
         float _778 = YKR_MaterialCurve(_775, _742);
         float _779 = YKR_MaterialCurve(_776, _743);
         float _780 = YKR_MaterialCurve(_777, _744);
         float _781 = sqrt(_778);
         float _782 = sqrt(_779);
         float _783 = sqrt(_780);
         _785 = _781;
         _786 = _782;
         _787 = _783;
      }
      else
      {
         _785 = _704;
         _786 = _705;
         _787 = _706;
      }
   }
   int _788 = c_bool & 4;
   bool _789 = (_788 == 0);
   if (_789)
   {
      float _791 = saturate(COLOR_1.x);
      float _796 = (c_fog_param[0].x) - _785;
      float _797 = (c_fog_param[0].y) - _786;
      float _798 = (c_fog_param[0].z) - _787;
      float _799 = _796 * _791;
      float _800 = _797 * _791;
      float _801 = _798 * _791;
      _803 = _799;
      _804 = _800;
      _805 = _801;
   }
   else
   {
      _803 = COLOR_1.x;
      _804 = COLOR_1.y;
      _805 = COLOR_1.z;
   }
   float _806 = _803 + _785;
   float _807 = _804 + _786;
   float _808 = _805 + _787;
   float _814 = (c_ps_col_scl[0].x) * _806;
   float _815 = (c_ps_col_scl[0].y) * _807;
   float _816 = (c_ps_col_scl[0].z) * _808;
   float _817 = (c_ps_col_scl[0].w) * _715;
   SV_Target.x = _814;
   SV_Target.y = _815;
   SV_Target.z = _816;
   SV_Target.w = _817;
   int _820 = c_bool & 8;
   bool _821 = (_820 != 0);
   float _822 = select(_821, _806, _814);
   float _823 = select(_821, _807, _815);
   float _824 = select(_821, _808, _816);
   float _825 = select(_821, _715, _817);
   SV_Target.x = _822;
   SV_Target.y = _823;
   SV_Target.z = _824;
   SV_Target.w = _825;
   return SV_Target;
}
