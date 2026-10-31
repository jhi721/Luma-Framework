// Yakuza Kiwami material s_p1dzt[rd][rs] (0x78D0B332, DXIL SM 6.0), decompiled verbatim (Shaders/Decompiler/decomp.exe) with two edits,
// applied by a script: the inline Hable tonemap's saturate goes through
// YKR_MaterialCurve (tangent continuation in HDR), and the light pre-pass buffer t14 through YKR_LightBuffer.

#include "Includes/Common.hlsl"

Texture2D<float4> s0_t : register(t0);

Texture2D<float4> s2_t : register(t2);

Texture2D<float4> s4_t : register(t4);

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

SamplerState s4_s : register(s4);

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
   float4 _58 = s2_t.Sample(s2_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _62 = s0_t.Sample(s0_s, float2(TEXCOORD.x, TEXCOORD.y));
   float _67 = _62.x * _62.x;
   float _68 = _62.y * _62.y;
   float _69 = _62.z * _62.z;
   float4 _70 = s5_t.Sample(s5_s, float2(TEXCOORD.x, TEXCOORD.y));
   float4 _73 = s4_t.Sample(s4_s, float2(TEXCOORD.z, TEXCOORD.w));
   float _75 = 1.0f - _73.x;
   float _76 = _75 * _58.z;
   float _77 = 1.0f - _76;
   float _80 = _70.x * 2.0f;
   float _81 = _70.y * 2.0f;
   float _82 = _80 + -1.0f;
   float _83 = _81 + -1.0f;
   float _84 = dot(float2(_82, _83), float2(_82, _83));
   float _85 = 1.0f - _84;
   float _86 = saturate(_85);
   float _87 = sqrt(_86);
   float _88 = TEXCOORD_2.y * TEXCOORD_3.z;
   float _89 = TEXCOORD_2.z * TEXCOORD_3.y;
   float _90 = _88 - _89;
   float _91 = TEXCOORD_2.z * TEXCOORD_3.x;
   float _92 = TEXCOORD_2.x * TEXCOORD_3.z;
   float _93 = _91 - _92;
   float _94 = TEXCOORD_2.x * TEXCOORD_3.y;
   float _95 = TEXCOORD_2.y * TEXCOORD_3.x;
   float _96 = _94 - _95;
   float _97 = _82 * TEXCOORD_3.x;
   float _98 = _82 * TEXCOORD_3.y;
   float _99 = _82 * TEXCOORD_3.z;
   float _100 = _83 * TEXCOORD_3.w;
   float _101 = _100 * _90;
   float _102 = _100 * _93;
   float _103 = _100 * _96;
   float _104 = _101 + _97;
   float _105 = _102 + _98;
   float _106 = _103 + _99;
   float _107 = _87 * TEXCOORD_2.x;
   float _108 = _87 * TEXCOORD_2.y;
   float _109 = _87 * TEXCOORD_2.z;
   float _110 = _104 + _107;
   float _111 = _105 + _108;
   float _112 = _106 + _109;
   float _113 = dot(float3(_110, _111, _112), float3(_110, _111, _112));
   float _114 = rsqrt(_113);
   float _115 = _110 * _114;
   float _116 = _111 * _114;
   float _117 = _112 * _114;
   float _122 = dot(float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z), float3(TEXCOORD_1.x, TEXCOORD_1.y, TEXCOORD_1.z));
   float _123 = rsqrt(_122);
   float _124 = _123 * TEXCOORD_1.x;
   float _125 = _123 * TEXCOORD_1.y;
   float _126 = _123 * TEXCOORD_1.z;
   float _127 = _124 + (c_light_vec[0].x);
   float _128 = _125 + (c_light_vec[0].y);
   float _129 = _126 + (c_light_vec[0].z);
   float _130 = dot(float3(_127, _128, _129), float3(_127, _128, _129));
   float _131 = rsqrt(_130);
   float _132 = _127 * _131;
   float _133 = _128 * _131;
   float _134 = _129 * _131;
   float _138 = (c_attribute_ps[2].x) * TEXCOORD.x;
   float _139 = (c_attribute_ps[2].y) * TEXCOORD.y;
   float4 _140 = s7_t.Sample(s7_s, float2(_138, _139));
   float _145 = _140.x * _140.x;
   float _146 = _140.y * _140.y;
   float _147 = _140.z * _140.z;
   float _148 = 1.0f - _145;
   float _149 = 1.0f - _146;
   float _150 = 1.0f - _147;
   float _151 = 1.0f - _58.z;
   float _152 = 1.0f - _140.w;
   float _153 = _152 * _151;
   float _154 = _153 * _148;
   float _155 = _153 * _149;
   float _156 = _153 * _150;
   float _157 = _154 + _145;
   float _158 = _155 + _146;
   float _159 = _156 + _147;
   float _160 = _67 * _157;
   float _161 = _68 * _158;
   float _162 = _69 * _159;
   float _163 = _116 + 0.10000000149011612f;
   float _164 = saturate(_163);
   float _167 = (c_attribute_ps[2].w) + 2.0f;
   float _169 = (c_attribute_ps[2].z) + 2.5f;
   float _170 = -0.0f - _124;
   float _171 = -0.0f - _125;
   float _172 = -0.0f - _126;
   float _173 = dot(float3(_170, _171, _172), float3(TEXCOORD_2.x, TEXCOORD_2.y, TEXCOORD_2.z));
   float _174 = 1.0f - _173;
   float _175 = saturate(_174);
   float _176 = dot(float3(_170, _171, _172), float3(_115, _116, _117));
   float _177 = 1.0f - _176;
   float _178 = saturate(_177);
   float _179 = _178 * _175;
   float _180 = log2(_179);
   float _181 = _180 * _167;
   float _182 = exp2(_181);
   float _183 = _182 * _169;
   int _186 = c_bool & 4096;
   bool _187 = (_186 == 0);
   float _196;
   float _220;
   float _221;
   float _233;
   float _234;
   float _422;
   float _423;
   float _424;
   float _425;
   float _426;
   float _427;
   float _460;
   float _461;
   float _462;
   float _463;
   float _628;
   float _795;
   float _796;
   float _797;
   float _873;
   float _874;
   float _875;
   float _891;
   float _892;
   float _893;
   float _918;
   float _919;
   float _920;
   if (!_187)
   {
      float _192 = (c_ps_user[9].w) * _180;
      float _193 = exp2(_192);
      float _194 = _193 * (c_ps_user[9].z);
      _196 = _194;
   }
   else
   {
      _196 = _183;
   }
   float _197 = _164 * _196;
   float _198 = _58.y * _197;
   float _201 = _198 * (c_ps_user[2].x);
   float _202 = _201 + 1.0f;
   float _203 = _160 * _202;
   float _204 = _161 * _202;
   float _205 = _162 * _202;
   float _209 = c_rt_size.z * SV_Position.x;
   float _210 = c_rt_size.w * SV_Position.y;
   int _213 = c_bool & 8;
   bool _214 = (_213 == 0);
   if (!_214)
   {
      float4 _216 = s13_t.Sample(s13_s, float2(_209, _210));
      _220 = _216.x;
      _221 = _216.y;
   }
   else
   {
      _220 = 1.0f;
      _221 = 1.0f;
   }
   if (!_214)
   {
      float _226 = (c_sdw_param_ch[0].x) * _220;
      float _227 = (c_sdw_param_ch[0].z) * _221;
      float _230 = _226 + (c_sdw_param_ch[0].y);
      float _231 = _227 + (c_sdw_param_ch[0].w);
      _233 = _230;
      _234 = _231;
   }
   else
   {
      _233 = _220;
      _234 = _221;
   }
   float _235 = min(_233, _234);
   float _244 = (c_light_vec[1].x) + _124;
   float _245 = (c_light_vec[1].y) + _125;
   float _246 = (c_light_vec[1].z) + _126;
   float _247 = dot(float3(_244, _245, _246), float3(_244, _245, _246));
   float _248 = rsqrt(_247);
   float _249 = _244 * _248;
   float _250 = _245 * _248;
   float _251 = _246 * _248;
   float _252 = -0.0f - (c_light_vec[1].x);
   float _253 = -0.0f - (c_light_vec[1].y);
   float _254 = -0.0f - (c_light_vec[1].z);
   float _255 = -0.0f - (c_light_vec[0].x);
   float _256 = -0.0f - (c_light_vec[0].y);
   float _257 = -0.0f - (c_light_vec[0].z);
   float _258 = dot(float3(_124, _125, _126), float3(_115, _116, _117));
   float _259 = _258 * 2.0f;
   float _260 = _259 * _115;
   float _261 = _259 * _116;
   float _262 = _259 * _117;
   float _263 = _124 - _260;
   float _264 = _125 - _261;
   float _265 = _126 - _262;
   float _266 = dot(float3(_263, _264, _265), float3(_255, _256, _257));
   float _267 = dot(float3(_263, _264, _265), float3(_252, _253, _254));
   float _268 = saturate(_266);
   float _269 = saturate(_267);
   float _270 = log2(_268);
   float _271 = log2(_269);
   float _272 = _270 * c_mat_power;
   float _273 = _271 * c_mat_power;
   float _274 = exp2(_272);
   float _275 = exp2(_273);
   float _280 = c_mat_fresnel * c_tone_control.w;
   float _281 = c_mat_power * 0.03974359855055809f;
   float _282 = _281 + 0.0856831967830658f;
   float _283 = _275 * _282;
   float _284 = dot(float3(_124, _125, _126), float3(_132, _133, _134));
   float _285 = dot(float3(_124, _125, _126), float3(_249, _250, _251));
   float _286 = 1.0f - _284;
   float _287 = 1.0f - _285;
   float _288 = _286 * _286;
   float _289 = _287 * _287;
   float _290 = 1.0f - _280;
   float _291 = _288 * _288;
   float _292 = _291 * _286;
   float _293 = _292 * _290;
   float _294 = _287 * _290;
   float _295 = _289 * _289;
   float _296 = _295 * _294;
   float _297 = _293 + _280;
   float _298 = _296 + _280;
   float _299 = dot(float3(_115, _116, _117), float3(_255, _256, _257));
   float _300 = saturate(_299);
   float _301 = dot(float3(_115, _116, _117), float3(_252, _253, _254));
   float _302 = saturate(_301);
   float _303 = _283 * _302;
   float _304 = _303 * _298;
   float _313 = c_specular_scale.x * c_mat_specular.x;
   float _314 = c_specular_scale.y * c_mat_specular.y;
   float _315 = c_specular_scale.z * c_mat_specular.z;
   float _320 = _235 * _282;
   float _321 = _320 * _274;
   float _322 = _321 * _300;
   float _323 = _322 * _297;
   float _324 = (c_light_col[0].x) * _323;
   float _325 = (c_light_col[0].y) * _323;
   float _326 = (c_light_col[0].z) * _323;
   float _331 = (c_light_col[1].x) * _304;
   float _332 = (c_light_col[1].y) * _304;
   float _333 = (c_light_col[1].z) * _304;
   float _334 = _331 + _324;
   float _335 = _332 + _325;
   float _336 = _333 + _326;
   float _337 = _313 * _334;
   float _338 = _314 * _335;
   float _339 = _315 * _336;
   int _343 = c_bool & 1;
   bool _344 = (_343 == 0);
   if (!_344)
   {
      float _346 = -0.0f - TEXCOORD_4.x;
      float _347 = -0.0f - TEXCOORD_4.y;
      float _348 = -0.0f - TEXCOORD_4.z;
      float _349 = dot(float3(_115, _116, _117), float3(_346, _347, _348));
      float _350 = saturate(_349);
      float _351 = _350 * TEXCOORD_1.w;
      float _352 = _350 * TEXCOORD_2.w;
      float _353 = _350 * TEXCOORD_4.w;
      float _354 = max(0.0f, _351);
      float _355 = max(0.0f, _352);
      float _356 = max(0.0f, _353);
      float _357 = -0.0f - TEXCOORD_5.x;
      float _358 = -0.0f - TEXCOORD_5.y;
      float _359 = -0.0f - TEXCOORD_5.z;
      float _360 = dot(float3(_115, _116, _117), float3(_357, _358, _359));
      float _361 = saturate(_360);
      float _362 = _361 * COLOR.w;
      float _363 = _361 * COLOR_1.w;
      float _364 = _361 * TEXCOORD_5.w;
      float _365 = max(0.0f, _362);
      float _366 = max(0.0f, _363);
      float _367 = max(0.0f, _364);
      float _368 = _365 + _354;
      float _369 = _366 + _355;
      float _370 = _367 + _356;
      float _371 = _124 + TEXCOORD_4.x;
      float _372 = _125 + TEXCOORD_4.y;
      float _373 = _126 + TEXCOORD_4.z;
      float _374 = dot(float3(_371, _372, _373), float3(_371, _372, _373));
      float _375 = rsqrt(_374);
      float _376 = _124 + TEXCOORD_5.x;
      float _377 = _125 + TEXCOORD_5.y;
      float _378 = _126 + TEXCOORD_5.z;
      float _379 = dot(float3(_376, _377, _378), float3(_376, _377, _378));
      float _380 = rsqrt(_379);
      float _381 = _376 * _380;
      float _382 = -0.0f - _381;
      float _383 = _377 * _380;
      float _384 = -0.0f - _383;
      float _385 = _378 * _380;
      float _386 = -0.0f - _385;
      float _387 = _371 * _375;
      float _388 = -0.0f - _387;
      float _389 = _372 * _375;
      float _390 = -0.0f - _389;
      float _391 = _373 * _375;
      float _392 = -0.0f - _391;
      float _393 = dot(float3(_388, _390, _392), float3(_115, _116, _117));
      float _394 = dot(float3(_382, _384, _386), float3(_115, _116, _117));
      float _395 = saturate(_393);
      float _396 = saturate(_394);
      float _397 = log2(_395);
      float _398 = log2(_396);
      float _399 = _397 * c_mat_power;
      float _400 = _398 * c_mat_power;
      float _401 = exp2(_399);
      float _402 = exp2(_400);
      float _403 = _401 * TEXCOORD_1.w;
      float _404 = _401 * TEXCOORD_2.w;
      float _405 = _401 * TEXCOORD_4.w;
      float _406 = _402 * COLOR.w;
      float _407 = _402 * COLOR_1.w;
      float _408 = _402 * TEXCOORD_5.w;
      float _409 = _406 + _403;
      float _410 = _407 + _404;
      float _411 = _408 + _405;
      float _412 = max(0.0f, _409);
      float _413 = max(0.0f, _410);
      float _414 = max(0.0f, _411);
      float _415 = _313 * _412;
      float _416 = _314 * _413;
      float _417 = _315 * _414;
      float _418 = min(_415, 1.0f);
      float _419 = min(_416, 1.0f);
      float _420 = min(_417, 1.0f);
      _422 = _418;
      _423 = _419;
      _424 = _420;
      _425 = _368;
      _426 = _369;
      _427 = _370;
   }
   else
   {
      _422 = 0.0f;
      _423 = 0.0f;
      _424 = 0.0f;
      _425 = 0.0f;
      _426 = 0.0f;
      _427 = 0.0f;
   }
   float _428 = c_rt_size.z * SV_Position.x;
   float _429 = c_rt_size.w * SV_Position.y;
   float4 _430 = YKR_LightBuffer(s14_t.Sample(s14_s, float2(_428, _429)));
   int _437 = c_bool & 16384;
   bool _438 = (_437 == 0);
   if (!_438)
   {
      float _440 = _430.x * 4.0f;
      float _441 = _430.y * 4.0f;
      float _442 = _430.z * 4.0f;
      float _443 = _430.w * 4.0f;
      _460 = _440;
      _461 = _441;
      _462 = _442;
      _463 = _443;
   }
   else
   {
      int _445 = c_bool & 8192;
      bool _446 = (_445 == 0);
      if (!_446)
      {
         float _448 = _430.x * 2.0f;
         float _449 = _430.y * 2.0f;
         float _450 = _430.z * 2.0f;
         float _451 = _430.w * 2.0f;
         _460 = _448;
         _461 = _449;
         _462 = _450;
         _463 = _451;
      }
      else
      {
         float _455 = _430.x / c_tone_control.z;
         float _456 = _430.y / c_tone_control.z;
         float _457 = _430.z / c_tone_control.z;
         float _458 = _430.w / c_tone_control.z;
         _460 = _455;
         _461 = _456;
         _462 = _457;
         _463 = _458;
      }
   }
   float _464 = _460 + _425;
   float _465 = _461 + _426;
   float _466 = _462 + _427;
   float _467 = _463 + _422;
   float _468 = _463 + _423;
   float _469 = _463 + _424;
   float _532 = dot(float3(_115, _116, _117), float3(_115, _116, _117));
   float _533 = rsqrt(_532);
   float _534 = _533 * _115;
   float _535 = _533 * _116;
   float _536 = _533 * _117;
   float _537 = dot(float4((c_ps_scratch[80].x), (c_ps_scratch[80].y), (c_ps_scratch[80].z), (c_ps_scratch[80].w)), float4(_534, _535, _536, 1.0f));
   float _538 = dot(float4((c_ps_scratch[81].x), (c_ps_scratch[81].y), (c_ps_scratch[81].z), (c_ps_scratch[81].w)), float4(_534, _535, _536, 1.0f));
   float _539 = dot(float4((c_ps_scratch[82].x), (c_ps_scratch[82].y), (c_ps_scratch[82].z), (c_ps_scratch[82].w)), float4(_534, _535, _536, 1.0f));
   float _540 = dot(float4((c_ps_scratch[83].x), (c_ps_scratch[83].y), (c_ps_scratch[83].z), (c_ps_scratch[83].w)), float4(_534, _535, _536, 1.0f));
   float _541 = dot(float4(_534, _535, _536, 1.0f), float4(_537, _538, _539, _540));
   float _542 = dot(float4((c_ps_scratch[84].x), (c_ps_scratch[84].y), (c_ps_scratch[84].z), (c_ps_scratch[84].w)), float4(_534, _535, _536, 1.0f));
   float _543 = dot(float4((c_ps_scratch[85].x), (c_ps_scratch[85].y), (c_ps_scratch[85].z), (c_ps_scratch[85].w)), float4(_534, _535, _536, 1.0f));
   float _544 = dot(float4((c_ps_scratch[86].x), (c_ps_scratch[86].y), (c_ps_scratch[86].z), (c_ps_scratch[86].w)), float4(_534, _535, _536, 1.0f));
   float _545 = dot(float4((c_ps_scratch[87].x), (c_ps_scratch[87].y), (c_ps_scratch[87].z), (c_ps_scratch[87].w)), float4(_534, _535, _536, 1.0f));
   float _546 = dot(float4(_534, _535, _536, 1.0f), float4(_542, _543, _544, _545));
   float _547 = dot(float4((c_ps_scratch[88].x), (c_ps_scratch[88].y), (c_ps_scratch[88].z), (c_ps_scratch[88].w)), float4(_534, _535, _536, 1.0f));
   float _548 = dot(float4((c_ps_scratch[89].x), (c_ps_scratch[89].y), (c_ps_scratch[89].z), (c_ps_scratch[89].w)), float4(_534, _535, _536, 1.0f));
   float _549 = dot(float4((c_ps_scratch[90].x), (c_ps_scratch[90].y), (c_ps_scratch[90].z), (c_ps_scratch[90].w)), float4(_534, _535, _536, 1.0f));
   float _550 = dot(float4((c_ps_scratch[91].x), (c_ps_scratch[91].y), (c_ps_scratch[91].z), (c_ps_scratch[91].w)), float4(_534, _535, _536, 1.0f));
   float _551 = dot(float4(_534, _535, _536, 1.0f), float4(_547, _548, _549, _550));
   float _552 = max(_541, 0.0625f);
   float _553 = max(_546, 0.0625f);
   float _554 = max(_551, 0.0625f);
   float _555 = _552 * (c_ps_scratch[79].x);
   float _556 = _553 * (c_ps_scratch[79].x);
   float _557 = _554 * (c_ps_scratch[79].x);
   float _562 = -0.0f - (c_light_vec[0].x);
   float _563 = -0.0f - (c_light_vec[0].y);
   float _564 = -0.0f - (c_light_vec[0].z);
   float _565 = dot(float3(_115, _116, _117), float3(_562, _563, _564));
   float _566 = -0.0f - (c_light_vec[1].x);
   float _567 = -0.0f - (c_light_vec[1].y);
   float _568 = -0.0f - (c_light_vec[1].z);
   float _569 = dot(float3(_115, _116, _117), float3(_566, _567, _568));
   float _578 = _116 * 0.5f;
   float _579 = _578 + 0.5f;
   float _580 = (c_light_col[4].x) - (c_light_vec[4].x);
   float _581 = (c_light_col[4].y) - (c_light_vec[4].y);
   float _582 = (c_light_col[4].z) - (c_light_vec[4].z);
   float _583 = _580 * _579;
   float _584 = _581 * _579;
   float _585 = _582 * _579;
   float _586 = saturate(_565);
   float _587 = saturate(_569);
   float _592 = (c_light_col[0].x) * _586;
   float _593 = (c_light_col[0].y) * _586;
   float _594 = (c_light_col[0].z) * _586;
   float _599 = (c_light_col[1].x) * _587;
   float _600 = (c_light_col[1].y) * _587;
   float _601 = (c_light_col[1].z) * _587;
   float _602 = _592 * _235;
   float _603 = _593 * _235;
   float _604 = _594 * _235;
   float _605 = (c_light_vec[4].x) + _555;
   float _606 = _605 + _583;
   float _607 = _606 + _464;
   float _608 = _607 + _599;
   float _609 = _608 + _602;
   float _610 = (c_light_vec[4].y) + _556;
   float _611 = _610 + _584;
   float _612 = _611 + _465;
   float _613 = _612 + _600;
   float _614 = _613 + _603;
   float _615 = (c_light_vec[4].z) + _557;
   float _616 = _615 + _585;
   float _617 = _616 + _466;
   float _618 = _617 + _601;
   float _619 = _618 + _604;
   int _620 = c_bool & 64;
   bool _621 = (_620 == 0);
   if (!_621)
   {
      float _623 = 1.0f - _58.y;
      float _624 = _235 * _623;
      float _625 = _624 * _586;
      float _626 = _625 + _58.y;
      _628 = _626;
   }
   else
   {
      _628 = _58.y;
   }
   float _629 = _467 + _337;
   float _630 = _468 + _338;
   float _631 = _469 + _339;
   int _632 = c_bool & 32768;
   bool _633 = (_632 == 0);
   if (!_633)
   {
      float _645 = (c_ps_user[11].x) * _203;
      float _646 = (c_ps_user[11].x) * _204;
      float _647 = (c_ps_user[11].x) * _205;
      float _649 = _115 + 1.0f;
      float _650 = _116 + 1.0f;
      float _651 = _117 + 1.0f;
      float _652 = _649 * 0.5f;
      float _653 = _650 * 0.5f;
      float _654 = _651 * 0.5f;
      float _655 = saturate(_652);
      float _656 = saturate(_653);
      float _657 = saturate(_654);
      float _658 = (c_ps_user[11].y) * _655;
      float _659 = _656 * (c_ps_user[11].y);
      float _660 = _657 * (c_ps_user[11].y);
      float _670 = (c_ps_user[11].z) * _58.x;
      float _672 = (c_ps_user[11].w) * _629;
      float _675 = (c_ps_user[12].x) * _235;
      float _677 = (c_ps_user[12].y) * _58.y;
      float _682 = _177 * _177;
      float _683 = _177 * 0.9722222089767456f;
      float _684 = _682 * _682;
      float _685 = _684 * _683;
      float _686 = _685 + 0.02777777425944805f;
      float _687 = (c_ps_user[13].z) * _686;
      float _689 = (c_ps_user[13].w) * _606;
      float _690 = (c_ps_user[13].w) * _611;
      float _691 = (c_ps_user[13].w) * _616;
      float _694 = (c_ps_user[14].y) * _592;
      float _695 = (c_ps_user[14].y) * _593;
      float _696 = (c_ps_user[14].y) * _594;
      float _698 = (c_ps_user[14].z) * _599;
      float _699 = (c_ps_user[14].z) * _600;
      float _700 = (c_ps_user[14].z) * _601;
      float _702 = (c_ps_user[14].w) * _464;
      float _703 = (c_ps_user[14].w) * _465;
      float _704 = (c_ps_user[14].w) * _466;
      float _709 = (c_ps_user[15].z) * _467;
      float _710 = (c_ps_user[15].z) * _468;
      float _711 = (c_ps_user[15].z) * _469;
      float _712 = (c_ps_user[15].y) + (c_ps_user[15].x);
      float _713 = c_specular_scale.x * _712;
      float _714 = _713 * _629;
      float _715 = _694 + (c_ps_user[13].x);
      float _716 = _715 + _698;
      float _717 = c_diffuse_scale.x * _716;
      float _718 = _717 * c_mat_diffuse.x;
      float _719 = (c_ps_user[13].y) + _670;
      float _720 = _719 * c_specular_scale.x;
      float _721 = _720 + _714;
      float _722 = _721 * c_mat_specular.x;
      float _723 = _658 + _645;
      float _724 = _723 + _672;
      float _725 = _724 + _675;
      float _726 = _725 + _677;
      float _727 = _726 + _689;
      float _728 = _727 + _687;
      float _729 = _728 + _702;
      float _730 = _729 + _709;
      float _731 = _730 + _718;
      float _732 = _731 + _722;
      float _733 = c_specular_scale.y * _712;
      float _734 = _733 * _630;
      float _735 = _695 + (c_ps_user[13].x);
      float _736 = _735 + _699;
      float _737 = c_diffuse_scale.y * _736;
      float _738 = _737 * c_mat_diffuse.y;
      float _739 = _719 * c_specular_scale.y;
      float _740 = _739 + _734;
      float _741 = _740 * c_mat_specular.y;
      float _742 = _672 + _646;
      float _743 = _742 + _659;
      float _744 = _743 + _675;
      float _745 = _744 + _677;
      float _746 = _745 + _690;
      float _747 = _746 + _687;
      float _748 = _747 + _703;
      float _749 = _748 + _710;
      float _750 = _749 + _738;
      float _751 = _750 + _741;
      float _752 = c_specular_scale.z * _712;
      float _753 = _752 * _631;
      float _754 = _696 + (c_ps_user[13].x);
      float _755 = _754 + _700;
      float _756 = c_diffuse_scale.z * _755;
      float _757 = _756 * c_mat_diffuse.z;
      float _758 = _719 * c_specular_scale.z;
      float _759 = _758 + _753;
      float _760 = _759 * c_mat_specular.z;
      float _761 = _672 + _647;
      float _762 = _761 + _660;
      float _763 = _762 + _675;
      float _764 = _763 + _677;
      float _765 = _764 + _691;
      float _766 = _765 + _687;
      float _767 = _766 + _704;
      float _768 = _767 + _711;
      float _769 = _768 + _757;
      float _770 = _769 + _760;
      float _771 = saturate(_732);
      float _772 = saturate(_751);
      float _773 = saturate(_770);
      _795 = _771;
      _796 = _772;
      _797 = _773;
   }
   else
   {
      float _775 = _628 * _77;
      float _776 = c_diffuse_scale.x * c_mat_diffuse.x;
      float _777 = _776 * _203;
      float _778 = _777 * _609;
      float _779 = _778 * _775;
      float _780 = c_diffuse_scale.y * c_mat_diffuse.y;
      float _781 = _780 * _204;
      float _782 = _781 * _614;
      float _783 = _782 * _775;
      float _784 = c_diffuse_scale.z * c_mat_diffuse.z;
      float _785 = _784 * _205;
      float _786 = _785 * _619;
      float _787 = _786 * _775;
      float _788 = _629 * _58.x;
      float _789 = _630 * _58.x;
      float _790 = _631 * _58.x;
      float _791 = _779 + _788;
      float _792 = _783 + _789;
      float _793 = _787 + _790;
      _795 = _791;
      _796 = _792;
      _797 = _793;
   }
   float _802 = c_mat_opacity * _62.w;
   float _803 = _802 * c_opacity_scale;
   float _804 = _803 * c_global_opacity;
   float _805 = _795 * COLOR.x;
   float _806 = _796 * COLOR.y;
   float _807 = _797 * COLOR.z;
   int _808 = c_bool & 16;
   bool _809 = (_808 == 0);
   if (!_809)
   {
      float _813 = c_tone_control.x * _805;
      float _814 = c_tone_control.x * _806;
      float _815 = c_tone_control.x * _807;
      float _817 = log2(_813);
      float _818 = log2(_814);
      float _819 = log2(_815);
      float _820 = _817 * c_tone_control.y;
      float _821 = _818 * c_tone_control.y;
      float _822 = _819 * c_tone_control.y;
      float _823 = exp2(_820);
      float _824 = exp2(_821);
      float _825 = exp2(_822);
      _873 = _823;
      _874 = _824;
      _875 = _825;
   }
   else
   {
      if (_633)
      {
         float _830 = c_tone_control.x * _805;
         float _831 = c_tone_control.x * _806;
         float _832 = c_tone_control.x * _807;
         float _833 = _830 * 2.0f;
         float _834 = _831 * 2.0f;
         float _835 = _832 * 2.0f;
         float _836 = _830 * 0.30000001192092896f;
         float _837 = _831 * 0.30000001192092896f;
         float _838 = _832 * 0.30000001192092896f;
         float _839 = _836 + 0.05000000074505806f;
         float _840 = _837 + 0.05000000074505806f;
         float _841 = _838 + 0.05000000074505806f;
         float _842 = _839 * _833;
         float _843 = _840 * _834;
         float _844 = _841 * _835;
         float _845 = _842 + 0.004000000189989805f;
         float _846 = _843 + 0.004000000189989805f;
         float _847 = _844 + 0.004000000189989805f;
         float _848 = _836 + 0.5f;
         float _849 = _837 + 0.5f;
         float _850 = _838 + 0.5f;
         float _851 = _848 * _833;
         float _852 = _849 * _834;
         float _853 = _850 * _835;
         float _854 = _851 + 0.06000000238418579f;
         float _855 = _852 + 0.06000000238418579f;
         float _856 = _853 + 0.06000000238418579f;
         float _857 = _845 / _854;
         float _858 = _846 / _855;
         float _859 = _847 / _856;
         float _860 = _857 + -0.06666666269302368f;
         float _861 = _858 + -0.06666666269302368f;
         float _862 = _859 + -0.06666666269302368f;
         float _863 = _860 * 1.3790643215179443f;
         float _864 = _861 * 1.3790643215179443f;
         float _865 = _862 * 1.3790643215179443f;
         float _866 = YKR_MaterialCurve(_863, _830);
         float _867 = YKR_MaterialCurve(_864, _831);
         float _868 = YKR_MaterialCurve(_865, _832);
         float _869 = sqrt(_866);
         float _870 = sqrt(_867);
         float _871 = sqrt(_868);
         _873 = _869;
         _874 = _870;
         _875 = _871;
      }
      else
      {
         _873 = _805;
         _874 = _806;
         _875 = _807;
      }
   }
   int _876 = c_bool & 4;
   bool _877 = (_876 == 0);
   if (_877)
   {
      float _879 = saturate(COLOR_1.x);
      float _884 = (c_fog_param[0].x) - _873;
      float _885 = (c_fog_param[0].y) - _874;
      float _886 = (c_fog_param[0].z) - _875;
      float _887 = _884 * _879;
      float _888 = _885 * _879;
      float _889 = _886 * _879;
      _891 = _887;
      _892 = _888;
      _893 = _889;
   }
   else
   {
      _891 = COLOR_1.x;
      _892 = COLOR_1.y;
      _893 = COLOR_1.z;
   }
   float _894 = _891 + _873;
   float _895 = _892 + _874;
   float _896 = _893 + _875;
   if (!_633)
   {
      float _904 = (c_ps_user[3].x) * (c_ps_user[12].z);
      float _905 = (c_ps_user[3].y) * (c_ps_user[12].z);
      float _906 = (c_ps_user[3].z) * (c_ps_user[12].z);
      float _908 = (c_ps_user[12].w) * COLOR.x;
      float _909 = (c_ps_user[12].w) * COLOR.y;
      float _910 = (c_ps_user[12].w) * COLOR.z;
      float _911 = _908 + _894;
      float _912 = _911 + _904;
      float _913 = _909 + _895;
      float _914 = _913 + _905;
      float _915 = _910 + _896;
      float _916 = _915 + _906;
      _918 = _912;
      _919 = _914;
      _920 = _916;
   }
   else
   {
      _918 = _894;
      _919 = _895;
      _920 = _896;
   }
   float _923 = _804 - c_mat_punchthrough_ref;
   bool _924 = (_923 < 0.0f);
   if (_924)
      discard;
   float _930 = (c_ps_col_scl[0].x) * _918;
   float _931 = (c_ps_col_scl[0].y) * _919;
   float _932 = (c_ps_col_scl[0].z) * _920;
   float _933 = (c_ps_col_scl[0].w) * _804;
   SV_Target.x = _930;
   SV_Target.y = _931;
   SV_Target.z = _932;
   SV_Target.w = _933;
   int _936 = c_bool & 8;
   bool _937 = (_936 != 0);
   float _938 = select(_937, _918, _930);
   float _939 = select(_937, _919, _931);
   float _940 = select(_937, _920, _932);
   float _941 = select(_937, _804, _933);
   SV_Target.x = _938;
   SV_Target.y = _939;
   SV_Target.z = _940;
   SV_Target.w = _941;
   return SV_Target;
}
