// Yakuza Kiwami material s_o1dst (0x3FB76E49, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s3_t : register(t3);

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

SamplerState s3_s : register(s3);

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
   float4 _52 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _57 = _52.x * _52.x;
   float _58 = _52.y * _52.y;
   float _59 = _52.z * _52.z;
   float4 _60 = s3_t.Sample(s3_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _64 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _69 = _64.x * 2.0f;
   float _70 = _64.y * 2.0f;
   float _71 = _69 + -1.0f;
   float _72 = _70 + -1.0f;
   float _73 = dot(float2(_71, _72), float2(_71, _72));
   float _74 = 1.0f - _73;
   float _75 = saturate(_74);
   float _76 = sqrt(_75);
   float _77 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _78 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _79 = _77 - _78;
   float _80 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _81 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _82 = _80 - _81;
   float _83 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _84 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _85 = _83 - _84;
   float _86 = _71 * TEXCOORD_3.x;
   float _87 = _71 * TEXCOORD_3.y;
   float _88 = _71 * TEXCOORD_3.z;
   float _89 = _72 * TEXCOORD_3.w;
   float _90 = _89 * _79;
   float _91 = _89 * _82;
   float _92 = _89 * _85;
   float _93 = _90 + _86;
   float _94 = _91 + _87;
   float _95 = _92 + _88;
   float _96 = _76 * TEXCOORD_2.x;
   float _97 = _76 * TEXCOORD_2.y;
   float _98 = _76 * TEXCOORD_2.z;
   float _99 = _93 + _96;
   float _100 = _94 + _97;
   float _101 = _95 + _98;
   float _102 = dot(float3(_99, _100, _101), float3(_99, _100, _101));
   float _103 = rsqrt(_102);
   float _104 = _99 * _103;
   float _105 = _100 * _103;
   float _106 = _101 * _103;
   float _111 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _112 = rsqrt(_111);
   float _113 = _112 * TEXCOORD_1.x;
   float _114 = _112 * TEXCOORD_1.y;
   float _115 = _112 * TEXCOORD_1.z;
   float _116 = _113 + (c_light_vec[0].x);
   float _117 = _114 + (c_light_vec[0].y);
   float _118 = _115 + (c_light_vec[0].z);
   float _119 = dot(float3(_116, _117, _118), float3(_116, _117, _118));
   float _120 = rsqrt(_119);
   float _121 = _116 * _120;
   float _122 = _117 * _120;
   float _123 = _118 * _120;
   float _124 = _105 + 0.10000000149011612f;
   float _125 = saturate(_124);
   float _128 = (c_attribute_ps[2].w) + 2.0f;
   float _130 = (c_attribute_ps[2].z) + 2.5f;
   float _131 = -0.0f - _113;
   float _132 = -0.0f - _114;
   float _133 = -0.0f - _115;
   float _134 = dot(float3(_131, _132, _133), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _135 = 1.0f - _134;
   float _136 = saturate(_135);
   float _137 = dot(float3(_131, _132, _133), float3(_104, _105, _106));
   float _138 = 1.0f - _137;
   float _139 = saturate(_138);
   float _140 = _139 * _136;
   float _141 = log2(_140);
   float _142 = _141 * _128;
   float _143 = exp2(_142);
   float _144 = _143 * _130;
   int _147 = c_bool & 4096;
   bool _148 = (_147 == 0);
   float _157;
   float _180;
   float _181;
   float _193;
   float _194;
   float _382;
   float _383;
   float _384;
   float _385;
   float _386;
   float _387;
   float _420;
   float _421;
   float _422;
   float _423;
   float _745;
   float _746;
   float _747;
   float _823;
   float _824;
   float _825;
   float _841;
   float _842;
   float _843;
   float _868;
   float _869;
   float _870;
   if (!_148)
   {
      float _153 = (c_ps_user[9].w) * _141;
      float _154 = exp2(_153);
      float _155 = _154 * (c_ps_user[9].z);
      _157 = _155;
   }
   else
   {
      _157 = _144;
   }
   float _158 = _125 * _157;
   float _161 = (c_ps_user[2].x) * _158;
   float _162 = _161 + 1.0f;
   float _163 = _57 * _162;
   float _164 = _58 * _162;
   float _165 = _59 * _162;
   float _169 = c_rt_size.z * SV_Position.x;
   float _170 = c_rt_size.w * SV_Position.y;
   int _173 = c_bool & 8;
   bool _174 = (_173 == 0);
   if (!_174)
   {
      float4 _176 = s13_t.Sample(s13_s, float2(_169, _170));
      _180 = _176.x;
      _181 = _176.y;
   }
   else
   {
      _180 = 1.0f;
      _181 = 1.0f;
   }
   if (!_174)
   {
      float _186 = (c_sdw_param_ch[0].x) * _180;
      float _187 = (c_sdw_param_ch[0].z) * _181;
      float _190 = _186 + (c_sdw_param_ch[0].y);
      float _191 = _187 + (c_sdw_param_ch[0].w);
      _193 = _190;
      _194 = _191;
   }
   else
   {
      _193 = _180;
      _194 = _181;
   }
   float _195 = min(_193, _194);
   float _204 = (c_light_vec[1].x) + _113;
   float _205 = (c_light_vec[1].y) + _114;
   float _206 = (c_light_vec[1].z) + _115;
   float _207 = dot(float3(_204, _205, _206), float3(_204, _205, _206));
   float _208 = rsqrt(_207);
   float _209 = _204 * _208;
   float _210 = _205 * _208;
   float _211 = _206 * _208;
   float _212 = -0.0f - (c_light_vec[1].x);
   float _213 = -0.0f - (c_light_vec[1].y);
   float _214 = -0.0f - (c_light_vec[1].z);
   float _215 = -0.0f - (c_light_vec[0].x);
   float _216 = -0.0f - (c_light_vec[0].y);
   float _217 = -0.0f - (c_light_vec[0].z);
   float _218 = dot(float3(_113, _114, _115), float3(_104, _105, _106));
   float _219 = _218 * 2.0f;
   float _220 = _219 * _104;
   float _221 = _219 * _105;
   float _222 = _219 * _106;
   float _223 = _113 - _220;
   float _224 = _114 - _221;
   float _225 = _115 - _222;
   float _226 = dot(float3(_223, _224, _225), float3(_215, _216, _217));
   float _227 = dot(float3(_223, _224, _225), float3(_212, _213, _214));
   float _228 = saturate(_226);
   float _229 = saturate(_227);
   float _230 = log2(_228);
   float _231 = log2(_229);
   float _232 = _230 * c_mat_power;
   float _233 = _231 * c_mat_power;
   float _234 = exp2(_232);
   float _235 = exp2(_233);
   float _240 = c_mat_fresnel * c_tone_control.w;
   float _241 = c_mat_power * 0.03974359855055809f;
   float _242 = _241 + 0.0856831967830658f;
   float _243 = _235 * _242;
   float _244 = dot(float3(_113, _114, _115), float3(_121, _122, _123));
   float _245 = dot(float3(_113, _114, _115), float3(_209, _210, _211));
   float _246 = 1.0f - _244;
   float _247 = 1.0f - _245;
   float _248 = _246 * _246;
   float _249 = _247 * _247;
   float _250 = 1.0f - _240;
   float _251 = _248 * _248;
   float _252 = _251 * _246;
   float _253 = _252 * _250;
   float _254 = _247 * _250;
   float _255 = _249 * _249;
   float _256 = _255 * _254;
   float _257 = _253 + _240;
   float _258 = _256 + _240;
   float _259 = dot(float3(_104, _105, _106), float3(_215, _216, _217));
   float _260 = saturate(_259);
   float _261 = dot(float3(_104, _105, _106), float3(_212, _213, _214));
   float _262 = saturate(_261);
   float _263 = _243 * _262;
   float _264 = _263 * _258;
   float _273 = c_specular_scale.x * c_mat_specular.x;
   float _274 = c_specular_scale.y * c_mat_specular.y;
   float _275 = c_specular_scale.z * c_mat_specular.z;
   float _280 = _195 * _242;
   float _281 = _280 * _234;
   float _282 = _281 * _260;
   float _283 = _282 * _257;
   float _284 = (c_light_col[0].x) * _283;
   float _285 = (c_light_col[0].y) * _283;
   float _286 = (c_light_col[0].z) * _283;
   float _291 = (c_light_col[1].x) * _264;
   float _292 = (c_light_col[1].y) * _264;
   float _293 = (c_light_col[1].z) * _264;
   float _294 = _291 + _284;
   float _295 = _292 + _285;
   float _296 = _293 + _286;
   float _297 = _273 * _294;
   float _298 = _274 * _295;
   float _299 = _275 * _296;
   int _303 = c_bool & 1;
   bool _304 = (_303 == 0);
   if (!_304)
   {
      float _306 = -0.0f - TEXCOORD_4.x;
      float _307 = -0.0f - TEXCOORD_4.y;
      float _308 = -0.0f - TEXCOORD_4.z;
      float _309 = dot(float3(_104, _105, _106), float3(_306, _307, _308));
      float _310 = saturate(_309);
      float _311 = _310 * TEXCOORD_1.w;
      float _312 = _310 * TEXCOORD_2.w;
      float _313 = _310 * TEXCOORD_4.w;
      float _314 = max(0.0f, _311);
      float _315 = max(0.0f, _312);
      float _316 = max(0.0f, _313);
      float _317 = -0.0f - TEXCOORD_5.x;
      float _318 = -0.0f - TEXCOORD_5.y;
      float _319 = -0.0f - TEXCOORD_5.z;
      float _320 = dot(float3(_104, _105, _106), float3(_317, _318, _319));
      float _321 = saturate(_320);
      float _322 = _321 * COLOR.w;
      float _323 = _321 * COLOR_1.w;
      float _324 = _321 * TEXCOORD_5.w;
      float _325 = max(0.0f, _322);
      float _326 = max(0.0f, _323);
      float _327 = max(0.0f, _324);
      float _328 = _325 + _314;
      float _329 = _326 + _315;
      float _330 = _327 + _316;
      float _331 = _113 + TEXCOORD_4.x;
      float _332 = _114 + TEXCOORD_4.y;
      float _333 = _115 + TEXCOORD_4.z;
      float _334 = dot(float3(_331, _332, _333), float3(_331, _332, _333));
      float _335 = rsqrt(_334);
      float _336 = _113 + TEXCOORD_5.x;
      float _337 = _114 + TEXCOORD_5.y;
      float _338 = _115 + TEXCOORD_5.z;
      float _339 = dot(float3(_336, _337, _338), float3(_336, _337, _338));
      float _340 = rsqrt(_339);
      float _341 = _336 * _340;
      float _342 = -0.0f - _341;
      float _343 = _337 * _340;
      float _344 = -0.0f - _343;
      float _345 = _338 * _340;
      float _346 = -0.0f - _345;
      float _347 = _331 * _335;
      float _348 = -0.0f - _347;
      float _349 = _332 * _335;
      float _350 = -0.0f - _349;
      float _351 = _333 * _335;
      float _352 = -0.0f - _351;
      float _353 = dot(float3(_348, _350, _352), float3(_104, _105, _106));
      float _354 = dot(float3(_342, _344, _346), float3(_104, _105, _106));
      float _355 = saturate(_353);
      float _356 = saturate(_354);
      float _357 = log2(_355);
      float _358 = log2(_356);
      float _359 = _357 * c_mat_power;
      float _360 = _358 * c_mat_power;
      float _361 = exp2(_359);
      float _362 = exp2(_360);
      float _363 = _361 * TEXCOORD_1.w;
      float _364 = _361 * TEXCOORD_2.w;
      float _365 = _361 * TEXCOORD_4.w;
      float _366 = _362 * COLOR.w;
      float _367 = _362 * COLOR_1.w;
      float _368 = _362 * TEXCOORD_5.w;
      float _369 = _366 + _363;
      float _370 = _367 + _364;
      float _371 = _368 + _365;
      float _372 = max(0.0f, _369);
      float _373 = max(0.0f, _370);
      float _374 = max(0.0f, _371);
      float _375 = _273 * _372;
      float _376 = _274 * _373;
      float _377 = _275 * _374;
      float _378 = min(_375, 1.0f);
      float _379 = min(_376, 1.0f);
      float _380 = min(_377, 1.0f);
      _382 = _378;
      _383 = _379;
      _384 = _380;
      _385 = _328;
      _386 = _329;
      _387 = _330;
   }
   else
   {
      _382 = 0.0f;
      _383 = 0.0f;
      _384 = 0.0f;
      _385 = 0.0f;
      _386 = 0.0f;
      _387 = 0.0f;
   }
   float _388 = c_rt_size.z * SV_Position.x;
   float _389 = c_rt_size.w * SV_Position.y;
   float4 _390 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_388, _389)));
   int _397 = c_bool & 16384;
   bool _398 = (_397 == 0);
   if (!_398)
   {
      float _400 = _390.x * 4.0f;
      float _401 = _390.y * 4.0f;
      float _402 = _390.z * 4.0f;
      float _403 = _390.w * 4.0f;
      _420 = _400;
      _421 = _401;
      _422 = _402;
      _423 = _403;
   }
   else
   {
      int _405 = c_bool & 8192;
      bool _406 = (_405 == 0);
      if (!_406)
      {
         float _408 = _390.x * 2.0f;
         float _409 = _390.y * 2.0f;
         float _410 = _390.z * 2.0f;
         float _411 = _390.w * 2.0f;
         _420 = _408;
         _421 = _409;
         _422 = _410;
         _423 = _411;
      }
      else
      {
         float _415 = _390.x / c_tone_control.z;
         float _416 = _390.y / c_tone_control.z;
         float _417 = _390.z / c_tone_control.z;
         float _418 = _390.w / c_tone_control.z;
         _420 = _415;
         _421 = _416;
         _422 = _417;
         _423 = _418;
      }
   }
   float _424 = _420 + _385;
   float _425 = _421 + _386;
   float _426 = _422 + _387;
   float _427 = _423 + _382;
   float _428 = _423 + _383;
   float _429 = _423 + _384;
   float _492 = dot(float3(_104, _105, _106), float3(_104, _105, _106));
   float _493 = rsqrt(_492);
   float _494 = _493 * _104;
   float _495 = _493 * _105;
   float _496 = _493 * _106;
   float _497 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_494, _495, _496, 1.0f));
   float _498 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_494, _495, _496, 1.0f));
   float _499 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_494, _495, _496, 1.0f));
   float _500 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_494, _495, _496, 1.0f));
   float _501 = dot(float4(_494, _495, _496, 1.0f), float4(_497, _498, _499, _500));
   float _502 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_494, _495, _496, 1.0f));
   float _503 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_494, _495, _496, 1.0f));
   float _504 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_494, _495, _496, 1.0f));
   float _505 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_494, _495, _496, 1.0f));
   float _506 = dot(float4(_494, _495, _496, 1.0f), float4(_502, _503, _504, _505));
   float _507 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_494, _495, _496, 1.0f));
   float _508 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_494, _495, _496, 1.0f));
   float _509 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_494, _495, _496, 1.0f));
   float _510 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_494, _495, _496, 1.0f));
   float _511 = dot(float4(_494, _495, _496, 1.0f), float4(_507, _508, _509, _510));
   float _512 = max(_501, 0.0625f);
   float _513 = max(_506, 0.0625f);
   float _514 = max(_511, 0.0625f);
   float _515 = _512 * (c_ps_scratch[79].x);
   float _516 = _513 * (c_ps_scratch[79].x);
   float _517 = _514 * (c_ps_scratch[79].x);
   float _522 = -0.0f - (c_light_vec[0].x);
   float _523 = -0.0f - (c_light_vec[0].y);
   float _524 = -0.0f - (c_light_vec[0].z);
   float _525 = dot(float3(_104, _105, _106), float3(_522, _523, _524));
   float _526 = -0.0f - (c_light_vec[1].x);
   float _527 = -0.0f - (c_light_vec[1].y);
   float _528 = -0.0f - (c_light_vec[1].z);
   float _529 = dot(float3(_104, _105, _106), float3(_526, _527, _528));
   float _538 = _105 * 0.5f;
   float _539 = _538 + 0.5f;
   float _540 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _541 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _542 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _543 = _540 * _539;
   float _544 = _541 * _539;
   float _545 = _542 * _539;
   float _546 = saturate(_525);
   float _547 = saturate(_529);
   float _552 = (c_light_col[0].x) * _546;
   float _553 = (c_light_col[0].y) * _546;
   float _554 = (c_light_col[0].z) * _546;
   float _559 = (c_light_col[1].x) * _547;
   float _560 = (c_light_col[1].y) * _547;
   float _561 = (c_light_col[1].z) * _547;
   float _562 = _427 + _297;
   float _563 = _428 + _298;
   float _564 = _429 + _299;
   int _565 = c_bool & 32768;
   bool _566 = (_565 == 0);
   float _575 = (c_light_vec[4].x) + _515;
   float _576 = _575 + _543;
   float _577 = (c_light_vec[4].y) + _516;
   float _578 = _577 + _544;
   float _579 = (c_light_vec[4].z) + _517;
   float _580 = _579 + _545;
   if (!_566)
   {
      float _584 = (c_ps_user[11].x) * _163;
      float _585 = (c_ps_user[11].x) * _164;
      float _586 = (c_ps_user[11].x) * _165;
      float _588 = _104 + 1.0f;
      float _589 = _105 + 1.0f;
      float _590 = _106 + 1.0f;
      float _591 = _588 * 0.5f;
      float _592 = _589 * 0.5f;
      float _593 = _590 * 0.5f;
      float _594 = saturate(_591);
      float _595 = saturate(_592);
      float _596 = saturate(_593);
      float _597 = (c_ps_user[11].y) * _594;
      float _598 = _595 * (c_ps_user[11].y);
      float _599 = _596 * (c_ps_user[11].y);
      float _609 = (c_ps_user[11].z) * _60.x;
      float _610 = (c_ps_user[11].z) * _60.y;
      float _611 = (c_ps_user[11].z) * _60.z;
      float _613 = (c_ps_user[11].w) * _562;
      float _616 = (c_ps_user[12].x) * _195;
      float _622 = _138 * _138;
      float _623 = _138 * 0.9722222089767456f;
      float _624 = _622 * _622;
      float _625 = _624 * _623;
      float _626 = _625 + 0.02777777425944805f;
      float _627 = (c_ps_user[13].z) * _626;
      float _629 = (c_ps_user[13].w) * _576;
      float _630 = (c_ps_user[13].w) * _578;
      float _631 = (c_ps_user[13].w) * _580;
      float _634 = (c_ps_user[14].y) * _552;
      float _635 = (c_ps_user[14].y) * _553;
      float _636 = (c_ps_user[14].y) * _554;
      float _638 = (c_ps_user[14].z) * _559;
      float _639 = (c_ps_user[14].z) * _560;
      float _640 = (c_ps_user[14].z) * _561;
      float _642 = (c_ps_user[14].w) * _424;
      float _643 = (c_ps_user[14].w) * _425;
      float _644 = (c_ps_user[14].w) * _426;
      float _649 = (c_ps_user[15].z) * _427;
      float _650 = (c_ps_user[15].z) * _428;
      float _651 = (c_ps_user[15].z) * _429;
      float _652 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _653 = c_specular_scale.x * _652;
      float _654 = _653 * _562;
      float _655 = _634 + (c_ps_user[13].x);
      float _656 = _655 + _638;
      float _657 = c_diffuse_scale.x * _656;
      float _658 = _657 * c_mat_diffuse.x;
      float _659 = (c_ps_user[13].y) + _609;
      float _660 = _659 * c_specular_scale.x;
      float _661 = _660 + _654;
      float _662 = _661 * c_mat_specular.x;
      float _663 = _597 + _584;
      float _664 = _663 + _613;
      float _665 = _664 + (c_ps_user[12].y);
      float _666 = _665 + _616;
      float _667 = _666 + _629;
      float _668 = _667 + _627;
      float _669 = _668 + _642;
      float _670 = _669 + _649;
      float _671 = _670 + _658;
      float _672 = _671 + _662;
      float _673 = c_specular_scale.y * _652;
      float _674 = _673 * _563;
      float _675 = _635 + (c_ps_user[13].x);
      float _676 = _675 + _639;
      float _677 = c_diffuse_scale.y * _676;
      float _678 = _677 * c_mat_diffuse.y;
      float _679 = (c_ps_user[13].y) + _610;
      float _680 = _679 * c_specular_scale.y;
      float _681 = _680 + _674;
      float _682 = _681 * c_mat_specular.y;
      float _683 = _613 + _585;
      float _684 = _683 + _598;
      float _685 = _684 + (c_ps_user[12].y);
      float _686 = _685 + _616;
      float _687 = _686 + _630;
      float _688 = _687 + _627;
      float _689 = _688 + _643;
      float _690 = _689 + _650;
      float _691 = _690 + _678;
      float _692 = _691 + _682;
      float _693 = c_specular_scale.z * _652;
      float _694 = _693 * _564;
      float _695 = _636 + (c_ps_user[13].x);
      float _696 = _695 + _640;
      float _697 = c_diffuse_scale.z * _696;
      float _698 = _697 * c_mat_diffuse.z;
      float _699 = (c_ps_user[13].y) + _611;
      float _700 = _699 * c_specular_scale.z;
      float _701 = _700 + _694;
      float _702 = _701 * c_mat_specular.z;
      float _703 = _613 + _586;
      float _704 = _703 + _599;
      float _705 = _704 + (c_ps_user[12].y);
      float _706 = _705 + _616;
      float _707 = _706 + _631;
      float _708 = _707 + _627;
      float _709 = _708 + _644;
      float _710 = _709 + _651;
      float _711 = _710 + _698;
      float _712 = _711 + _702;
      float _713 = saturate(_672);
      float _714 = saturate(_692);
      float _715 = saturate(_712);
      _745 = _713;
      _746 = _714;
      _747 = _715;
   }
   else
   {
      float _717 = _554 * _195;
      float _718 = _580 + _426;
      float _719 = _718 + _561;
      float _720 = _719 + _717;
      float _721 = _553 * _195;
      float _722 = _578 + _425;
      float _723 = _722 + _560;
      float _724 = _723 + _721;
      float _725 = _552 * _195;
      float _726 = _576 + _424;
      float _727 = _726 + _559;
      float _728 = _727 + _725;
      float _729 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _730 = _729 * _163;
      float _731 = _730 * _728;
      float _732 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _733 = _732 * _164;
      float _734 = _733 * _724;
      float _735 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _736 = _735 * _165;
      float _737 = _736 * _720;
      float _738 = _562 * _60.x;
      float _739 = _563 * _60.y;
      float _740 = _564 * _60.z;
      float _741 = _731 + _738;
      float _742 = _734 + _739;
      float _743 = _737 + _740;
      _745 = _741;
      _746 = _742;
      _747 = _743;
   }
   float _752 = c_mat_opacity * _52.w;
   float _753 = _752 * c_opacity_scale;
   float _754 = _753 * c_global_opacity;
   float _755 = _745 * COLOR.x;
   float _756 = _746 * COLOR.y;
   float _757 = _747 * COLOR.z;
   int _758 = c_bool & 16;
   bool _759 = (_758 == 0);
   if (!_759)
   {
      float _763 = c_tone_control.x * _755;
      float _764 = c_tone_control.x * _756;
      float _765 = c_tone_control.x * _757;
      float _767 = log2(_763);
      float _768 = log2(_764);
      float _769 = log2(_765);
      float _770 = _767 * c_tone_control.y;
      float _771 = _768 * c_tone_control.y;
      float _772 = _769 * c_tone_control.y;
      float _773 = exp2(_770);
      float _774 = exp2(_771);
      float _775 = exp2(_772);
      _823 = _773;
      _824 = _774;
      _825 = _775;
   }
   else
   {
      if (_566)
      {
         float _780 = c_tone_control.x * _755;
         float _781 = c_tone_control.x * _756;
         float _782 = c_tone_control.x * _757;
         float _783 = _780 * 2.0f;
         float _784 = _781 * 2.0f;
         float _785 = _782 * 2.0f;
         float _786 = _780 * 0.30000001192092896f;
         float _787 = _781 * 0.30000001192092896f;
         float _788 = _782 * 0.30000001192092896f;
         float _789 = _786 + 0.05000000074505806f;
         float _790 = _787 + 0.05000000074505806f;
         float _791 = _788 + 0.05000000074505806f;
         float _792 = _789 * _783;
         float _793 = _790 * _784;
         float _794 = _791 * _785;
         float _795 = _792 + 0.004000000189989805f;
         float _796 = _793 + 0.004000000189989805f;
         float _797 = _794 + 0.004000000189989805f;
         float _798 = _786 + 0.5f;
         float _799 = _787 + 0.5f;
         float _800 = _788 + 0.5f;
         float _801 = _798 * _783;
         float _802 = _799 * _784;
         float _803 = _800 * _785;
         float _804 = _801 + 0.06000000238418579f;
         float _805 = _802 + 0.06000000238418579f;
         float _806 = _803 + 0.06000000238418579f;
         float _807 = _795 / _804;
         float _808 = _796 / _805;
         float _809 = _797 / _806;
         float _810 = _807 + -0.06666666269302368f;
         float _811 = _808 + -0.06666666269302368f;
         float _812 = _809 + -0.06666666269302368f;
         float _813 = _810 * 1.3790643215179443f;
         float _814 = _811 * 1.3790643215179443f;
         float _815 = _812 * 1.3790643215179443f;
         float _816 = YKR_MaterialCurve(_813, _780);
         float _817 = YKR_MaterialCurve(_814, _781);
         float _818 = YKR_MaterialCurve(_815, _782);
         float _819 = sqrt(_816);
         float _820 = sqrt(_817);
         float _821 = sqrt(_818);
         _823 = _819;
         _824 = _820;
         _825 = _821;
      }
      else
      {
         _823 = _755;
         _824 = _756;
         _825 = _757;
      }
   }
   int _826 = c_bool & 4;
   bool _827 = (_826 == 0);
   if (_827)
   {
      float _829 = saturate(COLOR_1.x);
      float _834 = (c_fog_param[0].x) - _823;
      float _835 = (c_fog_param[0].y) - _824;
      float _836 = (c_fog_param[0].z) - _825;
      float _837 = _834 * _829;
      float _838 = _835 * _829;
      float _839 = _836 * _829;
      _841 = _837;
      _842 = _838;
      _843 = _839;
   }
   else
   {
      _841 = COLOR_1.x;
      _842 = COLOR_1.y;
      _843 = COLOR_1.z;
   }
   float _844 = _841 + _823;
   float _845 = _842 + _824;
   float _846 = _843 + _825;
   if (!_566)
   {
      float _854 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _855 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _856 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _858 = (c_ps_user[12].w) * COLOR.x;
      float _859 = (c_ps_user[12].w) * COLOR.y;
      float _860 = (c_ps_user[12].w) * COLOR.z;
      float _861 = _858 + _844;
      float _862 = _861 + _854;
      float _863 = _859 + _845;
      float _864 = _863 + _855;
      float _865 = _860 + _846;
      float _866 = _865 + _856;
      _868 = _862;
      _869 = _864;
      _870 = _866;
   }
   else
   {
      _868 = _844;
      _869 = _845;
      _870 = _846;
   }
   float _876 = (c_ps_col_scl[0].x) * _868;
   float _877 = (c_ps_col_scl[0].y) * _869;
   float _878 = (c_ps_col_scl[0].z) * _870;
   float _879 = (c_ps_col_scl[0].w) * _754;
   SV_Target.x = _876;
   SV_Target.y = _877;
   SV_Target.z = _878;
   SV_Target.w = _879;
   int _882 = c_bool & 8;
   bool _883 = (_882 != 0);
   float _884 = select(_883, _868, _876);
   float _885 = select(_883, _869, _877);
   float _886 = select(_883, _870, _878);
   float _887 = select(_883, _754, _879);
   SV_Target.x = _884;
   SV_Target.y = _885;
   SV_Target.z = _886;
   SV_Target.w = _887;
   return SV_Target;
}
