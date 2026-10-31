// Yakuza Kiwami material s_o1dt[rd][dedit][ag] (0x181BDCAE, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _59 = _54.x * _54.x;
   float _60 = _54.y * _54.y;
   float _61 = _54.z * _54.z;
   float4 _62 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _69 = _59 * (c_ps_user[3].x);
   float _70 = _60 * (c_ps_user[3].y);
   float _71 = _61 * (c_ps_user[3].z);
   float _74 = _62.x * 2.0f;
   float _75 = _62.y * 2.0f;
   float _76 = _74 + -1.0f;
   float _77 = _75 + -1.0f;
   float _78 = dot(float2(_76, _77), float2(_76, _77));
   float _79 = 1.0f - _78;
   float _80 = saturate(_79);
   float _81 = sqrt(_80);
   float _82 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _83 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _84 = _82 - _83;
   float _85 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _86 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _87 = _85 - _86;
   float _88 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _89 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _90 = _88 - _89;
   float _91 = _76 * TEXCOORD_3.x;
   float _92 = _76 * TEXCOORD_3.y;
   float _93 = _76 * TEXCOORD_3.z;
   float _94 = _77 * TEXCOORD_3.w;
   float _95 = _94 * _84;
   float _96 = _94 * _87;
   float _97 = _94 * _90;
   float _98 = _95 + _91;
   float _99 = _96 + _92;
   float _100 = _97 + _93;
   float _101 = _81 * TEXCOORD_2.x;
   float _102 = _81 * TEXCOORD_2.y;
   float _103 = _81 * TEXCOORD_2.z;
   float _104 = _98 + _101;
   float _105 = _99 + _102;
   float _106 = _100 + _103;
   float _107 = dot(float3(_104, _105, _106), float3(_104, _105, _106));
   float _108 = rsqrt(_107);
   float _109 = _104 * _108;
   float _110 = _105 * _108;
   float _111 = _106 * _108;
   float _116 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _117 = rsqrt(_116);
   float _118 = _117 * TEXCOORD_1.x;
   float _119 = _117 * TEXCOORD_1.y;
   float _120 = _117 * TEXCOORD_1.z;
   float4 _121 = s7_t.Sample(s7_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _126 = _121.x * _121.x;
   float _127 = _121.y * _121.y;
   float _128 = _121.z * _121.z;
   float _129 = 1.0f - _126;
   float _130 = 1.0f - _127;
   float _131 = 1.0f - _128;
   float _132 = 1.0f - _121.w;
   float _133 = _129 * _132;
   float _134 = _130 * _132;
   float _135 = _131 * _132;
   float _136 = _133 + _126;
   float _137 = _134 + _127;
   float _138 = _135 + _128;
   float _139 = _69 * _136;
   float _140 = _70 * _137;
   float _141 = _71 * _138;
   float _145 = c_rt_size.z * SV_Position.x;
   float _146 = c_rt_size.w * SV_Position.y;
   int _149 = c_bool & 8;
   bool _150 = (_149 == 0);
   float _156;
   float _157;
   float _169;
   float _170;
   float _265;
   float _266;
   float _267;
   float _268;
   float _269;
   float _270;
   float _303;
   float _304;
   float _305;
   float _306;
   float _573;
   float _574;
   float _575;
   float _651;
   float _652;
   float _653;
   float _669;
   float _670;
   float _671;
   float _696;
   float _697;
   float _698;
   if (!_150)
   {
      float4 _152 = s13_t.Sample(s13_s, float2(_145, _146));
      _156 = _152.x;
      _157 = _152.y;
   }
   else
   {
      _156 = 1.0f;
      _157 = 1.0f;
   }
   if (!_150)
   {
      float _162 = (c_sdw_param_ch[0].x) * _156;
      float _163 = (c_sdw_param_ch[0].z) * _157;
      float _166 = _162 + (c_sdw_param_ch[0].y);
      float _167 = _163 + (c_sdw_param_ch[0].w);
      _169 = _166;
      _170 = _167;
   }
   else
   {
      _169 = _156;
      _170 = _157;
   }
   float _171 = min(_169, _170);
   int _175 = c_bool & 1;
   bool _176 = (_175 == 0);
   if (!_176)
   {
      float _178 = -0.0f - TEXCOORD_4.x;
      float _179 = -0.0f - TEXCOORD_4.y;
      float _180 = -0.0f - TEXCOORD_4.z;
      float _181 = dot(float3(_109, _110, _111), float3(_178, _179, _180));
      float _182 = saturate(_181);
      float _183 = _182 * TEXCOORD_1.w;
      float _184 = _182 * TEXCOORD_2.w;
      float _185 = _182 * TEXCOORD_4.w;
      float _186 = max(0.0f, _183);
      float _187 = max(0.0f, _184);
      float _188 = max(0.0f, _185);
      float _189 = -0.0f - TEXCOORD_5.x;
      float _190 = -0.0f - TEXCOORD_5.y;
      float _191 = -0.0f - TEXCOORD_5.z;
      float _192 = dot(float3(_109, _110, _111), float3(_189, _190, _191));
      float _193 = saturate(_192);
      float _194 = _193 * COLOR.w;
      float _195 = _193 * COLOR_1.w;
      float _196 = _193 * TEXCOORD_5.w;
      float _197 = max(0.0f, _194);
      float _198 = max(0.0f, _195);
      float _199 = max(0.0f, _196);
      float _200 = _197 + _186;
      float _201 = _198 + _187;
      float _202 = _199 + _188;
      float _203 = _118 + TEXCOORD_4.x;
      float _204 = _119 + TEXCOORD_4.y;
      float _205 = _120 + TEXCOORD_4.z;
      float _206 = dot(float3(_203, _204, _205), float3(_203, _204, _205));
      float _207 = rsqrt(_206);
      float _208 = _118 + TEXCOORD_5.x;
      float _209 = _119 + TEXCOORD_5.y;
      float _210 = _120 + TEXCOORD_5.z;
      float _211 = dot(float3(_208, _209, _210), float3(_208, _209, _210));
      float _212 = rsqrt(_211);
      float _213 = _208 * _212;
      float _214 = -0.0f - _213;
      float _215 = _209 * _212;
      float _216 = -0.0f - _215;
      float _217 = _210 * _212;
      float _218 = -0.0f - _217;
      float _219 = _203 * _207;
      float _220 = -0.0f - _219;
      float _221 = _204 * _207;
      float _222 = -0.0f - _221;
      float _223 = _205 * _207;
      float _224 = -0.0f - _223;
      float _225 = dot(float3(_220, _222, _224), float3(_109, _110, _111));
      float _226 = dot(float3(_214, _216, _218), float3(_109, _110, _111));
      float _227 = saturate(_225);
      float _228 = saturate(_226);
      float _229 = log2(_227);
      float _230 = log2(_228);
      float _231 = _229 * c_mat_power;
      float _232 = _230 * c_mat_power;
      float _233 = exp2(_231);
      float _234 = exp2(_232);
      float _235 = _233 * TEXCOORD_1.w;
      float _236 = _233 * TEXCOORD_2.w;
      float _237 = _233 * TEXCOORD_4.w;
      float _238 = _234 * COLOR.w;
      float _239 = _234 * COLOR_1.w;
      float _240 = _234 * TEXCOORD_5.w;
      float _241 = _238 + _235;
      float _242 = _239 + _236;
      float _243 = _240 + _237;
      float _244 = max(0.0f, _241);
      float _245 = max(0.0f, _242);
      float _246 = max(0.0f, _243);
      float _255 = c_mat_specular.x * _244;
      float _256 = _255 * c_specular_scale.x;
      float _257 = c_mat_specular.y * _245;
      float _258 = _257 * c_specular_scale.y;
      float _259 = c_mat_specular.z * _246;
      float _260 = _259 * c_specular_scale.z;
      float _261 = min(_256, 1.0f);
      float _262 = min(_258, 1.0f);
      float _263 = min(_260, 1.0f);
      _265 = _261;
      _266 = _262;
      _267 = _263;
      _268 = _200;
      _269 = _201;
      _270 = _202;
   }
   else
   {
      _265 = 0.0f;
      _266 = 0.0f;
      _267 = 0.0f;
      _268 = 0.0f;
      _269 = 0.0f;
      _270 = 0.0f;
   }
   float _271 = c_rt_size.z * SV_Position.x;
   float _272 = c_rt_size.w * SV_Position.y;
   float4 _273 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_271, _272)));
   int _280 = c_bool & 16384;
   bool _281 = (_280 == 0);
   if (!_281)
   {
      float _283 = _273.x * 4.0f;
      float _284 = _273.y * 4.0f;
      float _285 = _273.z * 4.0f;
      float _286 = _273.w * 4.0f;
      _303 = _283;
      _304 = _284;
      _305 = _285;
      _306 = _286;
   }
   else
   {
      int _288 = c_bool & 8192;
      bool _289 = (_288 == 0);
      if (!_289)
      {
         float _291 = _273.x * 2.0f;
         float _292 = _273.y * 2.0f;
         float _293 = _273.z * 2.0f;
         float _294 = _273.w * 2.0f;
         _303 = _291;
         _304 = _292;
         _305 = _293;
         _306 = _294;
      }
      else
      {
         float _298 = _273.x / c_tone_control.z;
         float _299 = _273.y / c_tone_control.z;
         float _300 = _273.z / c_tone_control.z;
         float _301 = _273.w / c_tone_control.z;
         _303 = _298;
         _304 = _299;
         _305 = _300;
         _306 = _301;
      }
   }
   float _307 = _303 + _268;
   float _308 = _304 + _269;
   float _309 = _305 + _270;
   float _372 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _373 = rsqrt(_372);
   float _374 = _373 * _109;
   float _375 = _373 * _110;
   float _376 = _373 * _111;
   float _377 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_374, _375, _376, 1.0f));
   float _378 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_374, _375, _376, 1.0f));
   float _379 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_374, _375, _376, 1.0f));
   float _380 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_374, _375, _376, 1.0f));
   float _381 = dot(float4(_374, _375, _376, 1.0f), float4(_377, _378, _379, _380));
   float _382 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_374, _375, _376, 1.0f));
   float _383 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_374, _375, _376, 1.0f));
   float _384 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_374, _375, _376, 1.0f));
   float _385 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_374, _375, _376, 1.0f));
   float _386 = dot(float4(_374, _375, _376, 1.0f), float4(_382, _383, _384, _385));
   float _387 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_374, _375, _376, 1.0f));
   float _388 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_374, _375, _376, 1.0f));
   float _389 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_374, _375, _376, 1.0f));
   float _390 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_374, _375, _376, 1.0f));
   float _391 = dot(float4(_374, _375, _376, 1.0f), float4(_387, _388, _389, _390));
   float _392 = max(_381, 0.0625f);
   float _393 = max(_386, 0.0625f);
   float _394 = max(_391, 0.0625f);
   float _395 = _392 * (c_ps_scratch[79].x);
   float _396 = _393 * (c_ps_scratch[79].x);
   float _397 = _394 * (c_ps_scratch[79].x);
   float _402 = -0.0f - (c_light_vec[0].x);
   float _403 = -0.0f - (c_light_vec[0].y);
   float _404 = -0.0f - (c_light_vec[0].z);
   float _405 = dot(float3(_109, _110, _111), float3(_402, _403, _404));
   float _406 = -0.0f - (c_light_vec[1].x);
   float _407 = -0.0f - (c_light_vec[1].y);
   float _408 = -0.0f - (c_light_vec[1].z);
   float _409 = dot(float3(_109, _110, _111), float3(_406, _407, _408));
   float _418 = _110 * 0.5f;
   float _419 = _418 + 0.5f;
   float _420 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _421 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _422 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _423 = _420 * _419;
   float _424 = _421 * _419;
   float _425 = _422 * _419;
   float _426 = saturate(_405);
   float _427 = saturate(_409);
   float _432 = (c_light_col[0].x) * _426;
   float _433 = (c_light_col[0].y) * _426;
   float _434 = (c_light_col[0].z) * _426;
   float _439 = (c_light_col[1].x) * _427;
   float _440 = (c_light_col[1].y) * _427;
   float _441 = (c_light_col[1].z) * _427;
   int _442 = c_bool & 32768;
   bool _443 = (_442 == 0);
   float _452 = (c_light_vec[4].x) + _395;
   float _453 = _452 + _423;
   float _454 = (c_light_vec[4].y) + _396;
   float _455 = _454 + _424;
   float _456 = (c_light_vec[4].z) + _397;
   float _457 = _456 + _425;
   if (!_443)
   {
      float _461 = (c_ps_user[11].x) * _139;
      float _462 = (c_ps_user[11].x) * _140;
      float _463 = (c_ps_user[11].x) * _141;
      float _465 = _109 + 1.0f;
      float _466 = _110 + 1.0f;
      float _467 = _111 + 1.0f;
      float _468 = _465 * 0.5f;
      float _469 = _466 * 0.5f;
      float _470 = _467 * 0.5f;
      float _471 = saturate(_468);
      float _472 = saturate(_469);
      float _473 = saturate(_470);
      float _474 = (c_ps_user[11].y) * _471;
      float _475 = _472 * (c_ps_user[11].y);
      float _476 = _473 * (c_ps_user[11].y);
      float _479 = (c_ps_user[12].x) * _171;
      float _484 = -0.0f - _118;
      float _485 = -0.0f - _119;
      float _486 = -0.0f - _120;
      float _487 = dot(float3(_484, _485, _486), float3(_109, _110, _111));
      float _488 = 1.0f - _487;
      float _489 = _488 * _488;
      float _490 = _488 * 0.9722222089767456f;
      float _491 = _489 * _489;
      float _492 = _491 * _490;
      float _493 = _492 + 0.02777777425944805f;
      float _494 = _493 * (c_ps_user[13].z);
      float _496 = (c_ps_user[13].w) * _453;
      float _497 = (c_ps_user[13].w) * _455;
      float _498 = (c_ps_user[13].w) * _457;
      float _501 = (c_ps_user[14].y) * _432;
      float _502 = (c_ps_user[14].y) * _433;
      float _503 = (c_ps_user[14].y) * _434;
      float _505 = (c_ps_user[14].z) * _439;
      float _506 = (c_ps_user[14].z) * _440;
      float _507 = (c_ps_user[14].z) * _441;
      float _509 = (c_ps_user[14].w) * _307;
      float _510 = (c_ps_user[14].w) * _308;
      float _511 = (c_ps_user[14].w) * _309;
      float _512 = _479 + (c_ps_user[12].y);
      float _513 = _501 + (c_ps_user[13].x);
      float _514 = _513 + _505;
      float _515 = c_diffuse_scale.x * _514;
      float _516 = _515 * c_mat_diffuse.x;
      float _517 = _494 + _512;
      float _518 = _517 + _461;
      float _519 = _518 + _474;
      float _520 = _519 + _496;
      float _521 = _520 + _509;
      float _522 = _521 + _516;
      float _523 = _502 + (c_ps_user[13].x);
      float _524 = _523 + _506;
      float _525 = c_diffuse_scale.y * _524;
      float _526 = _525 * c_mat_diffuse.y;
      float _527 = _517 + _462;
      float _528 = _527 + _475;
      float _529 = _528 + _497;
      float _530 = _529 + _510;
      float _531 = _530 + _526;
      float _532 = _503 + (c_ps_user[13].x);
      float _533 = _532 + _507;
      float _534 = c_diffuse_scale.z * _533;
      float _535 = _534 * c_mat_diffuse.z;
      float _536 = _517 + _463;
      float _537 = _536 + _476;
      float _538 = _537 + _498;
      float _539 = _538 + _511;
      float _540 = _539 + _535;
      float _541 = saturate(_522);
      float _542 = saturate(_531);
      float _543 = saturate(_540);
      _573 = _541;
      _574 = _542;
      _575 = _543;
   }
   else
   {
      float _545 = _434 * _171;
      float _546 = _457 + _309;
      float _547 = _546 + _441;
      float _548 = _547 + _545;
      float _549 = _433 * _171;
      float _550 = _455 + _308;
      float _551 = _550 + _440;
      float _552 = _551 + _549;
      float _553 = _432 * _171;
      float _554 = _453 + _307;
      float _555 = _554 + _439;
      float _556 = _555 + _553;
      float _557 = _306 + _267;
      float _558 = _306 + _266;
      float _559 = _306 + _265;
      float _560 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _561 = _560 * _139;
      float _562 = _561 * _556;
      float _563 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _564 = _563 * _140;
      float _565 = _564 * _552;
      float _566 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _567 = _566 * _141;
      float _568 = _567 * _548;
      float _569 = _559 + _562;
      float _570 = _558 + _565;
      float _571 = _557 + _568;
      _573 = _569;
      _574 = _570;
      _575 = _571;
   }
   float _580 = c_mat_opacity * _54.w;
   float _581 = _580 * c_opacity_scale;
   float _582 = _581 * c_global_opacity;
   float _583 = _573 * COLOR.x;
   float _584 = _574 * COLOR.y;
   float _585 = _575 * COLOR.z;
   int _586 = c_bool & 16;
   bool _587 = (_586 == 0);
   if (!_587)
   {
      float _591 = c_tone_control.x * _583;
      float _592 = c_tone_control.x * _584;
      float _593 = c_tone_control.x * _585;
      float _595 = log2(_591);
      float _596 = log2(_592);
      float _597 = log2(_593);
      float _598 = _595 * c_tone_control.y;
      float _599 = _596 * c_tone_control.y;
      float _600 = _597 * c_tone_control.y;
      float _601 = exp2(_598);
      float _602 = exp2(_599);
      float _603 = exp2(_600);
      _651 = _601;
      _652 = _602;
      _653 = _603;
   }
   else
   {
      if (_443)
      {
         float _608 = c_tone_control.x * _583;
         float _609 = c_tone_control.x * _584;
         float _610 = c_tone_control.x * _585;
         float _611 = _608 * 2.0f;
         float _612 = _609 * 2.0f;
         float _613 = _610 * 2.0f;
         float _614 = _608 * 0.30000001192092896f;
         float _615 = _609 * 0.30000001192092896f;
         float _616 = _610 * 0.30000001192092896f;
         float _617 = _614 + 0.05000000074505806f;
         float _618 = _615 + 0.05000000074505806f;
         float _619 = _616 + 0.05000000074505806f;
         float _620 = _617 * _611;
         float _621 = _618 * _612;
         float _622 = _619 * _613;
         float _623 = _620 + 0.004000000189989805f;
         float _624 = _621 + 0.004000000189989805f;
         float _625 = _622 + 0.004000000189989805f;
         float _626 = _614 + 0.5f;
         float _627 = _615 + 0.5f;
         float _628 = _616 + 0.5f;
         float _629 = _626 * _611;
         float _630 = _627 * _612;
         float _631 = _628 * _613;
         float _632 = _629 + 0.06000000238418579f;
         float _633 = _630 + 0.06000000238418579f;
         float _634 = _631 + 0.06000000238418579f;
         float _635 = _623 / _632;
         float _636 = _624 / _633;
         float _637 = _625 / _634;
         float _638 = _635 + -0.06666666269302368f;
         float _639 = _636 + -0.06666666269302368f;
         float _640 = _637 + -0.06666666269302368f;
         float _641 = _638 * 1.3790643215179443f;
         float _642 = _639 * 1.3790643215179443f;
         float _643 = _640 * 1.3790643215179443f;
         float _644 = YKR_MaterialCurve(_641, _608);
         float _645 = YKR_MaterialCurve(_642, _609);
         float _646 = YKR_MaterialCurve(_643, _610);
         float _647 = sqrt(_644);
         float _648 = sqrt(_645);
         float _649 = sqrt(_646);
         _651 = _647;
         _652 = _648;
         _653 = _649;
      }
      else
      {
         _651 = _583;
         _652 = _584;
         _653 = _585;
      }
   }
   int _654 = c_bool & 4;
   bool _655 = (_654 == 0);
   if (_655)
   {
      float _657 = saturate(COLOR_1.x);
      float _662 = (c_fog_param[0].x) - _651;
      float _663 = (c_fog_param[0].y) - _652;
      float _664 = (c_fog_param[0].z) - _653;
      float _665 = _662 * _657;
      float _666 = _663 * _657;
      float _667 = _664 * _657;
      _669 = _665;
      _670 = _666;
      _671 = _667;
   }
   else
   {
      _669 = COLOR_1.x;
      _670 = COLOR_1.y;
      _671 = COLOR_1.z;
   }
   float _672 = _669 + _651;
   float _673 = _670 + _652;
   float _674 = _671 + _653;
   if (!_443)
   {
      float _682 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _683 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _684 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _686 = (c_ps_user[12].w) * COLOR.x;
      float _687 = (c_ps_user[12].w) * COLOR.y;
      float _688 = (c_ps_user[12].w) * COLOR.z;
      float _689 = _686 + _672;
      float _690 = _689 + _682;
      float _691 = _687 + _673;
      float _692 = _691 + _683;
      float _693 = _688 + _674;
      float _694 = _693 + _684;
      _696 = _690;
      _697 = _692;
      _698 = _694;
   }
   else
   {
      _696 = _672;
      _697 = _673;
      _698 = _674;
   }
   float _704 = (c_ps_col_scl[0].x) * _696;
   float _705 = (c_ps_col_scl[0].y) * _697;
   float _706 = (c_ps_col_scl[0].z) * _698;
   float _707 = (c_ps_col_scl[0].w) * _582;
   SV_Target.x = _704;
   SV_Target.y = _705;
   SV_Target.z = _706;
   SV_Target.w = _707;
   int _710 = c_bool & 8;
   bool _711 = (_710 != 0);
   float _712 = select(_711, _696, _704);
   float _713 = select(_711, _697, _705);
   float _714 = select(_711, _698, _706);
   float _715 = select(_711, _582, _707);
   SV_Target.x = _712;
   SV_Target.y = _713;
   SV_Target.z = _714;
   SV_Target.w = _715;
   return SV_Target;
}
