// Yakuza Kiwami material s_o1dzt[aref] (0x24592396, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
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
   float _162 = _110 + 0.10000000149011612f;
   float _163 = saturate(_162);
   float _166 = (c_attribute_ps[2].w) + 2.0f;
   float _168 = (c_attribute_ps[2].z) + 2.5f;
   float _169 = -0.0f - _118;
   float _170 = -0.0f - _119;
   float _171 = -0.0f - _120;
   float _172 = dot(float3(_169, _170, _171), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _173 = 1.0f - _172;
   float _174 = saturate(_173);
   float _175 = dot(float3(_169, _170, _171), float3(_109, _110, _111));
   float _176 = 1.0f - _175;
   float _177 = saturate(_176);
   float _178 = _177 * _174;
   float _179 = log2(_178);
   float _180 = _179 * _166;
   float _181 = exp2(_180);
   float _182 = _181 * _168;
   int _185 = c_bool & 4096;
   bool _186 = (_185 == 0);
   float _195;
   float _219;
   float _220;
   float _232;
   float _233;
   float _413;
   float _414;
   float _415;
   float _416;
   float _417;
   float _418;
   float _451;
   float _452;
   float _453;
   float _454;
   float _619;
   float _785;
   float _786;
   float _787;
   float _863;
   float _864;
   float _865;
   float _881;
   float _882;
   float _883;
   float _908;
   float _909;
   float _910;
   if (!_186)
   {
      float _191 = (c_ps_user[9].w) * _179;
      float _192 = exp2(_191);
      float _193 = _192 * (c_ps_user[9].z);
      _195 = _193;
   }
   else
   {
      _195 = _182;
   }
   float _196 = _163 * _195;
   float _197 = _54.y * _196;
   float _200 = _197 * (c_ps_user[2].x);
   float _201 = _200 + 1.0f;
   float _202 = _201 * _159;
   float _203 = _201 * _160;
   float _204 = _201 * _161;
   float _208 = c_rt_size.z * SV_Position.x;
   float _209 = c_rt_size.w * SV_Position.y;
   int _212 = c_bool & 8;
   bool _213 = (_212 == 0);
   if (!_213)
   {
      float4 _215 = s13_t.Sample(s13_s, float2(_208, _209));
      _219 = _215.x;
      _220 = _215.y;
   }
   else
   {
      _219 = 1.0f;
      _220 = 1.0f;
   }
   if (!_213)
   {
      float _225 = (c_sdw_param_ch[0].x) * _219;
      float _226 = (c_sdw_param_ch[0].z) * _220;
      float _229 = _225 + (c_sdw_param_ch[0].y);
      float _230 = _226 + (c_sdw_param_ch[0].w);
      _232 = _229;
      _233 = _230;
   }
   else
   {
      _232 = _219;
      _233 = _220;
   }
   float _234 = min(_232, _233);
   float _243 = (c_light_vec[1].x) + _118;
   float _244 = (c_light_vec[1].y) + _119;
   float _245 = (c_light_vec[1].z) + _120;
   float _246 = dot(float3(_243, _244, _245), float3(_243, _244, _245));
   float _247 = rsqrt(_246);
   float _248 = _243 * _247;
   float _249 = _244 * _247;
   float _250 = _245 * _247;
   float _251 = -0.0f - (c_light_vec[1].x);
   float _252 = -0.0f - (c_light_vec[1].y);
   float _253 = -0.0f - (c_light_vec[1].z);
   float _254 = -0.0f - (c_light_vec[0].x);
   float _255 = -0.0f - (c_light_vec[0].y);
   float _256 = -0.0f - (c_light_vec[0].z);
   float _257 = dot(float3(_134, _135, _136), float3(_254, _255, _256));
   float _258 = dot(float3(_134, _135, _136), float3(_251, _252, _253));
   float _259 = saturate(_257);
   float _260 = saturate(_258);
   float _261 = log2(_259);
   float _262 = log2(_260);
   float _263 = _261 * _73;
   float _264 = _262 * _73;
   float _265 = exp2(_263);
   float _266 = exp2(_264);
   float _271 = c_mat_fresnel * c_tone_control.w;
   float _272 = _73 * 0.03974359855055809f;
   float _273 = _272 + 0.0856831967830658f;
   float _274 = _266 * _273;
   float _275 = dot(float3(_118, _119, _120), float3(_126, _127, _128));
   float _276 = dot(float3(_118, _119, _120), float3(_248, _249, _250));
   float _277 = 1.0f - _275;
   float _278 = 1.0f - _276;
   float _279 = _277 * _277;
   float _280 = _278 * _278;
   float _281 = 1.0f - _271;
   float _282 = _279 * _279;
   float _283 = _282 * _277;
   float _284 = _283 * _281;
   float _285 = _278 * _281;
   float _286 = _280 * _280;
   float _287 = _286 * _285;
   float _288 = _284 + _271;
   float _289 = _287 + _271;
   float _290 = dot(float3(_109, _110, _111), float3(_254, _255, _256));
   float _291 = saturate(_290);
   float _292 = dot(float3(_109, _110, _111), float3(_251, _252, _253));
   float _293 = saturate(_292);
   float _294 = _274 * _293;
   float _295 = _294 * _289;
   float _304 = c_specular_scale.x * c_mat_specular.x;
   float _305 = c_specular_scale.y * c_mat_specular.y;
   float _306 = c_specular_scale.z * c_mat_specular.z;
   float _311 = _234 * _273;
   float _312 = _311 * _265;
   float _313 = _312 * _291;
   float _314 = _313 * _288;
   float _315 = (c_light_col[0].x) * _314;
   float _316 = (c_light_col[0].y) * _314;
   float _317 = (c_light_col[0].z) * _314;
   float _322 = (c_light_col[1].x) * _295;
   float _323 = (c_light_col[1].y) * _295;
   float _324 = (c_light_col[1].z) * _295;
   float _325 = _322 + _315;
   float _326 = _323 + _316;
   float _327 = _324 + _317;
   float _328 = _304 * _325;
   float _329 = _305 * _326;
   float _330 = _306 * _327;
   int _334 = c_bool & 1;
   bool _335 = (_334 == 0);
   if (!_335)
   {
      float _337 = -0.0f - TEXCOORD_4.x;
      float _338 = -0.0f - TEXCOORD_4.y;
      float _339 = -0.0f - TEXCOORD_4.z;
      float _340 = dot(float3(_109, _110, _111), float3(_337, _338, _339));
      float _341 = saturate(_340);
      float _342 = _341 * TEXCOORD_1.w;
      float _343 = _341 * TEXCOORD_2.w;
      float _344 = _341 * TEXCOORD_4.w;
      float _345 = max(0.0f, _342);
      float _346 = max(0.0f, _343);
      float _347 = max(0.0f, _344);
      float _348 = -0.0f - TEXCOORD_5.x;
      float _349 = -0.0f - TEXCOORD_5.y;
      float _350 = -0.0f - TEXCOORD_5.z;
      float _351 = dot(float3(_109, _110, _111), float3(_348, _349, _350));
      float _352 = saturate(_351);
      float _353 = _352 * COLOR.w;
      float _354 = _352 * COLOR_1.w;
      float _355 = _352 * TEXCOORD_5.w;
      float _356 = max(0.0f, _353);
      float _357 = max(0.0f, _354);
      float _358 = max(0.0f, _355);
      float _359 = _356 + _345;
      float _360 = _357 + _346;
      float _361 = _358 + _347;
      float _362 = _118 + TEXCOORD_4.x;
      float _363 = _119 + TEXCOORD_4.y;
      float _364 = _120 + TEXCOORD_4.z;
      float _365 = dot(float3(_362, _363, _364), float3(_362, _363, _364));
      float _366 = rsqrt(_365);
      float _367 = _118 + TEXCOORD_5.x;
      float _368 = _119 + TEXCOORD_5.y;
      float _369 = _120 + TEXCOORD_5.z;
      float _370 = dot(float3(_367, _368, _369), float3(_367, _368, _369));
      float _371 = rsqrt(_370);
      float _372 = _367 * _371;
      float _373 = -0.0f - _372;
      float _374 = _368 * _371;
      float _375 = -0.0f - _374;
      float _376 = _369 * _371;
      float _377 = -0.0f - _376;
      float _378 = _362 * _366;
      float _379 = -0.0f - _378;
      float _380 = _363 * _366;
      float _381 = -0.0f - _380;
      float _382 = _364 * _366;
      float _383 = -0.0f - _382;
      float _384 = dot(float3(_379, _381, _383), float3(_109, _110, _111));
      float _385 = dot(float3(_373, _375, _377), float3(_109, _110, _111));
      float _386 = saturate(_384);
      float _387 = saturate(_385);
      float _388 = log2(_386);
      float _389 = log2(_387);
      float _390 = _388 * _73;
      float _391 = _389 * _73;
      float _392 = exp2(_390);
      float _393 = exp2(_391);
      float _394 = _392 * TEXCOORD_1.w;
      float _395 = _392 * TEXCOORD_2.w;
      float _396 = _392 * TEXCOORD_4.w;
      float _397 = _393 * COLOR.w;
      float _398 = _393 * COLOR_1.w;
      float _399 = _393 * TEXCOORD_5.w;
      float _400 = _397 + _394;
      float _401 = _398 + _395;
      float _402 = _399 + _396;
      float _403 = max(0.0f, _400);
      float _404 = max(0.0f, _401);
      float _405 = max(0.0f, _402);
      float _406 = _304 * _403;
      float _407 = _305 * _404;
      float _408 = _306 * _405;
      float _409 = min(_406, 1.0f);
      float _410 = min(_407, 1.0f);
      float _411 = min(_408, 1.0f);
      _413 = _409;
      _414 = _410;
      _415 = _411;
      _416 = _359;
      _417 = _360;
      _418 = _361;
   }
   else
   {
      _413 = 0.0f;
      _414 = 0.0f;
      _415 = 0.0f;
      _416 = 0.0f;
      _417 = 0.0f;
      _418 = 0.0f;
   }
   float _419 = c_rt_size.z * SV_Position.x;
   float _420 = c_rt_size.w * SV_Position.y;
   float4 _421 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_419, _420)));
   int _428 = c_bool & 16384;
   bool _429 = (_428 == 0);
   if (!_429)
   {
      float _431 = _421.x * 4.0f;
      float _432 = _421.y * 4.0f;
      float _433 = _421.z * 4.0f;
      float _434 = _421.w * 4.0f;
      _451 = _431;
      _452 = _432;
      _453 = _433;
      _454 = _434;
   }
   else
   {
      int _436 = c_bool & 8192;
      bool _437 = (_436 == 0);
      if (!_437)
      {
         float _439 = _421.x * 2.0f;
         float _440 = _421.y * 2.0f;
         float _441 = _421.z * 2.0f;
         float _442 = _421.w * 2.0f;
         _451 = _439;
         _452 = _440;
         _453 = _441;
         _454 = _442;
      }
      else
      {
         float _446 = _421.x / c_tone_control.z;
         float _447 = _421.y / c_tone_control.z;
         float _448 = _421.z / c_tone_control.z;
         float _449 = _421.w / c_tone_control.z;
         _451 = _446;
         _452 = _447;
         _453 = _448;
         _454 = _449;
      }
   }
   float _455 = _451 + _416;
   float _456 = _452 + _417;
   float _457 = _453 + _418;
   float _458 = _454 + _413;
   float _459 = _454 + _414;
   float _460 = _454 + _415;
   float _523 = dot(float3(_109, _110, _111), float3(_109, _110, _111));
   float _524 = rsqrt(_523);
   float _525 = _524 * _109;
   float _526 = _524 * _110;
   float _527 = _524 * _111;
   float _528 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_525, _526, _527, 1.0f));
   float _529 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_525, _526, _527, 1.0f));
   float _530 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_525, _526, _527, 1.0f));
   float _531 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_525, _526, _527, 1.0f));
   float _532 = dot(float4(_525, _526, _527, 1.0f), float4(_528, _529, _530, _531));
   float _533 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_525, _526, _527, 1.0f));
   float _534 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_525, _526, _527, 1.0f));
   float _535 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_525, _526, _527, 1.0f));
   float _536 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_525, _526, _527, 1.0f));
   float _537 = dot(float4(_525, _526, _527, 1.0f), float4(_533, _534, _535, _536));
   float _538 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_525, _526, _527, 1.0f));
   float _539 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_525, _526, _527, 1.0f));
   float _540 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_525, _526, _527, 1.0f));
   float _541 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_525, _526, _527, 1.0f));
   float _542 = dot(float4(_525, _526, _527, 1.0f), float4(_538, _539, _540, _541));
   float _543 = max(_532, 0.0625f);
   float _544 = max(_537, 0.0625f);
   float _545 = max(_542, 0.0625f);
   float _546 = _543 * (c_ps_scratch[79].x);
   float _547 = _544 * (c_ps_scratch[79].x);
   float _548 = _545 * (c_ps_scratch[79].x);
   float _553 = -0.0f - (c_light_vec[0].x);
   float _554 = -0.0f - (c_light_vec[0].y);
   float _555 = -0.0f - (c_light_vec[0].z);
   float _556 = dot(float3(_109, _110, _111), float3(_553, _554, _555));
   float _557 = -0.0f - (c_light_vec[1].x);
   float _558 = -0.0f - (c_light_vec[1].y);
   float _559 = -0.0f - (c_light_vec[1].z);
   float _560 = dot(float3(_109, _110, _111), float3(_557, _558, _559));
   float _569 = _110 * 0.5f;
   float _570 = _569 + 0.5f;
   float _571 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _572 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _573 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _574 = _571 * _570;
   float _575 = _572 * _570;
   float _576 = _573 * _570;
   float _577 = saturate(_556);
   float _578 = saturate(_560);
   float _583 = (c_light_col[0].x) * _577;
   float _584 = (c_light_col[0].y) * _577;
   float _585 = (c_light_col[0].z) * _577;
   float _590 = (c_light_col[1].x) * _578;
   float _591 = (c_light_col[1].y) * _578;
   float _592 = (c_light_col[1].z) * _578;
   float _593 = _583 * _234;
   float _594 = _584 * _234;
   float _595 = _585 * _234;
   float _596 = (c_light_vec[4].x) + _546;
   float _597 = _596 + _574;
   float _598 = _597 + _455;
   float _599 = _598 + _590;
   float _600 = _599 + _593;
   float _601 = (c_light_vec[4].y) + _547;
   float _602 = _601 + _575;
   float _603 = _602 + _456;
   float _604 = _603 + _591;
   float _605 = _604 + _594;
   float _606 = (c_light_vec[4].z) + _548;
   float _607 = _606 + _576;
   float _608 = _607 + _457;
   float _609 = _608 + _592;
   float _610 = _609 + _595;
   int _611 = c_bool & 64;
   bool _612 = (_611 == 0);
   if (!_612)
   {
      float _614 = 1.0f - _54.y;
      float _615 = _234 * _614;
      float _616 = _615 * _577;
      float _617 = _616 + _54.y;
      _619 = _617;
   }
   else
   {
      _619 = _54.y;
   }
   float _620 = _458 + _328;
   float _621 = _459 + _329;
   float _622 = _460 + _330;
   int _623 = c_bool & 32768;
   bool _624 = (_623 == 0);
   if (!_624)
   {
      float _636 = (c_ps_user[11].x) * _202;
      float _637 = (c_ps_user[11].x) * _203;
      float _638 = (c_ps_user[11].x) * _204;
      float _640 = _109 + 1.0f;
      float _641 = _110 + 1.0f;
      float _642 = _111 + 1.0f;
      float _643 = _640 * 0.5f;
      float _644 = _641 * 0.5f;
      float _645 = _642 * 0.5f;
      float _646 = saturate(_643);
      float _647 = saturate(_644);
      float _648 = saturate(_645);
      float _649 = (c_ps_user[11].y) * _646;
      float _650 = _647 * (c_ps_user[11].y);
      float _651 = _648 * (c_ps_user[11].y);
      float _661 = (c_ps_user[11].z) * _54.x;
      float _663 = (c_ps_user[11].w) * _620;
      float _666 = (c_ps_user[12].x) * _234;
      float _668 = (c_ps_user[12].y) * _54.y;
      float _673 = _176 * _176;
      float _674 = _176 * 0.9722222089767456f;
      float _675 = _673 * _673;
      float _676 = _675 * _674;
      float _677 = _676 + 0.02777777425944805f;
      float _678 = (c_ps_user[13].z) * _677;
      float _680 = (c_ps_user[13].w) * _597;
      float _681 = (c_ps_user[13].w) * _602;
      float _682 = (c_ps_user[13].w) * _607;
      float _685 = (c_ps_user[14].y) * _583;
      float _686 = (c_ps_user[14].y) * _584;
      float _687 = (c_ps_user[14].y) * _585;
      float _689 = (c_ps_user[14].z) * _590;
      float _690 = (c_ps_user[14].z) * _591;
      float _691 = (c_ps_user[14].z) * _592;
      float _693 = (c_ps_user[14].w) * _455;
      float _694 = (c_ps_user[14].w) * _456;
      float _695 = (c_ps_user[14].w) * _457;
      float _700 = (c_ps_user[15].z) * _458;
      float _701 = (c_ps_user[15].z) * _459;
      float _702 = (c_ps_user[15].z) * _460;
      float _703 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _704 = c_specular_scale.x * _703;
      float _705 = _704 * _620;
      float _706 = _685 + (c_ps_user[13].x);
      float _707 = _706 + _689;
      float _708 = c_diffuse_scale.x * _707;
      float _709 = _708 * c_mat_diffuse.x;
      float _710 = (c_ps_user[13].y) + _661;
      float _711 = _710 * c_specular_scale.x;
      float _712 = _711 + _705;
      float _713 = _712 * c_mat_specular.x;
      float _714 = _649 + _636;
      float _715 = _714 + _663;
      float _716 = _715 + _666;
      float _717 = _716 + _668;
      float _718 = _717 + _680;
      float _719 = _718 + _678;
      float _720 = _719 + _693;
      float _721 = _720 + _700;
      float _722 = _721 + _709;
      float _723 = _722 + _713;
      float _724 = c_specular_scale.y * _703;
      float _725 = _724 * _621;
      float _726 = _686 + (c_ps_user[13].x);
      float _727 = _726 + _690;
      float _728 = c_diffuse_scale.y * _727;
      float _729 = _728 * c_mat_diffuse.y;
      float _730 = _710 * c_specular_scale.y;
      float _731 = _730 + _725;
      float _732 = _731 * c_mat_specular.y;
      float _733 = _663 + _637;
      float _734 = _733 + _650;
      float _735 = _734 + _666;
      float _736 = _735 + _668;
      float _737 = _736 + _681;
      float _738 = _737 + _678;
      float _739 = _738 + _694;
      float _740 = _739 + _701;
      float _741 = _740 + _729;
      float _742 = _741 + _732;
      float _743 = c_specular_scale.z * _703;
      float _744 = _743 * _622;
      float _745 = _687 + (c_ps_user[13].x);
      float _746 = _745 + _691;
      float _747 = c_diffuse_scale.z * _746;
      float _748 = _747 * c_mat_diffuse.z;
      float _749 = _710 * c_specular_scale.z;
      float _750 = _749 + _744;
      float _751 = _750 * c_mat_specular.z;
      float _752 = _663 + _638;
      float _753 = _752 + _651;
      float _754 = _753 + _666;
      float _755 = _754 + _668;
      float _756 = _755 + _682;
      float _757 = _756 + _678;
      float _758 = _757 + _695;
      float _759 = _758 + _702;
      float _760 = _759 + _748;
      float _761 = _760 + _751;
      float _762 = saturate(_723);
      float _763 = saturate(_742);
      float _764 = saturate(_761);
      _785 = _762;
      _786 = _763;
      _787 = _764;
   }
   else
   {
      float _766 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _767 = _766 * _202;
      float _768 = _767 * _600;
      float _769 = _768 * _619;
      float _770 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _771 = _770 * _203;
      float _772 = _771 * _605;
      float _773 = _772 * _619;
      float _774 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _775 = _774 * _204;
      float _776 = _775 * _610;
      float _777 = _776 * _619;
      float _778 = _620 * _54.x;
      float _779 = _621 * _54.x;
      float _780 = _622 * _54.x;
      float _781 = _769 + _778;
      float _782 = _773 + _779;
      float _783 = _777 + _780;
      _785 = _781;
      _786 = _782;
      _787 = _783;
   }
   float _792 = c_mat_opacity * _59.w;
   float _793 = _792 * c_opacity_scale;
   float _794 = _793 * c_global_opacity;
   float _795 = _785 * COLOR.x;
   float _796 = _786 * COLOR.y;
   float _797 = _787 * COLOR.z;
   int _798 = c_bool & 16;
   bool _799 = (_798 == 0);
   if (!_799)
   {
      float _803 = c_tone_control.x * _795;
      float _804 = c_tone_control.x * _796;
      float _805 = c_tone_control.x * _797;
      float _807 = log2(_803);
      float _808 = log2(_804);
      float _809 = log2(_805);
      float _810 = _807 * c_tone_control.y;
      float _811 = _808 * c_tone_control.y;
      float _812 = _809 * c_tone_control.y;
      float _813 = exp2(_810);
      float _814 = exp2(_811);
      float _815 = exp2(_812);
      _863 = _813;
      _864 = _814;
      _865 = _815;
   }
   else
   {
      if (_624)
      {
         float _820 = c_tone_control.x * _795;
         float _821 = c_tone_control.x * _796;
         float _822 = c_tone_control.x * _797;
         float _823 = _820 * 2.0f;
         float _824 = _821 * 2.0f;
         float _825 = _822 * 2.0f;
         float _826 = _820 * 0.30000001192092896f;
         float _827 = _821 * 0.30000001192092896f;
         float _828 = _822 * 0.30000001192092896f;
         float _829 = _826 + 0.05000000074505806f;
         float _830 = _827 + 0.05000000074505806f;
         float _831 = _828 + 0.05000000074505806f;
         float _832 = _829 * _823;
         float _833 = _830 * _824;
         float _834 = _831 * _825;
         float _835 = _832 + 0.004000000189989805f;
         float _836 = _833 + 0.004000000189989805f;
         float _837 = _834 + 0.004000000189989805f;
         float _838 = _826 + 0.5f;
         float _839 = _827 + 0.5f;
         float _840 = _828 + 0.5f;
         float _841 = _838 * _823;
         float _842 = _839 * _824;
         float _843 = _840 * _825;
         float _844 = _841 + 0.06000000238418579f;
         float _845 = _842 + 0.06000000238418579f;
         float _846 = _843 + 0.06000000238418579f;
         float _847 = _835 / _844;
         float _848 = _836 / _845;
         float _849 = _837 / _846;
         float _850 = _847 + -0.06666666269302368f;
         float _851 = _848 + -0.06666666269302368f;
         float _852 = _849 + -0.06666666269302368f;
         float _853 = _850 * 1.3790643215179443f;
         float _854 = _851 * 1.3790643215179443f;
         float _855 = _852 * 1.3790643215179443f;
         float _856 = YKR_MaterialCurve(_853, _820);
         float _857 = YKR_MaterialCurve(_854, _821);
         float _858 = YKR_MaterialCurve(_855, _822);
         float _859 = sqrt(_856);
         float _860 = sqrt(_857);
         float _861 = sqrt(_858);
         _863 = _859;
         _864 = _860;
         _865 = _861;
      }
      else
      {
         _863 = _795;
         _864 = _796;
         _865 = _797;
      }
   }
   int _866 = c_bool & 4;
   bool _867 = (_866 == 0);
   if (_867)
   {
      float _869 = saturate(COLOR_1.x);
      float _874 = (c_fog_param[0].x) - _863;
      float _875 = (c_fog_param[0].y) - _864;
      float _876 = (c_fog_param[0].z) - _865;
      float _877 = _874 * _869;
      float _878 = _875 * _869;
      float _879 = _876 * _869;
      _881 = _877;
      _882 = _878;
      _883 = _879;
   }
   else
   {
      _881 = COLOR_1.x;
      _882 = COLOR_1.y;
      _883 = COLOR_1.z;
   }
   float _884 = _881 + _863;
   float _885 = _882 + _864;
   float _886 = _883 + _865;
   if (!_624)
   {
      float _894 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _895 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _896 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _898 = (c_ps_user[12].w) * COLOR.x;
      float _899 = (c_ps_user[12].w) * COLOR.y;
      float _900 = (c_ps_user[12].w) * COLOR.z;
      float _901 = _898 + _884;
      float _902 = _901 + _894;
      float _903 = _899 + _885;
      float _904 = _903 + _895;
      float _905 = _900 + _886;
      float _906 = _905 + _896;
      _908 = _902;
      _909 = _904;
      _910 = _906;
   }
   else
   {
      _908 = _884;
      _909 = _885;
      _910 = _886;
   }
   float _916 = (c_ps_col_scl[0].x) * _908;
   float _917 = (c_ps_col_scl[0].y) * _909;
   float _918 = (c_ps_col_scl[0].z) * _910;
   float _919 = (c_ps_col_scl[0].w) * _794;
   SV_Target.x = _916;
   SV_Target.y = _917;
   SV_Target.z = _918;
   SV_Target.w = _919;
   int _922 = c_bool & 8;
   bool _923 = (_922 != 0);
   float _924 = select(_923, _908, _916);
   float _925 = select(_923, _909, _917);
   float _926 = select(_923, _910, _918);
   float _927 = select(_923, _794, _919);
   SV_Target.x = _924;
   SV_Target.y = _925;
   SV_Target.z = _926;
   SV_Target.w = _927;
   return SV_Target;
}
