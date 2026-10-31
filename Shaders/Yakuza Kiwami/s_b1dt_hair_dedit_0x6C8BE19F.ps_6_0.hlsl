// Yakuza Kiwami material s_b1dt[hair][dedit] (0x6C8BE19F, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _121 = _118 + (c_light_vec[0].x);
   float _122 = _119 + (c_light_vec[0].y);
   float _123 = _120 + (c_light_vec[0].z);
   float _124 = dot(float3(_121, _122, _123), float3(_121, _122, _123));
   float _125 = rsqrt(_124);
   float _126 = _121 * _125;
   float _127 = _122 * _125;
   float _128 = _123 * _125;
   float _132 = c_rt_size.z * SV_Position.x;
   float _133 = c_rt_size.w * SV_Position.y;
   int _136 = c_bool & 8;
   bool _137 = (_136 == 0);
   float _143;
   float _144;
   float _156;
   float _157;
   float _322;
   float _323;
   float _324;
   float _353;
   float _354;
   float _355;
   float _661;
   float _662;
   float _663;
   float _739;
   float _740;
   float _741;
   float _757;
   float _758;
   float _759;
   float _784;
   float _785;
   float _786;
   if (!_137)
   {
      float4 _139 = s13_t.Sample(s13_s, float2(_132, _133));
      _143 = _139.x;
      _144 = _139.y;
   }
   else
   {
      _143 = 1.0f;
      _144 = 1.0f;
   }
   if (!_137)
   {
      float _149 = (c_sdw_param_ch[0].x) * _143;
      float _150 = (c_sdw_param_ch[0].z) * _144;
      float _153 = _149 + (c_sdw_param_ch[0].y);
      float _154 = _150 + (c_sdw_param_ch[0].w);
      _156 = _153;
      _157 = _154;
   }
   else
   {
      _156 = _143;
      _157 = _144;
   }
   float _158 = min(_156, _157);
   float _163 = (c_light_vec[1].x) + _118;
   float _164 = (c_light_vec[1].y) + _119;
   float _165 = (c_light_vec[1].z) + _120;
   float _166 = dot(float3(_163, _164, _165), float3(_163, _164, _165));
   float _167 = rsqrt(_166);
   float _168 = _163 * _167;
   float _169 = _164 * _167;
   float _170 = _165 * _167;
   float _171 = _110 * TEXCOORD_3.z;
   float _172 = _111 * TEXCOORD_3.y;
   float _173 = _171 - _172;
   float _174 = _111 * TEXCOORD_3.x;
   float _175 = _109 * TEXCOORD_3.z;
   float _176 = _174 - _175;
   float _177 = _109 * TEXCOORD_3.y;
   float _178 = _110 * TEXCOORD_3.x;
   float _179 = _177 - _178;
   float _180 = _173 * TEXCOORD_3.w;
   float _181 = _176 * TEXCOORD_3.w;
   float _182 = _179 * TEXCOORD_3.w;
   float _183 = _181 * _111;
   float _184 = _182 * _110;
   float _185 = _183 - _184;
   float _186 = _182 * _109;
   float _187 = _180 * _111;
   float _188 = _186 - _187;
   float _189 = _180 * _110;
   float _190 = _181 * _109;
   float _191 = _189 - _190;
   float _192 = -0.0f - _126;
   float _193 = -0.0f - _127;
   float _194 = -0.0f - _128;
   float _195 = dot(float3(_109, _110, _111), float3(_192, _193, _194));
   float _196 = dot(float3(_192, _193, _194), float3(_180, _181, _182));
   float _197 = TEXCOORD.z * 0.20000000298023224f;
   float _198 = _196 * _197;
   float _199 = _198 + 0.5f;
   float4 _200 = s7_t.Sample(s7_s, float2(_195, _199));
   float _202 = dot(float3(_192, _193, _194), float3(_185, _188, _191));
   float _203 = TEXCOORD.w * 0.20000000298023224f;
   float _204 = _202 * _203;
   float _205 = _204 + 0.5f;
   float4 _206 = s7_t.Sample(s7_s, float2(_195, _205));
   float _208 = _206.x * _200.x;
   float _209 = -0.0f - _168;
   float _210 = -0.0f - _169;
   float _211 = -0.0f - _170;
   float _212 = dot(float3(_109, _110, _111), float3(_209, _210, _211));
   float _213 = dot(float3(_209, _210, _211), float3(_180, _181, _182));
   float _214 = _213 * _197;
   float _215 = _214 + 0.5f;
   float4 _216 = s7_t.Sample(s7_s, float2(_212, _215));
   float _218 = dot(float3(_209, _210, _211), float3(_185, _188, _191));
   float _219 = _218 * _203;
   float _220 = _219 + 0.5f;
   float4 _221 = s7_t.Sample(s7_s, float2(_212, _220));
   float _223 = _221.x * _216.x;
   float _224 = log2(_208);
   float _225 = log2(_223);
   float _226 = _224 * c_mat_power;
   float _227 = _225 * c_mat_power;
   float _228 = exp2(_226);
   float _229 = exp2(_227);
   float _234 = c_mat_fresnel * c_tone_control.w;
   float _235 = c_mat_power * 0.03974359855055809f;
   float _236 = _235 + 0.0856831967830658f;
   float _237 = _229 * _236;
   float _238 = dot(float3(_118, _119, _120), float3(_126, _127, _128));
   float _239 = dot(float3(_118, _119, _120), float3(_168, _169, _170));
   float _240 = 1.0f - _238;
   float _241 = 1.0f - _239;
   float _242 = _240 * _240;
   float _243 = _241 * _241;
   float _244 = 1.0f - _234;
   float _245 = _242 * _242;
   float _246 = _245 * _240;
   float _247 = _246 * _244;
   float _248 = _241 * _244;
   float _249 = _243 * _243;
   float _250 = _249 * _248;
   float _251 = _247 + _234;
   float _252 = _250 + _234;
   float _253 = _237 * _252;
   float _262 = c_specular_scale.x * c_mat_specular.x;
   float _263 = c_specular_scale.y * c_mat_specular.y;
   float _264 = c_specular_scale.z * c_mat_specular.z;
   float _269 = _158 * _236;
   float _270 = _269 * _228;
   float _271 = _270 * _251;
   float _272 = _271 * (c_light_col[0].x);
   float _273 = _271 * (c_light_col[0].y);
   float _274 = _271 * (c_light_col[0].z);
   float _279 = _253 * (c_light_col[1].x);
   float _280 = _253 * (c_light_col[1].y);
   float _281 = _253 * (c_light_col[1].z);
   float _282 = _272 + _279;
   float _283 = _273 + _280;
   float _284 = _274 + _281;
   float _285 = _262 * _282;
   float _286 = _263 * _283;
   float _287 = _264 * _284;
   int _293 = c_bool & 1;
   bool _294 = (_293 == 0);
   if (!_294)
   {
      float _296 = -0.0f - TEXCOORD_4.x;
      float _297 = -0.0f - TEXCOORD_4.y;
      float _298 = -0.0f - TEXCOORD_4.z;
      float _299 = dot(float3(_109, _110, _111), float3(_296, _297, _298));
      float _300 = saturate(_299);
      float _301 = _300 * TEXCOORD_1.w;
      float _302 = _300 * TEXCOORD_2.w;
      float _303 = _300 * TEXCOORD_4.w;
      float _304 = max(0.0f, _301);
      float _305 = max(0.0f, _302);
      float _306 = max(0.0f, _303);
      float _307 = -0.0f - TEXCOORD_5.x;
      float _308 = -0.0f - TEXCOORD_5.y;
      float _309 = -0.0f - TEXCOORD_5.z;
      float _310 = dot(float3(_109, _110, _111), float3(_307, _308, _309));
      float _311 = saturate(_310);
      float _312 = _311 * COLOR.w;
      float _313 = _311 * COLOR_1.w;
      float _314 = _311 * TEXCOORD_5.w;
      float _315 = max(0.0f, _312);
      float _316 = max(0.0f, _313);
      float _317 = max(0.0f, _314);
      float _318 = _315 + _304;
      float _319 = _316 + _305;
      float _320 = _317 + _306;
      _322 = _318;
      _323 = _319;
      _324 = _320;
   }
   else
   {
      _322 = 0.0f;
      _323 = 0.0f;
      _324 = 0.0f;
   }
   float _325 = c_rt_size.z * SV_Position.x;
   float _326 = c_rt_size.w * SV_Position.y;
   float4 _327 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_325, _326)));
   int _333 = c_bool & 16384;
   bool _334 = (_333 == 0);
   if (!_334)
   {
      float _336 = _327.x * 4.0f;
      float _337 = _327.y * 4.0f;
      float _338 = _327.z * 4.0f;
      _353 = _336;
      _354 = _337;
      _355 = _338;
   }
   else
   {
      int _340 = c_bool & 8192;
      bool _341 = (_340 == 0);
      if (!_341)
      {
         float _343 = _327.x * 2.0f;
         float _344 = _327.y * 2.0f;
         float _345 = _327.z * 2.0f;
         _353 = _343;
         _354 = _344;
         _355 = _345;
      }
      else
      {
         float _349 = _327.x / c_tone_control.z;
         float _350 = _327.y / c_tone_control.z;
         float _351 = _327.z / c_tone_control.z;
         _353 = _349;
         _354 = _350;
         _355 = _351;
      }
   }
   float _356 = _353 + _322;
   float _357 = _354 + _323;
   float _358 = _355 + _324;
   float _421 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _422 = rsqrt(_421);
   float _423 = _422 * _109;
   float _424 = _422 * _110;
   float _425 = _422 * _111;
   float _426 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_423, _424, _425, 1.0f));
   float _427 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_423, _424, _425, 1.0f));
   float _428 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_423, _424, _425, 1.0f));
   float _429 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_423, _424, _425, 1.0f));
   float _430 = dot(float4(_423, _424, _425, 1.0f), float4(_426, _427, _428, _429));
   float _431 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_423, _424, _425, 1.0f));
   float _432 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_423, _424, _425, 1.0f));
   float _433 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_423, _424, _425, 1.0f));
   float _434 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_423, _424, _425, 1.0f));
   float _435 = dot(float4(_423, _424, _425, 1.0f), float4(_431, _432, _433, _434));
   float _436 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_423, _424, _425, 1.0f));
   float _437 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_423, _424, _425, 1.0f));
   float _438 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_423, _424, _425, 1.0f));
   float _439 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_423, _424, _425, 1.0f));
   float _440 = dot(float4(_423, _424, _425, 1.0f), float4(_436, _437, _438, _439));
   float _441 = max(_430, 0.0625f);
   float _442 = max(_435, 0.0625f);
   float _443 = max(_440, 0.0625f);
   float _444 = _441 * (c_ps_scratch[79].x);
   float _445 = _442 * (c_ps_scratch[79].x);
   float _446 = _443 * (c_ps_scratch[79].x);
   float _451 = -0.0f - (c_light_vec[0].x);
   float _452 = -0.0f - (c_light_vec[0].y);
   float _453 = -0.0f - (c_light_vec[0].z);
   float _454 = dot(float3(_109, _110, _111), float3(_451, _452, _453));
   float _455 = -0.0f - (c_light_vec[1].x);
   float _456 = -0.0f - (c_light_vec[1].y);
   float _457 = -0.0f - (c_light_vec[1].z);
   float _458 = dot(float3(_109, _110, _111), float3(_455, _456, _457));
   float _467 = _110 * 0.5f;
   float _468 = _467 + 0.5f;
   float _469 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _470 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _471 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _472 = _469 * _468;
   float _473 = _470 * _468;
   float _474 = _471 * _468;
   float _475 = saturate(_454);
   float _476 = saturate(_458);
   float _481 = (c_light_col[0].x) * _475;
   float _482 = (c_light_col[0].y) * _475;
   float _483 = (c_light_col[0].z) * _475;
   float _488 = (c_light_col[1].x) * _476;
   float _489 = (c_light_col[1].y) * _476;
   float _490 = (c_light_col[1].z) * _476;
   int _491 = c_bool & 32768;
   bool _492 = (_491 == 0);
   float _501 = (c_light_vec[4].x) + _444;
   float _502 = _501 + _472;
   float _503 = (c_light_vec[4].y) + _445;
   float _504 = _503 + _473;
   float _505 = (c_light_vec[4].z) + _446;
   float _506 = _505 + _474;
   if (!_492)
   {
      float _510 = (c_ps_user[11].x) * _69;
      float _511 = (c_ps_user[11].x) * _70;
      float _512 = (c_ps_user[11].x) * _71;
      float _514 = _109 + 1.0f;
      float _515 = _110 + 1.0f;
      float _516 = _111 + 1.0f;
      float _517 = _514 * 0.5f;
      float _518 = _515 * 0.5f;
      float _519 = _516 * 0.5f;
      float _520 = saturate(_517);
      float _521 = saturate(_518);
      float _522 = saturate(_519);
      float _523 = (c_ps_user[11].y) * _520;
      float _524 = _521 * (c_ps_user[11].y);
      float _525 = _522 * (c_ps_user[11].y);
      float _536 = (c_ps_user[11].w) * _285;
      float _539 = (c_ps_user[12].x) * _158;
      float _545 = -0.0f - _118;
      float _546 = -0.0f - _119;
      float _547 = -0.0f - _120;
      float _548 = dot(float3(_545, _546, _547), float3(_109, _110, _111));
      float _549 = 1.0f - _548;
      float _550 = _549 * _549;
      float _551 = _549 * 0.9722222089767456f;
      float _552 = _550 * _550;
      float _553 = _552 * _551;
      float _554 = _553 + 0.02777777425944805f;
      float _555 = _554 * (c_ps_user[13].z);
      float _557 = (c_ps_user[13].w) * _502;
      float _558 = (c_ps_user[13].w) * _504;
      float _559 = (c_ps_user[13].w) * _506;
      float _562 = (c_ps_user[14].y) * _481;
      float _563 = (c_ps_user[14].y) * _482;
      float _564 = (c_ps_user[14].y) * _483;
      float _566 = (c_ps_user[14].z) * _488;
      float _567 = (c_ps_user[14].z) * _489;
      float _568 = (c_ps_user[14].z) * _490;
      float _570 = (c_ps_user[14].w) * _356;
      float _571 = (c_ps_user[14].w) * _357;
      float _572 = (c_ps_user[14].w) * _358;
      float _576 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _577 = c_specular_scale.x * _576;
      float _578 = _577 * _285;
      float _579 = _562 + (c_ps_user[13].x);
      float _580 = _579 + _566;
      float _581 = c_diffuse_scale.x * _580;
      float _582 = _581 * c_mat_diffuse.x;
      float _583 = (c_ps_user[13].y) + (c_ps_user[11].z);
      float _584 = _583 * c_specular_scale.x;
      float _585 = _584 + _578;
      float _586 = _585 * c_mat_specular.x;
      float _587 = _523 + _510;
      float _588 = _587 + _536;
      float _589 = _588 + (c_ps_user[12].y);
      float _590 = _589 + _539;
      float _591 = _590 + _557;
      float _592 = _591 + _570;
      float _593 = _592 + _555;
      float _594 = _593 + _582;
      float _595 = _594 + _586;
      float _596 = c_specular_scale.y * _576;
      float _597 = _596 * _286;
      float _598 = _563 + (c_ps_user[13].x);
      float _599 = _598 + _567;
      float _600 = c_diffuse_scale.y * _599;
      float _601 = _600 * c_mat_diffuse.y;
      float _602 = _583 * c_specular_scale.y;
      float _603 = _602 + _597;
      float _604 = _603 * c_mat_specular.y;
      float _605 = _536 + _511;
      float _606 = _605 + _524;
      float _607 = _606 + (c_ps_user[12].y);
      float _608 = _607 + _539;
      float _609 = _608 + _558;
      float _610 = _609 + _571;
      float _611 = _610 + _555;
      float _612 = _611 + _601;
      float _613 = _612 + _604;
      float _614 = c_specular_scale.z * _576;
      float _615 = _614 * _287;
      float _616 = _564 + (c_ps_user[13].x);
      float _617 = _616 + _568;
      float _618 = c_diffuse_scale.z * _617;
      float _619 = _618 * c_mat_diffuse.z;
      float _620 = _583 * c_specular_scale.z;
      float _621 = _620 + _615;
      float _622 = _621 * c_mat_specular.z;
      float _623 = _536 + _512;
      float _624 = _623 + _525;
      float _625 = _624 + (c_ps_user[12].y);
      float _626 = _625 + _539;
      float _627 = _626 + _559;
      float _628 = _627 + _572;
      float _629 = _628 + _555;
      float _630 = _629 + _619;
      float _631 = _630 + _622;
      float _632 = saturate(_595);
      float _633 = saturate(_613);
      float _634 = saturate(_631);
      _661 = _632;
      _662 = _633;
      _663 = _634;
   }
   else
   {
      float _636 = _483 * _158;
      float _637 = _506 + _358;
      float _638 = _637 + _490;
      float _639 = _638 + _636;
      float _640 = _482 * _158;
      float _641 = _504 + _357;
      float _642 = _641 + _489;
      float _643 = _642 + _640;
      float _644 = _481 * _158;
      float _645 = _502 + _356;
      float _646 = _645 + _488;
      float _647 = _646 + _644;
      float _648 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _649 = _648 * _69;
      float _650 = _649 * _647;
      float _651 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _652 = _651 * _70;
      float _653 = _652 * _643;
      float _654 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _655 = _654 * _71;
      float _656 = _655 * _639;
      float _657 = _650 + _285;
      float _658 = _653 + _286;
      float _659 = _656 + _287;
      _661 = _657;
      _662 = _658;
      _663 = _659;
   }
   float _668 = c_mat_opacity * _54.w;
   float _669 = _668 * c_opacity_scale;
   float _670 = _669 * c_global_opacity;
   float _671 = _661 * COLOR.x;
   float _672 = _662 * COLOR.y;
   float _673 = _663 * COLOR.z;
   int _674 = c_bool & 16;
   bool _675 = (_674 == 0);
   if (!_675)
   {
      float _679 = c_tone_control.x * _671;
      float _680 = c_tone_control.x * _672;
      float _681 = c_tone_control.x * _673;
      float _683 = log2(_679);
      float _684 = log2(_680);
      float _685 = log2(_681);
      float _686 = _683 * c_tone_control.y;
      float _687 = _684 * c_tone_control.y;
      float _688 = _685 * c_tone_control.y;
      float _689 = exp2(_686);
      float _690 = exp2(_687);
      float _691 = exp2(_688);
      _739 = _689;
      _740 = _690;
      _741 = _691;
   }
   else
   {
      if (_492)
      {
         float _696 = c_tone_control.x * _671;
         float _697 = c_tone_control.x * _672;
         float _698 = c_tone_control.x * _673;
         float _699 = _696 * 2.0f;
         float _700 = _697 * 2.0f;
         float _701 = _698 * 2.0f;
         float _702 = _696 * 0.30000001192092896f;
         float _703 = _697 * 0.30000001192092896f;
         float _704 = _698 * 0.30000001192092896f;
         float _705 = _702 + 0.05000000074505806f;
         float _706 = _703 + 0.05000000074505806f;
         float _707 = _704 + 0.05000000074505806f;
         float _708 = _705 * _699;
         float _709 = _706 * _700;
         float _710 = _707 * _701;
         float _711 = _708 + 0.004000000189989805f;
         float _712 = _709 + 0.004000000189989805f;
         float _713 = _710 + 0.004000000189989805f;
         float _714 = _702 + 0.5f;
         float _715 = _703 + 0.5f;
         float _716 = _704 + 0.5f;
         float _717 = _714 * _699;
         float _718 = _715 * _700;
         float _719 = _716 * _701;
         float _720 = _717 + 0.06000000238418579f;
         float _721 = _718 + 0.06000000238418579f;
         float _722 = _719 + 0.06000000238418579f;
         float _723 = _711 / _720;
         float _724 = _712 / _721;
         float _725 = _713 / _722;
         float _726 = _723 + -0.06666666269302368f;
         float _727 = _724 + -0.06666666269302368f;
         float _728 = _725 + -0.06666666269302368f;
         float _729 = _726 * 1.3790643215179443f;
         float _730 = _727 * 1.3790643215179443f;
         float _731 = _728 * 1.3790643215179443f;
         float _732 = YKR_MaterialCurve(_729, _696);
         float _733 = YKR_MaterialCurve(_730, _697);
         float _734 = YKR_MaterialCurve(_731, _698);
         float _735 = sqrt(_732);
         float _736 = sqrt(_733);
         float _737 = sqrt(_734);
         _739 = _735;
         _740 = _736;
         _741 = _737;
      }
      else
      {
         _739 = _671;
         _740 = _672;
         _741 = _673;
      }
   }
   int _742 = c_bool & 4;
   bool _743 = (_742 == 0);
   if (_743)
   {
      float _745 = saturate(COLOR_1.x);
      float _750 = (c_fog_param[0].x) - _739;
      float _751 = (c_fog_param[0].y) - _740;
      float _752 = (c_fog_param[0].z) - _741;
      float _753 = _750 * _745;
      float _754 = _751 * _745;
      float _755 = _752 * _745;
      _757 = _753;
      _758 = _754;
      _759 = _755;
   }
   else
   {
      _757 = COLOR_1.x;
      _758 = COLOR_1.y;
      _759 = COLOR_1.z;
   }
   float _760 = _757 + _739;
   float _761 = _758 + _740;
   float _762 = _759 + _741;
   if (!_492)
   {
      float _770 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _771 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _772 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _774 = (c_ps_user[12].w) * COLOR.x;
      float _775 = (c_ps_user[12].w) * COLOR.y;
      float _776 = (c_ps_user[12].w) * COLOR.z;
      float _777 = _774 + _760;
      float _778 = _777 + _770;
      float _779 = _775 + _761;
      float _780 = _779 + _771;
      float _781 = _776 + _762;
      float _782 = _781 + _772;
      _784 = _778;
      _785 = _780;
      _786 = _782;
   }
   else
   {
      _784 = _760;
      _785 = _761;
      _786 = _762;
   }
   float _789 = _670 - c_mat_punchthrough_ref;
   bool _790 = (_789 < 0.0f);
   if (_790)
      discard;
   float _796 = (c_ps_col_scl[0].x) * _784;
   float _797 = (c_ps_col_scl[0].y) * _785;
   float _798 = (c_ps_col_scl[0].z) * _786;
   float _799 = (c_ps_col_scl[0].w) * _670;
   SV_Target.x = _796;
   SV_Target.y = _797;
   SV_Target.z = _798;
   SV_Target.w = _799;
   int _802 = c_bool & 8;
   bool _803 = (_802 != 0);
   float _804 = select(_803, _784, _796);
   float _805 = select(_803, _785, _797);
   float _806 = select(_803, _786, _798);
   float _807 = select(_803, _670, _799);
   SV_Target.x = _804;
   SV_Target.y = _805;
   SV_Target.z = _806;
   SV_Target.w = _807;
   return SV_Target;
}
