// Yakuza Kiwami material s_o1dzt[eye] (0x730CC4A7, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

cbuffer cb2 : register(b2)
{
   float4 c_ps_scratch[92] : packoffset(c000.x);
};

cbuffer cb3 : register(b3)
{
   float4 c_light_col[6] : packoffset(c000.x);
   float4 c_light_vec[6] : packoffset(c006.x);
};

SamplerState s0_s : register(s0);

SamplerState s2_s : register(s2);

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float3 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _53 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _62 = _57.x * _57.x;
   float _63 = _57.y * _57.y;
   float _64 = _57.z * _57.z;
   float4 _65 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _70 = c_mat_power * _53.z;
   float _71 = max(1.0f, _70);
   float _72 = _65.x * 2.0f;
   float _73 = _65.y * 2.0f;
   float _74 = _72 + -1.0f;
   float _75 = _73 + -1.0f;
   float _76 = dot(float2(_74, _75), float2(_74, _75));
   float _77 = 1.0f - _76;
   float _78 = saturate(_77);
   float _79 = sqrt(_78);
   float _80 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _81 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _82 = _80 - _81;
   float _83 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _84 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _87 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _88 = _86 - _87;
   float _89 = _74 * TEXCOORD_3.x;
   float _90 = _74 * TEXCOORD_3.y;
   float _91 = _74 * TEXCOORD_3.z;
   float _92 = _75 * TEXCOORD_3.w;
   float _93 = _92 * _82;
   float _94 = _92 * _85;
   float _95 = _92 * _88;
   float _96 = _93 + _89;
   float _97 = _94 + _90;
   float _98 = _95 + _91;
   float _99 = _79 * TEXCOORD_2.x;
   float _100 = _79 * TEXCOORD_2.y;
   float _101 = _79 * TEXCOORD_2.z;
   float _102 = _96 + _99;
   float _103 = _97 + _100;
   float _104 = _98 + _101;
   float _105 = dot(float3(_102, _103, _104), float3(_102, _103, _104));
   float _106 = rsqrt(_105);
   float _107 = _102 * _106;
   float _108 = _103 * _106;
   float _109 = _104 * _106;
   float _114 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _115 = rsqrt(_114);
   float _116 = _115 * TEXCOORD_1.x;
   float _117 = _115 * TEXCOORD_1.y;
   float _118 = _115 * TEXCOORD_1.z;
   float _123 = (c_ps_user[4].x) + _116;
   float _124 = (c_ps_user[4].y) + _117;
   float _125 = (c_ps_user[4].z) + _118;
   float _126 = dot(float3(_123, _124, _125), float3(_123, _124, _125));
   float _127 = rsqrt(_126);
   float _132 = (c_ps_user[5].x) + _116;
   float _133 = (c_ps_user[5].y) + _117;
   float _134 = (c_ps_user[5].z) + _118;
   float _135 = dot(float3(_132, _133, _134), float3(_132, _133, _134));
   float _136 = rsqrt(_135);
   float _140 = c_rt_size.z * SV_Position.x;
   float _141 = c_rt_size.w * SV_Position.y;
   int _144 = c_bool & 8;
   bool _145 = (_144 == 0);
   float _151;
   float _152;
   float _164;
   float _165;
   float _254;
   float _255;
   float _256;
   float _285;
   float _286;
   float _287;
   float _452;
   float _623;
   float _624;
   float _625;
   float _626;
   float _706;
   float _707;
   float _708;
   float _724;
   float _725;
   float _726;
   float _751;
   float _752;
   float _753;
   if (!_145)
   {
      float4 _147 = s13_t.Sample(s13_s, float2(_140, _141));
      _151 = _147.x;
      _152 = _147.y;
   }
   else
   {
      _151 = 1.0f;
      _152 = 1.0f;
   }
   if (!_145)
   {
      float _157 = (c_sdw_param_ch[0].x) * _151;
      float _158 = (c_sdw_param_ch[0].z) * _152;
      float _161 = _157 + (c_sdw_param_ch[0].y);
      float _162 = _158 + (c_sdw_param_ch[0].w);
      _164 = _161;
      _165 = _162;
   }
   else
   {
      _164 = _151;
      _165 = _152;
   }
   float _166 = min(_164, _165);
   float _167 = _132 * _136;
   float _168 = -0.0f - _167;
   float _169 = _133 * _136;
   float _170 = -0.0f - _169;
   float _171 = _134 * _136;
   float _172 = -0.0f - _171;
   float _173 = _123 * _127;
   float _174 = -0.0f - _173;
   float _175 = _124 * _127;
   float _176 = -0.0f - _175;
   float _177 = _125 * _127;
   float _178 = -0.0f - _177;
   float _185 = (c_ps_user[6].x) * _71;
   float _186 = dot(float3(_174, _176, _178), float3(_107, _108, _109));
   float _187 = saturate(_186);
   float _188 = log2(_187);
   float _189 = _185 * _188;
   float _190 = exp2(_189);
   float _201 = (c_ps_user[4].w) * _190;
   float _203 = (c_ps_user[6].y) * _71;
   float _204 = dot(float3(_168, _170, _172), float3(_107, _108, _109));
   float _205 = saturate(_204);
   float _206 = log2(_205);
   float _207 = _203 * _206;
   float _208 = exp2(_207);
   float _211 = (c_ps_user[5].w) * _208;
   float _212 = _211 + _201;
   float _213 = c_specular_scale.x * _212;
   float _214 = (c_light_col[0].x) * c_mat_specular.x;
   float _215 = _214 * _213;
   float _216 = c_specular_scale.y * _212;
   float _217 = (c_light_col[0].y) * c_mat_specular.y;
   float _218 = _217 * _216;
   float _219 = c_specular_scale.z * _212;
   float _220 = (c_light_col[0].z) * c_mat_specular.z;
   float _221 = _220 * _219;
   int _225 = c_bool & 1;
   bool _226 = (_225 == 0);
   if (!_226)
   {
      float _228 = -0.0f - TEXCOORD_4.x;
      float _229 = -0.0f - TEXCOORD_4.y;
      float _230 = -0.0f - TEXCOORD_4.z;
      float _231 = dot(float3(_107, _108, _109), float3(_228, _229, _230));
      float _232 = saturate(_231);
      float _233 = _232 * TEXCOORD_1.w;
      float _234 = _232 * TEXCOORD_2.w;
      float _235 = _232 * TEXCOORD_4.w;
      float _236 = max(0.0f, _233);
      float _237 = max(0.0f, _234);
      float _238 = max(0.0f, _235);
      float _239 = -0.0f - TEXCOORD_5.x;
      float _240 = -0.0f - TEXCOORD_5.y;
      float _241 = -0.0f - TEXCOORD_5.z;
      float _242 = dot(float3(_107, _108, _109), float3(_239, _240, _241));
      float _243 = saturate(_242);
      float _244 = _243 * COLOR.w;
      float _245 = _243 * COLOR_1.w;
      float _246 = _243 * TEXCOORD_5.w;
      float _247 = max(0.0f, _244);
      float _248 = max(0.0f, _245);
      float _249 = max(0.0f, _246);
      float _250 = _247 + _236;
      float _251 = _248 + _237;
      float _252 = _249 + _238;
      _254 = _250;
      _255 = _251;
      _256 = _252;
   }
   else
   {
      _254 = 0.0f;
      _255 = 0.0f;
      _256 = 0.0f;
   }
   float _257 = c_rt_size.z * SV_Position.x;
   float _258 = c_rt_size.w * SV_Position.y;
   float4 _259 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_257, _258)));
   int _265 = c_bool & 16384;
   bool _266 = (_265 == 0);
   if (!_266)
   {
      float _268 = _259.x * 4.0f;
      float _269 = _259.y * 4.0f;
      float _270 = _259.z * 4.0f;
      _285 = _268;
      _286 = _269;
      _287 = _270;
   }
   else
   {
      int _272 = c_bool & 8192;
      bool _273 = (_272 == 0);
      if (!_273)
      {
         float _275 = _259.x * 2.0f;
         float _276 = _259.y * 2.0f;
         float _277 = _259.z * 2.0f;
         _285 = _275;
         _286 = _276;
         _287 = _277;
      }
      else
      {
         float _281 = _259.x / c_tone_control.z;
         float _282 = _259.y / c_tone_control.z;
         float _283 = _259.z / c_tone_control.z;
         _285 = _281;
         _286 = _282;
         _287 = _283;
      }
   }
   float _288 = _285 + _254;
   float _289 = _286 + _255;
   float _290 = _287 + _256;
   float _291 = saturate(_215);
   float _292 = saturate(_218);
   float _293 = saturate(_221);
   float _356 = dot(float3(_107, _108, _109), float3(_107, _108, _109));
   float _357 = rsqrt(_356);
   float _358 = _357 * _107;
   float _359 = _357 * _108;
   float _360 = _357 * _109;
   float _361 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_358, _359, _360, 1.0f));
   float _362 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_358, _359, _360, 1.0f));
   float _363 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_358, _359, _360, 1.0f));
   float _364 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_358, _359, _360, 1.0f));
   float _365 = dot(float4(_358, _359, _360, 1.0f), float4(_361, _362, _363, _364));
   float _366 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_358, _359, _360, 1.0f));
   float _367 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_358, _359, _360, 1.0f));
   float _368 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_358, _359, _360, 1.0f));
   float _369 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_358, _359, _360, 1.0f));
   float _370 = dot(float4(_358, _359, _360, 1.0f), float4(_366, _367, _368, _369));
   float _371 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_358, _359, _360, 1.0f));
   float _372 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_358, _359, _360, 1.0f));
   float _373 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_358, _359, _360, 1.0f));
   float _374 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_358, _359, _360, 1.0f));
   float _375 = dot(float4(_358, _359, _360, 1.0f), float4(_371, _372, _373, _374));
   float _376 = max(_365, 0.0625f);
   float _377 = max(_370, 0.0625f);
   float _378 = max(_375, 0.0625f);
   float _379 = _376 * (c_ps_scratch[79].x);
   float _380 = _377 * (c_ps_scratch[79].x);
   float _381 = _378 * (c_ps_scratch[79].x);
   float _386 = -0.0f - (c_light_vec[0].x);
   float _387 = -0.0f - (c_light_vec[0].y);
   float _388 = -0.0f - (c_light_vec[0].z);
   float _389 = dot(float3(_107, _108, _109), float3(_386, _387, _388));
   float _390 = -0.0f - (c_light_vec[1].x);
   float _391 = -0.0f - (c_light_vec[1].y);
   float _392 = -0.0f - (c_light_vec[1].z);
   float _393 = dot(float3(_107, _108, _109), float3(_390, _391, _392));
   float _402 = _108 * 0.5f;
   float _403 = _402 + 0.5f;
   float _404 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _405 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _406 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _407 = _404 * _403;
   float _408 = _405 * _403;
   float _409 = _406 * _403;
   float _410 = saturate(_389);
   float _411 = saturate(_393);
   float _416 = (c_light_col[0].x) * _410;
   float _417 = (c_light_col[0].y) * _410;
   float _418 = (c_light_col[0].z) * _410;
   float _423 = (c_light_col[1].x) * _411;
   float _424 = (c_light_col[1].y) * _411;
   float _425 = (c_light_col[1].z) * _411;
   float _426 = _416 * _166;
   float _427 = _417 * _166;
   float _428 = _418 * _166;
   float _429 = (c_light_vec[4].x) + _379;
   float _430 = _429 + _407;
   float _431 = _430 + _288;
   float _432 = _431 + _423;
   float _433 = _432 + _426;
   float _434 = (c_light_vec[4].y) + _380;
   float _435 = _434 + _408;
   float _436 = _435 + _289;
   float _437 = _436 + _424;
   float _438 = _437 + _427;
   float _439 = (c_light_vec[4].z) + _381;
   float _440 = _439 + _409;
   float _441 = _440 + _290;
   float _442 = _441 + _425;
   float _443 = _442 + _428;
   int _444 = c_bool & 64;
   bool _445 = (_444 == 0);
   if (!_445)
   {
      float _447 = 1.0f - _53.y;
      float _448 = _166 * _447;
      float _449 = _448 * _410;
      float _450 = _449 + _53.y;
      _452 = _450;
   }
   else
   {
      _452 = _53.y;
   }
   float _453 = _452 * _433;
   float _454 = _452 * _438;
   float _455 = _452 * _443;
   float _456 = max(_453, _454);
   float _457 = max(_456, _455);
   float _460 = (c_ps_user[6].z) + _457;
   float _461 = saturate(_460);
   float _462 = _461 * TEXCOORD.z;
   float _463 = _462 * _291;
   float _464 = _462 * _292;
   float _465 = _462 * _293;
   int _466 = c_bool & 32768;
   bool _467 = (_466 == 0);
   if (!_467)
   {
      float _479 = (c_ps_user[11].x) * _62;
      float _480 = (c_ps_user[11].x) * _63;
      float _481 = (c_ps_user[11].x) * _64;
      float _483 = _107 + 1.0f;
      float _484 = _108 + 1.0f;
      float _485 = _109 + 1.0f;
      float _486 = _483 * 0.5f;
      float _487 = _484 * 0.5f;
      float _488 = _485 * 0.5f;
      float _489 = saturate(_486);
      float _490 = saturate(_487);
      float _491 = saturate(_488);
      float _492 = (c_ps_user[11].y) * _489;
      float _493 = _490 * (c_ps_user[11].y);
      float _494 = _491 * (c_ps_user[11].y);
      float _504 = (c_ps_user[11].z) * _53.x;
      float _506 = (c_ps_user[11].w) * _463;
      float _509 = (c_ps_user[12].x) * _166;
      float _511 = (c_ps_user[12].y) * _53.y;
      float _516 = -0.0f - _116;
      float _517 = -0.0f - _117;
      float _518 = -0.0f - _118;
      float _519 = dot(float3(_516, _517, _518), float3(_107, _108, _109));
      float _520 = 1.0f - _519;
      float _521 = _520 * _520;
      float _522 = _520 * 0.9722222089767456f;
      float _523 = _521 * _521;
      float _524 = _523 * _522;
      float _525 = _524 + 0.02777777425944805f;
      float _526 = _525 * (c_ps_user[13].z);
      float _528 = (c_ps_user[13].w) * _430;
      float _529 = (c_ps_user[13].w) * _435;
      float _530 = (c_ps_user[13].w) * _440;
      float _533 = (c_ps_user[14].y) * _416;
      float _534 = (c_ps_user[14].y) * _417;
      float _535 = (c_ps_user[14].y) * _418;
      float _537 = (c_ps_user[14].z) * _423;
      float _538 = (c_ps_user[14].z) * _424;
      float _539 = (c_ps_user[14].z) * _425;
      float _541 = (c_ps_user[14].w) * _288;
      float _542 = (c_ps_user[14].w) * _289;
      float _543 = (c_ps_user[14].w) * _290;
      float _547 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _548 = c_specular_scale.x * _547;
      float _549 = _548 * _463;
      float _550 = _533 + (c_ps_user[13].x);
      float _551 = _550 + _537;
      float _552 = c_diffuse_scale.x * _551;
      float _553 = _552 * c_mat_diffuse.x;
      float _554 = (c_ps_user[13].y) + _504;
      float _555 = _554 * c_specular_scale.x;
      float _556 = _555 + _549;
      float _557 = _556 * c_mat_specular.x;
      float _558 = _492 + _479;
      float _559 = _558 + _506;
      float _560 = _559 + _509;
      float _561 = _560 + _511;
      float _562 = _561 + _528;
      float _563 = _562 + _541;
      float _564 = _563 + _526;
      float _565 = _564 + _553;
      float _566 = _565 + _557;
      float _567 = c_specular_scale.y * _547;
      float _568 = _567 * _464;
      float _569 = _534 + (c_ps_user[13].x);
      float _570 = _569 + _538;
      float _571 = c_diffuse_scale.y * _570;
      float _572 = _571 * c_mat_diffuse.y;
      float _573 = _554 * c_specular_scale.y;
      float _574 = _573 + _568;
      float _575 = _574 * c_mat_specular.y;
      float _576 = _506 + _480;
      float _577 = _576 + _493;
      float _578 = _577 + _509;
      float _579 = _578 + _511;
      float _580 = _579 + _529;
      float _581 = _580 + _542;
      float _582 = _581 + _526;
      float _583 = _582 + _572;
      float _584 = _583 + _575;
      float _585 = c_specular_scale.z * _547;
      float _586 = _585 * _465;
      float _587 = _535 + (c_ps_user[13].x);
      float _588 = _587 + _539;
      float _589 = c_diffuse_scale.z * _588;
      float _590 = _589 * c_mat_diffuse.z;
      float _591 = _554 * c_specular_scale.z;
      float _592 = _591 + _586;
      float _593 = _592 * c_mat_specular.z;
      float _594 = _506 + _481;
      float _595 = _594 + _494;
      float _596 = _595 + _509;
      float _597 = _596 + _511;
      float _598 = _597 + _530;
      float _599 = _598 + _543;
      float _600 = _599 + _526;
      float _601 = _600 + _590;
      float _602 = _601 + _593;
      float _603 = saturate(_566);
      float _604 = saturate(_584);
      float _605 = saturate(_602);
      _623 = _603;
      _624 = _604;
      _625 = _605;
      _626 = 1.0f;
   }
   else
   {
      float _607 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _608 = _607 * _62;
      float _609 = _608 * _453;
      float _610 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _611 = _610 * _63;
      float _612 = _611 * _454;
      float _613 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _614 = _613 * _64;
      float _615 = _614 * _455;
      float _616 = _463 * _53.x;
      float _617 = _464 * _53.x;
      float _618 = _465 * _53.x;
      float _619 = _609 + _616;
      float _620 = _612 + _617;
      float _621 = _615 + _618;
      _623 = _619;
      _624 = _620;
      _625 = _621;
      _626 = _463;
   }
   float _631 = c_mat_opacity * _57.w;
   float _632 = _631 * c_opacity_scale;
   float _633 = _632 * c_global_opacity;
   float _634 = 1.0f - _633;
   float _635 = saturate(_626);
   float _636 = _634 * _635;
   float _637 = _636 + _633;
   float _638 = _623 * COLOR.x;
   float _639 = _624 * COLOR.y;
   float _640 = _625 * COLOR.z;
   int _641 = c_bool & 16;
   bool _642 = (_641 == 0);
   if (!_642)
   {
      float _646 = c_tone_control.x * _638;
      float _647 = c_tone_control.x * _639;
      float _648 = c_tone_control.x * _640;
      float _650 = log2(_646);
      float _651 = log2(_647);
      float _652 = log2(_648);
      float _653 = _650 * c_tone_control.y;
      float _654 = _651 * c_tone_control.y;
      float _655 = _652 * c_tone_control.y;
      float _656 = exp2(_653);
      float _657 = exp2(_654);
      float _658 = exp2(_655);
      _706 = _656;
      _707 = _657;
      _708 = _658;
   }
   else
   {
      if (_467)
      {
         float _663 = c_tone_control.x * _638;
         float _664 = c_tone_control.x * _639;
         float _665 = c_tone_control.x * _640;
         float _666 = _663 * 2.0f;
         float _667 = _664 * 2.0f;
         float _668 = _665 * 2.0f;
         float _669 = _663 * 0.30000001192092896f;
         float _670 = _664 * 0.30000001192092896f;
         float _671 = _665 * 0.30000001192092896f;
         float _672 = _669 + 0.05000000074505806f;
         float _673 = _670 + 0.05000000074505806f;
         float _674 = _671 + 0.05000000074505806f;
         float _675 = _672 * _666;
         float _676 = _673 * _667;
         float _677 = _674 * _668;
         float _678 = _675 + 0.004000000189989805f;
         float _679 = _676 + 0.004000000189989805f;
         float _680 = _677 + 0.004000000189989805f;
         float _681 = _669 + 0.5f;
         float _682 = _670 + 0.5f;
         float _683 = _671 + 0.5f;
         float _684 = _681 * _666;
         float _685 = _682 * _667;
         float _686 = _683 * _668;
         float _687 = _684 + 0.06000000238418579f;
         float _688 = _685 + 0.06000000238418579f;
         float _689 = _686 + 0.06000000238418579f;
         float _690 = _678 / _687;
         float _691 = _679 / _688;
         float _692 = _680 / _689;
         float _693 = _690 + -0.06666666269302368f;
         float _694 = _691 + -0.06666666269302368f;
         float _695 = _692 + -0.06666666269302368f;
         float _696 = _693 * 1.3790643215179443f;
         float _697 = _694 * 1.3790643215179443f;
         float _698 = _695 * 1.3790643215179443f;
         float _699 = YKR_MaterialCurve(_696, _663);
         float _700 = YKR_MaterialCurve(_697, _664);
         float _701 = YKR_MaterialCurve(_698, _665);
         float _702 = sqrt(_699);
         float _703 = sqrt(_700);
         float _704 = sqrt(_701);
         _706 = _702;
         _707 = _703;
         _708 = _704;
      }
      else
      {
         _706 = _638;
         _707 = _639;
         _708 = _640;
      }
   }
   int _709 = c_bool & 4;
   bool _710 = (_709 == 0);
   if (_710)
   {
      float _712 = saturate(COLOR_1.x);
      float _717 = (c_fog_param[0].x) - _706;
      float _718 = (c_fog_param[0].y) - _707;
      float _719 = (c_fog_param[0].z) - _708;
      float _720 = _717 * _712;
      float _721 = _718 * _712;
      float _722 = _719 * _712;
      _724 = _720;
      _725 = _721;
      _726 = _722;
   }
   else
   {
      _724 = COLOR_1.x;
      _725 = COLOR_1.y;
      _726 = COLOR_1.z;
   }
   float _727 = _724 + _706;
   float _728 = _725 + _707;
   float _729 = _726 + _708;
   if (!_467)
   {
      float _737 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _738 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _739 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _741 = (c_ps_user[12].w) * COLOR.x;
      float _742 = (c_ps_user[12].w) * COLOR.y;
      float _743 = (c_ps_user[12].w) * COLOR.z;
      float _744 = _741 + _727;
      float _745 = _744 + _737;
      float _746 = _742 + _728;
      float _747 = _746 + _738;
      float _748 = _743 + _729;
      float _749 = _748 + _739;
      _751 = _745;
      _752 = _747;
      _753 = _749;
   }
   else
   {
      _751 = _727;
      _752 = _728;
      _753 = _729;
   }
   float _759 = (c_ps_col_scl[0].x) * _751;
   float _760 = (c_ps_col_scl[0].y) * _752;
   float _761 = (c_ps_col_scl[0].z) * _753;
   float _762 = (c_ps_col_scl[0].w) * _637;
   SV_Target.x = _759;
   SV_Target.y = _760;
   SV_Target.z = _761;
   SV_Target.w = _762;
   int _765 = c_bool & 8;
   bool _766 = (_765 != 0);
   float _767 = select(_766, _751, _759);
   float _768 = select(_766, _752, _760);
   float _769 = select(_766, _753, _761);
   float _770 = select(_766, _637, _762);
   SV_Target.x = _767;
   SV_Target.y = _768;
   SV_Target.z = _769;
   SV_Target.w = _770;
   return SV_Target;
}
