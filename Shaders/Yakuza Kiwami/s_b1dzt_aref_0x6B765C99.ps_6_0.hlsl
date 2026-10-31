// Yakuza Kiwami material s_b1dzt[aref] (0x6B765C99, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
    linear float2 TEXCOORD : TEXCOORD,
    linear float4 TEXCOORD_1 : TEXCOORD1,
    linear float4 TEXCOORD_2 : TEXCOORD2,
    linear float4 TEXCOORD_3 : TEXCOORD3,
    linear float4 TEXCOORD_4 : TEXCOORD4,
    linear float4 TEXCOORD_5 : TEXCOORD5,
    noperspective float4 SV_Position : SV_Position) : SV_Target
{
   float4 SV_Target = 0;
   float4 _54 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _59 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _64 = _59.x * _59.x;
   float _65 = _59.y * _59.y;
   float _66 = _59.z * _59.z;
   float4 _67 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _72 = c_mat_power * _54.z;
   float _73 = max(1.0f, _72);
   float _74 = _67.x * 2.0f;
   float _75 = _67.y * 2.0f;
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
   float _129 = dot(float3(_118, _119, _120), float3(_109, _110, _111));
   float _130 = _129 * 2.0f;
   float _131 = _130 * _109;
   float _132 = _130 * _110;
   float _133 = _130 * _111;
   float _134 = _118 - _131;
   float _135 = _119 - _132;
   float _136 = _120 - _133;
   float _137 = dot(float3(_134, _135, _136), float3(_134, _135, _136));
   float _138 = rsqrt(_137);
   float _139 = _135 * _138;
   float _140 = _136 * _138;
   float _141 = _138 * _134;
   float _142 = -0.0f - _141;
   float4 _143 = s1c_t.Sample(s1c_s, float3(_142, _139, _140));
   float _148 = max(_143.w, 0.003921568859368563f);
   float _149 = 0.25f / _148;
   float _150 = _149 * _143.x;
   float _151 = _149 * _143.y;
   float _152 = _149 * _143.z;
   float _153 = _150 * _150;
   float _154 = _153 * _54.w;
   float _155 = _151 * _151;
   float _156 = _155 * _54.w;
   float _157 = _152 * _152;
   float _158 = _157 * _54.w;
   float _159 = _154 + _64;
   float _160 = _156 + _65;
   float _161 = _158 + _66;
   float _165 = c_rt_size.z * SV_Position.x;
   float _166 = c_rt_size.w * SV_Position.y;
   int _169 = c_bool & 8;
   bool _170 = (_169 == 0);
   float _176;
   float _177;
   float _189;
   float _190;
   float _370;
   float _371;
   float _372;
   float _373;
   float _374;
   float _375;
   float _408;
   float _409;
   float _410;
   float _411;
   float _576;
   float _747;
   float _748;
   float _749;
   float _825;
   float _826;
   float _827;
   float _843;
   float _844;
   float _845;
   float _870;
   float _871;
   float _872;
   if (!_170)
   {
      float4 _172 = s13_t.Sample(s13_s, float2(_165, _166));
      _176 = _172.x;
      _177 = _172.y;
   }
   else
   {
      _176 = 1.0f;
      _177 = 1.0f;
   }
   if (!_170)
   {
      float _182 = (c_sdw_param_ch[0].x) * _176;
      float _183 = (c_sdw_param_ch[0].z) * _177;
      float _186 = _182 + (c_sdw_param_ch[0].y);
      float _187 = _183 + (c_sdw_param_ch[0].w);
      _189 = _186;
      _190 = _187;
   }
   else
   {
      _189 = _176;
      _190 = _177;
   }
   float _191 = min(_189, _190);
   float _200 = (c_light_vec[1].x) + _118;
   float _201 = (c_light_vec[1].y) + _119;
   float _202 = (c_light_vec[1].z) + _120;
   float _203 = dot(float3(_200, _201, _202), float3(_200, _201, _202));
   float _204 = rsqrt(_203);
   float _205 = _200 * _204;
   float _206 = _201 * _204;
   float _207 = _202 * _204;
   float _208 = -0.0f - (c_light_vec[1].x);
   float _209 = -0.0f - (c_light_vec[1].y);
   float _210 = -0.0f - (c_light_vec[1].z);
   float _211 = -0.0f - (c_light_vec[0].x);
   float _212 = -0.0f - (c_light_vec[0].y);
   float _213 = -0.0f - (c_light_vec[0].z);
   float _214 = dot(float3(_134, _135, _136), float3(_211, _212, _213));
   float _215 = dot(float3(_134, _135, _136), float3(_208, _209, _210));
   float _216 = saturate(_214);
   float _217 = saturate(_215);
   float _218 = log2(_216);
   float _219 = log2(_217);
   float _220 = _218 * _73;
   float _221 = _219 * _73;
   float _222 = exp2(_220);
   float _223 = exp2(_221);
   float _228 = c_mat_fresnel * c_tone_control.w;
   float _229 = _73 * 0.03974359855055809f;
   float _230 = _229 + 0.0856831967830658f;
   float _231 = _223 * _230;
   float _232 = dot(float3(_118, _119, _120), float3(_126, _127, _128));
   float _233 = dot(float3(_118, _119, _120), float3(_205, _206, _207));
   float _234 = 1.0f - _232;
   float _235 = 1.0f - _233;
   float _236 = _234 * _234;
   float _237 = _235 * _235;
   float _238 = 1.0f - _228;
   float _239 = _236 * _236;
   float _240 = _239 * _234;
   float _241 = _240 * _238;
   float _242 = _235 * _238;
   float _243 = _237 * _237;
   float _244 = _243 * _242;
   float _245 = _241 + _228;
   float _246 = _244 + _228;
   float _247 = dot(float3(_109, _110, _111), float3(_211, _212, _213));
   float _248 = saturate(_247);
   float _249 = dot(float3(_109, _110, _111), float3(_208, _209, _210));
   float _250 = saturate(_249);
   float _251 = _231 * _250;
   float _252 = _251 * _246;
   float _261 = c_specular_scale.x * c_mat_specular.x;
   float _262 = c_specular_scale.y * c_mat_specular.y;
   float _263 = c_specular_scale.z * c_mat_specular.z;
   float _268 = _191 * _230;
   float _269 = _268 * _222;
   float _270 = _269 * _248;
   float _271 = _270 * _245;
   float _272 = (c_light_col[0].x) * _271;
   float _273 = (c_light_col[0].y) * _271;
   float _274 = (c_light_col[0].z) * _271;
   float _279 = (c_light_col[1].x) * _252;
   float _280 = (c_light_col[1].y) * _252;
   float _281 = (c_light_col[1].z) * _252;
   float _282 = _279 + _272;
   float _283 = _280 + _273;
   float _284 = _281 + _274;
   float _285 = _261 * _282;
   float _286 = _262 * _283;
   float _287 = _263 * _284;
   int _291 = c_bool & 1;
   bool _292 = (_291 == 0);
   if (!_292)
   {
      float _294 = -0.0f - TEXCOORD_4.x;
      float _295 = -0.0f - TEXCOORD_4.y;
      float _296 = -0.0f - TEXCOORD_4.z;
      float _297 = dot(float3(_109, _110, _111), float3(_294, _295, _296));
      float _298 = saturate(_297);
      float _299 = _298 * TEXCOORD_1.w;
      float _300 = _298 * TEXCOORD_2.w;
      float _301 = _298 * TEXCOORD_4.w;
      float _302 = max(0.0f, _299);
      float _303 = max(0.0f, _300);
      float _304 = max(0.0f, _301);
      float _305 = -0.0f - TEXCOORD_5.x;
      float _306 = -0.0f - TEXCOORD_5.y;
      float _307 = -0.0f - TEXCOORD_5.z;
      float _308 = dot(float3(_109, _110, _111), float3(_305, _306, _307));
      float _309 = saturate(_308);
      float _310 = _309 * COLOR.w;
      float _311 = _309 * COLOR_1.w;
      float _312 = _309 * TEXCOORD_5.w;
      float _313 = max(0.0f, _310);
      float _314 = max(0.0f, _311);
      float _315 = max(0.0f, _312);
      float _316 = _313 + _302;
      float _317 = _314 + _303;
      float _318 = _315 + _304;
      float _319 = _118 + TEXCOORD_4.x;
      float _320 = _119 + TEXCOORD_4.y;
      float _321 = _120 + TEXCOORD_4.z;
      float _322 = dot(float3(_319, _320, _321), float3(_319, _320, _321));
      float _323 = rsqrt(_322);
      float _324 = _118 + TEXCOORD_5.x;
      float _325 = _119 + TEXCOORD_5.y;
      float _326 = _120 + TEXCOORD_5.z;
      float _327 = dot(float3(_324, _325, _326), float3(_324, _325, _326));
      float _328 = rsqrt(_327);
      float _329 = _324 * _328;
      float _330 = -0.0f - _329;
      float _331 = _325 * _328;
      float _332 = -0.0f - _331;
      float _333 = _326 * _328;
      float _334 = -0.0f - _333;
      float _335 = _319 * _323;
      float _336 = -0.0f - _335;
      float _337 = _320 * _323;
      float _338 = -0.0f - _337;
      float _339 = _321 * _323;
      float _340 = -0.0f - _339;
      float _341 = dot(float3(_336, _338, _340), float3(_109, _110, _111));
      float _342 = dot(float3(_330, _332, _334), float3(_109, _110, _111));
      float _343 = saturate(_341);
      float _344 = saturate(_342);
      float _345 = log2(_343);
      float _346 = log2(_344);
      float _347 = _345 * _73;
      float _348 = _346 * _73;
      float _349 = exp2(_347);
      float _350 = exp2(_348);
      float _351 = _349 * TEXCOORD_1.w;
      float _352 = _349 * TEXCOORD_2.w;
      float _353 = _349 * TEXCOORD_4.w;
      float _354 = _350 * COLOR.w;
      float _355 = _350 * COLOR_1.w;
      float _356 = _350 * TEXCOORD_5.w;
      float _357 = _354 + _351;
      float _358 = _355 + _352;
      float _359 = _356 + _353;
      float _360 = max(0.0f, _357);
      float _361 = max(0.0f, _358);
      float _362 = max(0.0f, _359);
      float _363 = _261 * _360;
      float _364 = _262 * _361;
      float _365 = _263 * _362;
      float _366 = min(_363, 1.0f);
      float _367 = min(_364, 1.0f);
      float _368 = min(_365, 1.0f);
      _370 = _366;
      _371 = _367;
      _372 = _368;
      _373 = _316;
      _374 = _317;
      _375 = _318;
   }
   else
   {
      _370 = 0.0f;
      _371 = 0.0f;
      _372 = 0.0f;
      _373 = 0.0f;
      _374 = 0.0f;
      _375 = 0.0f;
   }
   float _376 = c_rt_size.z * SV_Position.x;
   float _377 = c_rt_size.w * SV_Position.y;
   float4 _378 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_376, _377)));
   int _385 = c_bool & 16384;
   bool _386 = (_385 == 0);
   if (!_386)
   {
      float _388 = _378.x * 4.0f;
      float _389 = _378.y * 4.0f;
      float _390 = _378.z * 4.0f;
      float _391 = _378.w * 4.0f;
      _408 = _388;
      _409 = _389;
      _410 = _390;
      _411 = _391;
   }
   else
   {
      int _393 = c_bool & 8192;
      bool _394 = (_393 == 0);
      if (!_394)
      {
         float _396 = _378.x * 2.0f;
         float _397 = _378.y * 2.0f;
         float _398 = _378.z * 2.0f;
         float _399 = _378.w * 2.0f;
         _408 = _396;
         _409 = _397;
         _410 = _398;
         _411 = _399;
      }
      else
      {
         float _403 = _378.x / c_tone_control.z;
         float _404 = _378.y / c_tone_control.z;
         float _405 = _378.z / c_tone_control.z;
         float _406 = _378.w / c_tone_control.z;
         _408 = _403;
         _409 = _404;
         _410 = _405;
         _411 = _406;
      }
   }
   float _412 = _408 + _373;
   float _413 = _409 + _374;
   float _414 = _410 + _375;
   float _415 = _411 + _370;
   float _416 = _411 + _371;
   float _417 = _411 + _372;
   float _480 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _481 = rsqrt(_480);
   float _482 = _481 * _109;
   float _483 = _481 * _110;
   float _484 = _481 * _111;
   float _485 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_482, _483, _484, 1.0f));
   float _486 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_482, _483, _484, 1.0f));
   float _487 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_482, _483, _484, 1.0f));
   float _488 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_482, _483, _484, 1.0f));
   float _489 = dot(float4(_482, _483, _484, 1.0f), float4(_485, _486, _487, _488));
   float _490 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_482, _483, _484, 1.0f));
   float _491 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_482, _483, _484, 1.0f));
   float _492 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_482, _483, _484, 1.0f));
   float _493 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_482, _483, _484, 1.0f));
   float _494 = dot(float4(_482, _483, _484, 1.0f), float4(_490, _491, _492, _493));
   float _495 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_482, _483, _484, 1.0f));
   float _496 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_482, _483, _484, 1.0f));
   float _497 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_482, _483, _484, 1.0f));
   float _498 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_482, _483, _484, 1.0f));
   float _499 = dot(float4(_482, _483, _484, 1.0f), float4(_495, _496, _497, _498));
   float _500 = max(_489, 0.0625f);
   float _501 = max(_494, 0.0625f);
   float _502 = max(_499, 0.0625f);
   float _503 = _500 * (c_ps_scratch[79].x);
   float _504 = _501 * (c_ps_scratch[79].x);
   float _505 = _502 * (c_ps_scratch[79].x);
   float _510 = -0.0f - (c_light_vec[0].x);
   float _511 = -0.0f - (c_light_vec[0].y);
   float _512 = -0.0f - (c_light_vec[0].z);
   float _513 = dot(float3(_109, _110, _111), float3(_510, _511, _512));
   float _514 = -0.0f - (c_light_vec[1].x);
   float _515 = -0.0f - (c_light_vec[1].y);
   float _516 = -0.0f - (c_light_vec[1].z);
   float _517 = dot(float3(_109, _110, _111), float3(_514, _515, _516));
   float _526 = _110 * 0.5f;
   float _527 = _526 + 0.5f;
   float _528 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _529 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _530 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _531 = _528 * _527;
   float _532 = _529 * _527;
   float _533 = _530 * _527;
   float _534 = saturate(_513);
   float _535 = saturate(_517);
   float _540 = (c_light_col[0].x) * _534;
   float _541 = (c_light_col[0].y) * _534;
   float _542 = (c_light_col[0].z) * _534;
   float _547 = (c_light_col[1].x) * _535;
   float _548 = (c_light_col[1].y) * _535;
   float _549 = (c_light_col[1].z) * _535;
   float _550 = _540 * _191;
   float _551 = _541 * _191;
   float _552 = _542 * _191;
   float _553 = (c_light_vec[4].x) + _503;
   float _554 = _553 + _531;
   float _555 = _554 + _412;
   float _556 = _555 + _547;
   float _557 = _556 + _550;
   float _558 = (c_light_vec[4].y) + _504;
   float _559 = _558 + _532;
   float _560 = _559 + _413;
   float _561 = _560 + _548;
   float _562 = _561 + _551;
   float _563 = (c_light_vec[4].z) + _505;
   float _564 = _563 + _533;
   float _565 = _564 + _414;
   float _566 = _565 + _549;
   float _567 = _566 + _552;
   int _568 = c_bool & 64;
   bool _569 = (_568 == 0);
   if (!_569)
   {
      float _571 = 1.0f - _54.y;
      float _572 = _191 * _571;
      float _573 = _572 * _534;
      float _574 = _573 + _54.y;
      _576 = _574;
   }
   else
   {
      _576 = _54.y;
   }
   float _577 = _415 + _285;
   float _578 = _416 + _286;
   float _579 = _417 + _287;
   int _580 = c_bool & 32768;
   bool _581 = (_580 == 0);
   if (!_581)
   {
      float _593 = (c_ps_user[11].x) * _159;
      float _594 = (c_ps_user[11].x) * _160;
      float _595 = (c_ps_user[11].x) * _161;
      float _597 = _109 + 1.0f;
      float _598 = _110 + 1.0f;
      float _599 = _111 + 1.0f;
      float _600 = _597 * 0.5f;
      float _601 = _598 * 0.5f;
      float _602 = _599 * 0.5f;
      float _603 = saturate(_600);
      float _604 = saturate(_601);
      float _605 = saturate(_602);
      float _606 = (c_ps_user[11].y) * _603;
      float _607 = _604 * (c_ps_user[11].y);
      float _608 = _605 * (c_ps_user[11].y);
      float _618 = (c_ps_user[11].z) * _54.x;
      float _620 = (c_ps_user[11].w) * _577;
      float _623 = (c_ps_user[12].x) * _191;
      float _625 = (c_ps_user[12].y) * _54.y;
      float _630 = -0.0f - _118;
      float _631 = -0.0f - _119;
      float _632 = -0.0f - _120;
      float _633 = dot(float3(_630, _631, _632), float3(_109, _110, _111));
      float _634 = 1.0f - _633;
      float _635 = _634 * _634;
      float _636 = _634 * 0.9722222089767456f;
      float _637 = _635 * _635;
      float _638 = _637 * _636;
      float _639 = _638 + 0.02777777425944805f;
      float _640 = _639 * (c_ps_user[13].z);
      float _642 = (c_ps_user[13].w) * _554;
      float _643 = (c_ps_user[13].w) * _559;
      float _644 = (c_ps_user[13].w) * _564;
      float _647 = (c_ps_user[14].y) * _540;
      float _648 = (c_ps_user[14].y) * _541;
      float _649 = (c_ps_user[14].y) * _542;
      float _651 = (c_ps_user[14].z) * _547;
      float _652 = (c_ps_user[14].z) * _548;
      float _653 = (c_ps_user[14].z) * _549;
      float _655 = (c_ps_user[14].w) * _412;
      float _656 = (c_ps_user[14].w) * _413;
      float _657 = (c_ps_user[14].w) * _414;
      float _662 = (c_ps_user[15].z) * _415;
      float _663 = (c_ps_user[15].z) * _416;
      float _664 = (c_ps_user[15].z) * _417;
      float _665 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _666 = c_specular_scale.x * _665;
      float _667 = _666 * _577;
      float _668 = _647 + (c_ps_user[13].x);
      float _669 = _668 + _651;
      float _670 = c_diffuse_scale.x * _669;
      float _671 = _670 * c_mat_diffuse.x;
      float _672 = (c_ps_user[13].y) + _618;
      float _673 = _672 * c_specular_scale.x;
      float _674 = _673 + _667;
      float _675 = _674 * c_mat_specular.x;
      float _676 = _606 + _593;
      float _677 = _676 + _620;
      float _678 = _677 + _623;
      float _679 = _678 + _625;
      float _680 = _679 + _642;
      float _681 = _680 + _655;
      float _682 = _681 + _662;
      float _683 = _682 + _640;
      float _684 = _683 + _671;
      float _685 = _684 + _675;
      float _686 = c_specular_scale.y * _665;
      float _687 = _686 * _578;
      float _688 = _648 + (c_ps_user[13].x);
      float _689 = _688 + _652;
      float _690 = c_diffuse_scale.y * _689;
      float _691 = _690 * c_mat_diffuse.y;
      float _692 = _672 * c_specular_scale.y;
      float _693 = _692 + _687;
      float _694 = _693 * c_mat_specular.y;
      float _695 = _620 + _594;
      float _696 = _695 + _607;
      float _697 = _696 + _623;
      float _698 = _697 + _625;
      float _699 = _698 + _643;
      float _700 = _699 + _656;
      float _701 = _700 + _663;
      float _702 = _701 + _640;
      float _703 = _702 + _691;
      float _704 = _703 + _694;
      float _705 = c_specular_scale.z * _665;
      float _706 = _705 * _579;
      float _707 = _649 + (c_ps_user[13].x);
      float _708 = _707 + _653;
      float _709 = c_diffuse_scale.z * _708;
      float _710 = _709 * c_mat_diffuse.z;
      float _711 = _672 * c_specular_scale.z;
      float _712 = _711 + _706;
      float _713 = _712 * c_mat_specular.z;
      float _714 = _620 + _595;
      float _715 = _714 + _608;
      float _716 = _715 + _623;
      float _717 = _716 + _625;
      float _718 = _717 + _644;
      float _719 = _718 + _657;
      float _720 = _719 + _664;
      float _721 = _720 + _640;
      float _722 = _721 + _710;
      float _723 = _722 + _713;
      float _724 = saturate(_685);
      float _725 = saturate(_704);
      float _726 = saturate(_723);
      _747 = _724;
      _748 = _725;
      _749 = _726;
   }
   else
   {
      float _728 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _729 = _728 * _159;
      float _730 = _729 * _557;
      float _731 = _730 * _576;
      float _732 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _733 = _732 * _160;
      float _734 = _733 * _562;
      float _735 = _734 * _576;
      float _736 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _737 = _736 * _161;
      float _738 = _737 * _567;
      float _739 = _738 * _576;
      float _740 = _577 * _54.x;
      float _741 = _578 * _54.x;
      float _742 = _579 * _54.x;
      float _743 = _731 + _740;
      float _744 = _735 + _741;
      float _745 = _739 + _742;
      _747 = _743;
      _748 = _744;
      _749 = _745;
   }
   float _754 = c_mat_opacity * _59.w;
   float _755 = _754 * c_opacity_scale;
   float _756 = _755 * c_global_opacity;
   float _757 = _747 * COLOR.x;
   float _758 = _748 * COLOR.y;
   float _759 = _749 * COLOR.z;
   int _760 = c_bool & 16;
   bool _761 = (_760 == 0);
   if (!_761)
   {
      float _765 = c_tone_control.x * _757;
      float _766 = c_tone_control.x * _758;
      float _767 = c_tone_control.x * _759;
      float _769 = log2(_765);
      float _770 = log2(_766);
      float _771 = log2(_767);
      float _772 = _769 * c_tone_control.y;
      float _773 = _770 * c_tone_control.y;
      float _774 = _771 * c_tone_control.y;
      float _775 = exp2(_772);
      float _776 = exp2(_773);
      float _777 = exp2(_774);
      _825 = _775;
      _826 = _776;
      _827 = _777;
   }
   else
   {
      if (_581)
      {
         float _782 = c_tone_control.x * _757;
         float _783 = c_tone_control.x * _758;
         float _784 = c_tone_control.x * _759;
         float _785 = _782 * 2.0f;
         float _786 = _783 * 2.0f;
         float _787 = _784 * 2.0f;
         float _788 = _782 * 0.30000001192092896f;
         float _789 = _783 * 0.30000001192092896f;
         float _790 = _784 * 0.30000001192092896f;
         float _791 = _788 + 0.05000000074505806f;
         float _792 = _789 + 0.05000000074505806f;
         float _793 = _790 + 0.05000000074505806f;
         float _794 = _791 * _785;
         float _795 = _792 * _786;
         float _796 = _793 * _787;
         float _797 = _794 + 0.004000000189989805f;
         float _798 = _795 + 0.004000000189989805f;
         float _799 = _796 + 0.004000000189989805f;
         float _800 = _788 + 0.5f;
         float _801 = _789 + 0.5f;
         float _802 = _790 + 0.5f;
         float _803 = _800 * _785;
         float _804 = _801 * _786;
         float _805 = _802 * _787;
         float _806 = _803 + 0.06000000238418579f;
         float _807 = _804 + 0.06000000238418579f;
         float _808 = _805 + 0.06000000238418579f;
         float _809 = _797 / _806;
         float _810 = _798 / _807;
         float _811 = _799 / _808;
         float _812 = _809 + -0.06666666269302368f;
         float _813 = _810 + -0.06666666269302368f;
         float _814 = _811 + -0.06666666269302368f;
         float _815 = _812 * 1.3790643215179443f;
         float _816 = _813 * 1.3790643215179443f;
         float _817 = _814 * 1.3790643215179443f;
         float _818 = YKR_MaterialCurve(_815, _782);
         float _819 = YKR_MaterialCurve(_816, _783);
         float _820 = YKR_MaterialCurve(_817, _784);
         float _821 = sqrt(_818);
         float _822 = sqrt(_819);
         float _823 = sqrt(_820);
         _825 = _821;
         _826 = _822;
         _827 = _823;
      }
      else
      {
         _825 = _757;
         _826 = _758;
         _827 = _759;
      }
   }
   int _828 = c_bool & 4;
   bool _829 = (_828 == 0);
   if (_829)
   {
      float _831 = saturate(COLOR_1.x);
      float _836 = (c_fog_param[0].x) - _825;
      float _837 = (c_fog_param[0].y) - _826;
      float _838 = (c_fog_param[0].z) - _827;
      float _839 = _836 * _831;
      float _840 = _837 * _831;
      float _841 = _838 * _831;
      _843 = _839;
      _844 = _840;
      _845 = _841;
   }
   else
   {
      _843 = COLOR_1.x;
      _844 = COLOR_1.y;
      _845 = COLOR_1.z;
   }
   float _846 = _843 + _825;
   float _847 = _844 + _826;
   float _848 = _845 + _827;
   if (!_581)
   {
      float _856 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _857 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _858 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _860 = (c_ps_user[12].w) * COLOR.x;
      float _861 = (c_ps_user[12].w) * COLOR.y;
      float _862 = (c_ps_user[12].w) * COLOR.z;
      float _863 = _860 + _846;
      float _864 = _863 + _856;
      float _865 = _861 + _847;
      float _866 = _865 + _857;
      float _867 = _862 + _848;
      float _868 = _867 + _858;
      _870 = _864;
      _871 = _866;
      _872 = _868;
   }
   else
   {
      _870 = _846;
      _871 = _847;
      _872 = _848;
   }
   float _875 = _756 - c_mat_punchthrough_ref;
   bool _876 = (_875 < 0.0f);
   if (_876)
      discard;
   float _882 = (c_ps_col_scl[0].x) * _870;
   float _883 = (c_ps_col_scl[0].y) * _871;
   float _884 = (c_ps_col_scl[0].z) * _872;
   float _885 = (c_ps_col_scl[0].w) * _756;
   SV_Target.x = _882;
   SV_Target.y = _883;
   SV_Target.z = _884;
   SV_Target.w = _885;
   int _888 = c_bool & 8;
   bool _889 = (_888 != 0);
   float _890 = select(_889, _870, _882);
   float _891 = select(_889, _871, _883);
   float _892 = select(_889, _872, _884);
   float _893 = select(_889, _756, _885);
   SV_Target.x = _890;
   SV_Target.y = _891;
   SV_Target.z = _892;
   SV_Target.w = _893;
   return SV_Target;
}
