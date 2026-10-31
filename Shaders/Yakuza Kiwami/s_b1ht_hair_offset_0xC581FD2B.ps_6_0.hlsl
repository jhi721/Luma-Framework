// Yakuza Kiwami material s_b1ht[hair][offset] (0xC581FD2B, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

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
   float4 _54 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _56 = _54.y * _54.y;
   float4 _57 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _60 = _57.x + -0.49803921580314636f;
   float4 _61 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   bool _64 = (_61.y < 0.5f);
   float _65 = _60 * 0.25f;
   float _66 = _65 + 0.9980392456054688f;
   float _67 = _61.y * _66;
   float _68 = 1.0f - _61.y;
   float _69 = 1.0019607543945312f - _65;
   float _70 = _68 * _69;
   float _71 = 1.0f - _70;
   float _72 = select(_64, _67, _71);
   float _75 = _61.x * 2.0f;
   float _76 = _72 * 2.0f;
   float _77 = _75 + -1.0f;
   float _78 = _76 + -1.0f;
   float _79 = dot(float2(_77, _78), float2(_77, _78));
   float _80 = 1.0f - _79;
   float _81 = saturate(_80);
   float _82 = sqrt(_81);
   float _83 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _84 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _85 = _83 - _84;
   float _86 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _87 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _88 = _86 - _87;
   float _89 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _90 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _91 = _89 - _90;
   float _92 = _77 * TEXCOORD_3.x;
   float _93 = _77 * TEXCOORD_3.y;
   float _94 = _77 * TEXCOORD_3.z;
   float _95 = _78 * TEXCOORD_3.w;
   float _96 = _95 * _85;
   float _97 = _95 * _88;
   float _98 = _95 * _91;
   float _99 = _82 * TEXCOORD_2.x;
   float _100 = _82 * TEXCOORD_2.y;
   float _101 = _82 * TEXCOORD_2.z;
   float _102 = _99 + _92;
   float _103 = _102 + _96;
   float _104 = _100 + _93;
   float _105 = _104 + _97;
   float _106 = _101 + _94;
   float _107 = _106 + _98;
   float _108 = dot(float3(_103, _105, _107), float3(_103, _105, _107));
   float _109 = rsqrt(_108);
   float _110 = _103 * _109;
   float _111 = _105 * _109;
   float _112 = _107 * _109;
   float _117 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _118 = rsqrt(_117);
   float _119 = _118 * TEXCOORD_1.x;
   float _120 = _118 * TEXCOORD_1.y;
   float _121 = _118 * TEXCOORD_1.z;
   float _122 = _119 + (c_light_vec[0].x);
   float _123 = _120 + (c_light_vec[0].y);
   float _124 = _121 + (c_light_vec[0].z);
   float _125 = dot(float3(_122, _123, _124), float3(_122, _123, _124));
   float _126 = rsqrt(_125);
   float _127 = _122 * _126;
   float _128 = _123 * _126;
   float _129 = _124 * _126;
   float _133 = c_rt_size.z * SV_Position.x;
   float _134 = c_rt_size.w * SV_Position.y;
   int _137 = c_bool & 8;
   bool _138 = (_137 == 0);
   float _144;
   float _145;
   float _157;
   float _158;
   float _323;
   float _324;
   float _325;
   float _354;
   float _355;
   float _356;
   float _659;
   float _660;
   float _661;
   float _737;
   float _738;
   float _739;
   float _755;
   float _756;
   float _757;
   float _782;
   float _783;
   float _784;
   if (!_138)
   {
      float4 _140 = s13_t.Sample(s13_s, float2(_133, _134));
      _144 = _140.x;
      _145 = _140.y;
   }
   else
   {
      _144 = 1.0f;
      _145 = 1.0f;
   }
   if (!_138)
   {
      float _150 = (c_sdw_param_ch[0].x) * _144;
      float _151 = (c_sdw_param_ch[0].z) * _145;
      float _154 = _150 + (c_sdw_param_ch[0].y);
      float _155 = _151 + (c_sdw_param_ch[0].w);
      _157 = _154;
      _158 = _155;
   }
   else
   {
      _157 = _144;
      _158 = _145;
   }
   float _159 = min(_157, _158);
   float _164 = (c_light_vec[1].x) + _119;
   float _165 = (c_light_vec[1].y) + _120;
   float _166 = (c_light_vec[1].z) + _121;
   float _167 = dot(float3(_164, _165, _166), float3(_164, _165, _166));
   float _168 = rsqrt(_167);
   float _169 = _164 * _168;
   float _170 = _165 * _168;
   float _171 = _166 * _168;
   float _172 = _111 * TEXCOORD_3.z;
   float _173 = _112 * TEXCOORD_3.y;
   float _174 = _172 - _173;
   float _175 = _112 * TEXCOORD_3.x;
   float _176 = _110 * TEXCOORD_3.z;
   float _177 = _175 - _176;
   float _178 = _110 * TEXCOORD_3.y;
   float _179 = _111 * TEXCOORD_3.x;
   float _180 = _178 - _179;
   float _181 = _174 * TEXCOORD_3.w;
   float _182 = _177 * TEXCOORD_3.w;
   float _183 = _180 * TEXCOORD_3.w;
   float _184 = _182 * _112;
   float _185 = _183 * _111;
   float _186 = _184 - _185;
   float _187 = _183 * _110;
   float _188 = _181 * _112;
   float _189 = _187 - _188;
   float _190 = _181 * _111;
   float _191 = _182 * _110;
   float _192 = _190 - _191;
   float _193 = -0.0f - _127;
   float _194 = -0.0f - _128;
   float _195 = -0.0f - _129;
   float _196 = dot(float3(_110, _111, _112), float3(_193, _194, _195));
   float _197 = dot(float3(_193, _194, _195), float3(_181, _182, _183));
   float _198 = TEXCOORD.z * 0.20000000298023224f;
   float _199 = _197 * _198;
   float _200 = _199 + 0.5f;
   float4 _201 = s7_t.Sample(s7_s, float2(_196, _200));
   float _203 = dot(float3(_193, _194, _195), float3(_186, _189, _192));
   float _204 = TEXCOORD.w * 0.20000000298023224f;
   float _205 = _203 * _204;
   float _206 = _205 + 0.5f;
   float4 _207 = s7_t.Sample(s7_s, float2(_196, _206));
   float _209 = _207.x * _201.x;
   float _210 = -0.0f - _169;
   float _211 = -0.0f - _170;
   float _212 = -0.0f - _171;
   float _213 = dot(float3(_110, _111, _112), float3(_210, _211, _212));
   float _214 = dot(float3(_210, _211, _212), float3(_181, _182, _183));
   float _215 = _214 * _198;
   float _216 = _215 + 0.5f;
   float4 _217 = s7_t.Sample(s7_s, float2(_213, _216));
   float _219 = dot(float3(_210, _211, _212), float3(_186, _189, _192));
   float _220 = _219 * _204;
   float _221 = _220 + 0.5f;
   float4 _222 = s7_t.Sample(s7_s, float2(_213, _221));
   float _224 = _222.x * _217.x;
   float _225 = log2(_209);
   float _226 = log2(_224);
   float _227 = _225 * c_mat_power;
   float _228 = _226 * c_mat_power;
   float _229 = exp2(_227);
   float _230 = exp2(_228);
   float _235 = c_mat_fresnel * c_tone_control.w;
   float _236 = c_mat_power * 0.03974359855055809f;
   float _237 = _236 + 0.0856831967830658f;
   float _238 = _230 * _237;
   float _239 = dot(float3(_119, _120, _121), float3(_127, _128, _129));
   float _240 = dot(float3(_119, _120, _121), float3(_169, _170, _171));
   float _241 = 1.0f - _239;
   float _242 = 1.0f - _240;
   float _243 = _241 * _241;
   float _244 = _242 * _242;
   float _245 = 1.0f - _235;
   float _246 = _243 * _243;
   float _247 = _246 * _241;
   float _248 = _247 * _245;
   float _249 = _242 * _245;
   float _250 = _244 * _244;
   float _251 = _250 * _249;
   float _252 = _248 + _235;
   float _253 = _251 + _235;
   float _254 = _238 * _253;
   float _263 = c_specular_scale.x * c_mat_specular.x;
   float _264 = c_specular_scale.y * c_mat_specular.y;
   float _265 = c_specular_scale.z * c_mat_specular.z;
   float _270 = _159 * _237;
   float _271 = _270 * _229;
   float _272 = _271 * _252;
   float _273 = _272 * (c_light_col[0].x);
   float _274 = _272 * (c_light_col[0].y);
   float _275 = _272 * (c_light_col[0].z);
   float _280 = _254 * (c_light_col[1].x);
   float _281 = _254 * (c_light_col[1].y);
   float _282 = _254 * (c_light_col[1].z);
   float _283 = _273 + _280;
   float _284 = _274 + _281;
   float _285 = _275 + _282;
   float _286 = _263 * _283;
   float _287 = _264 * _284;
   float _288 = _265 * _285;
   int _294 = c_bool & 1;
   bool _295 = (_294 == 0);
   if (!_295)
   {
      float _297 = -0.0f - TEXCOORD_4.x;
      float _298 = -0.0f - TEXCOORD_4.y;
      float _299 = -0.0f - TEXCOORD_4.z;
      float _300 = dot(float3(_110, _111, _112), float3(_297, _298, _299));
      float _301 = saturate(_300);
      float _302 = _301 * TEXCOORD_1.w;
      float _303 = _301 * TEXCOORD_2.w;
      float _304 = _301 * TEXCOORD_4.w;
      float _305 = max(0.0f, _302);
      float _306 = max(0.0f, _303);
      float _307 = max(0.0f, _304);
      float _308 = -0.0f - TEXCOORD_5.x;
      float _309 = -0.0f - TEXCOORD_5.y;
      float _310 = -0.0f - TEXCOORD_5.z;
      float _311 = dot(float3(_110, _111, _112), float3(_308, _309, _310));
      float _312 = saturate(_311);
      float _313 = _312 * COLOR.w;
      float _314 = _312 * COLOR_1.w;
      float _315 = _312 * TEXCOORD_5.w;
      float _316 = max(0.0f, _313);
      float _317 = max(0.0f, _314);
      float _318 = max(0.0f, _315);
      float _319 = _316 + _305;
      float _320 = _317 + _306;
      float _321 = _318 + _307;
      _323 = _319;
      _324 = _320;
      _325 = _321;
   }
   else
   {
      _323 = 0.0f;
      _324 = 0.0f;
      _325 = 0.0f;
   }
   float _326 = c_rt_size.z * SV_Position.x;
   float _327 = c_rt_size.w * SV_Position.y;
   float4 _328 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_326, _327)));
   int _334 = c_bool & 16384;
   bool _335 = (_334 == 0);
   if (!_335)
   {
      float _337 = _328.x * 4.0f;
      float _338 = _328.y * 4.0f;
      float _339 = _328.z * 4.0f;
      _354 = _337;
      _355 = _338;
      _356 = _339;
   }
   else
   {
      int _341 = c_bool & 8192;
      bool _342 = (_341 == 0);
      if (!_342)
      {
         float _344 = _328.x * 2.0f;
         float _345 = _328.y * 2.0f;
         float _346 = _328.z * 2.0f;
         _354 = _344;
         _355 = _345;
         _356 = _346;
      }
      else
      {
         float _350 = _328.x / c_tone_control.z;
         float _351 = _328.y / c_tone_control.z;
         float _352 = _328.z / c_tone_control.z;
         _354 = _350;
         _355 = _351;
         _356 = _352;
      }
   }
   float _357 = _354 + _323;
   float _358 = _355 + _324;
   float _359 = _356 + _325;
   float _422 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _423 = rsqrt(_422);
   float _424 = _423 * _110;
   float _425 = _423 * _111;
   float _426 = _423 * _112;
   float _427 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_424, _425, _426, 1.0f));
   float _428 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_424, _425, _426, 1.0f));
   float _429 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_424, _425, _426, 1.0f));
   float _430 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_424, _425, _426, 1.0f));
   float _431 = dot(float4(_424, _425, _426, 1.0f), float4(_427, _428, _429, _430));
   float _432 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_424, _425, _426, 1.0f));
   float _433 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_424, _425, _426, 1.0f));
   float _434 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_424, _425, _426, 1.0f));
   float _435 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_424, _425, _426, 1.0f));
   float _436 = dot(float4(_424, _425, _426, 1.0f), float4(_432, _433, _434, _435));
   float _437 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_424, _425, _426, 1.0f));
   float _438 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_424, _425, _426, 1.0f));
   float _439 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_424, _425, _426, 1.0f));
   float _440 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_424, _425, _426, 1.0f));
   float _441 = dot(float4(_424, _425, _426, 1.0f), float4(_437, _438, _439, _440));
   float _442 = max(_431, 0.0625f);
   float _443 = max(_436, 0.0625f);
   float _444 = max(_441, 0.0625f);
   float _445 = _442 * (c_ps_scratch[79].x);
   float _446 = _443 * (c_ps_scratch[79].x);
   float _447 = _444 * (c_ps_scratch[79].x);
   float _452 = -0.0f - (c_light_vec[0].x);
   float _453 = -0.0f - (c_light_vec[0].y);
   float _454 = -0.0f - (c_light_vec[0].z);
   float _455 = dot(float3(_110, _111, _112), float3(_452, _453, _454));
   float _456 = -0.0f - (c_light_vec[1].x);
   float _457 = -0.0f - (c_light_vec[1].y);
   float _458 = -0.0f - (c_light_vec[1].z);
   float _459 = dot(float3(_110, _111, _112), float3(_456, _457, _458));
   float _468 = _111 * 0.5f;
   float _469 = _468 + 0.5f;
   float _470 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _471 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _472 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _473 = _470 * _469;
   float _474 = _471 * _469;
   float _475 = _472 * _469;
   float _476 = saturate(_455);
   float _477 = saturate(_459);
   float _482 = (c_light_col[0].x) * _476;
   float _483 = (c_light_col[0].y) * _476;
   float _484 = (c_light_col[0].z) * _476;
   float _489 = (c_light_col[1].x) * _477;
   float _490 = (c_light_col[1].y) * _477;
   float _491 = (c_light_col[1].z) * _477;
   int _492 = c_bool & 32768;
   bool _493 = (_492 == 0);
   float _502 = (c_light_vec[4].x) + _445;
   float _503 = _502 + _473;
   float _504 = (c_light_vec[4].y) + _446;
   float _505 = _504 + _474;
   float _506 = (c_light_vec[4].z) + _447;
   float _507 = _506 + _475;
   if (!_493)
   {
      float _511 = (c_ps_user[11].x) * _56;
      float _513 = _110 + 1.0f;
      float _514 = _111 + 1.0f;
      float _515 = _112 + 1.0f;
      float _516 = _513 * 0.5f;
      float _517 = _514 * 0.5f;
      float _518 = _515 * 0.5f;
      float _519 = saturate(_516);
      float _520 = saturate(_517);
      float _521 = saturate(_518);
      float _522 = (c_ps_user[11].y) * _519;
      float _523 = _520 * (c_ps_user[11].y);
      float _524 = _521 * (c_ps_user[11].y);
      float _535 = (c_ps_user[11].w) * _286;
      float _538 = (c_ps_user[12].x) * _159;
      float _544 = -0.0f - _119;
      float _545 = -0.0f - _120;
      float _546 = -0.0f - _121;
      float _547 = dot(float3(_544, _545, _546), float3(_110, _111, _112));
      float _548 = 1.0f - _547;
      float _549 = _548 * _548;
      float _550 = _548 * 0.9722222089767456f;
      float _551 = _549 * _549;
      float _552 = _551 * _550;
      float _553 = _552 + 0.02777777425944805f;
      float _554 = _553 * (c_ps_user[13].z);
      float _556 = (c_ps_user[13].w) * _503;
      float _557 = (c_ps_user[13].w) * _505;
      float _558 = (c_ps_user[13].w) * _507;
      float _561 = (c_ps_user[14].y) * _482;
      float _562 = (c_ps_user[14].y) * _483;
      float _563 = (c_ps_user[14].y) * _484;
      float _565 = (c_ps_user[14].z) * _489;
      float _566 = (c_ps_user[14].z) * _490;
      float _567 = (c_ps_user[14].z) * _491;
      float _569 = (c_ps_user[14].w) * _357;
      float _570 = (c_ps_user[14].w) * _358;
      float _571 = (c_ps_user[14].w) * _359;
      float _575 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _576 = c_specular_scale.x * _575;
      float _577 = _576 * _286;
      float _578 = _561 + (c_ps_user[13].x);
      float _579 = _578 + _565;
      float _580 = c_diffuse_scale.x * _579;
      float _581 = _580 * c_mat_diffuse.x;
      float _582 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _583 = _582 * c_specular_scale.x;
      float _584 = _583 + _577;
      float _585 = _584 * c_mat_specular.x;
      float _586 = _522 + _511;
      float _587 = _586 + _535;
      float _588 = _587 + (c_ps_user[12].y);
      float _589 = _588 + _538;
      float _590 = _589 + _556;
      float _591 = _590 + _569;
      float _592 = _591 + _554;
      float _593 = _592 + _581;
      float _594 = _593 + _585;
      float _595 = c_specular_scale.y * _575;
      float _596 = _595 * _287;
      float _597 = _562 + (c_ps_user[13].x);
      float _598 = _597 + _566;
      float _599 = c_diffuse_scale.y * _598;
      float _600 = _599 * c_mat_diffuse.y;
      float _601 = _582 * c_specular_scale.y;
      float _602 = _601 + _596;
      float _603 = _602 * c_mat_specular.y;
      float _604 = _535 + _511;
      float _605 = _604 + _523;
      float _606 = _605 + (c_ps_user[12].y);
      float _607 = _606 + _538;
      float _608 = _607 + _557;
      float _609 = _608 + _570;
      float _610 = _609 + _554;
      float _611 = _610 + _600;
      float _612 = _611 + _603;
      float _613 = c_specular_scale.z * _575;
      float _614 = _613 * _288;
      float _615 = _563 + (c_ps_user[13].x);
      float _616 = _615 + _567;
      float _617 = c_diffuse_scale.z * _616;
      float _618 = _617 * c_mat_diffuse.z;
      float _619 = _582 * c_specular_scale.z;
      float _620 = _619 + _614;
      float _621 = _620 * c_mat_specular.z;
      float _622 = _604 + _524;
      float _623 = _622 + (c_ps_user[12].y);
      float _624 = _623 + _538;
      float _625 = _624 + _558;
      float _626 = _625 + _571;
      float _627 = _626 + _554;
      float _628 = _627 + _618;
      float _629 = _628 + _621;
      float _630 = saturate(_594);
      float _631 = saturate(_612);
      float _632 = saturate(_629);
      _659 = _630;
      _660 = _631;
      _661 = _632;
   }
   else
   {
      float _634 = _484 * _159;
      float _635 = _507 + _359;
      float _636 = _635 + _491;
      float _637 = _636 + _634;
      float _638 = _483 * _159;
      float _639 = _505 + _358;
      float _640 = _639 + _490;
      float _641 = _640 + _638;
      float _642 = _482 * _159;
      float _643 = _503 + _357;
      float _644 = _643 + _489;
      float _645 = _644 + _642;
      float _646 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _647 = _646 * _56;
      float _648 = _647 * _645;
      float _649 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _650 = _649 * _56;
      float _651 = _650 * _641;
      float _652 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _653 = _652 * _56;
      float _654 = _653 * _637;
      float _655 = _648 + _286;
      float _656 = _651 + _287;
      float _657 = _654 + _288;
      _659 = _655;
      _660 = _656;
      _661 = _657;
   }
   float _666 = c_mat_opacity * _57.w;
   float _667 = _666 * c_opacity_scale;
   float _668 = _667 * c_global_opacity;
   float _669 = _659 * COLOR.x;
   float _670 = _660 * COLOR.y;
   float _671 = _661 * COLOR.z;
   int _672 = c_bool & 16;
   bool _673 = (_672 == 0);
   if (!_673)
   {
      float _677 = c_tone_control.x * _669;
      float _678 = c_tone_control.x * _670;
      float _679 = c_tone_control.x * _671;
      float _681 = log2(_677);
      float _682 = log2(_678);
      float _683 = log2(_679);
      float _684 = _681 * c_tone_control.y;
      float _685 = _682 * c_tone_control.y;
      float _686 = _683 * c_tone_control.y;
      float _687 = exp2(_684);
      float _688 = exp2(_685);
      float _689 = exp2(_686);
      _737 = _687;
      _738 = _688;
      _739 = _689;
   }
   else
   {
      if (_493)
      {
         float _694 = c_tone_control.x * _669;
         float _695 = c_tone_control.x * _670;
         float _696 = c_tone_control.x * _671;
         float _697 = _694 * 2.0f;
         float _698 = _695 * 2.0f;
         float _699 = _696 * 2.0f;
         float _700 = _694 * 0.30000001192092896f;
         float _701 = _695 * 0.30000001192092896f;
         float _702 = _696 * 0.30000001192092896f;
         float _703 = _700 + 0.05000000074505806f;
         float _704 = _701 + 0.05000000074505806f;
         float _705 = _702 + 0.05000000074505806f;
         float _706 = _703 * _697;
         float _707 = _704 * _698;
         float _708 = _705 * _699;
         float _709 = _706 + 0.004000000189989805f;
         float _710 = _707 + 0.004000000189989805f;
         float _711 = _708 + 0.004000000189989805f;
         float _712 = _700 + 0.5f;
         float _713 = _701 + 0.5f;
         float _714 = _702 + 0.5f;
         float _715 = _712 * _697;
         float _716 = _713 * _698;
         float _717 = _714 * _699;
         float _718 = _715 + 0.06000000238418579f;
         float _719 = _716 + 0.06000000238418579f;
         float _720 = _717 + 0.06000000238418579f;
         float _721 = _709 / _718;
         float _722 = _710 / _719;
         float _723 = _711 / _720;
         float _724 = _721 + -0.06666666269302368f;
         float _725 = _722 + -0.06666666269302368f;
         float _726 = _723 + -0.06666666269302368f;
         float _727 = _724 * 1.3790643215179443f;
         float _728 = _725 * 1.3790643215179443f;
         float _729 = _726 * 1.3790643215179443f;
         float _730 = YKR_MaterialCurve(_727, _694);
         float _731 = YKR_MaterialCurve(_728, _695);
         float _732 = YKR_MaterialCurve(_729, _696);
         float _733 = sqrt(_730);
         float _734 = sqrt(_731);
         float _735 = sqrt(_732);
         _737 = _733;
         _738 = _734;
         _739 = _735;
      }
      else
      {
         _737 = _669;
         _738 = _670;
         _739 = _671;
      }
   }
   int _740 = c_bool & 4;
   bool _741 = (_740 == 0);
   if (_741)
   {
      float _743 = saturate(COLOR_1.x);
      float _748 = (c_fog_param[0].x) - _737;
      float _749 = (c_fog_param[0].y) - _738;
      float _750 = (c_fog_param[0].z) - _739;
      float _751 = _748 * _743;
      float _752 = _749 * _743;
      float _753 = _750 * _743;
      _755 = _751;
      _756 = _752;
      _757 = _753;
   }
   else
   {
      _755 = COLOR_1.x;
      _756 = COLOR_1.y;
      _757 = COLOR_1.z;
   }
   float _758 = _755 + _737;
   float _759 = _756 + _738;
   float _760 = _757 + _739;
   if (!_493)
   {
      float _768 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _769 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _770 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _772 = (c_ps_user[12].w) * COLOR.x;
      float _773 = (c_ps_user[12].w) * COLOR.y;
      float _774 = (c_ps_user[12].w) * COLOR.z;
      float _775 = _772 + _758;
      float _776 = _775 + _768;
      float _777 = _773 + _759;
      float _778 = _777 + _769;
      float _779 = _774 + _760;
      float _780 = _779 + _770;
      _782 = _776;
      _783 = _778;
      _784 = _780;
   }
   else
   {
      _782 = _758;
      _783 = _759;
      _784 = _760;
   }
   float _787 = _668 - c_mat_punchthrough_ref;
   bool _788 = (_787 < 0.0f);
   if (_788)
      discard;
   float _794 = (c_ps_col_scl[0].x) * _782;
   float _795 = (c_ps_col_scl[0].y) * _783;
   float _796 = (c_ps_col_scl[0].z) * _784;
   float _797 = (c_ps_col_scl[0].w) * _668;
   SV_Target.x = _794;
   SV_Target.y = _795;
   SV_Target.z = _796;
   SV_Target.w = _797;
   int _800 = c_bool & 8;
   bool _801 = (_800 != 0);
   float _802 = select(_801, _782, _794);
   float _803 = select(_801, _783, _795);
   float _804 = select(_801, _784, _796);
   float _805 = select(_801, _668, _797);
   SV_Target.x = _802;
   SV_Target.y = _803;
   SV_Target.z = _804;
   SV_Target.w = _805;
   return SV_Target;
}
