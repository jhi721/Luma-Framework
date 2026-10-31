// Yakuza Kiwami material r_o1mt_b(vr)2mt_m4d (0x8B974DE7, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s1_t : register(t1);

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

SamplerState s5_s : register(s5);

SamplerState s6_s : register(s6);

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 TEXCOORD : TEXCOORD,
    linear float2 TEXCOORD_1 : TEXCOORD1,
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
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float _65 = (c_attribute_ps[0].z) * TEXCOORD.x;
   float _66 = (c_attribute_ps[0].w) * TEXCOORD.y;
   float4 _67 = s5_t.Sample(s5_s, float2(_65, _66));
   float4 _70 = s1_t.Sample(s1_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _75 = _70.x * _70.x;
   float _76 = _70.y * _70.y;
   float _77 = _70.z * _70.z;
   float _81 = (c_attribute_ps[1].z) * TEXCOORD.z;
   float _82 = (c_attribute_ps[1].w) * TEXCOORD.w;
   float4 _83 = s6_t.Sample(s6_s, float2(_81, _82));
   float4 _86 = s7_t.Sample(s7_s, float2(TEXCOORD_1.x, TEXCOORD_1.y));
   float _89 = _75 - _59;
   float _90 = _76 - _60;
   float _91 = _77 - _61;
   float _92 = _89 * COLOR_1.w;
   float _93 = _90 * COLOR_1.w;
   float _94 = _91 * COLOR_1.w;
   float _95 = _92 + _59;
   float _96 = _93 + _60;
   float _97 = _94 + _61;
   float _98 = _70.w - _54.w;
   float _99 = _98 * COLOR_1.w;
   float _100 = _99 + _54.w;
   float _101 = _83.x - _67.x;
   float _102 = _83.y - _67.y;
   float _103 = _101 * COLOR_1.w;
   float _104 = _102 * COLOR_1.w;
   float _105 = _103 + _67.x;
   float _106 = _104 + _67.y;
   float _107 = _105 * 2.0f;
   float _108 = _106 * 2.0f;
   float _109 = _107 + -1.0f;
   float _110 = _108 + -1.0f;
   float _111 = dot(float2(_109, _110), float2(_109, _110));
   float _112 = 1.0f - _111;
   float _113 = saturate(_112);
   float _114 = sqrt(_113);
   float _115 = TEXCOORD_2.y * TEXCOORD_6.z;
   float _116 = TEXCOORD_2.z * TEXCOORD_6.y;
   float _117 = _115 - _116;
   float _118 = TEXCOORD_2.z * TEXCOORD_6.x;
   float _119 = TEXCOORD_2.x * TEXCOORD_6.z;
   float _120 = _118 - _119;
   float _121 = TEXCOORD_2.x * TEXCOORD_6.y;
   float _122 = TEXCOORD_2.y * TEXCOORD_6.x;
   float _123 = _121 - _122;
   float _124 = _109 * TEXCOORD_6.x;
   float _125 = _109 * TEXCOORD_6.y;
   float _126 = _109 * TEXCOORD_6.z;
   float _127 = _110 * TEXCOORD_6.w;
   float _128 = _127 * _117;
   float _129 = _127 * _120;
   float _130 = _127 * _123;
   float _131 = _128 + _124;
   float _132 = _129 + _125;
   float _133 = _130 + _126;
   float _134 = _114 * TEXCOORD_2.x;
   float _135 = _114 * TEXCOORD_2.y;
   float _136 = _114 * TEXCOORD_2.z;
   float _137 = _131 + _134;
   float _138 = _132 + _135;
   float _139 = _133 + _136;
   float _140 = dot(float3(_137, _138, _139), float3(_137, _138, _139));
   float _141 = rsqrt(_140);
   float _142 = _137 * _141;
   float _143 = _138 * _141;
   float _144 = _139 * _141;
   float _145 = dot(float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z), float3(TEXCOORD_3.x, TEXCOORD_3.y, TEXCOORD_3.z));
   float _146 = rsqrt(_145);
   float _147 = _146 * TEXCOORD_3.x;
   float _148 = _146 * TEXCOORD_3.y;
   float _149 = _146 * TEXCOORD_3.z;
   float _154 = -0.0f - (c_light_vec[0].x);
   float _155 = -0.0f - (c_light_vec[0].y);
   float _156 = -0.0f - (c_light_vec[0].z);
   float _157 = dot(float3(_142, _143, _144), float3(_154, _155, _156));
   float _161 = c_rt_size.z * SV_Position.x;
   float _162 = c_rt_size.w * SV_Position.y;
   int _165 = c_bool & 8;
   bool _166 = (_165 == 0);
   float _173;
   float _174;
   float _175;
   float _187;
   float _188;
   float _243;
   float _244;
   float _245;
   float _246;
   float _260;
   float _261;
   float _307;
   float _308;
   float _309;
   float _344;
   float _345;
   float _346;
   float _347;
   float _348;
   float _349;
   float _503;
   float _504;
   float _505;
   float _506;
   float _507;
   float _508;
   float _680;
   float _681;
   float _682;
   float _683;
   float _684;
   float _685;
   float _760;
   float _761;
   float _762;
   float _778;
   float _779;
   float _780;
   if (!_166)
   {
      float4 _168 = s13_t.Sample(s13_s, float2(_161, _162));
      _173 = _168.x;
      _174 = _168.y;
      _175 = _168.w;
   }
   else
   {
      _173 = 1.0f;
      _174 = 1.0f;
      _175 = 1.0f;
   }
   if (!_166)
   {
      float _180 = (c_sdw_param[0].x) * _173;
      float _181 = (c_sdw_param[0].z) * _174;
      float _184 = _180 + (c_sdw_param[0].y);
      float _185 = _181 + (c_sdw_param[0].w);
      _187 = _184;
      _188 = _185;
   }
   else
   {
      _187 = _173;
      _188 = _174;
   }
   float _195 = (c_sdw_param[1].x) * _86.y;
   float _197 = _195 + (c_sdw_param[1].y);
   float _198 = _197 * _86.x;
   float _199 = 1.0f - (c_light_col[5].x);
   float _200 = 1.0f - (c_light_col[5].y);
   float _201 = 1.0f - (c_light_col[5].z);
   float _202 = _198 * _199;
   float _203 = _198 * _200;
   float _204 = _198 * _201;
   float _205 = _202 + (c_light_col[5].x);
   float _206 = _203 + (c_light_col[5].y);
   float _207 = _204 + (c_light_col[5].z);
   float _211 = c_rt_size.z * SV_Position.x;
   float _212 = c_rt_size.w * SV_Position.y;
   float4 _213 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_211, _212)));
   int _220 = c_bool & 16384;
   bool _221 = (_220 == 0);
   if (!_221)
   {
      float _223 = _213.x * 2.0f;
      float _224 = _213.y * 2.0f;
      float _225 = _213.z * 2.0f;
      float _226 = _213.w * 2.0f;
      _243 = _223;
      _244 = _224;
      _245 = _225;
      _246 = _226;
   }
   else
   {
      int _228 = c_bool & 8192;
      bool _229 = (_228 == 0);
      if (!_229)
      {
         float _231 = _213.x * 2.0f;
         float _232 = _213.y * 2.0f;
         float _233 = _213.z * 2.0f;
         float _234 = _213.w * 2.0f;
         _243 = _231;
         _244 = _232;
         _245 = _233;
         _246 = _234;
      }
      else
      {
         float _238 = _213.x / c_tone_control.z;
         float _239 = _213.y / c_tone_control.z;
         float _240 = _213.z / c_tone_control.z;
         float _241 = _213.w / c_tone_control.z;
         _243 = _238;
         _244 = _239;
         _245 = _240;
         _246 = _241;
      }
   }
   int _247 = c_bool & 8;
   bool _248 = (_247 == 0);
   if (!_248)
   {
      float _253 = (c_sdw_param[0].x) * _187;
      float _254 = (c_sdw_param[0].z) * _188;
      float _257 = _253 + (c_sdw_param[0].y);
      float _258 = _254 + (c_sdw_param[0].w);
      _260 = _257;
      _261 = _258;
   }
   else
   {
      _260 = _187;
      _261 = _188;
   }
   float _262 = _261 * _260;
   float _267 = 1.0f - (c_light_col[5].x);
   float _268 = 1.0f - (c_light_col[5].y);
   float _269 = 1.0f - (c_light_col[5].z);
   float _270 = _267 * _262;
   float _271 = _268 * _262;
   float _272 = _269 * _262;
   float _273 = _270 + (c_light_col[5].x);
   float _274 = _271 + (c_light_col[5].y);
   float _275 = _272 + (c_light_col[5].z);
   float _276 = _175 * COLOR.x;
   float _277 = _276 * _205;
   float _278 = _175 * COLOR.y;
   float _279 = _278 * _206;
   float _280 = _175 * COLOR.z;
   float _281 = _280 * _207;
   float _282 = _273 * _205;
   float _283 = _274 * _206;
   float _284 = _275 * _207;
   if (!_248)
   {
      float _288 = (c_sdw_param[2].x) * _273;
      float _289 = (c_sdw_param[2].x) * _274;
      float _290 = (c_sdw_param[2].x) * _275;
      float _292 = _288 + (c_sdw_param[2].y);
      float _293 = _289 + (c_sdw_param[2].y);
      float _294 = _290 + (c_sdw_param[2].y);
      float _296 = (c_sdw_param[2].z) * _205;
      float _297 = (c_sdw_param[2].z) * _206;
      float _298 = (c_sdw_param[2].z) * _207;
      float _300 = _296 + (c_sdw_param[2].w);
      float _301 = _297 + (c_sdw_param[2].w);
      float _302 = _298 + (c_sdw_param[2].w);
      float _303 = _300 * _292;
      float _304 = _301 * _293;
      float _305 = _302 * _294;
      _307 = _303;
      _308 = _304;
      _309 = _305;
   }
   else
   {
      _307 = _282;
      _308 = _283;
      _309 = _284;
   }
   float _310 = _175 * _86.x;
   float _311 = _307 * _310;
   float _312 = _308 * _310;
   float _313 = _309 * _310;
   float _324 = c_mat_emissive.x * _95;
   float _325 = _324 * c_emissive_scale;
   float _326 = c_mat_emissive.y * _96;
   float _327 = _326 * c_emissive_scale;
   float _328 = c_mat_emissive.z * _97;
   float _329 = _328 * c_emissive_scale;
   int _330 = c_bool & 2048;
   bool _331 = (_330 == 0);
   if (_331)
   {
      float _333 = _261 * _243;
      float _334 = _261 * _244;
      float _335 = _261 * _245;
      float _336 = _333 + TEXCOORD_4.x;
      float _337 = _334 + TEXCOORD_4.y;
      float _338 = _335 + TEXCOORD_4.z;
      float _339 = _261 * _246;
      float _340 = _339 + TEXCOORD_5.x;
      float _341 = _339 + TEXCOORD_5.y;
      float _342 = _339 + TEXCOORD_5.z;
      _344 = _336;
      _345 = _337;
      _346 = _338;
      _347 = _340;
      _348 = _341;
      _349 = _342;
   }
   else
   {
      _344 = TEXCOORD_4.x;
      _345 = TEXCOORD_4.y;
      _346 = TEXCOORD_4.z;
      _347 = TEXCOORD_5.x;
      _348 = TEXCOORD_5.y;
      _349 = TEXCOORD_5.z;
   }
   float _354 = -0.0f - (c_light_vec[1].x);
   float _355 = -0.0f - (c_light_vec[1].y);
   float _356 = -0.0f - (c_light_vec[1].z);
   float _357 = dot(float3(_142, _143, _144), float3(_354, _355, _356));
   float _358 = saturate(_357);
   float _363 = saturate(_157);
   float _364 = (c_light_col[0].x) * _363;
   float _365 = (c_light_col[0].y) * _363;
   float _366 = (c_light_col[0].z) * _363;
   float _371 = (c_light_col[1].x) * _358;
   float _372 = (c_light_col[1].y) * _358;
   float _373 = (c_light_col[1].z) * _358;
   float _382 = c_diffuse_scale.x * c_mat_diffuse.x;
   float _383 = c_diffuse_scale.y * c_mat_diffuse.y;
   float _384 = c_diffuse_scale.z * c_mat_diffuse.z;
   float _385 = _364 * _273;
   float _386 = _365 * _274;
   float _387 = _366 * _275;
   float _388 = _371 * _261;
   float _389 = _372 * _261;
   float _390 = _373 * _261;
   float _391 = _388 + _385;
   float _392 = _389 + _386;
   float _393 = _390 + _387;
   float _394 = _382 * _391;
   float _395 = _383 * _392;
   float _396 = _384 * _393;
   float _397 = _325 + c_scene_ambient.x;
   float _398 = _397 + _394;
   float _399 = _327 + c_scene_ambient.y;
   float _400 = _399 + _395;
   float _401 = _329 + c_scene_ambient.z;
   float _402 = _401 + _396;
   float _403 = _398 * _277;
   float _404 = _400 * _279;
   float _405 = _402 * _281;
   float _406 = _344 * _311;
   float _407 = _345 * _312;
   float _408 = _346 * _313;
   float _409 = _403 + _406;
   float _410 = _404 + _407;
   float _411 = _405 + _408;
   float _416 = (c_light_vec[0].x) + _147;
   float _417 = (c_light_vec[0].y) + _148;
   float _418 = (c_light_vec[0].z) + _149;
   float _419 = dot(float3(_416, _417, _418), float3(_416, _417, _418));
   float _420 = rsqrt(_419);
   float _421 = _416 * _420;
   float _422 = -0.0f - _421;
   float _423 = _417 * _420;
   float _424 = -0.0f - _423;
   float _425 = _418 * _420;
   float _426 = -0.0f - _425;
   float _427 = dot(float3(_422, _424, _426), float3(_142, _143, _144));
   float _428 = saturate(_427);
   float _429 = _148 + -0.30000001192092896f;
   float _430 = dot(float3(_147, _429, _149), float3(_147, _429, _149));
   float _431 = rsqrt(_430);
   float _432 = _147 * _431;
   float _433 = -0.0f - _432;
   float _434 = _429 * _431;
   float _435 = -0.0f - _434;
   float _436 = _149 * _431;
   float _437 = -0.0f - _436;
   float _438 = dot(float3(_433, _435, _437), float3(_142, _143, _144));
   float _439 = saturate(_438);
   float _442 = log2(_428);
   float _443 = log2(_439);
   float _444 = _442 * c_mat_power;
   float _445 = _443 * c_mat_power;
   float _446 = exp2(_444);
   float _447 = exp2(_445);
   float _450 = _446 * (c_light_col[0].x);
   float _451 = _446 * (c_light_col[0].y);
   float _452 = _446 * (c_light_col[0].z);
   float _453 = _447 * 2.0f;
   float _454 = _453 * (c_light_vec[5].x);
   float _455 = _454 * (c_light_col[1].x);
   float _456 = _454 * (c_light_col[1].y);
   float _457 = _454 * (c_light_col[1].z);
   float _458 = _450 * _273;
   float _459 = _451 * _274;
   float _460 = _452 * _275;
   float _461 = _455 * _358;
   float _462 = _456 * _358;
   float _463 = _457 * _358;
   float _464 = _461 + _458;
   float _465 = _462 + _459;
   float _466 = _463 + _460;
   float _473 = c_mat_specular.x * c_specular_scale.x;
   float _474 = _473 * _277;
   float _475 = _474 * _464;
   float _476 = c_mat_specular.y * c_specular_scale.y;
   float _477 = _476 * _279;
   float _478 = _477 * _465;
   float _479 = c_mat_specular.z * c_specular_scale.z;
   float _480 = _479 * _281;
   float _481 = _480 * _466;
   float _482 = _347 * _311;
   float _483 = _482 * _447;
   float _484 = _348 * _312;
   float _485 = _484 * _447;
   float _486 = _349 * _313;
   float _487 = _486 * _447;
   float _488 = _475 + _483;
   float _489 = _478 + _485;
   float _490 = _481 + _487;
   if (!_331)
   {
      float _492 = _261 * _243;
      float _493 = _261 * _244;
      float _494 = _261 * _245;
      float _495 = _409 + _492;
      float _496 = _410 + _493;
      float _497 = _411 + _494;
      float _498 = _261 * _246;
      float _499 = _488 + _498;
      float _500 = _489 + _498;
      float _501 = _490 + _498;
      _503 = _495;
      _504 = _496;
      _505 = _497;
      _506 = _499;
      _507 = _500;
      _508 = _501;
   }
   else
   {
      _503 = _409;
      _504 = _410;
      _505 = _411;
      _506 = _488;
      _507 = _489;
      _508 = _490;
   }
   int _509 = c_bool & 128;
   bool _510 = (_509 != 0);
   float _511 = select(_510, 0.0f, _506);
   float _512 = select(_510, 0.0f, _507);
   float _513 = select(_510, 0.0f, _508);
   float _514 = _503 * _95;
   float _515 = _504 * _96;
   float _516 = _505 * _97;
   float _517 = _511 * _100;
   float _518 = _512 * _100;
   float _519 = _513 * _100;
   int _520 = c_bool & 32768;
   bool _521 = (_520 == 0);
   if (!_521)
   {
      float _525 = (c_ps_user[11].x) * _95;
      float _526 = (c_ps_user[11].x) * _96;
      float _527 = (c_ps_user[11].x) * _97;
      float _529 = _142 + 1.0f;
      float _530 = _143 + 1.0f;
      float _531 = _144 + 1.0f;
      float _532 = _529 * 0.5f;
      float _533 = _530 * 0.5f;
      float _534 = _531 * 0.5f;
      float _535 = saturate(_532);
      float _536 = saturate(_533);
      float _537 = saturate(_534);
      float _538 = (c_ps_user[11].y) * _535;
      float _539 = _536 * (c_ps_user[11].y);
      float _540 = _537 * (c_ps_user[11].y);
      float _542 = (c_ps_user[11].z) * _100;
      float _544 = c_mat_power * 0.009999999776482582f;
      float _545 = _544 * (c_ps_user[11].w);
      float _548 = (c_ps_user[12].x) * _273;
      float _549 = (c_ps_user[12].x) * _274;
      float _550 = (c_ps_user[12].x) * _275;
      float _552 = (c_ps_user[12].y) * _277;
      float _553 = (c_ps_user[12].y) * _279;
      float _554 = (c_ps_user[12].y) * _281;
      float _556 = (c_ps_user[12].z) * _311;
      float _557 = (c_ps_user[12].z) * _312;
      float _558 = (c_ps_user[12].z) * _313;
      float _564 = -0.0f - _147;
      float _565 = -0.0f - _148;
      float _566 = -0.0f - _149;
      float _567 = dot(float3(_564, _565, _566), float3(_142, _143, _144));
      float _568 = 1.0f - _567;
      float _569 = _568 * _568;
      float _570 = _568 * 0.9722222089767456f;
      float _571 = _569 * _569;
      float _572 = _571 * _570;
      float _573 = _572 + 0.02777777425944805f;
      float _574 = _573 * (c_ps_user[13].z);
      float _576 = (c_ps_user[13].w) * c_scene_ambient.x;
      float _577 = (c_ps_user[13].w) * c_scene_ambient.y;
      float _578 = (c_ps_user[13].w) * c_scene_ambient.z;
      float _581 = (c_ps_user[14].x) * _325;
      float _582 = (c_ps_user[14].x) * _327;
      float _583 = (c_ps_user[14].x) * _329;
      float _585 = (c_ps_user[14].y) * _364;
      float _586 = (c_ps_user[14].y) * _365;
      float _587 = (c_ps_user[14].y) * _366;
      float _589 = (c_ps_user[14].z) * _371;
      float _590 = (c_ps_user[14].z) * _372;
      float _591 = (c_ps_user[14].z) * _373;
      float _593 = (c_ps_user[14].w) * _344;
      float _594 = (c_ps_user[14].w) * _345;
      float _595 = (c_ps_user[14].w) * _346;
      float _598 = (c_ps_user[15].x) * _450;
      float _599 = (c_ps_user[15].x) * _451;
      float _600 = (c_ps_user[15].x) * _452;
      float _602 = _473 * _455;
      float _603 = _602 * (c_ps_user[15].y);
      float _604 = _476 * _456;
      float _605 = _604 * (c_ps_user[15].y);
      float _606 = _479 * _457;
      float _607 = _606 * (c_ps_user[15].y);
      float _609 = (c_ps_user[15].z) * _347;
      float _610 = (c_ps_user[15].z) * _348;
      float _611 = (c_ps_user[15].z) * _349;
      float _612 = _603 + _609;
      float _613 = _612 * _447;
      float _614 = _585 + (c_ps_user[13].x);
      float _615 = _614 + _589;
      float _616 = c_diffuse_scale.x * _615;
      float _617 = _616 * c_mat_diffuse.x;
      float _618 = (c_ps_user[13].y) + _542;
      float _619 = _618 + _598;
      float _620 = c_mat_specular.x * _619;
      float _621 = _620 * c_specular_scale.x;
      float _622 = _538 + _525;
      float _623 = _622 + _545;
      float _624 = _623 + (c_ps_user[12].w);
      float _625 = _624 + _548;
      float _626 = _625 + _552;
      float _627 = _626 + _556;
      float _628 = _627 + _576;
      float _629 = _628 + _581;
      float _630 = _629 + _593;
      float _631 = _630 + _574;
      float _632 = _631 + _613;
      float _633 = _632 + _617;
      float _634 = _633 + _621;
      float _635 = _605 + _610;
      float _636 = _635 * _447;
      float _637 = _586 + (c_ps_user[13].x);
      float _638 = _637 + _590;
      float _639 = c_diffuse_scale.y * _638;
      float _640 = _639 * c_mat_diffuse.y;
      float _641 = _618 + _599;
      float _642 = c_mat_specular.y * _641;
      float _643 = _642 * c_specular_scale.y;
      float _644 = _539 + _526;
      float _645 = _644 + _545;
      float _646 = _645 + (c_ps_user[12].w);
      float _647 = _646 + _549;
      float _648 = _647 + _553;
      float _649 = _648 + _557;
      float _650 = _649 + _577;
      float _651 = _650 + _582;
      float _652 = _651 + _594;
      float _653 = _652 + _574;
      float _654 = _653 + _636;
      float _655 = _654 + _640;
      float _656 = _655 + _643;
      float _657 = _607 + _611;
      float _658 = _657 * _447;
      float _659 = _587 + (c_ps_user[13].x);
      float _660 = _659 + _591;
      float _661 = c_diffuse_scale.z * _660;
      float _662 = _661 * c_mat_diffuse.z;
      float _663 = _618 + _600;
      float _664 = c_mat_specular.z * _663;
      float _665 = _664 * c_specular_scale.z;
      float _666 = _545 + _527;
      float _667 = _666 + _540;
      float _668 = _667 + (c_ps_user[12].w);
      float _669 = _668 + _550;
      float _670 = _669 + _554;
      float _671 = _670 + _558;
      float _672 = _671 + _578;
      float _673 = _672 + _583;
      float _674 = _673 + _595;
      float _675 = _674 + _574;
      float _676 = _675 + _658;
      float _677 = _676 + _662;
      float _678 = _677 + _665;
      _680 = _634;
      _681 = _656;
      _682 = _678;
      _683 = 0.0f;
      _684 = 0.0f;
      _685 = 0.0f;
   }
   else
   {
      _680 = _514;
      _681 = _515;
      _682 = _516;
      _683 = _517;
      _684 = _518;
      _685 = _519;
   }
   float _686 = _683 + _680;
   float _687 = _684 + _681;
   float _688 = _685 + _682;
   float _693 = c_global_opacity * c_mat_opacity;
   float _694 = _693 * c_opacity_scale;
   int _695 = c_bool & 16;
   bool _696 = (_695 == 0);
   if (!_696)
   {
      float _700 = c_tone_control.x * _686;
      float _701 = c_tone_control.x * _687;
      float _702 = c_tone_control.x * _688;
      float _704 = log2(_700);
      float _705 = log2(_701);
      float _706 = log2(_702);
      float _707 = _704 * c_tone_control.y;
      float _708 = _705 * c_tone_control.y;
      float _709 = _706 * c_tone_control.y;
      float _710 = exp2(_707);
      float _711 = exp2(_708);
      float _712 = exp2(_709);
      _760 = _710;
      _761 = _711;
      _762 = _712;
   }
   else
   {
      if (_521)
      {
         float _717 = c_tone_control.x * _686;
         float _718 = c_tone_control.x * _687;
         float _719 = c_tone_control.x * _688;
         float _720 = _717 * 2.0f;
         float _721 = _718 * 2.0f;
         float _722 = _719 * 2.0f;
         float _723 = _717 * 0.30000001192092896f;
         float _724 = _718 * 0.30000001192092896f;
         float _725 = _719 * 0.30000001192092896f;
         float _726 = _723 + 0.05000000074505806f;
         float _727 = _724 + 0.05000000074505806f;
         float _728 = _725 + 0.05000000074505806f;
         float _729 = _726 * _720;
         float _730 = _727 * _721;
         float _731 = _728 * _722;
         float _732 = _729 + 0.004000000189989805f;
         float _733 = _730 + 0.004000000189989805f;
         float _734 = _731 + 0.004000000189989805f;
         float _735 = _723 + 0.5f;
         float _736 = _724 + 0.5f;
         float _737 = _725 + 0.5f;
         float _738 = _735 * _720;
         float _739 = _736 * _721;
         float _740 = _737 * _722;
         float _741 = _738 + 0.06000000238418579f;
         float _742 = _739 + 0.06000000238418579f;
         float _743 = _740 + 0.06000000238418579f;
         float _744 = _732 / _741;
         float _745 = _733 / _742;
         float _746 = _734 / _743;
         float _747 = _744 + -0.06666666269302368f;
         float _748 = _745 + -0.06666666269302368f;
         float _749 = _746 + -0.06666666269302368f;
         float _750 = _747 * 1.3790643215179443f;
         float _751 = _748 * 1.3790643215179443f;
         float _752 = _749 * 1.3790643215179443f;
         float _753 = YKR_MaterialCurve(_750, _717);
         float _754 = YKR_MaterialCurve(_751, _718);
         float _755 = YKR_MaterialCurve(_752, _719);
         float _756 = sqrt(_753);
         float _757 = sqrt(_754);
         float _758 = sqrt(_755);
         _760 = _756;
         _761 = _757;
         _762 = _758;
      }
      else
      {
         _760 = _686;
         _761 = _687;
         _762 = _688;
      }
   }
   int _763 = c_bool & 4;
   bool _764 = (_763 == 0);
   if (_764)
   {
      float _766 = saturate(COLOR_1.x);
      float _771 = (c_fog_param[0].x) - _760;
      float _772 = (c_fog_param[0].y) - _761;
      float _773 = (c_fog_param[0].z) - _762;
      float _774 = _771 * _766;
      float _775 = _772 * _766;
      float _776 = _773 * _766;
      _778 = _774;
      _779 = _775;
      _780 = _776;
   }
   else
   {
      _778 = COLOR_1.x;
      _779 = COLOR_1.y;
      _780 = COLOR_1.z;
   }
   float _781 = _778 + _760;
   float _782 = _779 + _761;
   float _783 = _780 + _762;
   float _789 = (c_ps_col_scl[0].x) * _781;
   float _790 = (c_ps_col_scl[0].y) * _782;
   float _791 = (c_ps_col_scl[0].z) * _783;
   float _792 = (c_ps_col_scl[0].w) * _694;
   SV_Target.x = _789;
   SV_Target.y = _790;
   SV_Target.z = _791;
   SV_Target.w = _792;
   int _795 = c_bool & 8;
   bool _796 = (_795 != 0);
   float _797 = select(_796, _781, _789);
   float _798 = select(_796, _782, _790);
   float _799 = select(_796, _783, _791);
   float _800 = select(_796, _694, _792);
   SV_Target.x = _797;
   SV_Target.y = _798;
   SV_Target.z = _799;
   SV_Target.w = _800;
   return SV_Target;
}
