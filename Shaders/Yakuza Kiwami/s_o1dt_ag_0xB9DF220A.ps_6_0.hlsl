// Yakuza Kiwami material s_o1dt[ag] (0xB9DF220A, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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

SamplerState s5_s : register(s5);

SamplerState s13_s : register(s13);

SamplerState s14_s : register(s14);

float4 main(
    linear float4 COLOR : COLOR,
    linear float4 COLOR_1 : COLOR1,
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _50 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _55 = _50.x * _50.x;
   float _56 = _50.y * _50.y;
   float _57 = _50.z * _50.z;
   float4 _58 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _63 = _58.x * 2.0f;
   float _64 = _58.y * 2.0f;
   float _65 = _63 + -1.0f;
   float _66 = _64 + -1.0f;
   float _67 = dot(float2(_65, _66), float2(_65, _66));
   float _68 = 1.0f - _67;
   float _69 = saturate(_68);
   float _70 = sqrt(_69);
   float _71 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _72 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _73 = _71 - _72;
   float _74 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _75 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _76 = _74 - _75;
   float _77 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _78 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _79 = _77 - _78;
   float _80 = _65 * TEXCOORD_3.x;
   float _81 = _65 * TEXCOORD_3.y;
   float _82 = _65 * TEXCOORD_3.z;
   float _83 = _66 * TEXCOORD_3.w;
   float _84 = _83 * _73;
   float _85 = _83 * _76;
   float _86 = _83 * _79;
   float _87 = _84 + _80;
   float _88 = _85 + _81;
   float _89 = _86 + _82;
   float _90 = _70 * TEXCOORD_2.x;
   float _91 = _70 * TEXCOORD_2.y;
   float _92 = _70 * TEXCOORD_2.z;
   float _93 = _87 + _90;
   float _94 = _88 + _91;
   float _95 = _89 + _92;
   float _96 = dot(float3(_93, _94, _95), float3(_93, _94, _95));
   float _97 = rsqrt(_96);
   float _98 = _93 * _97;
   float _99 = _94 * _97;
   float _100 = _95 * _97;
   float _105 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _106 = rsqrt(_105);
   float _107 = _106 * TEXCOORD_1.x;
   float _108 = _106 * TEXCOORD_1.y;
   float _109 = _106 * TEXCOORD_1.z;
   float _113 = c_rt_size.z * SV_Position.x;
   float _114 = c_rt_size.w * SV_Position.y;
   int _117 = c_bool & 8;
   bool _118 = (_117 == 0);
   float _124;
   float _125;
   float _137;
   float _138;
   float _233;
   float _234;
   float _235;
   float _236;
   float _237;
   float _238;
   float _271;
   float _272;
   float _273;
   float _274;
   float _541;
   float _542;
   float _543;
   float _619;
   float _620;
   float _621;
   float _637;
   float _638;
   float _639;
   float _664;
   float _665;
   float _666;
   if (!_118)
   {
      float4 _120 = s13_t.Sample(s13_s, float2(_113, _114));
      _124 = _120.x;
      _125 = _120.y;
   }
   else
   {
      _124 = 1.0f;
      _125 = 1.0f;
   }
   if (!_118)
   {
      float _130 = (c_sdw_param_ch[0].x) * _124;
      float _131 = (c_sdw_param_ch[0].z) * _125;
      float _134 = _130 + (c_sdw_param_ch[0].y);
      float _135 = _131 + (c_sdw_param_ch[0].w);
      _137 = _134;
      _138 = _135;
   }
   else
   {
      _137 = _124;
      _138 = _125;
   }
   float _139 = min(_137, _138);
   int _143 = c_bool & 1;
   bool _144 = (_143 == 0);
   if (!_144)
   {
      float _146 = -0.0f - TEXCOORD_4.x;
      float _147 = -0.0f - TEXCOORD_4.y;
      float _148 = -0.0f - TEXCOORD_4.z;
      float _149 = dot(float3(_98, _99, _100), float3(_146, _147, _148));
      float _150 = saturate(_149);
      float _151 = _150 * TEXCOORD_1.w;
      float _152 = _150 * TEXCOORD_2.w;
      float _153 = _150 * TEXCOORD_4.w;
      float _154 = max(0.0f, _151);
      float _155 = max(0.0f, _152);
      float _156 = max(0.0f, _153);
      float _157 = -0.0f - TEXCOORD_5.x;
      float _158 = -0.0f - TEXCOORD_5.y;
      float _159 = -0.0f - TEXCOORD_5.z;
      float _160 = dot(float3(_98, _99, _100), float3(_157, _158, _159));
      float _161 = saturate(_160);
      float _162 = _161 * COLOR.w;
      float _163 = _161 * COLOR_1.w;
      float _164 = _161 * TEXCOORD_5.w;
      float _165 = max(0.0f, _162);
      float _166 = max(0.0f, _163);
      float _167 = max(0.0f, _164);
      float _168 = _165 + _154;
      float _169 = _166 + _155;
      float _170 = _167 + _156;
      float _171 = _107 + TEXCOORD_4.x;
      float _172 = _108 + TEXCOORD_4.y;
      float _173 = _109 + TEXCOORD_4.z;
      float _174 = dot(float3(_171, _172, _173), float3(_171, _172, _173));
      float _175 = rsqrt(_174);
      float _176 = _107 + TEXCOORD_5.x;
      float _177 = _108 + TEXCOORD_5.y;
      float _178 = _109 + TEXCOORD_5.z;
      float _179 = dot(float3(_176, _177, _178), float3(_176, _177, _178));
      float _180 = rsqrt(_179);
      float _181 = _176 * _180;
      float _182 = -0.0f - _181;
      float _183 = _177 * _180;
      float _184 = -0.0f - _183;
      float _185 = _178 * _180;
      float _186 = -0.0f - _185;
      float _187 = _171 * _175;
      float _188 = -0.0f - _187;
      float _189 = _172 * _175;
      float _190 = -0.0f - _189;
      float _191 = _173 * _175;
      float _192 = -0.0f - _191;
      float _193 = dot(float3(_188, _190, _192), float3(_98, _99, _100));
      float _194 = dot(float3(_182, _184, _186), float3(_98, _99, _100));
      float _195 = saturate(_193);
      float _196 = saturate(_194);
      float _197 = log2(_195);
      float _198 = log2(_196);
      float _199 = _197 * c_mat_power;
      float _200 = _198 * c_mat_power;
      float _201 = exp2(_199);
      float _202 = exp2(_200);
      float _203 = _201 * TEXCOORD_1.w;
      float _204 = _201 * TEXCOORD_2.w;
      float _205 = _201 * TEXCOORD_4.w;
      float _206 = _202 * COLOR.w;
      float _207 = _202 * COLOR_1.w;
      float _208 = _202 * TEXCOORD_5.w;
      float _209 = _206 + _203;
      float _210 = _207 + _204;
      float _211 = _208 + _205;
      float _212 = max(0.0f, _209);
      float _213 = max(0.0f, _210);
      float _214 = max(0.0f, _211);
      float _223 = c_mat_specular.x * _212;
      float _224 = _223 * c_specular_scale.x;
      float _225 = c_mat_specular.y * _213;
      float _226 = _225 * c_specular_scale.y;
      float _227 = c_mat_specular.z * _214;
      float _228 = _227 * c_specular_scale.z;
      float _229 = min(_224, 1.0f);
      float _230 = min(_226, 1.0f);
      float _231 = min(_228, 1.0f);
      _233 = _229;
      _234 = _230;
      _235 = _231;
      _236 = _168;
      _237 = _169;
      _238 = _170;
   }
   else
   {
      _233 = 0.0f;
      _234 = 0.0f;
      _235 = 0.0f;
      _236 = 0.0f;
      _237 = 0.0f;
      _238 = 0.0f;
   }
   float _239 = c_rt_size.z * SV_Position.x;
   float _240 = c_rt_size.w * SV_Position.y;
   float4 _241 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_239, _240)));
   int _248 = c_bool & 16384;
   bool _249 = (_248 == 0);
   if (!_249)
   {
      float _251 = _241.x * 4.0f;
      float _252 = _241.y * 4.0f;
      float _253 = _241.z * 4.0f;
      float _254 = _241.w * 4.0f;
      _271 = _251;
      _272 = _252;
      _273 = _253;
      _274 = _254;
   }
   else
   {
      int _256 = c_bool & 8192;
      bool _257 = (_256 == 0);
      if (!_257)
      {
         float _259 = _241.x * 2.0f;
         float _260 = _241.y * 2.0f;
         float _261 = _241.z * 2.0f;
         float _262 = _241.w * 2.0f;
         _271 = _259;
         _272 = _260;
         _273 = _261;
         _274 = _262;
      }
      else
      {
         float _266 = _241.x / c_tone_control.z;
         float _267 = _241.y / c_tone_control.z;
         float _268 = _241.z / c_tone_control.z;
         float _269 = _241.w / c_tone_control.z;
         _271 = _266;
         _272 = _267;
         _273 = _268;
         _274 = _269;
      }
   }
   float _275 = _271 + _236;
   float _276 = _272 + _237;
   float _277 = _273 + _238;
   float _340 = dot(float3(_98, _99, _100), float3(_98, _99, _100));
   float _341 = rsqrt(_340);
   float _342 = _341 * _98;
   float _343 = _341 * _99;
   float _344 = _341 * _100;
   float _345 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_342, _343, _344, 1.0f));
   float _346 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_342, _343, _344, 1.0f));
   float _347 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_342, _343, _344, 1.0f));
   float _348 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_342, _343, _344, 1.0f));
   float _349 = dot(float4(_342, _343, _344, 1.0f), float4(_345, _346, _347, _348));
   float _350 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_342, _343, _344, 1.0f));
   float _351 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_342, _343, _344, 1.0f));
   float _352 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_342, _343, _344, 1.0f));
   float _353 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_342, _343, _344, 1.0f));
   float _354 = dot(float4(_342, _343, _344, 1.0f), float4(_350, _351, _352, _353));
   float _355 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_342, _343, _344, 1.0f));
   float _356 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_342, _343, _344, 1.0f));
   float _357 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_342, _343, _344, 1.0f));
   float _358 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_342, _343, _344, 1.0f));
   float _359 = dot(float4(_342, _343, _344, 1.0f), float4(_355, _356, _357, _358));
   float _360 = max(_349, 0.0625f);
   float _361 = max(_354, 0.0625f);
   float _362 = max(_359, 0.0625f);
   float _363 = _360 * (c_ps_scratch[79].x);
   float _364 = _361 * (c_ps_scratch[79].x);
   float _365 = _362 * (c_ps_scratch[79].x);
   float _370 = -0.0f - (c_light_vec[0].x);
   float _371 = -0.0f - (c_light_vec[0].y);
   float _372 = -0.0f - (c_light_vec[0].z);
   float _373 = dot(float3(_98, _99, _100), float3(_370, _371, _372));
   float _374 = -0.0f - (c_light_vec[1].x);
   float _375 = -0.0f - (c_light_vec[1].y);
   float _376 = -0.0f - (c_light_vec[1].z);
   float _377 = dot(float3(_98, _99, _100), float3(_374, _375, _376));
   float _386 = _99 * 0.5f;
   float _387 = _386 + 0.5f;
   float _388 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _389 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _390 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _391 = _388 * _387;
   float _392 = _389 * _387;
   float _393 = _390 * _387;
   float _394 = saturate(_373);
   float _395 = saturate(_377);
   float _400 = (c_light_col[0].x) * _394;
   float _401 = (c_light_col[0].y) * _394;
   float _402 = (c_light_col[0].z) * _394;
   float _407 = (c_light_col[1].x) * _395;
   float _408 = (c_light_col[1].y) * _395;
   float _409 = (c_light_col[1].z) * _395;
   int _410 = c_bool & 32768;
   bool _411 = (_410 == 0);
   float _420 = (c_light_vec[4].x) + _363;
   float _421 = _420 + _391;
   float _422 = (c_light_vec[4].y) + _364;
   float _423 = _422 + _392;
   float _424 = (c_light_vec[4].z) + _365;
   float _425 = _424 + _393;
   if (!_411)
   {
      float _429 = (c_ps_user[11].x) * _55;
      float _430 = (c_ps_user[11].x) * _56;
      float _431 = (c_ps_user[11].x) * _57;
      float _433 = _98 + 1.0f;
      float _434 = _99 + 1.0f;
      float _435 = _100 + 1.0f;
      float _436 = _433 * 0.5f;
      float _437 = _434 * 0.5f;
      float _438 = _435 * 0.5f;
      float _439 = saturate(_436);
      float _440 = saturate(_437);
      float _441 = saturate(_438);
      float _442 = (c_ps_user[11].y) * _439;
      float _443 = _440 * (c_ps_user[11].y);
      float _444 = _441 * (c_ps_user[11].y);
      float _447 = (c_ps_user[12].x) * _139;
      float _452 = -0.0f - _107;
      float _453 = -0.0f - _108;
      float _454 = -0.0f - _109;
      float _455 = dot(float3(_452, _453, _454), float3(_98, _99, _100));
      float _456 = 1.0f - _455;
      float _457 = _456 * _456;
      float _458 = _456 * 0.9722222089767456f;
      float _459 = _457 * _457;
      float _460 = _459 * _458;
      float _461 = _460 + 0.02777777425944805f;
      float _462 = _461 * (c_ps_user[13].z);
      float _464 = (c_ps_user[13].w) * _421;
      float _465 = (c_ps_user[13].w) * _423;
      float _466 = (c_ps_user[13].w) * _425;
      float _469 = (c_ps_user[14].y) * _400;
      float _470 = (c_ps_user[14].y) * _401;
      float _471 = (c_ps_user[14].y) * _402;
      float _473 = (c_ps_user[14].z) * _407;
      float _474 = (c_ps_user[14].z) * _408;
      float _475 = (c_ps_user[14].z) * _409;
      float _477 = (c_ps_user[14].w) * _275;
      float _478 = (c_ps_user[14].w) * _276;
      float _479 = (c_ps_user[14].w) * _277;
      float _480 = _447 + (c_ps_user[12].y);
      float _481 = _469 + (c_ps_user[13].x);
      float _482 = _481 + _473;
      float _483 = c_diffuse_scale.x * _482;
      float _484 = _483 * c_mat_diffuse.x;
      float _485 = _462 + _480;
      float _486 = _485 + _429;
      float _487 = _486 + _442;
      float _488 = _487 + _464;
      float _489 = _488 + _477;
      float _490 = _489 + _484;
      float _491 = _470 + (c_ps_user[13].x);
      float _492 = _491 + _474;
      float _493 = c_diffuse_scale.y * _492;
      float _494 = _493 * c_mat_diffuse.y;
      float _495 = _485 + _430;
      float _496 = _495 + _443;
      float _497 = _496 + _465;
      float _498 = _497 + _478;
      float _499 = _498 + _494;
      float _500 = _471 + (c_ps_user[13].x);
      float _501 = _500 + _475;
      float _502 = c_diffuse_scale.z * _501;
      float _503 = _502 * c_mat_diffuse.z;
      float _504 = _485 + _431;
      float _505 = _504 + _444;
      float _506 = _505 + _466;
      float _507 = _506 + _479;
      float _508 = _507 + _503;
      float _509 = saturate(_490);
      float _510 = saturate(_499);
      float _511 = saturate(_508);
      _541 = _509;
      _542 = _510;
      _543 = _511;
   }
   else
   {
      float _513 = _402 * _139;
      float _514 = _425 + _277;
      float _515 = _514 + _409;
      float _516 = _515 + _513;
      float _517 = _401 * _139;
      float _518 = _423 + _276;
      float _519 = _518 + _408;
      float _520 = _519 + _517;
      float _521 = _400 * _139;
      float _522 = _421 + _275;
      float _523 = _522 + _407;
      float _524 = _523 + _521;
      float _525 = _274 + _235;
      float _526 = _274 + _234;
      float _527 = _274 + _233;
      float _528 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _529 = _528 * _55;
      float _530 = _529 * _524;
      float _531 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _532 = _531 * _56;
      float _533 = _532 * _520;
      float _534 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _535 = _534 * _57;
      float _536 = _535 * _516;
      float _537 = _527 + _530;
      float _538 = _526 + _533;
      float _539 = _525 + _536;
      _541 = _537;
      _542 = _538;
      _543 = _539;
   }
   float _548 = c_mat_opacity * _50.w;
   float _549 = _548 * c_opacity_scale;
   float _550 = _549 * c_global_opacity;
   float _551 = _541 * COLOR.x;
   float _552 = _542 * COLOR.y;
   float _553 = _543 * COLOR.z;
   int _554 = c_bool & 16;
   bool _555 = (_554 == 0);
   if (!_555)
   {
      float _559 = c_tone_control.x * _551;
      float _560 = c_tone_control.x * _552;
      float _561 = c_tone_control.x * _553;
      float _563 = log2(_559);
      float _564 = log2(_560);
      float _565 = log2(_561);
      float _566 = _563 * c_tone_control.y;
      float _567 = _564 * c_tone_control.y;
      float _568 = _565 * c_tone_control.y;
      float _569 = exp2(_566);
      float _570 = exp2(_567);
      float _571 = exp2(_568);
      _619 = _569;
      _620 = _570;
      _621 = _571;
   }
   else
   {
      if (_411)
      {
         float _576 = c_tone_control.x * _551;
         float _577 = c_tone_control.x * _552;
         float _578 = c_tone_control.x * _553;
         float _579 = _576 * 2.0f;
         float _580 = _577 * 2.0f;
         float _581 = _578 * 2.0f;
         float _582 = _576 * 0.30000001192092896f;
         float _583 = _577 * 0.30000001192092896f;
         float _584 = _578 * 0.30000001192092896f;
         float _585 = _582 + 0.05000000074505806f;
         float _586 = _583 + 0.05000000074505806f;
         float _587 = _584 + 0.05000000074505806f;
         float _588 = _585 * _579;
         float _589 = _586 * _580;
         float _590 = _587 * _581;
         float _591 = _588 + 0.004000000189989805f;
         float _592 = _589 + 0.004000000189989805f;
         float _593 = _590 + 0.004000000189989805f;
         float _594 = _582 + 0.5f;
         float _595 = _583 + 0.5f;
         float _596 = _584 + 0.5f;
         float _597 = _594 * _579;
         float _598 = _595 * _580;
         float _599 = _596 * _581;
         float _600 = _597 + 0.06000000238418579f;
         float _601 = _598 + 0.06000000238418579f;
         float _602 = _599 + 0.06000000238418579f;
         float _603 = _591 / _600;
         float _604 = _592 / _601;
         float _605 = _593 / _602;
         float _606 = _603 + -0.06666666269302368f;
         float _607 = _604 + -0.06666666269302368f;
         float _608 = _605 + -0.06666666269302368f;
         float _609 = _606 * 1.3790643215179443f;
         float _610 = _607 * 1.3790643215179443f;
         float _611 = _608 * 1.3790643215179443f;
         float _612 = YKR_MaterialCurve(_609, _576);
         float _613 = YKR_MaterialCurve(_610, _577);
         float _614 = YKR_MaterialCurve(_611, _578);
         float _615 = sqrt(_612);
         float _616 = sqrt(_613);
         float _617 = sqrt(_614);
         _619 = _615;
         _620 = _616;
         _621 = _617;
      }
      else
      {
         _619 = _551;
         _620 = _552;
         _621 = _553;
      }
   }
   int _622 = c_bool & 4;
   bool _623 = (_622 == 0);
   if (_623)
   {
      float _625 = saturate(COLOR_1.x);
      float _630 = (c_fog_param[0].x) - _619;
      float _631 = (c_fog_param[0].y) - _620;
      float _632 = (c_fog_param[0].z) - _621;
      float _633 = _630 * _625;
      float _634 = _631 * _625;
      float _635 = _632 * _625;
      _637 = _633;
      _638 = _634;
      _639 = _635;
   }
   else
   {
      _637 = COLOR_1.x;
      _638 = COLOR_1.y;
      _639 = COLOR_1.z;
   }
   float _640 = _637 + _619;
   float _641 = _638 + _620;
   float _642 = _639 + _621;
   if (!_411)
   {
      float _650 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _651 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _652 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _654 = (c_ps_user[12].w) * COLOR.x;
      float _655 = (c_ps_user[12].w) * COLOR.y;
      float _656 = (c_ps_user[12].w) * COLOR.z;
      float _657 = _654 + _640;
      float _658 = _657 + _650;
      float _659 = _655 + _641;
      float _660 = _659 + _651;
      float _661 = _656 + _642;
      float _662 = _661 + _652;
      _664 = _658;
      _665 = _660;
      _666 = _662;
   }
   else
   {
      _664 = _640;
      _665 = _641;
      _666 = _642;
   }
   float _672 = (c_ps_col_scl[0].x) * _664;
   float _673 = (c_ps_col_scl[0].y) * _665;
   float _674 = (c_ps_col_scl[0].z) * _666;
   float _675 = (c_ps_col_scl[0].w) * _550;
   SV_Target.x = _672;
   SV_Target.y = _673;
   SV_Target.z = _674;
   SV_Target.w = _675;
   int _678 = c_bool & 8;
   bool _679 = (_678 != 0);
   float _680 = select(_679, _664, _672);
   float _681 = select(_679, _665, _673);
   float _682 = select(_679, _666, _674);
   float _683 = select(_679, _550, _675);
   SV_Target.x = _680;
   SV_Target.y = _681;
   SV_Target.z = _682;
   SV_Target.w = _683;
   return SV_Target;
}
