// Yakuza Kiwami material s_o1dt[rdup][dedit][ag] (0x9983691E, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float4 _60 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _57 * (c_ps_user[3].x);
   float _68 = _58 * (c_ps_user[3].y);
   float _69 = _59 * (c_ps_user[3].z);
   float _72 = _60.x * 2.0f;
   float _73 = _60.y * 2.0f;
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
   float _120 = (c_ps_user[3].w) * TEXCOORD.x;
   float _121 = (c_ps_user[3].w) * TEXCOORD.y;
   float4 _122 = s7_t.Sample(s7_s, float2(_120, _121));
   float _127 = _122.x * _122.x;
   float _128 = _122.y * _122.y;
   float _129 = _122.z * _122.z;
   float _130 = 1.0f - _127;
   float _131 = 1.0f - _128;
   float _132 = 1.0f - _129;
   float _133 = 1.0f - _122.w;
   float _134 = _130 * _133;
   float _135 = _131 * _133;
   float _136 = _132 * _133;
   float _137 = _134 + _127;
   float _138 = _135 + _128;
   float _139 = _136 + _129;
   float _140 = _67 * _137;
   float _141 = _68 * _138;
   float _142 = _69 * _139;
   float _146 = c_rt_size.z * SV_Position.x;
   float _147 = c_rt_size.w * SV_Position.y;
   int _150 = c_bool & 8;
   bool _151 = (_150 == 0);
   float _157;
   float _158;
   float _170;
   float _171;
   float _266;
   float _267;
   float _268;
   float _269;
   float _270;
   float _271;
   float _304;
   float _305;
   float _306;
   float _307;
   float _574;
   float _575;
   float _576;
   float _652;
   float _653;
   float _654;
   float _670;
   float _671;
   float _672;
   float _697;
   float _698;
   float _699;
   if (!_151)
   {
      float4 _153 = s13_t.Sample(s13_s, float2(_146, _147));
      _157 = _153.x;
      _158 = _153.y;
   }
   else
   {
      _157 = 1.0f;
      _158 = 1.0f;
   }
   if (!_151)
   {
      float _163 = (c_sdw_param_ch[0].x) * _157;
      float _164 = (c_sdw_param_ch[0].z) * _158;
      float _167 = _163 + (c_sdw_param_ch[0].y);
      float _168 = _164 + (c_sdw_param_ch[0].w);
      _170 = _167;
      _171 = _168;
   }
   else
   {
      _170 = _157;
      _171 = _158;
   }
   float _172 = min(_170, _171);
   int _176 = c_bool & 1;
   bool _177 = (_176 == 0);
   if (!_177)
   {
      float _179 = -0.0f - TEXCOORD_4.x;
      float _180 = -0.0f - TEXCOORD_4.y;
      float _181 = -0.0f - TEXCOORD_4.z;
      float _182 = dot(float3(_107, _108, _109), float3(_179, _180, _181));
      float _183 = saturate(_182);
      float _184 = _183 * TEXCOORD_1.w;
      float _185 = _183 * TEXCOORD_2.w;
      float _186 = _183 * TEXCOORD_4.w;
      float _187 = max(0.0f, _184);
      float _188 = max(0.0f, _185);
      float _189 = max(0.0f, _186);
      float _190 = -0.0f - TEXCOORD_5.x;
      float _191 = -0.0f - TEXCOORD_5.y;
      float _192 = -0.0f - TEXCOORD_5.z;
      float _193 = dot(float3(_107, _108, _109), float3(_190, _191, _192));
      float _194 = saturate(_193);
      float _195 = _194 * COLOR.w;
      float _196 = _194 * COLOR_1.w;
      float _197 = _194 * TEXCOORD_5.w;
      float _198 = max(0.0f, _195);
      float _199 = max(0.0f, _196);
      float _200 = max(0.0f, _197);
      float _201 = _198 + _187;
      float _202 = _199 + _188;
      float _203 = _200 + _189;
      float _204 = _116 + TEXCOORD_4.x;
      float _205 = _117 + TEXCOORD_4.y;
      float _206 = _118 + TEXCOORD_4.z;
      float _207 = dot(float3(_204, _205, _206), float3(_204, _205, _206));
      float _208 = rsqrt(_207);
      float _209 = _116 + TEXCOORD_5.x;
      float _210 = _117 + TEXCOORD_5.y;
      float _211 = _118 + TEXCOORD_5.z;
      float _212 = dot(float3(_209, _210, _211), float3(_209, _210, _211));
      float _213 = rsqrt(_212);
      float _214 = _209 * _213;
      float _215 = -0.0f - _214;
      float _216 = _210 * _213;
      float _217 = -0.0f - _216;
      float _218 = _211 * _213;
      float _219 = -0.0f - _218;
      float _220 = _204 * _208;
      float _221 = -0.0f - _220;
      float _222 = _205 * _208;
      float _223 = -0.0f - _222;
      float _224 = _206 * _208;
      float _225 = -0.0f - _224;
      float _226 = dot(float3(_221, _223, _225), float3(_107, _108, _109));
      float _227 = dot(float3(_215, _217, _219), float3(_107, _108, _109));
      float _228 = saturate(_226);
      float _229 = saturate(_227);
      float _230 = log2(_228);
      float _231 = log2(_229);
      float _232 = _230 * c_mat_power;
      float _233 = _231 * c_mat_power;
      float _234 = exp2(_232);
      float _235 = exp2(_233);
      float _236 = _234 * TEXCOORD_1.w;
      float _237 = _234 * TEXCOORD_2.w;
      float _238 = _234 * TEXCOORD_4.w;
      float _239 = _235 * COLOR.w;
      float _240 = _235 * COLOR_1.w;
      float _241 = _235 * TEXCOORD_5.w;
      float _242 = _239 + _236;
      float _243 = _240 + _237;
      float _244 = _241 + _238;
      float _245 = max(0.0f, _242);
      float _246 = max(0.0f, _243);
      float _247 = max(0.0f, _244);
      float _256 = c_mat_specular.x * _245;
      float _257 = _256 * c_specular_scale.x;
      float _258 = c_mat_specular.y * _246;
      float _259 = _258 * c_specular_scale.y;
      float _260 = c_mat_specular.z * _247;
      float _261 = _260 * c_specular_scale.z;
      float _262 = min(_257, 1.0f);
      float _263 = min(_259, 1.0f);
      float _264 = min(_261, 1.0f);
      _266 = _262;
      _267 = _263;
      _268 = _264;
      _269 = _201;
      _270 = _202;
      _271 = _203;
   }
   else
   {
      _266 = 0.0f;
      _267 = 0.0f;
      _268 = 0.0f;
      _269 = 0.0f;
      _270 = 0.0f;
      _271 = 0.0f;
   }
   float _272 = c_rt_size.z * SV_Position.x;
   float _273 = c_rt_size.w * SV_Position.y;
   float4 _274 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_272, _273)));
   int _281 = c_bool & 16384;
   bool _282 = (_281 == 0);
   if (!_282)
   {
      float _284 = _274.x * 4.0f;
      float _285 = _274.y * 4.0f;
      float _286 = _274.z * 4.0f;
      float _287 = _274.w * 4.0f;
      _304 = _284;
      _305 = _285;
      _306 = _286;
      _307 = _287;
   }
   else
   {
      int _289 = c_bool & 8192;
      bool _290 = (_289 == 0);
      if (!_290)
      {
         float _292 = _274.x * 2.0f;
         float _293 = _274.y * 2.0f;
         float _294 = _274.z * 2.0f;
         float _295 = _274.w * 2.0f;
         _304 = _292;
         _305 = _293;
         _306 = _294;
         _307 = _295;
      }
      else
      {
         float _299 = _274.x / c_tone_control.z;
         float _300 = _274.y / c_tone_control.z;
         float _301 = _274.z / c_tone_control.z;
         float _302 = _274.w / c_tone_control.z;
         _304 = _299;
         _305 = _300;
         _306 = _301;
         _307 = _302;
      }
   }
   float _308 = _304 + _269;
   float _309 = _305 + _270;
   float _310 = _306 + _271;
   float _373 = dot(float3(_107, _108, _109), float3(_107, _108, _109));
   float _374 = rsqrt(_373);
   float _375 = _374 * _107;
   float _376 = _374 * _108;
   float _377 = _374 * _109;
   float _378 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_375, _376, _377, 1.0f));
   float _379 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_375, _376, _377, 1.0f));
   float _380 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_375, _376, _377, 1.0f));
   float _381 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_375, _376, _377, 1.0f));
   float _382 = dot(float4(_375, _376, _377, 1.0f), float4(_378, _379, _380, _381));
   float _383 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_375, _376, _377, 1.0f));
   float _384 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_375, _376, _377, 1.0f));
   float _385 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_375, _376, _377, 1.0f));
   float _386 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_375, _376, _377, 1.0f));
   float _387 = dot(float4(_375, _376, _377, 1.0f), float4(_383, _384, _385, _386));
   float _388 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_375, _376, _377, 1.0f));
   float _389 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_375, _376, _377, 1.0f));
   float _390 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_375, _376, _377, 1.0f));
   float _391 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_375, _376, _377, 1.0f));
   float _392 = dot(float4(_375, _376, _377, 1.0f), float4(_388, _389, _390, _391));
   float _393 = max(_382, 0.0625f);
   float _394 = max(_387, 0.0625f);
   float _395 = max(_392, 0.0625f);
   float _396 = _393 * (c_ps_scratch[79].x);
   float _397 = _394 * (c_ps_scratch[79].x);
   float _398 = _395 * (c_ps_scratch[79].x);
   float _403 = -0.0f - (c_light_vec[0].x);
   float _404 = -0.0f - (c_light_vec[0].y);
   float _405 = -0.0f - (c_light_vec[0].z);
   float _406 = dot(float3(_107, _108, _109), float3(_403, _404, _405));
   float _407 = -0.0f - (c_light_vec[1].x);
   float _408 = -0.0f - (c_light_vec[1].y);
   float _409 = -0.0f - (c_light_vec[1].z);
   float _410 = dot(float3(_107, _108, _109), float3(_407, _408, _409));
   float _419 = _108 * 0.5f;
   float _420 = _419 + 0.5f;
   float _421 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _422 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _423 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _424 = _421 * _420;
   float _425 = _422 * _420;
   float _426 = _423 * _420;
   float _427 = saturate(_406);
   float _428 = saturate(_410);
   float _433 = (c_light_col[0].x) * _427;
   float _434 = (c_light_col[0].y) * _427;
   float _435 = (c_light_col[0].z) * _427;
   float _440 = (c_light_col[1].x) * _428;
   float _441 = (c_light_col[1].y) * _428;
   float _442 = (c_light_col[1].z) * _428;
   int _443 = c_bool & 32768;
   bool _444 = (_443 == 0);
   float _453 = (c_light_vec[4].x) + _396;
   float _454 = _453 + _424;
   float _455 = (c_light_vec[4].y) + _397;
   float _456 = _455 + _425;
   float _457 = (c_light_vec[4].z) + _398;
   float _458 = _457 + _426;
   if (!_444)
   {
      float _462 = (c_ps_user[11].x) * _140;
      float _463 = (c_ps_user[11].x) * _141;
      float _464 = (c_ps_user[11].x) * _142;
      float _466 = _107 + 1.0f;
      float _467 = _108 + 1.0f;
      float _468 = _109 + 1.0f;
      float _469 = _466 * 0.5f;
      float _470 = _467 * 0.5f;
      float _471 = _468 * 0.5f;
      float _472 = saturate(_469);
      float _473 = saturate(_470);
      float _474 = saturate(_471);
      float _475 = (c_ps_user[11].y) * _472;
      float _476 = _473 * (c_ps_user[11].y);
      float _477 = _474 * (c_ps_user[11].y);
      float _480 = (c_ps_user[12].x) * _172;
      float _485 = -0.0f - _116;
      float _486 = -0.0f - _117;
      float _487 = -0.0f - _118;
      float _488 = dot(float3(_485, _486, _487), float3(_107, _108, _109));
      float _489 = 1.0f - _488;
      float _490 = _489 * _489;
      float _491 = _489 * 0.9722222089767456f;
      float _492 = _490 * _490;
      float _493 = _492 * _491;
      float _494 = _493 + 0.02777777425944805f;
      float _495 = _494 * (c_ps_user[13].z);
      float _497 = (c_ps_user[13].w) * _454;
      float _498 = (c_ps_user[13].w) * _456;
      float _499 = (c_ps_user[13].w) * _458;
      float _502 = (c_ps_user[14].y) * _433;
      float _503 = (c_ps_user[14].y) * _434;
      float _504 = (c_ps_user[14].y) * _435;
      float _506 = (c_ps_user[14].z) * _440;
      float _507 = (c_ps_user[14].z) * _441;
      float _508 = (c_ps_user[14].z) * _442;
      float _510 = (c_ps_user[14].w) * _308;
      float _511 = (c_ps_user[14].w) * _309;
      float _512 = (c_ps_user[14].w) * _310;
      float _513 = _480 + (c_ps_user[12].y);
      float _514 = _502 + (c_ps_user[13].x);
      float _515 = _514 + _506;
      float _516 = c_diffuse_scale.x * _515;
      float _517 = _516 * c_mat_diffuse.x;
      float _518 = _495 + _513;
      float _519 = _518 + _462;
      float _520 = _519 + _475;
      float _521 = _520 + _497;
      float _522 = _521 + _510;
      float _523 = _522 + _517;
      float _524 = _503 + (c_ps_user[13].x);
      float _525 = _524 + _507;
      float _526 = c_diffuse_scale.y * _525;
      float _527 = _526 * c_mat_diffuse.y;
      float _528 = _518 + _463;
      float _529 = _528 + _476;
      float _530 = _529 + _498;
      float _531 = _530 + _511;
      float _532 = _531 + _527;
      float _533 = _504 + (c_ps_user[13].x);
      float _534 = _533 + _508;
      float _535 = c_diffuse_scale.z * _534;
      float _536 = _535 * c_mat_diffuse.z;
      float _537 = _518 + _464;
      float _538 = _537 + _477;
      float _539 = _538 + _499;
      float _540 = _539 + _512;
      float _541 = _540 + _536;
      float _542 = saturate(_523);
      float _543 = saturate(_532);
      float _544 = saturate(_541);
      _574 = _542;
      _575 = _543;
      _576 = _544;
   }
   else
   {
      float _546 = _435 * _172;
      float _547 = _458 + _310;
      float _548 = _547 + _442;
      float _549 = _548 + _546;
      float _550 = _434 * _172;
      float _551 = _456 + _309;
      float _552 = _551 + _441;
      float _553 = _552 + _550;
      float _554 = _433 * _172;
      float _555 = _454 + _308;
      float _556 = _555 + _440;
      float _557 = _556 + _554;
      float _558 = _307 + _268;
      float _559 = _307 + _267;
      float _560 = _307 + _266;
      float _561 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _562 = _561 * _140;
      float _563 = _562 * _557;
      float _564 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _565 = _564 * _141;
      float _566 = _565 * _553;
      float _567 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _568 = _567 * _142;
      float _569 = _568 * _549;
      float _570 = _560 + _563;
      float _571 = _559 + _566;
      float _572 = _558 + _569;
      _574 = _570;
      _575 = _571;
      _576 = _572;
   }
   float _581 = c_mat_opacity * _52.w;
   float _582 = _581 * c_opacity_scale;
   float _583 = _582 * c_global_opacity;
   float _584 = _574 * COLOR.x;
   float _585 = _575 * COLOR.y;
   float _586 = _576 * COLOR.z;
   int _587 = c_bool & 16;
   bool _588 = (_587 == 0);
   if (!_588)
   {
      float _592 = c_tone_control.x * _584;
      float _593 = c_tone_control.x * _585;
      float _594 = c_tone_control.x * _586;
      float _596 = log2(_592);
      float _597 = log2(_593);
      float _598 = log2(_594);
      float _599 = _596 * c_tone_control.y;
      float _600 = _597 * c_tone_control.y;
      float _601 = _598 * c_tone_control.y;
      float _602 = exp2(_599);
      float _603 = exp2(_600);
      float _604 = exp2(_601);
      _652 = _602;
      _653 = _603;
      _654 = _604;
   }
   else
   {
      if (_444)
      {
         float _609 = c_tone_control.x * _584;
         float _610 = c_tone_control.x * _585;
         float _611 = c_tone_control.x * _586;
         float _612 = _609 * 2.0f;
         float _613 = _610 * 2.0f;
         float _614 = _611 * 2.0f;
         float _615 = _609 * 0.30000001192092896f;
         float _616 = _610 * 0.30000001192092896f;
         float _617 = _611 * 0.30000001192092896f;
         float _618 = _615 + 0.05000000074505806f;
         float _619 = _616 + 0.05000000074505806f;
         float _620 = _617 + 0.05000000074505806f;
         float _621 = _618 * _612;
         float _622 = _619 * _613;
         float _623 = _620 * _614;
         float _624 = _621 + 0.004000000189989805f;
         float _625 = _622 + 0.004000000189989805f;
         float _626 = _623 + 0.004000000189989805f;
         float _627 = _615 + 0.5f;
         float _628 = _616 + 0.5f;
         float _629 = _617 + 0.5f;
         float _630 = _627 * _612;
         float _631 = _628 * _613;
         float _632 = _629 * _614;
         float _633 = _630 + 0.06000000238418579f;
         float _634 = _631 + 0.06000000238418579f;
         float _635 = _632 + 0.06000000238418579f;
         float _636 = _624 / _633;
         float _637 = _625 / _634;
         float _638 = _626 / _635;
         float _639 = _636 + -0.06666666269302368f;
         float _640 = _637 + -0.06666666269302368f;
         float _641 = _638 + -0.06666666269302368f;
         float _642 = _639 * 1.3790643215179443f;
         float _643 = _640 * 1.3790643215179443f;
         float _644 = _641 * 1.3790643215179443f;
         float _645 = YKR_MaterialCurve(_642, _609);
         float _646 = YKR_MaterialCurve(_643, _610);
         float _647 = YKR_MaterialCurve(_644, _611);
         float _648 = sqrt(_645);
         float _649 = sqrt(_646);
         float _650 = sqrt(_647);
         _652 = _648;
         _653 = _649;
         _654 = _650;
      }
      else
      {
         _652 = _584;
         _653 = _585;
         _654 = _586;
      }
   }
   int _655 = c_bool & 4;
   bool _656 = (_655 == 0);
   if (_656)
   {
      float _658 = saturate(COLOR_1.x);
      float _663 = (c_fog_param[0].x) - _652;
      float _664 = (c_fog_param[0].y) - _653;
      float _665 = (c_fog_param[0].z) - _654;
      float _666 = _663 * _658;
      float _667 = _664 * _658;
      float _668 = _665 * _658;
      _670 = _666;
      _671 = _667;
      _672 = _668;
   }
   else
   {
      _670 = COLOR_1.x;
      _671 = COLOR_1.y;
      _672 = COLOR_1.z;
   }
   float _673 = _670 + _652;
   float _674 = _671 + _653;
   float _675 = _672 + _654;
   if (!_444)
   {
      float _683 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _684 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _685 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _687 = (c_ps_user[12].w) * COLOR.x;
      float _688 = (c_ps_user[12].w) * COLOR.y;
      float _689 = (c_ps_user[12].w) * COLOR.z;
      float _690 = _687 + _673;
      float _691 = _690 + _683;
      float _692 = _688 + _674;
      float _693 = _692 + _684;
      float _694 = _689 + _675;
      float _695 = _694 + _685;
      _697 = _691;
      _698 = _693;
      _699 = _695;
   }
   else
   {
      _697 = _673;
      _698 = _674;
      _699 = _675;
   }
   float _705 = (c_ps_col_scl[0].x) * _697;
   float _706 = (c_ps_col_scl[0].y) * _698;
   float _707 = (c_ps_col_scl[0].z) * _699;
   float _708 = (c_ps_col_scl[0].w) * _583;
   SV_Target.x = _705;
   SV_Target.y = _706;
   SV_Target.z = _707;
   SV_Target.w = _708;
   int _711 = c_bool & 8;
   bool _712 = (_711 != 0);
   float _713 = select(_712, _697, _705);
   float _714 = select(_712, _698, _706);
   float _715 = select(_712, _699, _707);
   float _716 = select(_712, _583, _708);
   SV_Target.x = _713;
   SV_Target.y = _714;
   SV_Target.z = _715;
   SV_Target.w = _716;
   return SV_Target;
}
