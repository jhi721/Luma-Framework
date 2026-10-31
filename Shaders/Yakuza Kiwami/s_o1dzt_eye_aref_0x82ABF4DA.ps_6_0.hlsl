// Yakuza Kiwami material s_o1dzt[eye][aref] (0x82ABF4DA, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

TextureCube<float4> s1c_t : register(t1);

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

SamplerState s1c_s : register(s1);

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
    linear float3 TEXCOORD_6 : TEXCOORD6,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _58 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _63 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _68 = _63.x * _63.x;
   float _69 = _63.y * _63.y;
   float _70 = _63.z * _63.z;
   float4 _71 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _76 = c_mat_power * _58.z;
   float _77 = max(1.0f, _76);
   float _78 = _71.x * 2.0f;
   float _79 = _71.y * 2.0f;
   float _80 = _78 + -1.0f;
   float _81 = _79 + -1.0f;
   float _82 = dot(float2(_80, _81), float2(_80, _81));
   float _83 = 1.0f - _82;
   float _84 = saturate(_83);
   float _85 = sqrt(_84);
   float _86 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _87 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _90 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _91 = _89 - _90;
   float _92 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _93 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _94 = _92 - _93;
   float _95 = _80 * TEXCOORD_3.x;
   float _96 = _80 * TEXCOORD_3.y;
   float _97 = _80 * TEXCOORD_3.z;
   float _98 = _81 * TEXCOORD_3.w;
   float _99 = _98 * _88;
   float _100 = _98 * _91;
   float _101 = _98 * _94;
   float _102 = _99 + _95;
   float _103 = _100 + _96;
   float _104 = _101 + _97;
   float _105 = _85 * TEXCOORD_2.x;
   float _106 = _85 * TEXCOORD_2.y;
   float _107 = _85 * TEXCOORD_2.z;
   float _108 = _102 + _105;
   float _109 = _103 + _106;
   float _110 = _104 + _107;
   float _111 = dot(float3(_108, _109, _110), float3(_108, _109, _110));
   float _112 = rsqrt(_111);
   float _113 = _108 * _112;
   float _114 = _109 * _112;
   float _115 = _110 * _112;
   float _120 = dot(float3(TEXCOORD_6.x, TEXCOORD_6.y, TEXCOORD_6.z), float3(TEXCOORD_6.x, TEXCOORD_6.y, TEXCOORD_6.z));
   float _121 = rsqrt(_120);
   float _122 = _121 * TEXCOORD_6.x;
   float _123 = _121 * TEXCOORD_6.y;
   float _124 = _121 * TEXCOORD_6.z;
   float _129 = (c_ps_user[4].x) + TEXCOORD_1.x;
   float _130 = (c_ps_user[4].y) + TEXCOORD_1.y;
   float _131 = (c_ps_user[4].z) + TEXCOORD_1.z;
   float _132 = dot(float3(_129, _130, _131), float3(_129, _130, _131));
   float _133 = rsqrt(_132);
   float _138 = (c_ps_user[5].x) + TEXCOORD_1.x;
   float _139 = (c_ps_user[5].y) + TEXCOORD_1.y;
   float _140 = (c_ps_user[5].z) + TEXCOORD_1.z;
   float _141 = dot(float3(_138, _139, _140), float3(_138, _139, _140));
   float _142 = rsqrt(_141);
   float _143 = dot(float3(_122, _123, _124), float3(_113, _114, _115));
   float _144 = _143 * 2.0f;
   float _145 = _144 * _113;
   float _146 = _144 * _114;
   float _147 = _144 * _115;
   float _148 = _122 - _145;
   float _149 = _123 - _146;
   float _150 = _124 - _147;
   float _151 = dot(float3(_148, _149, _150), float3(_148, _149, _150));
   float _152 = rsqrt(_151);
   float _153 = _149 * _152;
   float _154 = _150 * _152;
   float _155 = _152 * _148;
   float _156 = -0.0f - _155;
   float4 _157 = s1c_t.Sample(s1c_s, float3(_156, _153, _154));
   float _162 = max(_157.w, 0.003921568859368563f);
   float _163 = 0.25f / _162;
   float _164 = _163 * _157.x;
   float _165 = _163 * _157.y;
   float _166 = _163 * _157.z;
   float _169 = (c_ps_user[6].w) * _58.w;
   float _170 = _164 * _164;
   float _171 = _170 * _169;
   float _172 = _165 * _165;
   float _173 = _172 * _169;
   float _174 = _166 * _166;
   float _175 = _174 * _169;
   float _176 = _171 + _68;
   float _177 = _173 + _69;
   float _178 = _175 + _70;
   float _182 = c_rt_size.z * SV_Position.x;
   float _183 = c_rt_size.w * SV_Position.y;
   int _186 = c_bool & 8;
   bool _187 = (_186 == 0);
   float _193;
   float _194;
   float _206;
   float _207;
   float _296;
   float _297;
   float _298;
   float _327;
   float _328;
   float _329;
   float _494;
   float _665;
   float _666;
   float _667;
   float _668;
   float _748;
   float _749;
   float _750;
   float _766;
   float _767;
   float _768;
   float _793;
   float _794;
   float _795;
   if (!_187)
   {
      float4 _189 = s13_t.Sample(s13_s, float2(_182, _183));
      _193 = _189.x;
      _194 = _189.y;
   }
   else
   {
      _193 = 1.0f;
      _194 = 1.0f;
   }
   if (!_187)
   {
      float _199 = (c_sdw_param_ch[0].x) * _193;
      float _200 = (c_sdw_param_ch[0].z) * _194;
      float _203 = _199 + (c_sdw_param_ch[0].y);
      float _204 = _200 + (c_sdw_param_ch[0].w);
      _206 = _203;
      _207 = _204;
   }
   else
   {
      _206 = _193;
      _207 = _194;
   }
   float _208 = min(_206, _207);
   float _209 = _138 * _142;
   float _210 = -0.0f - _209;
   float _211 = _139 * _142;
   float _212 = -0.0f - _211;
   float _213 = _140 * _142;
   float _214 = -0.0f - _213;
   float _215 = _129 * _133;
   float _216 = -0.0f - _215;
   float _217 = _130 * _133;
   float _218 = -0.0f - _217;
   float _219 = _131 * _133;
   float _220 = -0.0f - _219;
   float _227 = (c_ps_user[6].x) * _77;
   float _228 = dot(float3(_216, _218, _220), float3(_113, _114, _115));
   float _229 = saturate(_228);
   float _230 = log2(_229);
   float _231 = _227 * _230;
   float _232 = exp2(_231);
   float _243 = (c_ps_user[4].w) * _232;
   float _245 = (c_ps_user[6].y) * _77;
   float _246 = dot(float3(_210, _212, _214), float3(_113, _114, _115));
   float _247 = saturate(_246);
   float _248 = log2(_247);
   float _249 = _245 * _248;
   float _250 = exp2(_249);
   float _253 = (c_ps_user[5].w) * _250;
   float _254 = _253 + _243;
   float _255 = c_specular_scale.x * _254;
   float _256 = (c_light_col[0].x) * c_mat_specular.x;
   float _257 = _256 * _255;
   float _258 = c_specular_scale.y * _254;
   float _259 = (c_light_col[0].y) * c_mat_specular.y;
   float _260 = _259 * _258;
   float _261 = c_specular_scale.z * _254;
   float _262 = (c_light_col[0].z) * c_mat_specular.z;
   float _263 = _262 * _261;
   int _267 = c_bool & 1;
   bool _268 = (_267 == 0);
   if (!_268)
   {
      float _270 = -0.0f - TEXCOORD_4.x;
      float _271 = -0.0f - TEXCOORD_4.y;
      float _272 = -0.0f - TEXCOORD_4.z;
      float _273 = dot(float3(_113, _114, _115), float3(_270, _271, _272));
      float _274 = saturate(_273);
      float _275 = _274 * TEXCOORD_1.w;
      float _276 = _274 * TEXCOORD_2.w;
      float _277 = _274 * TEXCOORD_4.w;
      float _278 = max(0.0f, _275);
      float _279 = max(0.0f, _276);
      float _280 = max(0.0f, _277);
      float _281 = -0.0f - TEXCOORD_5.x;
      float _282 = -0.0f - TEXCOORD_5.y;
      float _283 = -0.0f - TEXCOORD_5.z;
      float _284 = dot(float3(_113, _114, _115), float3(_281, _282, _283));
      float _285 = saturate(_284);
      float _286 = _285 * COLOR.w;
      float _287 = _285 * COLOR_1.w;
      float _288 = _285 * TEXCOORD_5.w;
      float _289 = max(0.0f, _286);
      float _290 = max(0.0f, _287);
      float _291 = max(0.0f, _288);
      float _292 = _289 + _278;
      float _293 = _290 + _279;
      float _294 = _291 + _280;
      _296 = _292;
      _297 = _293;
      _298 = _294;
   }
   else
   {
      _296 = 0.0f;
      _297 = 0.0f;
      _298 = 0.0f;
   }
   float _299 = c_rt_size.z * SV_Position.x;
   float _300 = c_rt_size.w * SV_Position.y;
   float4 _301 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_299, _300)));
   int _307 = c_bool & 16384;
   bool _308 = (_307 == 0);
   if (!_308)
   {
      float _310 = _301.x * 4.0f;
      float _311 = _301.y * 4.0f;
      float _312 = _301.z * 4.0f;
      _327 = _310;
      _328 = _311;
      _329 = _312;
   }
   else
   {
      int _314 = c_bool & 8192;
      bool _315 = (_314 == 0);
      if (!_315)
      {
         float _317 = _301.x * 2.0f;
         float _318 = _301.y * 2.0f;
         float _319 = _301.z * 2.0f;
         _327 = _317;
         _328 = _318;
         _329 = _319;
      }
      else
      {
         float _323 = _301.x / c_tone_control.z;
         float _324 = _301.y / c_tone_control.z;
         float _325 = _301.z / c_tone_control.z;
         _327 = _323;
         _328 = _324;
         _329 = _325;
      }
   }
   float _330 = _327 + _296;
   float _331 = _328 + _297;
   float _332 = _329 + _298;
   float _333 = saturate(_257);
   float _334 = saturate(_260);
   float _335 = saturate(_263);
   float _398 = dot(float3(_113, _114, _115), float3(_113, _114, _115));
   float _399 = rsqrt(_398);
   float _400 = _399 * _113;
   float _401 = _399 * _114;
   float _402 = _399 * _115;
   float _403 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_400, _401, _402, 1.0f));
   float _404 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_400, _401, _402, 1.0f));
   float _405 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_400, _401, _402, 1.0f));
   float _406 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_400, _401, _402, 1.0f));
   float _407 = dot(float4(_400, _401, _402, 1.0f), float4(_403, _404, _405, _406));
   float _408 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_400, _401, _402, 1.0f));
   float _409 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_400, _401, _402, 1.0f));
   float _410 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_400, _401, _402, 1.0f));
   float _411 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_400, _401, _402, 1.0f));
   float _412 = dot(float4(_400, _401, _402, 1.0f), float4(_408, _409, _410, _411));
   float _413 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_400, _401, _402, 1.0f));
   float _414 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_400, _401, _402, 1.0f));
   float _415 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_400, _401, _402, 1.0f));
   float _416 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_400, _401, _402, 1.0f));
   float _417 = dot(float4(_400, _401, _402, 1.0f), float4(_413, _414, _415, _416));
   float _418 = max(_407, 0.0625f);
   float _419 = max(_412, 0.0625f);
   float _420 = max(_417, 0.0625f);
   float _421 = _418 * (c_ps_scratch[79].x);
   float _422 = _419 * (c_ps_scratch[79].x);
   float _423 = _420 * (c_ps_scratch[79].x);
   float _428 = -0.0f - (c_light_vec[0].x);
   float _429 = -0.0f - (c_light_vec[0].y);
   float _430 = -0.0f - (c_light_vec[0].z);
   float _431 = dot(float3(_113, _114, _115), float3(_428, _429, _430));
   float _432 = -0.0f - (c_light_vec[1].x);
   float _433 = -0.0f - (c_light_vec[1].y);
   float _434 = -0.0f - (c_light_vec[1].z);
   float _435 = dot(float3(_113, _114, _115), float3(_432, _433, _434));
   float _444 = _114 * 0.5f;
   float _445 = _444 + 0.5f;
   float _446 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _447 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _448 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _449 = _446 * _445;
   float _450 = _447 * _445;
   float _451 = _448 * _445;
   float _452 = saturate(_431);
   float _453 = saturate(_435);
   float _458 = (c_light_col[0].x) * _452;
   float _459 = (c_light_col[0].y) * _452;
   float _460 = (c_light_col[0].z) * _452;
   float _465 = (c_light_col[1].x) * _453;
   float _466 = (c_light_col[1].y) * _453;
   float _467 = (c_light_col[1].z) * _453;
   float _468 = _458 * _208;
   float _469 = _459 * _208;
   float _470 = _460 * _208;
   float _471 = (c_light_vec[4].x) + _421;
   float _472 = _471 + _449;
   float _473 = _472 + _330;
   float _474 = _473 + _465;
   float _475 = _474 + _468;
   float _476 = (c_light_vec[4].y) + _422;
   float _477 = _476 + _450;
   float _478 = _477 + _331;
   float _479 = _478 + _466;
   float _480 = _479 + _469;
   float _481 = (c_light_vec[4].z) + _423;
   float _482 = _481 + _451;
   float _483 = _482 + _332;
   float _484 = _483 + _467;
   float _485 = _484 + _470;
   int _486 = c_bool & 64;
   bool _487 = (_486 == 0);
   if (!_487)
   {
      float _489 = 1.0f - _58.y;
      float _490 = _208 * _489;
      float _491 = _490 * _452;
      float _492 = _491 + _58.y;
      _494 = _492;
   }
   else
   {
      _494 = _58.y;
   }
   float _495 = _494 * _475;
   float _496 = _494 * _480;
   float _497 = _494 * _485;
   float _498 = max(_495, _496);
   float _499 = max(_498, _497);
   float _502 = (c_ps_user[6].z) + _499;
   float _503 = saturate(_502);
   float _504 = _503 * TEXCOORD.z;
   float _505 = _504 * _333;
   float _506 = _504 * _334;
   float _507 = _504 * _335;
   int _508 = c_bool & 32768;
   bool _509 = (_508 == 0);
   if (!_509)
   {
      float _521 = (c_ps_user[11].x) * _176;
      float _522 = (c_ps_user[11].x) * _177;
      float _523 = (c_ps_user[11].x) * _178;
      float _525 = _113 + 1.0f;
      float _526 = _114 + 1.0f;
      float _527 = _115 + 1.0f;
      float _528 = _525 * 0.5f;
      float _529 = _526 * 0.5f;
      float _530 = _527 * 0.5f;
      float _531 = saturate(_528);
      float _532 = saturate(_529);
      float _533 = saturate(_530);
      float _534 = (c_ps_user[11].y) * _531;
      float _535 = _532 * (c_ps_user[11].y);
      float _536 = _533 * (c_ps_user[11].y);
      float _546 = (c_ps_user[11].z) * _58.x;
      float _548 = (c_ps_user[11].w) * _505;
      float _551 = (c_ps_user[12].x) * _208;
      float _553 = (c_ps_user[12].y) * _58.y;
      float _558 = -0.0f - _122;
      float _559 = -0.0f - _123;
      float _560 = -0.0f - _124;
      float _561 = dot(float3(_558, _559, _560), float3(_113, _114, _115));
      float _562 = 1.0f - _561;
      float _563 = _562 * _562;
      float _564 = _562 * 0.9722222089767456f;
      float _565 = _563 * _563;
      float _566 = _565 * _564;
      float _567 = _566 + 0.02777777425944805f;
      float _568 = _567 * (c_ps_user[13].z);
      float _570 = (c_ps_user[13].w) * _472;
      float _571 = (c_ps_user[13].w) * _477;
      float _572 = (c_ps_user[13].w) * _482;
      float _575 = (c_ps_user[14].y) * _458;
      float _576 = (c_ps_user[14].y) * _459;
      float _577 = (c_ps_user[14].y) * _460;
      float _579 = (c_ps_user[14].z) * _465;
      float _580 = (c_ps_user[14].z) * _466;
      float _581 = (c_ps_user[14].z) * _467;
      float _583 = (c_ps_user[14].w) * _330;
      float _584 = (c_ps_user[14].w) * _331;
      float _585 = (c_ps_user[14].w) * _332;
      float _589 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _590 = c_specular_scale.x * _589;
      float _591 = _590 * _505;
      float _592 = _575 + (c_ps_user[13].x);
      float _593 = _592 + _579;
      float _594 = c_diffuse_scale.x * _593;
      float _595 = _594 * c_mat_diffuse.x;
      float _596 = (c_ps_user[13].y) + _546;
      float _597 = _596 * c_specular_scale.x;
      float _598 = _597 + _591;
      float _599 = _598 * c_mat_specular.x;
      float _600 = _534 + _521;
      float _601 = _600 + _548;
      float _602 = _601 + _551;
      float _603 = _602 + _553;
      float _604 = _603 + _570;
      float _605 = _604 + _583;
      float _606 = _605 + _568;
      float _607 = _606 + _595;
      float _608 = _607 + _599;
      float _609 = c_specular_scale.y * _589;
      float _610 = _609 * _506;
      float _611 = _576 + (c_ps_user[13].x);
      float _612 = _611 + _580;
      float _613 = c_diffuse_scale.y * _612;
      float _614 = _613 * c_mat_diffuse.y;
      float _615 = _596 * c_specular_scale.y;
      float _616 = _615 + _610;
      float _617 = _616 * c_mat_specular.y;
      float _618 = _548 + _522;
      float _619 = _618 + _535;
      float _620 = _619 + _551;
      float _621 = _620 + _553;
      float _622 = _621 + _571;
      float _623 = _622 + _584;
      float _624 = _623 + _568;
      float _625 = _624 + _614;
      float _626 = _625 + _617;
      float _627 = c_specular_scale.z * _589;
      float _628 = _627 * _507;
      float _629 = _577 + (c_ps_user[13].x);
      float _630 = _629 + _581;
      float _631 = c_diffuse_scale.z * _630;
      float _632 = _631 * c_mat_diffuse.z;
      float _633 = _596 * c_specular_scale.z;
      float _634 = _633 + _628;
      float _635 = _634 * c_mat_specular.z;
      float _636 = _548 + _523;
      float _637 = _636 + _536;
      float _638 = _637 + _551;
      float _639 = _638 + _553;
      float _640 = _639 + _572;
      float _641 = _640 + _585;
      float _642 = _641 + _568;
      float _643 = _642 + _632;
      float _644 = _643 + _635;
      float _645 = saturate(_608);
      float _646 = saturate(_626);
      float _647 = saturate(_644);
      _665 = _645;
      _666 = _646;
      _667 = _647;
      _668 = 1.0f;
   }
   else
   {
      float _649 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _650 = _649 * _176;
      float _651 = _650 * _495;
      float _652 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _653 = _652 * _177;
      float _654 = _653 * _496;
      float _655 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _656 = _655 * _178;
      float _657 = _656 * _497;
      float _658 = _505 * _58.x;
      float _659 = _506 * _58.x;
      float _660 = _507 * _58.x;
      float _661 = _651 + _658;
      float _662 = _654 + _659;
      float _663 = _657 + _660;
      _665 = _661;
      _666 = _662;
      _667 = _663;
      _668 = _505;
   }
   float _673 = c_mat_opacity * _63.w;
   float _674 = _673 * c_opacity_scale;
   float _675 = _674 * c_global_opacity;
   float _676 = 1.0f - _675;
   float _677 = saturate(_668);
   float _678 = _676 * _677;
   float _679 = _678 + _675;
   float _680 = _665 * COLOR.x;
   float _681 = _666 * COLOR.y;
   float _682 = _667 * COLOR.z;
   int _683 = c_bool & 16;
   bool _684 = (_683 == 0);
   if (!_684)
   {
      float _688 = c_tone_control.x * _680;
      float _689 = c_tone_control.x * _681;
      float _690 = c_tone_control.x * _682;
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
      if (_509)
      {
         float _705 = c_tone_control.x * _680;
         float _706 = c_tone_control.x * _681;
         float _707 = c_tone_control.x * _682;
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
         _748 = _680;
         _749 = _681;
         _750 = _682;
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
   if (!_509)
   {
      float _779 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _780 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _781 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _783 = (c_ps_user[12].w) * COLOR.x;
      float _784 = (c_ps_user[12].w) * COLOR.y;
      float _785 = (c_ps_user[12].w) * COLOR.z;
      float _786 = _783 + _769;
      float _787 = _786 + _779;
      float _788 = _784 + _770;
      float _789 = _788 + _780;
      float _790 = _785 + _771;
      float _791 = _790 + _781;
      _793 = _787;
      _794 = _789;
      _795 = _791;
   }
   else
   {
      _793 = _769;
      _794 = _770;
      _795 = _771;
   }
   float _801 = (c_ps_col_scl[0].x) * _793;
   float _802 = (c_ps_col_scl[0].y) * _794;
   float _803 = (c_ps_col_scl[0].z) * _795;
   float _804 = (c_ps_col_scl[0].w) * _679;
   SV_Target.x = _801;
   SV_Target.y = _802;
   SV_Target.z = _803;
   SV_Target.w = _804;
   int _807 = c_bool & 8;
   bool _808 = (_807 != 0);
   float _809 = select(_808, _793, _801);
   float _810 = select(_808, _794, _802);
   float _811 = select(_808, _795, _803);
   float _812 = select(_808, _679, _804);
   SV_Target.x = _809;
   SV_Target.y = _810;
   SV_Target.z = _811;
   SV_Target.w = _812;
   return SV_Target;
}
