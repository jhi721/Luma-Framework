// Yakuza Kiwami material r_o1du2t_b(vr)3dut_m4d[ref] (0x076D0F60, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

TextureCube<float4> s2c_t : register(t2);

Texture2D<float4> s3_t : register(t3);

Texture2D<float4> s4_t : register(t4);

Texture2D<float4> s5_t : register(t5);

Texture2D<float4> s6_t : register(t6);

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

SamplerState s1_s : register(s1);

SamplerState s2c_s : register(s2);

SamplerState s3_s : register(s3);

SamplerState s4_s : register(s4);

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

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
   float4 _63 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _68 = _63.x * _63.x;
   float _69 = _63.y * _63.y;
   float _70 = _63.z * _63.z;
   float _74 = (c_attribute_ps[0].x) * TEXCOORD.x;
   float _75 = (c_attribute_ps[0].y) * TEXCOORD.y;
   float4 _76 = s3_t.Sample(s3_s, float2(_74, _75));
   float _82 = (c_attribute_ps[0].z) * TEXCOORD.z;
   float _83 = (c_attribute_ps[0].w) * TEXCOORD.w;
   float4 _84 = s5_t.Sample(s5_s, float2(_82, _83));
   float4 _87 = s1_t.Sample(s1_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _92 = _87.x * _87.x;
   float _93 = _87.y * _87.y;
   float _94 = _87.z * _87.z;
   float _98 = (c_attribute_ps[1].x) * TEXCOORD_1.x;
   float _99 = (c_attribute_ps[1].y) * TEXCOORD_1.y;
   float4 _100 = s4_t.Sample(s4_s, float2(_98, _99));
   float _106 = (c_attribute_ps[1].z) * TEXCOORD_1.x;
   float _107 = (c_attribute_ps[1].w) * TEXCOORD_1.y;
   float4 _108 = s6_t.Sample(s6_s, float2(_106, _107));
   float4 _111 = s7_t.Sample(s7_s, float2(TEXCOORD_1.z, TEXCOORD_1.w));
   float _114 = _87.w * COLOR_1.w;
   float _115 = _100.x - _76.x;
   float _116 = _100.y - _76.y;
   float _117 = _115 * _114;
   float _118 = _116 * _114;
   float _119 = _117 + _76.x;
   float _120 = _118 + _76.y;
   float _121 = _92 - _68;
   float _122 = _93 - _69;
   float _123 = _94 - _70;
   float _124 = _121 * _114;
   float _125 = _122 * _114;
   float _126 = _123 * _114;
   float _127 = _124 + _68;
   float _128 = _125 + _69;
   float _129 = _126 + _70;
   float _130 = _108.x - _84.x;
   float _131 = _108.y - _84.y;
   float _132 = _130 * _114;
   float _133 = _131 * _114;
   float _134 = _132 + _84.x;
   float _135 = _133 + _84.y;
   float _136 = _134 * 2.0f;
   float _137 = _135 * 2.0f;
   float _138 = _136 + -1.0f;
   float _139 = _137 + -1.0f;
   float _140 = dot(float2(_138, _139), float2(_138, _139));
   float _141 = 1.0f - _140;
   float _142 = saturate(_141);
   float _143 = sqrt(_142);
   float _144 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _145 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _146 = _144 - _145;
   float _147 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _148 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _149 = _147 - _148;
   float _150 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _151 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _152 = _150 - _151;
   float _153 = _138 * TEXCOORD_6.x;
   float _154 = _138 * TEXCOORD_6.y;
   float _155 = _138 * TEXCOORD_6.z;
   float _156 = _139 * TEXCOORD_6.w;
   float _157 = _156 * _146;
   float _158 = _156 * _149;
   float _159 = _156 * _152;
   float _160 = _157 + _153;
   float _161 = _158 + _154;
   float _162 = _159 + _155;
   float _163 = _143 * TEXCOORD_2.x;
   float _164 = _143 * TEXCOORD_2.y;
   float _165 = _143 * TEXCOORD_2.z;
   float _166 = _160 + _163;
   float _167 = _161 + _164;
   float _168 = _162 + _165;
   float _169 = dot(float3(_166, _167, _168), float3(_166, _167, _168));
   float _170 = rsqrt(_169);
   float _171 = _166 * _170;
   float _172 = _167 * _170;
   float _173 = _168 * _170;
   float _174 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _175 = rsqrt(_174);
   float _176 = _175 * TEXCOORD_3.x;
   float _177 = _175 * TEXCOORD_3.y;
   float _178 = _175 * TEXCOORD_3.z;
   float _183 = -0.0f - (c_light_vec[0].x);
   float _184 = -0.0f - (c_light_vec[0].y);
   float _185 = -0.0f - (c_light_vec[0].z);
   float _186 = dot(float3(_171, _172, _173), float3(_183, _184, _185));
   float _190 = c_rt_size.z * SV_Position.x;
   float _191 = c_rt_size.w * SV_Position.y;
   int _194 = c_bool & 8;
   bool _195 = (_194 == 0);
   float _202;
   float _203;
   float _204;
   float _216;
   float _217;
   float _272;
   float _273;
   float _274;
   float _275;
   float _289;
   float _290;
   float _336;
   float _337;
   float _338;
   float _373;
   float _374;
   float _375;
   float _376;
   float _377;
   float _378;
   float _532;
   float _533;
   float _534;
   float _535;
   float _536;
   float _537;
   float _709;
   float _710;
   float _711;
   float _712;
   float _713;
   float _714;
   float _837;
   float _838;
   float _839;
   float _855;
   float _856;
   float _857;
   if (!_195)
   {
      float4 _197 = s13_t.Sample(s13_s, float2(_190, _191));
      _202 = _197.x;
      _203 = _197.y;
      _204 = _197.w;
   }
   else
   {
      _202 = 1.0f;
      _203 = 1.0f;
      _204 = 1.0f;
   }
   if (!_195)
   {
      float _209 = (c_sdw_param[0].x) * _202;
      float _210 = (c_sdw_param[0].z) * _203;
      float _213 = _209 + (c_sdw_param[0].y);
      float _214 = _210 + (c_sdw_param[0].w);
      _216 = _213;
      _217 = _214;
   }
   else
   {
      _216 = _202;
      _217 = _203;
   }
   float _224 = (c_sdw_param[1].x) * _111.y;
   float _226 = _224 + (c_sdw_param[1].y);
   float _227 = _226 * _111.x;
   float _228 = 1.0f - (c_light_col[5].x);
   float _229 = 1.0f - (c_light_col[5].y);
   float _230 = 1.0f - (c_light_col[5].z);
   float _231 = _227 * _228;
   float _232 = _227 * _229;
   float _233 = _227 * _230;
   float _234 = _231 + (c_light_col[5].x);
   float _235 = _232 + (c_light_col[5].y);
   float _236 = _233 + (c_light_col[5].z);
   float _240 = c_rt_size.z * SV_Position.x;
   float _241 = c_rt_size.w * SV_Position.y;
   float4 _242 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_240, _241)));
   int _249 = c_bool & 16384;
   bool _250 = (_249 == 0);
   if (!_250)
   {
      float _252 = _242.x * 2.0f;
      float _253 = _242.y * 2.0f;
      float _254 = _242.z * 2.0f;
      float _255 = _242.w * 2.0f;
      _272 = _252;
      _273 = _253;
      _274 = _254;
      _275 = _255;
   }
   else
   {
      int _257 = c_bool & 8192;
      bool _258 = (_257 == 0);
      if (!_258)
      {
         float _260 = _242.x * 2.0f;
         float _261 = _242.y * 2.0f;
         float _262 = _242.z * 2.0f;
         float _263 = _242.w * 2.0f;
         _272 = _260;
         _273 = _261;
         _274 = _262;
         _275 = _263;
      }
      else
      {
         float _267 = _242.x / c_tone_control.z;
         float _268 = _242.y / c_tone_control.z;
         float _269 = _242.z / c_tone_control.z;
         float _270 = _242.w / c_tone_control.z;
         _272 = _267;
         _273 = _268;
         _274 = _269;
         _275 = _270;
      }
   }
   int _276 = c_bool & 8;
   bool _277 = (_276 == 0);
   if (!_277)
   {
      float _282 = (c_sdw_param[0].x) * _216;
      float _283 = (c_sdw_param[0].z) * _217;
      float _286 = _282 + (c_sdw_param[0].y);
      float _287 = _283 + (c_sdw_param[0].w);
      _289 = _286;
      _290 = _287;
   }
   else
   {
      _289 = _216;
      _290 = _217;
   }
   float _291 = _290 * _289;
   float _296 = 1.0f - (c_light_col[5].x);
   float _297 = 1.0f - (c_light_col[5].y);
   float _298 = 1.0f - (c_light_col[5].z);
   float _299 = _296 * _291;
   float _300 = _297 * _291;
   float _301 = _298 * _291;
   float _302 = _299 + (c_light_col[5].x);
   float _303 = _300 + (c_light_col[5].y);
   float _304 = _301 + (c_light_col[5].z);
   float _305 = _204 * COLOR.x;
   float _306 = _305 * _234;
   float _307 = _204 * COLOR.y;
   float _308 = _307 * _235;
   float _309 = _204 * COLOR.z;
   float _310 = _309 * _236;
   float _311 = _302 * _234;
   float _312 = _303 * _235;
   float _313 = _304 * _236;
   if (!_277)
   {
      float _317 = (c_sdw_param[2].x) * _302;
      float _318 = (c_sdw_param[2].x) * _303;
      float _319 = (c_sdw_param[2].x) * _304;
      float _321 = _317 + (c_sdw_param[2].y);
      float _322 = _318 + (c_sdw_param[2].y);
      float _323 = _319 + (c_sdw_param[2].y);
      float _325 = (c_sdw_param[2].z) * _234;
      float _326 = (c_sdw_param[2].z) * _235;
      float _327 = (c_sdw_param[2].z) * _236;
      float _329 = _325 + (c_sdw_param[2].w);
      float _330 = _326 + (c_sdw_param[2].w);
      float _331 = _327 + (c_sdw_param[2].w);
      float _332 = _329 * _321;
      float _333 = _330 * _322;
      float _334 = _331 * _323;
      _336 = _332;
      _337 = _333;
      _338 = _334;
   }
   else
   {
      _336 = _311;
      _337 = _312;
      _338 = _313;
   }
   float _339 = _204 * _111.x;
   float _340 = _336 * _339;
   float _341 = _337 * _339;
   float _342 = _338 * _339;
   float _353 = c_mat_emissive.x * _127;
   float _354 = _353 * c_emissive_scale;
   float _355 = c_mat_emissive.y * _128;
   float _356 = _355 * c_emissive_scale;
   float _357 = c_mat_emissive.z * _129;
   float _358 = _357 * c_emissive_scale;
   int _359 = c_bool & 2048;
   bool _360 = (_359 == 0);
   if (_360)
   {
      float _362 = _290 * _272;
      float _363 = _290 * _273;
      float _364 = _290 * _274;
      float _365 = _362 + TEXCOORD_4.x;
      float _366 = _363 + TEXCOORD_4.y;
      float _367 = _364 + TEXCOORD_4.z;
      float _368 = _290 * _275;
      float _369 = _368 + TEXCOORD_5.x;
      float _370 = _368 + TEXCOORD_5.y;
      float _371 = _368 + TEXCOORD_5.z;
      _373 = _365;
      _374 = _366;
      _375 = _367;
      _376 = _369;
      _377 = _370;
      _378 = _371;
   }
   else
   {
      _373 = TEXCOORD_4.x;
      _374 = TEXCOORD_4.y;
      _375 = TEXCOORD_4.z;
      _376 = TEXCOORD_5.x;
      _377 = TEXCOORD_5.y;
      _378 = TEXCOORD_5.z;
   }
   float _383 = -0.0f - (c_light_vec[1].x);
   float _384 = -0.0f - (c_light_vec[1].y);
   float _385 = -0.0f - (c_light_vec[1].z);
   float _386 = dot(float3(_171, _172, _173), float3(_383, _384, _385));
   float _387 = saturate(_386);
   float _392 = saturate(_186);
   float _393 = (c_light_col[0].x) * _392;
   float _394 = (c_light_col[0].y) * _392;
   float _395 = (c_light_col[0].z) * _392;
   float _400 = (c_light_col[1].x) * _387;
   float _401 = (c_light_col[1].y) * _387;
   float _402 = (c_light_col[1].z) * _387;
   float _411 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _412 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _413 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _414 = _393 * _302;
   float _415 = _394 * _303;
   float _416 = _395 * _304;
   float _417 = _400 * _290;
   float _418 = _401 * _290;
   float _419 = _402 * _290;
   float _420 = _417 + _414;
   float _421 = _418 + _415;
   float _422 = _419 + _416;
   float _423 = _411 * _420;
   float _424 = _412 * _421;
   float _425 = _413 * _422;
   float _426 = _354 + c_scene_ambient.x;
   float _427 = _426 + _423;
   float _428 = _356 + c_scene_ambient.y;
   float _429 = _428 + _424;
   float _430 = _358 + c_scene_ambient.z;
   float _431 = _430 + _425;
   float _432 = _427 * _306;
   float _433 = _429 * _308;
   float _434 = _431 * _310;
   float _435 = _373 * _340;
   float _436 = _374 * _341;
   float _437 = _375 * _342;
   float _438 = _432 + _435;
   float _439 = _433 + _436;
   float _440 = _434 + _437;
   float _445 = (c_light_vec[0].x) + _176;
   float _446 = (c_light_vec[0].y) + _177;
   float _447 = (c_light_vec[0].z) + _178;
   float _448 = dot(float3(_445, _446, _447), float3(_445, _446, _447));
   float _449 = rsqrt(_448);
   float _450 = _445 * _449;
   float _451 = -0.0f - _450;
   float _452 = _446 * _449;
   float _453 = -0.0f - _452;
   float _454 = _447 * _449;
   float _455 = -0.0f - _454;
   float _456 = dot(float3(_451, _453, _455), float3(_171, _172, _173));
   float _457 = saturate(_456);
   float _458 = _177 + -0.30000001192092896f;
   float _459 = dot(float3(_176, _458, _178), float3(_176, _458, _178));
   float _460 = rsqrt(_459);
   float _461 = _176 * _460;
   float _462 = -0.0f - _461;
   float _463 = _458 * _460;
   float _464 = -0.0f - _463;
   float _465 = _178 * _460;
   float _466 = -0.0f - _465;
   float _467 = dot(float3(_462, _464, _466), float3(_171, _172, _173));
   float _468 = saturate(_467);
   float _471 = log2(_457);
   float _472 = log2(_468);
   float _473 = _471 * c_mat_power;
   float _474 = _472 * c_mat_power;
   float _475 = exp2(_473);
   float _476 = exp2(_474);
   float _479 = _475 * (c_light_col[0].x);
   float _480 = _475 * (c_light_col[0].y);
   float _481 = _475 * (c_light_col[0].z);
   float _482 = _476 * 2.0f;
   float _483 = _482 * (c_light_vec[5].x);
   float _484 = _483 * (c_light_col[1].x);
   float _485 = _483 * (c_light_col[1].y);
   float _486 = _483 * (c_light_col[1].z);
   float _487 = _479 * _302;
   float _488 = _480 * _303;
   float _489 = _481 * _304;
   float _490 = _484 * _387;
   float _491 = _485 * _387;
   float _492 = _486 * _387;
   float _493 = _490 + _487;
   float _494 = _491 + _488;
   float _495 = _492 + _489;
   float _502 = c_mat_specular.x * c_specular_scale.x;
   float _503 = _502 * _306;
   float _504 = _503 * _493;
   float _505 = c_mat_specular.y * c_specular_scale.y;
   float _506 = _505 * _308;
   float _507 = _506 * _494;
   float _508 = c_mat_specular.z * c_specular_scale.z;
   float _509 = _508 * _310;
   float _510 = _509 * _495;
   float _511 = _376 * _340;
   float _512 = _511 * _476;
   float _513 = _377 * _341;
   float _514 = _513 * _476;
   float _515 = _378 * _342;
   float _516 = _515 * _476;
   float _517 = _504 + _512;
   float _518 = _507 + _514;
   float _519 = _510 + _516;
   if (!_360)
   {
      float _521 = _290 * _272;
      float _522 = _290 * _273;
      float _523 = _290 * _274;
      float _524 = _438 + _521;
      float _525 = _439 + _522;
      float _526 = _440 + _523;
      float _527 = _290 * _275;
      float _528 = _517 + _527;
      float _529 = _518 + _527;
      float _530 = _519 + _527;
      _532 = _524;
      _533 = _525;
      _534 = _526;
      _535 = _528;
      _536 = _529;
      _537 = _530;
   }
   else
   {
      _532 = _438;
      _533 = _439;
      _534 = _440;
      _535 = _517;
      _536 = _518;
      _537 = _519;
   }
   int _538 = c_bool & 128;
   bool _539 = (_538 != 0);
   float _540 = select(_539, 0.0f, _535);
   float _541 = select(_539, 0.0f, _536);
   float _542 = select(_539, 0.0f, _537);
   float _543 = _532 * _127;
   float _544 = _533 * _128;
   float _545 = _534 * _129;
   float _546 = _540 * _120;
   float _547 = _541 * _120;
   float _548 = _542 * _120;
   int _549 = c_bool & 32768;
   bool _550 = (_549 == 0);
   if (!_550)
   {
      float _554 = (c_ps_user[11].x) * _127;
      float _555 = (c_ps_user[11].x) * _128;
      float _556 = (c_ps_user[11].x) * _129;
      float _558 = _171 + 1.0f;
      float _559 = _172 + 1.0f;
      float _560 = _173 + 1.0f;
      float _561 = _558 * 0.5f;
      float _562 = _559 * 0.5f;
      float _563 = _560 * 0.5f;
      float _564 = saturate(_561);
      float _565 = saturate(_562);
      float _566 = saturate(_563);
      float _567 = (c_ps_user[11].y) * _564;
      float _568 = _565 * (c_ps_user[11].y);
      float _569 = _566 * (c_ps_user[11].y);
      float _571 = (c_ps_user[11].z) * _120;
      float _573 = c_mat_power * 0.009999999776482582f;
      float _574 = _573 * (c_ps_user[11].w);
      float _577 = (c_ps_user[12].x) * _302;
      float _578 = (c_ps_user[12].x) * _303;
      float _579 = (c_ps_user[12].x) * _304;
      float _581 = (c_ps_user[12].y) * _306;
      float _582 = (c_ps_user[12].y) * _308;
      float _583 = (c_ps_user[12].y) * _310;
      float _585 = (c_ps_user[12].z) * _340;
      float _586 = (c_ps_user[12].z) * _341;
      float _587 = (c_ps_user[12].z) * _342;
      float _593 = -0.0f - _176;
      float _594 = -0.0f - _177;
      float _595 = -0.0f - _178;
      float _596 = dot(float3(_593, _594, _595), float3(_171, _172, _173));
      float _597 = 1.0f - _596;
      float _598 = _597 * _597;
      float _599 = _597 * 0.9722222089767456f;
      float _600 = _598 * _598;
      float _601 = _600 * _599;
      float _602 = _601 + 0.02777777425944805f;
      float _603 = _602 * (c_ps_user[13].z);
      float _605 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _606 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _607 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _610 = (c_ps_user[14].x) * _354;
      float _611 = (c_ps_user[14].x) * _356;
      float _612 = (c_ps_user[14].x) * _358;
      float _614 = (c_ps_user[14].y) * _393;
      float _615 = (c_ps_user[14].y) * _394;
      float _616 = (c_ps_user[14].y) * _395;
      float _618 = (c_ps_user[14].z) * _400;
      float _619 = (c_ps_user[14].z) * _401;
      float _620 = (c_ps_user[14].z) * _402;
      float _622 = (c_ps_user[14].w) * _373;
      float _623 = (c_ps_user[14].w) * _374;
      float _624 = (c_ps_user[14].w) * _375;
      float _627 = (c_ps_user[15].x) * _479;
      float _628 = (c_ps_user[15].x) * _480;
      float _629 = (c_ps_user[15].x) * _481;
      float _631 = _502 * _484;
      float _632 = _631 * (c_ps_user[15].y);
      float _633 = _505 * _485;
      float _634 = _633 * (c_ps_user[15].y);
      float _635 = _508 * _486;
      float _636 = _635 * (c_ps_user[15].y);
      float _638 = (c_ps_user[15].z) * _376;
      float _639 = (c_ps_user[15].z) * _377;
      float _640 = (c_ps_user[15].z) * _378;
      float _641 = _632 + _638;
      float _642 = _641 * _476;
      float _643 = _614 + (c_ps_user[13].x);
      float _644 = _643 + _618;
      float _645 = c_diffuse_scale.x * _644;
      float _646 = _645 * c_mat_diffuse.x;
      float _647 = (c_ps_user[13].y) + _571;
      float _648 = _647 + _627;
      float _649 = c_mat_specular.x * _648;
      float _650 = _649 * c_specular_scale.x;
      float _651 = _567 + _554;
      float _652 = _651 + _574;
      float _653 = _652 + (c_ps_user[12].w);
      float _654 = _653 + _577;
      float _655 = _654 + _581;
      float _656 = _655 + _585;
      float _657 = _656 + _605;
      float _658 = _657 + _610;
      float _659 = _658 + _622;
      float _660 = _659 + _603;
      float _661 = _660 + _642;
      float _662 = _661 + _646;
      float _663 = _662 + _650;
      float _664 = _634 + _639;
      float _665 = _664 * _476;
      float _666 = _615 + (c_ps_user[13].x);
      float _667 = _666 + _619;
      float _668 = c_diffuse_scale.y * _667;
      float _669 = _668 * c_mat_diffuse.y;
      float _670 = _647 + _628;
      float _671 = c_mat_specular.y * _670;
      float _672 = _671 * c_specular_scale.y;
      float _673 = _568 + _555;
      float _674 = _673 + _574;
      float _675 = _674 + (c_ps_user[12].w);
      float _676 = _675 + _578;
      float _677 = _676 + _582;
      float _678 = _677 + _586;
      float _679 = _678 + _606;
      float _680 = _679 + _611;
      float _681 = _680 + _623;
      float _682 = _681 + _603;
      float _683 = _682 + _665;
      float _684 = _683 + _669;
      float _685 = _684 + _672;
      float _686 = _636 + _640;
      float _687 = _686 * _476;
      float _688 = _616 + (c_ps_user[13].x);
      float _689 = _688 + _620;
      float _690 = c_diffuse_scale.z * _689;
      float _691 = _690 * c_mat_diffuse.z;
      float _692 = _647 + _629;
      float _693 = c_mat_specular.z * _692;
      float _694 = _693 * c_specular_scale.z;
      float _695 = _574 + _556;
      float _696 = _695 + _569;
      float _697 = _696 + (c_ps_user[12].w);
      float _698 = _697 + _579;
      float _699 = _698 + _583;
      float _700 = _699 + _587;
      float _701 = _700 + _607;
      float _702 = _701 + _612;
      float _703 = _702 + _624;
      float _704 = _703 + _603;
      float _705 = _704 + _687;
      float _706 = _705 + _691;
      float _707 = _706 + _694;
      _709 = 0.0f;
      _710 = 0.0f;
      _711 = 0.0f;
      _712 = _663;
      _713 = _685;
      _714 = _707;
   }
   else
   {
      _709 = _546;
      _710 = _547;
      _711 = _548;
      _712 = _543;
      _713 = _544;
      _714 = _545;
   }
   float _715 = _712 + _709;
   float _716 = _713 + _710;
   float _717 = _714 + _711;
   float _718 = dot(float3(_176, _177, _178), float3(_171, _172, _173));
   float _719 = _718 * 2.0f;
   float _720 = _719 * _171;
   float _721 = _719 * _172;
   float _722 = _719 * _173;
   float _723 = _176 - _720;
   float _724 = _177 - _721;
   float _725 = _178 - _722;
   float _726 = -0.0f - _723;
   float4 _727 = s2c_t.Sample(s2c_s, float3(_726, _724, _725));
   float _734 = _217 * _216;
   float _735 = _186 + 0.75f;
   float _736 = saturate(_735);
   float _737 = _734 * _736;
   float _738 = max(_737, (c_light_vec[5].y));
   float _739 = max(_727.w, 0.003921568859368563f);
   float _740 = 0.25f / _739;
   float _741 = _740 * _727.x;
   float _742 = _740 * _727.y;
   float _743 = _740 * _727.z;
   float _744 = _119 * TEXCOORD_2.w;
   float _745 = _744 * _111.x;
   float _746 = _738 * _745;
   float _747 = _746 * _234;
   float _748 = _741 * _741;
   float _749 = _748 * _747;
   float _750 = _746 * _235;
   float _751 = _742 * _742;
   float _752 = _751 * _750;
   float _753 = _746 * _236;
   float _754 = _743 * _743;
   float _755 = _754 * _753;
   float _756 = _715 + _749;
   float _757 = _716 + _752;
   float _758 = _717 + _755;
   float _765 = c_mat_opacity * _63.w;
   float _766 = _765 * c_opacity_scale;
   float _767 = _766 * c_global_opacity;
   int _770 = c_bool & 16;
   bool _771 = (_770 == 0);
   if (!_771)
   {
      float _775 = c_tone_control.x * _756;
      float _776 = c_tone_control.x * _757;
      float _777 = c_tone_control.x * _758;
      float _779 = log2(_775);
      float _780 = log2(_776);
      float _781 = log2(_777);
      float _782 = _779 * c_tone_control.y;
      float _783 = _780 * c_tone_control.y;
      float _784 = _781 * c_tone_control.y;
      float _785 = exp2(_782);
      float _786 = exp2(_783);
      float _787 = exp2(_784);
      _837 = _785;
      _838 = _786;
      _839 = _787;
   }
   else
   {
      int _789 = c_bool & 32768;
      bool _790 = (_789 == 0);
      if (_790)
      {
         float _794 = c_tone_control.x * _756;
         float _795 = c_tone_control.x * _757;
         float _796 = c_tone_control.x * _758;
         float _797 = _794 * 2.0f;
         float _798 = _795 * 2.0f;
         float _799 = _796 * 2.0f;
         float _800 = _794 * 0.30000001192092896f;
         float _801 = _795 * 0.30000001192092896f;
         float _802 = _796 * 0.30000001192092896f;
         float _803 = _800 + 0.05000000074505806f;
         float _804 = _801 + 0.05000000074505806f;
         float _805 = _802 + 0.05000000074505806f;
         float _806 = _803 * _797;
         float _807 = _804 * _798;
         float _808 = _805 * _799;
         float _809 = _806 + 0.004000000189989805f;
         float _810 = _807 + 0.004000000189989805f;
         float _811 = _808 + 0.004000000189989805f;
         float _812 = _800 + 0.5f;
         float _813 = _801 + 0.5f;
         float _814 = _802 + 0.5f;
         float _815 = _812 * _797;
         float _816 = _813 * _798;
         float _817 = _814 * _799;
         float _818 = _815 + 0.06000000238418579f;
         float _819 = _816 + 0.06000000238418579f;
         float _820 = _817 + 0.06000000238418579f;
         float _821 = _809 / _818;
         float _822 = _810 / _819;
         float _823 = _811 / _820;
         float _824 = _821 + -0.06666666269302368f;
         float _825 = _822 + -0.06666666269302368f;
         float _826 = _823 + -0.06666666269302368f;
         float _827 = _824 * 1.3790643215179443f;
         float _828 = _825 * 1.3790643215179443f;
         float _829 = _826 * 1.3790643215179443f;
         float _830 = YKR_MaterialCurve(_827, _794);
         float _831 = YKR_MaterialCurve(_828, _795);
         float _832 = YKR_MaterialCurve(_829, _796);
         float _833 = sqrt(_830);
         float _834 = sqrt(_831);
         float _835 = sqrt(_832);
         _837 = _833;
         _838 = _834;
         _839 = _835;
      }
      else
      {
         _837 = _756;
         _838 = _757;
         _839 = _758;
      }
   }
   int _840 = c_bool & 4;
   bool _841 = (_840 == 0);
   if (_841)
   {
      float _843 = saturate(COLOR_1.x);
      float _848 = (c_fog_param[0].x) - _837;
      float _849 = (c_fog_param[0].y) - _838;
      float _850 = (c_fog_param[0].z) - _839;
      float _851 = _848 * _843;
      float _852 = _849 * _843;
      float _853 = _850 * _843;
      _855 = _851;
      _856 = _852;
      _857 = _853;
   }
   else
   {
      _855 = COLOR_1.x;
      _856 = COLOR_1.y;
      _857 = COLOR_1.z;
   }
   float _858 = _855 + _837;
   float _859 = _856 + _838;
   float _860 = _857 + _839;
   float _866 = (c_ps_col_scl[0].x) * _858;
   float _867 = (c_ps_col_scl[0].y) * _859;
   float _868 = (c_ps_col_scl[0].z) * _860;
   float _869 = (c_ps_col_scl[0].w) * _767;
   SV_Target.x = _866;
   SV_Target.y = _867;
   SV_Target.z = _868;
   SV_Target.w = _869;
   int _872 = c_bool & 8;
   bool _873 = (_872 != 0);
   float _874 = select(_873, _858, _866);
   float _875 = select(_873, _859, _867);
   float _876 = select(_873, _860, _868);
   float _877 = select(_873, _767, _869);
   SV_Target.x = _874;
   SV_Target.y = _875;
   SV_Target.z = _876;
   SV_Target.w = _877;
   return SV_Target;
}
