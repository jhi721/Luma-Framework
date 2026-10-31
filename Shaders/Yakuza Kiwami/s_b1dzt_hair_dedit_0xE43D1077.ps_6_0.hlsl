// Yakuza Kiwami material s_b1dzt[hair][dedit] (0xE43D1077, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

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

SamplerState s7_s : register(s7);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float4 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _56 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _60 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _65 = _60.x * _60.x;
   float _66 = _60.y * _60.y;
   float _67 = _60.z * _60.z;
   float4 _68 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _75 = _65 * (c_ps_user[3].x);
   float _76 = _66 * (c_ps_user[3].y);
   float _77 = _67 * (c_ps_user[3].z);
   float _80 = c_mat_power * _56.z;
   float _81 = max(1.0f, _80);
   float _82 = _68.x * 2.0f;
   float _83 = _68.y * 2.0f;
   float _84 = _82 + -1.0f;
   float _85 = _83 + -1.0f;
   float _86 = dot(float2(_84, _85), float2(_84, _85));
   float _87 = 1.0f - _86;
   float _88 = saturate(_87);
   float _89 = sqrt(_88);
   float _90 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _91 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _92 = _90 - _91;
   float _93 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _94 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _95 = _93 - _94;
   float _96 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _97 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _98 = _96 - _97;
   float _99 = _84 * TEXCOORD_3.x;
   float _100 = _84 * TEXCOORD_3.y;
   float _101 = _84 * TEXCOORD_3.z;
   float _102 = _85 * TEXCOORD_3.w;
   float _103 = _102 * _92;
   float _104 = _102 * _95;
   float _105 = _102 * _98;
   float _106 = _103 + _99;
   float _107 = _104 + _100;
   float _108 = _105 + _101;
   float _109 = _89 * TEXCOORD_2.x;
   float _110 = _89 * TEXCOORD_2.y;
   float _111 = _89 * TEXCOORD_2.z;
   float _112 = _106 + _109;
   float _113 = _107 + _110;
   float _114 = _108 + _111;
   float _115 = dot(float3(_112, _113, _114), float3(_112, _113, _114));
   float _116 = rsqrt(_115);
   float _117 = _112 * _116;
   float _118 = _113 * _116;
   float _119 = _114 * _116;
   float _124 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _125 = rsqrt(_124);
   float _126 = _125 * TEXCOORD_1.x;
   float _127 = _125 * TEXCOORD_1.y;
   float _128 = _125 * TEXCOORD_1.z;
   float _129 = _126 + (c_light_vec[0].x);
   float _130 = _127 + (c_light_vec[0].y);
   float _131 = _128 + (c_light_vec[0].z);
   float _132 = dot(float3(_129, _130, _131), float3(_129, _130, _131));
   float _133 = rsqrt(_132);
   float _134 = _129 * _133;
   float _135 = _130 * _133;
   float _136 = _131 * _133;
   float _140 = c_rt_size.z * SV_Position.x;
   float _141 = c_rt_size.w * SV_Position.y;
   int _144 = c_bool & 8;
   bool _145 = (_144 == 0);
   float _151;
   float _152;
   float _164;
   float _165;
   float _330;
   float _331;
   float _332;
   float _361;
   float _362;
   float _363;
   float _525;
   float _686;
   float _687;
   float _688;
   float _764;
   float _765;
   float _766;
   float _782;
   float _783;
   float _784;
   float _809;
   float _810;
   float _811;
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
   float _171 = (c_light_vec[1].x) + _126;
   float _172 = (c_light_vec[1].y) + _127;
   float _173 = (c_light_vec[1].z) + _128;
   float _174 = dot(float3(_171, _172, _173), float3(_171, _172, _173));
   float _175 = rsqrt(_174);
   float _176 = _171 * _175;
   float _177 = _172 * _175;
   float _178 = _173 * _175;
   float _179 = _118 * TEXCOORD_3.z;
   float _180 = _119 * TEXCOORD_3.y;
   float _181 = _179 - _180;
   float _182 = _119 * TEXCOORD_3.x;
   float _183 = _117 * TEXCOORD_3.z;
   float _184 = _182 - _183;
   float _185 = _117 * TEXCOORD_3.y;
   float _186 = _118 * TEXCOORD_3.x;
   float _187 = _185 - _186;
   float _188 = _181 * TEXCOORD_3.w;
   float _189 = _184 * TEXCOORD_3.w;
   float _190 = _187 * TEXCOORD_3.w;
   float _191 = _189 * _119;
   float _192 = _190 * _118;
   float _193 = _191 - _192;
   float _194 = _190 * _117;
   float _195 = _188 * _119;
   float _196 = _194 - _195;
   float _197 = _188 * _118;
   float _198 = _189 * _117;
   float _199 = _197 - _198;
   float _200 = -0.0f - _134;
   float _201 = -0.0f - _135;
   float _202 = -0.0f - _136;
   float _203 = dot(float3(_117, _118, _119), float3(_200, _201, _202));
   float _204 = dot(float3(_200, _201, _202), float3(_188, _189, _190));
   float _205 = TEXCOORD.z * 0.20000000298023224f;
   float _206 = _204 * _205;
   float _207 = _206 + 0.5f;
   float4 _208 = s7_t.Sample(s7_s, float2(_203, _207));
   float _210 = dot(float3(_200, _201, _202), float3(_193, _196, _199));
   float _211 = TEXCOORD.w * 0.20000000298023224f;
   float _212 = _210 * _211;
   float _213 = _212 + 0.5f;
   float4 _214 = s7_t.Sample(s7_s, float2(_203, _213));
   float _216 = _214.x * _208.x;
   float _217 = -0.0f - _176;
   float _218 = -0.0f - _177;
   float _219 = -0.0f - _178;
   float _220 = dot(float3(_117, _118, _119), float3(_217, _218, _219));
   float _221 = dot(float3(_217, _218, _219), float3(_188, _189, _190));
   float _222 = _221 * _205;
   float _223 = _222 + 0.5f;
   float4 _224 = s7_t.Sample(s7_s, float2(_220, _223));
   float _226 = dot(float3(_217, _218, _219), float3(_193, _196, _199));
   float _227 = _226 * _211;
   float _228 = _227 + 0.5f;
   float4 _229 = s7_t.Sample(s7_s, float2(_220, _228));
   float _231 = _229.x * _224.x;
   float _232 = log2(_216);
   float _233 = log2(_231);
   float _234 = _232 * _81;
   float _235 = _233 * _81;
   float _236 = exp2(_234);
   float _237 = exp2(_235);
   float _242 = c_mat_fresnel * c_tone_control.w;
   float _243 = _81 * 0.03974359855055809f;
   float _244 = _243 + 0.0856831967830658f;
   float _245 = _237 * _244;
   float _246 = dot(float3(_126, _127, _128), float3(_134, _135, _136));
   float _247 = dot(float3(_126, _127, _128), float3(_176, _177, _178));
   float _248 = 1.0f - _246;
   float _249 = 1.0f - _247;
   float _250 = _248 * _248;
   float _251 = _249 * _249;
   float _252 = 1.0f - _242;
   float _253 = _250 * _250;
   float _254 = _253 * _248;
   float _255 = _254 * _252;
   float _256 = _249 * _252;
   float _257 = _251 * _251;
   float _258 = _257 * _256;
   float _259 = _255 + _242;
   float _260 = _258 + _242;
   float _261 = _245 * _260;
   float _270 = c_specular_scale.x * c_mat_specular.x;
   float _271 = c_specular_scale.y * c_mat_specular.y;
   float _272 = c_specular_scale.z * c_mat_specular.z;
   float _277 = _166 * _244;
   float _278 = _277 * _236;
   float _279 = _278 * _259;
   float _280 = _279 * (c_light_col[0].x);
   float _281 = _279 * (c_light_col[0].y);
   float _282 = _279 * (c_light_col[0].z);
   float _287 = _261 * (c_light_col[1].x);
   float _288 = _261 * (c_light_col[1].y);
   float _289 = _261 * (c_light_col[1].z);
   float _290 = _280 + _287;
   float _291 = _281 + _288;
   float _292 = _282 + _289;
   float _293 = _270 * _290;
   float _294 = _271 * _291;
   float _295 = _272 * _292;
   int _301 = c_bool & 1;
   bool _302 = (_301 == 0);
   if (!_302)
   {
      float _304 = -0.0f - TEXCOORD_4.x;
      float _305 = -0.0f - TEXCOORD_4.y;
      float _306 = -0.0f - TEXCOORD_4.z;
      float _307 = dot(float3(_117, _118, _119), float3(_304, _305, _306));
      float _308 = saturate(_307);
      float _309 = _308 * TEXCOORD_1.w;
      float _310 = _308 * TEXCOORD_2.w;
      float _311 = _308 * TEXCOORD_4.w;
      float _312 = max(0.0f, _309);
      float _313 = max(0.0f, _310);
      float _314 = max(0.0f, _311);
      float _315 = -0.0f - TEXCOORD_5.x;
      float _316 = -0.0f - TEXCOORD_5.y;
      float _317 = -0.0f - TEXCOORD_5.z;
      float _318 = dot(float3(_117, _118, _119), float3(_315, _316, _317));
      float _319 = saturate(_318);
      float _320 = _319 * COLOR.w;
      float _321 = _319 * COLOR_1.w;
      float _322 = _319 * TEXCOORD_5.w;
      float _323 = max(0.0f, _320);
      float _324 = max(0.0f, _321);
      float _325 = max(0.0f, _322);
      float _326 = _323 + _312;
      float _327 = _324 + _313;
      float _328 = _325 + _314;
      _330 = _326;
      _331 = _327;
      _332 = _328;
   }
   else
   {
      _330 = 0.0f;
      _331 = 0.0f;
      _332 = 0.0f;
   }
   float _333 = c_rt_size.z * SV_Position.x;
   float _334 = c_rt_size.w * SV_Position.y;
   float4 _335 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_333, _334)));
   int _341 = c_bool & 16384;
   bool _342 = (_341 == 0);
   if (!_342)
   {
      float _344 = _335.x * 4.0f;
      float _345 = _335.y * 4.0f;
      float _346 = _335.z * 4.0f;
      _361 = _344;
      _362 = _345;
      _363 = _346;
   }
   else
   {
      int _348 = c_bool & 8192;
      bool _349 = (_348 == 0);
      if (!_349)
      {
         float _351 = _335.x * 2.0f;
         float _352 = _335.y * 2.0f;
         float _353 = _335.z * 2.0f;
         _361 = _351;
         _362 = _352;
         _363 = _353;
      }
      else
      {
         float _357 = _335.x / c_tone_control.z;
         float _358 = _335.y / c_tone_control.z;
         float _359 = _335.z / c_tone_control.z;
         _361 = _357;
         _362 = _358;
         _363 = _359;
      }
   }
   float _364 = _361 + _330;
   float _365 = _362 + _331;
   float _366 = _363 + _332;
   float _429 = dot(float3(_117, _118, _119), float3(_117, _118, _119));
   float _430 = rsqrt(_429);
   float _431 = _430 * _117;
   float _432 = _430 * _118;
   float _433 = _430 * _119;
   float _434 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_431, _432, _433, 1.0f));
   float _435 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_431, _432, _433, 1.0f));
   float _436 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_431, _432, _433, 1.0f));
   float _437 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_431, _432, _433, 1.0f));
   float _438 = dot(float4(_431, _432, _433, 1.0f), float4(_434, _435, _436, _437));
   float _439 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_431, _432, _433, 1.0f));
   float _440 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_431, _432, _433, 1.0f));
   float _441 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_431, _432, _433, 1.0f));
   float _442 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_431, _432, _433, 1.0f));
   float _443 = dot(float4(_431, _432, _433, 1.0f), float4(_439, _440, _441, _442));
   float _444 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_431, _432, _433, 1.0f));
   float _445 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_431, _432, _433, 1.0f));
   float _446 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_431, _432, _433, 1.0f));
   float _447 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_431, _432, _433, 1.0f));
   float _448 = dot(float4(_431, _432, _433, 1.0f), float4(_444, _445, _446, _447));
   float _449 = max(_438, 0.0625f);
   float _450 = max(_443, 0.0625f);
   float _451 = max(_448, 0.0625f);
   float _452 = _449 * (c_ps_scratch[79].x);
   float _453 = _450 * (c_ps_scratch[79].x);
   float _454 = _451 * (c_ps_scratch[79].x);
   float _459 = -0.0f - (c_light_vec[0].x);
   float _460 = -0.0f - (c_light_vec[0].y);
   float _461 = -0.0f - (c_light_vec[0].z);
   float _462 = dot(float3(_117, _118, _119), float3(_459, _460, _461));
   float _463 = -0.0f - (c_light_vec[1].x);
   float _464 = -0.0f - (c_light_vec[1].y);
   float _465 = -0.0f - (c_light_vec[1].z);
   float _466 = dot(float3(_117, _118, _119), float3(_463, _464, _465));
   float _475 = _118 * 0.5f;
   float _476 = _475 + 0.5f;
   float _477 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _478 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _479 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _480 = _477 * _476;
   float _481 = _478 * _476;
   float _482 = _479 * _476;
   float _483 = saturate(_462);
   float _484 = saturate(_466);
   float _489 = (c_light_col[0].x) * _483;
   float _490 = (c_light_col[0].y) * _483;
   float _491 = (c_light_col[0].z) * _483;
   float _496 = (c_light_col[1].x) * _484;
   float _497 = (c_light_col[1].y) * _484;
   float _498 = (c_light_col[1].z) * _484;
   float _499 = _489 * _166;
   float _500 = _490 * _166;
   float _501 = _491 * _166;
   float _502 = (c_light_vec[4].x) + _452;
   float _503 = _502 + _480;
   float _504 = _503 + _364;
   float _505 = _504 + _496;
   float _506 = _505 + _499;
   float _507 = (c_light_vec[4].y) + _453;
   float _508 = _507 + _481;
   float _509 = _508 + _365;
   float _510 = _509 + _497;
   float _511 = _510 + _500;
   float _512 = (c_light_vec[4].z) + _454;
   float _513 = _512 + _482;
   float _514 = _513 + _366;
   float _515 = _514 + _498;
   float _516 = _515 + _501;
   int _517 = c_bool & 64;
   bool _518 = (_517 == 0);
   if (!_518)
   {
      float _520 = 1.0f - _56.y;
      float _521 = _166 * _520;
      float _522 = _521 * _483;
      float _523 = _522 + _56.y;
      _525 = _523;
   }
   else
   {
      _525 = _56.y;
   }
   int _526 = c_bool & 32768;
   bool _527 = (_526 == 0);
   if (!_527)
   {
      float _539 = (c_ps_user[11].x) * _75;
      float _540 = (c_ps_user[11].x) * _76;
      float _541 = (c_ps_user[11].x) * _77;
      float _543 = _117 + 1.0f;
      float _544 = _118 + 1.0f;
      float _545 = _119 + 1.0f;
      float _546 = _543 * 0.5f;
      float _547 = _544 * 0.5f;
      float _548 = _545 * 0.5f;
      float _549 = saturate(_546);
      float _550 = saturate(_547);
      float _551 = saturate(_548);
      float _552 = (c_ps_user[11].y) * _549;
      float _553 = _550 * (c_ps_user[11].y);
      float _554 = _551 * (c_ps_user[11].y);
      float _564 = (c_ps_user[11].z) * _56.x;
      float _566 = (c_ps_user[11].w) * _293;
      float _569 = (c_ps_user[12].x) * _166;
      float _571 = (c_ps_user[12].y) * _56.y;
      float _576 = -0.0f - _126;
      float _577 = -0.0f - _127;
      float _578 = -0.0f - _128;
      float _579 = dot(float3(_576, _577, _578), float3(_117, _118, _119));
      float _580 = 1.0f - _579;
      float _581 = _580 * _580;
      float _582 = _580 * 0.9722222089767456f;
      float _583 = _581 * _581;
      float _584 = _583 * _582;
      float _585 = _584 + 0.02777777425944805f;
      float _586 = _585 * (c_ps_user[13].z);
      float _588 = (c_ps_user[13].w) * _503;
      float _589 = (c_ps_user[13].w) * _508;
      float _590 = (c_ps_user[13].w) * _513;
      float _593 = (c_ps_user[14].y) * _489;
      float _594 = (c_ps_user[14].y) * _490;
      float _595 = (c_ps_user[14].y) * _491;
      float _597 = (c_ps_user[14].z) * _496;
      float _598 = (c_ps_user[14].z) * _497;
      float _599 = (c_ps_user[14].z) * _498;
      float _601 = (c_ps_user[14].w) * _364;
      float _602 = (c_ps_user[14].w) * _365;
      float _603 = (c_ps_user[14].w) * _366;
      float _607 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _608 = c_specular_scale.x * _607;
      float _609 = _608 * _293;
      float _610 = _593 + (c_ps_user[13].x);
      float _611 = _610 + _597;
      float _612 = c_diffuse_scale.x * _611;
      float _613 = _612 * c_mat_diffuse.x;
      float _614 = (c_ps_user[13].y) + _564;
      float _615 = _614 * c_specular_scale.x;
      float _616 = _615 + _609;
      float _617 = _616 * c_mat_specular.x;
      float _618 = _552 + _539;
      float _619 = _618 + _566;
      float _620 = _619 + _569;
      float _621 = _620 + _571;
      float _622 = _621 + _588;
      float _623 = _622 + _601;
      float _624 = _623 + _586;
      float _625 = _624 + _613;
      float _626 = _625 + _617;
      float _627 = c_specular_scale.y * _607;
      float _628 = _627 * _294;
      float _629 = _594 + (c_ps_user[13].x);
      float _630 = _629 + _598;
      float _631 = c_diffuse_scale.y * _630;
      float _632 = _631 * c_mat_diffuse.y;
      float _633 = _614 * c_specular_scale.y;
      float _634 = _633 + _628;
      float _635 = _634 * c_mat_specular.y;
      float _636 = _566 + _540;
      float _637 = _636 + _553;
      float _638 = _637 + _569;
      float _639 = _638 + _571;
      float _640 = _639 + _589;
      float _641 = _640 + _602;
      float _642 = _641 + _586;
      float _643 = _642 + _632;
      float _644 = _643 + _635;
      float _645 = c_specular_scale.z * _607;
      float _646 = _645 * _295;
      float _647 = _595 + (c_ps_user[13].x);
      float _648 = _647 + _599;
      float _649 = c_diffuse_scale.z * _648;
      float _650 = _649 * c_mat_diffuse.z;
      float _651 = _614 * c_specular_scale.z;
      float _652 = _651 + _646;
      float _653 = _652 * c_mat_specular.z;
      float _654 = _566 + _541;
      float _655 = _654 + _554;
      float _656 = _655 + _569;
      float _657 = _656 + _571;
      float _658 = _657 + _590;
      float _659 = _658 + _603;
      float _660 = _659 + _586;
      float _661 = _660 + _650;
      float _662 = _661 + _653;
      float _663 = saturate(_626);
      float _664 = saturate(_644);
      float _665 = saturate(_662);
      _686 = _663;
      _687 = _664;
      _688 = _665;
   }
   else
   {
      float _667 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _668 = _667 * _75;
      float _669 = _668 * _506;
      float _670 = _669 * _525;
      float _671 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _672 = _671 * _76;
      float _673 = _672 * _511;
      float _674 = _673 * _525;
      float _675 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _676 = _675 * _77;
      float _677 = _676 * _516;
      float _678 = _677 * _525;
      float _679 = _293 * _56.x;
      float _680 = _294 * _56.x;
      float _681 = _295 * _56.x;
      float _682 = _670 + _679;
      float _683 = _674 + _680;
      float _684 = _678 + _681;
      _686 = _682;
      _687 = _683;
      _688 = _684;
   }
   float _693 = c_mat_opacity * _60.w;
   float _694 = _693 * c_opacity_scale;
   float _695 = _694 * c_global_opacity;
   float _696 = _686 * COLOR.x;
   float _697 = _687 * COLOR.y;
   float _698 = _688 * COLOR.z;
   int _699 = c_bool & 16;
   bool _700 = (_699 == 0);
   if (!_700)
   {
      float _704 = c_tone_control.x * _696;
      float _705 = c_tone_control.x * _697;
      float _706 = c_tone_control.x * _698;
      float _708 = log2(_704);
      float _709 = log2(_705);
      float _710 = log2(_706);
      float _711 = _708 * c_tone_control.y;
      float _712 = _709 * c_tone_control.y;
      float _713 = _710 * c_tone_control.y;
      float _714 = exp2(_711);
      float _715 = exp2(_712);
      float _716 = exp2(_713);
      _764 = _714;
      _765 = _715;
      _766 = _716;
   }
   else
   {
      if (_527)
      {
         float _721 = c_tone_control.x * _696;
         float _722 = c_tone_control.x * _697;
         float _723 = c_tone_control.x * _698;
         float _724 = _721 * 2.0f;
         float _725 = _722 * 2.0f;
         float _726 = _723 * 2.0f;
         float _727 = _721 * 0.30000001192092896f;
         float _728 = _722 * 0.30000001192092896f;
         float _729 = _723 * 0.30000001192092896f;
         float _730 = _727 + 0.05000000074505806f;
         float _731 = _728 + 0.05000000074505806f;
         float _732 = _729 + 0.05000000074505806f;
         float _733 = _730 * _724;
         float _734 = _731 * _725;
         float _735 = _732 * _726;
         float _736 = _733 + 0.004000000189989805f;
         float _737 = _734 + 0.004000000189989805f;
         float _738 = _735 + 0.004000000189989805f;
         float _739 = _727 + 0.5f;
         float _740 = _728 + 0.5f;
         float _741 = _729 + 0.5f;
         float _742 = _739 * _724;
         float _743 = _740 * _725;
         float _744 = _741 * _726;
         float _745 = _742 + 0.06000000238418579f;
         float _746 = _743 + 0.06000000238418579f;
         float _747 = _744 + 0.06000000238418579f;
         float _748 = _736 / _745;
         float _749 = _737 / _746;
         float _750 = _738 / _747;
         float _751 = _748 + -0.06666666269302368f;
         float _752 = _749 + -0.06666666269302368f;
         float _753 = _750 + -0.06666666269302368f;
         float _754 = _751 * 1.3790643215179443f;
         float _755 = _752 * 1.3790643215179443f;
         float _756 = _753 * 1.3790643215179443f;
         float _757 = YKR_MaterialCurve(_754, _721);
         float _758 = YKR_MaterialCurve(_755, _722);
         float _759 = YKR_MaterialCurve(_756, _723);
         float _760 = sqrt(_757);
         float _761 = sqrt(_758);
         float _762 = sqrt(_759);
         _764 = _760;
         _765 = _761;
         _766 = _762;
      }
      else
      {
         _764 = _696;
         _765 = _697;
         _766 = _698;
      }
   }
   int _767 = c_bool & 4;
   bool _768 = (_767 == 0);
   if (_768)
   {
      float _770 = saturate(COLOR_1.x);
      float _775 = (c_fog_param[0].x) - _764;
      float _776 = (c_fog_param[0].y) - _765;
      float _777 = (c_fog_param[0].z) - _766;
      float _778 = _775 * _770;
      float _779 = _776 * _770;
      float _780 = _777 * _770;
      _782 = _778;
      _783 = _779;
      _784 = _780;
   }
   else
   {
      _782 = COLOR_1.x;
      _783 = COLOR_1.y;
      _784 = COLOR_1.z;
   }
   float _785 = _782 + _764;
   float _786 = _783 + _765;
   float _787 = _784 + _766;
   if (!_527)
   {
      float _795 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _796 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _797 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _799 = (c_ps_user[12].w) * COLOR.x;
      float _800 = (c_ps_user[12].w) * COLOR.y;
      float _801 = (c_ps_user[12].w) * COLOR.z;
      float _802 = _799 + _785;
      float _803 = _802 + _795;
      float _804 = _800 + _786;
      float _805 = _804 + _796;
      float _806 = _801 + _787;
      float _807 = _806 + _797;
      _809 = _803;
      _810 = _805;
      _811 = _807;
   }
   else
   {
      _809 = _785;
      _810 = _786;
      _811 = _787;
   }
   float _814 = _695 - c_mat_punchthrough_ref;
   bool _815 = (_814 < 0.0f);
   if (_815)
      discard;
   float _821 = (c_ps_col_scl[0].x) * _809;
   float _822 = (c_ps_col_scl[0].y) * _810;
   float _823 = (c_ps_col_scl[0].z) * _811;
   float _824 = (c_ps_col_scl[0].w) * _695;
   SV_Target.x = _821;
   SV_Target.y = _822;
   SV_Target.z = _823;
   SV_Target.w = _824;
   int _827 = c_bool & 8;
   bool _828 = (_827 != 0);
   float _829 = select(_828, _809, _821);
   float _830 = select(_828, _810, _822);
   float _831 = select(_828, _811, _823);
   float _832 = select(_828, _695, _824);
   SV_Target.x = _829;
   SV_Target.y = _830;
   SV_Target.z = _831;
   SV_Target.w = _832;
   return SV_Target;
}
